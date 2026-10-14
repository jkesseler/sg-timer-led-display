import { test, expect } from '@playwright/test'
import { baseURL, login } from '../helpers/login'
import type { Page } from '@playwright/test'

// Runs against a database filled by `npm run seed`: the hidden UUID `id` field must not block admin forms.
const UUID_IN_URL = /\/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/

async function saveAndExpectUuid(page: Page): Promise<void> {
  await page.click('#action-save')
  await expect(page).toHaveURL(UUID_IN_URL, { timeout: 15000 })
}

async function pickOption(page: Page, fieldName: string, optionText: string): Promise<void> {
  await page.locator(`#field-${fieldName} .rs__control`).click()
  await page.locator('.rs__option', { hasText: optionText }).first().click()
}

test.beforeEach(async ({ page }) => {
  await login({ page, serverURL: baseURL, user: { email: 'admin@timer.tsd', password: 'qazwsx123' } })
})

test('creates a shooter with a UUID id', async ({ page }) => {
  await page.goto(`${baseURL}/admin/collections/shooters/create`)
  await page.fill('#field-firstName', 'Admin')
  await page.fill('#field-lastName', 'Created')
  await page.fill('#field-knsaNumber', `9${Date.now() % 100000}`)

  await saveAndExpectUuid(page)
})

test('creates a squad member through the relationship pickers', async ({ page }) => {
  await page.goto(`${baseURL}/admin/collections/squad-members/create`)
  await pickOption(page, 'squad', '08:00')
  await pickOption(page, 'shooter', 'Sam Visser')
  await pickOption(page, 'discipline', 'OKP')
  await page.fill('#field-startingPosition', '9')

  await saveAndExpectUuid(page)
})

test('creates a match with a date and a device', async ({ page }) => {
  await page.goto(`${baseURL}/admin/collections/matches/create`)
  await page.fill('#field-label', 'Admin match')
  await page.locator('#field-date input').fill('10/01/2026')
  await page.keyboard.press('Escape')
  await pickOption(page, 'device', 'Test lane')

  await saveAndExpectUuid(page)
})
