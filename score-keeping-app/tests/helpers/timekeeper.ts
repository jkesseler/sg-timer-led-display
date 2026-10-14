import { execSync } from 'node:child_process'
import mqtt from 'mqtt'
import { expect } from '@playwright/test'
import type { Page } from '@playwright/test'
import { baseURL } from './login'

export { baseURL }
export const brokerURL = process.env.MQTT_BROKER_URL ?? 'mqtt://localhost:1883'
export const DEVICE_ID = 'TKUI01'

/** A fresh active match, so no test depends on another's results. */
export function seedMatch(): void {
  execSync('npx tsx scripts/seed-match.ts', { stdio: 'ignore' })
}

export async function publishSessionStarted(sessionId: number): Promise<void> {
  const client = await mqtt.connectAsync(brokerURL)

  await client.publishAsync(`timer/${DEVICE_ID}/presence`, 'online', { retain: true })
  await client.publishAsync(`timer/${DEVICE_ID}/session/started`, JSON.stringify({ sessionId, timestamp: Date.now(), startDelaySeconds: 0 }))
  await client.endAsync()
}

export async function publishTimerSession(lastShotTimeMs: number): Promise<void> {
  const sessionId = Date.now()
  await publishSessionStarted(sessionId)

  const client = await mqtt.connectAsync(brokerURL)
  await client.publishAsync(`timer/${DEVICE_ID}/shot/detected`, JSON.stringify({
    sessionId, shotNumber: 1, absoluteTimeMs: lastShotTimeMs, splitTimeMs: lastShotTimeMs, deviceModel: 'Test', isFirstShot: true, timestamp: Date.now(),
  }))
  await client.publishAsync(`timer/${DEVICE_ID}/session/stopped`, JSON.stringify({ sessionId, timestamp: Date.now(), totalShots: 1, lastShotTimeMs }))
  await client.endAsync()
}

/** A barcode scanner types the digits ~10 ms apart and ends with Enter; no input needs focus. */
export async function scanCard(page: Page, knsaNumber: string): Promise<void> {
  await page.keyboard.type(knsaNumber, { delay: 10 })
  await page.keyboard.press('Enter')
}

export async function logIn(page: Page): Promise<void> {
  await page.goto(`${baseURL}/timekeeper/login`)
  await page.fill('#email', 'admin@timer.tsd')
  await page.fill('#password', 'qazwsx123')
  await page.click('button[type="submit"]')
  await page.waitForURL(`${baseURL}/timekeeper`)
}

export async function openBoard(page: Page): Promise<void> {
  await logIn(page)
  await expect(page.getByText('MQTT connected')).toBeVisible({ timeout: 15000 })
}
