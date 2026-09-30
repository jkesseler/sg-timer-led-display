import mqtt from 'mqtt'
import { test, expect } from '@playwright/test'
import type { Page } from '@playwright/test'

// Runs against a database filled by `npm run seed` (scripts/seed-match.ts).
const baseURL = process.env.PLAYWRIGHT_BASE_URL ?? 'http://localhost:3000'
const brokerURL = process.env.MQTT_BROKER_URL ?? 'mqtt://localhost:1883'
const DEVICE_ID = 'TKUI01'

async function publishTimerSession(lastShotTimeMs: number): Promise<void> {
  const client = await mqtt.connectAsync(brokerURL)
  const sessionId = Date.now()

  await client.publishAsync(`timer/${DEVICE_ID}/presence`, 'online', { retain: true })
  await client.publishAsync(`timer/${DEVICE_ID}/session/started`, JSON.stringify({ sessionId, timestamp: Date.now(), startDelaySeconds: 0 }))
  await client.publishAsync(`timer/${DEVICE_ID}/shot/detected`, JSON.stringify({
    sessionId, shotNumber: 1, absoluteTimeMs: lastShotTimeMs, splitTimeMs: lastShotTimeMs, deviceModel: 'Test', isFirstShot: true, timestamp: Date.now(),
  }))
  await client.publishAsync(`timer/${DEVICE_ID}/session/stopped`, JSON.stringify({ sessionId, timestamp: Date.now(), totalShots: 1, lastShotTimeMs }))
  await client.endAsync()
}

async function logIn(page: Page): Promise<void> {
  await page.goto(`${baseURL}/timekeeper/login`)
  await page.fill('#email', 'admin@timer.tsd')
  await page.fill('#password', 'qazwsx123')
  await page.click('button[type="submit"]')
  await page.waitForURL(`${baseURL}/timekeeper`)
}

test('a scanned shooter\'s timer result lands on the card and survives a reload from the server copy', async ({ page }) => {
  await logIn(page)
  await expect(page.getByText('MQTT connected')).toBeVisible({ timeout: 15000 })

  await page.fill('.tk-scan-input', '111111')
  await page.click('button:has-text("Arm")')
  await expect(page.locator('.tk-status-badge')).toContainText('Alex Rivera')

  await publishTimerSession(4321)

  const firstRow = page.locator('.tk-card-row', { hasText: 'Alex Rivera' }).first()
  await expect(firstRow.locator('.tk-round-cell').first()).toContainText('04.32')
  await expect(page.locator('.tk-pill').first()).toHaveText('Saved')

  await page.evaluate(() => localStorage.clear())
  await page.reload()

  const reloadedRow = page.locator('.tk-card-row', { hasText: 'Alex Rivera' }).first()
  await expect(reloadedRow.locator('.tk-round-cell').first()).toContainText('04.32')
})

test('a timer result with nobody armed is kept as unassigned and can be assigned', async ({ page }) => {
  await logIn(page)
  await expect(page.getByText('MQTT connected')).toBeVisible({ timeout: 15000 })

  await publishTimerSession(7777)

  const unassigned = page.locator('.tk-card--warning')
  await expect(unassigned).toContainText('07.77')

  await unassigned.getByLabel('Shooter').selectOption({ label: 'Jamie Chen' })
  await unassigned.getByRole('button', { name: 'Assign' }).click()

  const row = page.locator('.tk-card-row', { hasText: 'Jamie Chen' }).first()
  await expect(row.locator('.tk-round-cell').first()).toContainText('07.77')
})

test('/display shows the shooter armed on the timekeeper board', async ({ page, browser }) => {
  await logIn(page)
  await expect(page.getByText('MQTT connected')).toBeVisible({ timeout: 15000 })
  await page.fill('.tk-scan-input', '222222')
  await page.click('button:has-text("Arm")')
  await expect(page.locator('.tk-pill').first()).toHaveText('Saved')

  const client = await mqtt.connectAsync(brokerURL)
  await client.publishAsync(`timer/${DEVICE_ID}/presence`, 'online', { retain: true })
  await client.publishAsync(`timer/${DEVICE_ID}/connection/state`, JSON.stringify({ state: 'CONNECTED', deviceName: 'Test timer', timestamp: Date.now() }), { retain: true })
  await client.endAsync()

  const display = await (await browser.newContext()).newPage()
  await display.goto(`${baseURL}/display`)
  await expect(display.getByText('Timer ready')).toBeVisible({ timeout: 15000 })

  // The roster is shown on the session screen, so start a stage.
  const starter = await mqtt.connectAsync(brokerURL)
  await starter.publishAsync(`timer/${DEVICE_ID}/session/started`, JSON.stringify({ sessionId: Date.now(), timestamp: Date.now(), startDelaySeconds: 0 }))
  await starter.endAsync()

  await expect(display.getByText('Jamie Chen')).toBeVisible({ timeout: 15000 })
})
