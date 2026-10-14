import { test, expect } from '@playwright/test'
import type { Locator, Page, Route } from '@playwright/test'
import { openBoard, publishSessionStarted, publishTimerSession, seedMatch } from '../helpers/timekeeper'

const ACTION_ID_PATTERN = /"actionId":"([0-9a-f-]{36})"/g

test.beforeEach(async ({ page }) => {
  seedMatch()
  // Signing off prints a score sheet; headless Chromium must not wait on a print dialog.
  await page.addInitScript(() => {
    window.print = () => {}
  })
  await openBoard(page)
})

function cardRow(page: Page, shooterName: string): Locator {
  return page.locator('.tk-card-row', { hasText: shooterName }).first()
}

function roundCell(row: Locator, heading: string): Locator {
  return row.locator('.tk-round-cell', { hasText: heading })
}

async function openRoundEditor(row: Locator, round: number): Promise<Locator> {
  await roundCell(row, `R${round}`).click()
  const editor = row.locator('.tk-round-editor')
  await expect(editor).toBeVisible()

  return editor
}

async function enterRoundTime(row: Locator, round: number, seconds: string): Promise<void> {
  const editor = await openRoundEditor(row, round)
  await editor.getByLabel('Time (s)').fill(seconds)
  await editor.getByRole('button', { name: 'Save time' }).click()
  await editor.getByRole('button', { name: 'Close' }).click()
}

async function selectSquad(page: Page, startTime: string): Promise<void> {
  // The trailing " -" stops "09:00" matching "08:00 - 09:00".
  await page.locator('.tk-tab', { hasText: `${startTime} -` }).click()
}

test('five timed rounds give the mean of the three fastest as the final score', async ({ page }) => {
  const row = cardRow(page, 'Alex Rivera')
  const roundTimesMs = [3000, 2000, 2000, 1000, 1000]

  for (const [index, timeMs] of roundTimesMs.entries()) {
    await row.locator('.tk-queue-row__name').click()
    await expect(page.locator('.tk-status-badge')).toContainText('Alex Rivera')
    await publishTimerSession(timeMs)
    await expect(roundCell(row, `R${index + 1}`)).toContainText(`0${timeMs / 1000}.00`)
  }

  await expect(row.locator('.tk-score')).toHaveText('01.33')
  await expect(row.locator('.tk-queue-row__name')).toBeDisabled()
})

test('a reshoot time replaces the original round time in the score', async ({ page }) => {
  const row = cardRow(page, 'Alex Rivera')

  await enterRoundTime(row, 1, '0.50')
  const editor = await openRoundEditor(row, 1)
  await editor.getByRole('button', { name: 'RS', exact: true }).click()
  await editor.getByLabel('Reshoot (s)').fill('4')
  await editor.getByRole('button', { name: 'Save reshoot' }).click()
  await editor.getByRole('button', { name: 'Close' }).click()

  for (const [index, seconds] of ['1', '1', '2', '2'].entries()) {
    await enterRoundTime(row, index + 2, seconds)
  }

  await expect(roundCell(row, 'RS').last()).toContainText('04.00')
  // Counting the original 0.50 instead would give 00.83.
  await expect(row.locator('.tk-score')).toHaveText('01.33')
})

test('a DQ voids the shooter\'s cards in every squad and can be undone', async ({ page }) => {
  await selectSquad(page, '08:00')
  const morningRow = cardRow(page, 'Morgan Blake')
  await morningRow.getByRole('button', { name: 'DQ', exact: true }).click()
  await morningRow.getByLabel('Disqualification reason').fill('Unsafe gun handling')
  await morningRow.getByRole('button', { name: 'Disqualify' }).click()
  await expect(morningRow.locator('.tk-score')).toHaveText('DQ')

  await selectSquad(page, '09:00')
  const laterRow = cardRow(page, 'Morgan Blake')
  await expect(laterRow.locator('.tk-score')).toHaveText('DQ')

  await laterRow.getByRole('button', { name: 'Undo DQ' }).click()
  await expect(laterRow.locator('.tk-score')).not.toHaveText('DQ')

  await selectSquad(page, '08:00')
  await expect(cardRow(page, 'Morgan Blake').locator('.tk-score')).not.toHaveText('DQ')
})

test('a signed card can still be corrected', async ({ page }) => {
  const row = cardRow(page, 'Alex Rivera')
  for (const [index, seconds] of ['1', '1', '1', '2', '2'].entries()) {
    await enterRoundTime(row, index + 1, seconds)
  }
  await row.getByRole('button', { name: 'Signed' }).click()
  await expect(row.getByRole('button', { name: 'Unsign' })).toBeVisible()

  await enterRoundTime(row, 1, '0.70')

  await expect(roundCell(row, 'R1')).toContainText('00.70')
  await expect(row.locator('.tk-score')).toHaveText('00.90')
  await expect(row.getByRole('button', { name: 'Unsign' })).toBeVisible()
})

test('a session that never stops does not freeze the board', async ({ page }) => {
  const alexRow = cardRow(page, 'Alex Rivera')
  await alexRow.locator('.tk-queue-row__name').click()
  await publishSessionStarted(Date.now())
  await expect(page.locator('.tk-status-badge')).toContainText('Alex Rivera')

  const jamieRow = cardRow(page, 'Jamie Chen')
  await enterRoundTime(jamieRow, 1, '5.55')
  await expect(roundCell(jamieRow, 'R1')).toContainText('05.55')

  await page.getByRole('button', { name: 'Cancel turn' }).click()
  await expect(page.locator('.tk-status-badge')).toContainText('Nobody armed')
})

test('edits made while the server is unreachable are saved once, with one audit entry per action', async ({ page }) => {
  // The retry backoff doubles up to 30 s.
  test.setTimeout(120000)
  const sentActionIds = new Set<string>()
  let phase: 'down' | 'response-lost' | 'up' = 'down'
  let lostResponses = 0

  // Server actions are POSTs carrying a Next-Action header.
  await page.route('**/timekeeper', async (route: Route) => {
    const request = route.request()
    if (request.method() !== 'POST' || !request.headers()['next-action']) {
      await route.continue()

      return
    }

    for (const match of (request.postData() ?? '').matchAll(ACTION_ID_PATTERN)) {
      sentActionIds.add(match[1])
    }

    if (phase === 'down') {
      await route.abort('connectionrefused')

      return
    }
    if (phase === 'response-lost' && lostResponses < 2) {
      // The server stores the write but the browser never hears back, so it retries the same entries.
      lostResponses += 1
      await route.fetch()
      await route.abort('connectionreset')

      return
    }
    await route.continue()
  })

  const row = cardRow(page, 'Jamie Chen')
  await enterRoundTime(row, 1, '3')
  await enterRoundTime(row, 2, '2')
  await expect(page.locator('.tk-pill').first()).toHaveText('Not saved to server — retrying', { timeout: 15000 })
  await expect(roundCell(row, 'R1')).toContainText('03.00')

  phase = 'response-lost'
  await expect.poll(() => lostResponses, { timeout: 30000 }).toBe(2)
  phase = 'up'
  await expect(page.locator('.tk-pill').first()).toHaveText('Saved', { timeout: 60000 })

  const matchId = await page.evaluate(() => JSON.parse(localStorage.getItem('matchState') ?? '{}').matchId as string)
  const response = await page.request.get(`/api/match-audit?where[match][equals]=${matchId}&limit=1000&depth=0`)
  expect(response.ok()).toBe(true)
  const auditActionIds: string[] = (await response.json()).docs.map((doc: { actionId: string }) => doc.actionId)

  expect(sentActionIds.size).toBeGreaterThan(0)
  expect(new Set(auditActionIds).size).toBe(auditActionIds.length)
  expect(new Set(auditActionIds)).toEqual(sentActionIds)

  await page.evaluate(() => localStorage.clear())
  await page.reload()
  const reloadedRow = cardRow(page, 'Jamie Chen')
  await expect(roundCell(reloadedRow, 'R1')).toContainText('03.00')
  await expect(roundCell(reloadedRow, 'R2')).toContainText('02.00')
})
