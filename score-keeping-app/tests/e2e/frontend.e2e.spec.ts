import { test, expect } from '@playwright/test'
import { baseURL } from '../helpers/login'

test('the home page opens the TV display', async ({ page }) => {
  await page.goto(baseURL)

  await expect(page).toHaveURL(`${baseURL}/display`)
})
