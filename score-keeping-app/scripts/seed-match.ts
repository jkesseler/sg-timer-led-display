import 'dotenv/config'
import { getPayload } from 'payload'
import config from '../src/payload.config.js'

type PayloadInstance = Awaited<ReturnType<typeof getPayload>>

const ADMIN_EMAIL = 'admin@timer.tsd'
const ADMIN_PASSWORD = 'qazwsx123'
const DEVICE_ID = 'TKUI01'

const SHOOTERS = [
  { firstName: 'Alex', lastName: 'Rivera', knsaNumber: '111111' },
  { firstName: 'Jamie', lastName: 'Chen', knsaNumber: '222222' },
  { firstName: 'Morgan', lastName: 'Blake', knsaNumber: '333333' },
  { firstName: 'Sam', lastName: 'Visser', knsaNumber: '444444' },
]

async function findOrCreateShooter(payload: PayloadInstance, shooter: (typeof SHOOTERS)[number]) {
  const existing = await payload.find({ collection: 'shooters', where: { knsaNumber: { equals: shooter.knsaNumber } }, limit: 1 })

  return existing.docs[0] ?? payload.create({ collection: 'shooters', data: shooter })
}

/** Dev/test data: an admin, a timer, and one active match with two squads. Morgan shoots in both. */
async function main(): Promise<void> {
  const payload = await getPayload({ config })

  const admins = await payload.find({ collection: 'users', where: { email: { equals: ADMIN_EMAIL } }, limit: 1 })
  if (admins.docs.length === 0) {
    await payload.create({ collection: 'users', data: { email: ADMIN_EMAIL, password: ADMIN_PASSWORD, role: 'admin' } })
  }

  const devices = await payload.find({ collection: 'devices', where: { deviceId: { equals: DEVICE_ID } }, limit: 1 })
  const device = devices.docs[0] ?? await payload.create({ collection: 'devices', data: { deviceId: DEVICE_ID, label: 'Test lane' } })

  await payload.update({ collection: 'matches', where: { active: { equals: true } }, data: { active: false } })
  const match = await payload.create({
    collection: 'matches',
    data: { label: 'Seeded match', date: new Date().toISOString(), device: device.id, active: true },
  })

  const shooters = await Promise.all(SHOOTERS.map(shooter => findOrCreateShooter(payload, shooter)))

  const morningSquad = await payload.create({
    collection: 'squads',
    data: { match: match.id, startTime: '08:00', endTime: '09:00' },
  })
  const laterSquad = await payload.create({
    collection: 'squads',
    data: { match: match.id, startTime: '09:00', endTime: '10:00' },
  })

  const memberships = [
    { squad: morningSquad.id, shooter: shooters[0].id, startingPosition: 1, discipline: 'OKP' as const },
    { squad: morningSquad.id, shooter: shooters[1].id, startingPosition: 2, discipline: 'SKP' as const },
    { squad: morningSquad.id, shooter: shooters[2].id, startingPosition: 3, discipline: 'OKP' as const },
    { squad: laterSquad.id, shooter: shooters[2].id, startingPosition: 1, discipline: 'SKP' as const },
    { squad: laterSquad.id, shooter: shooters[3].id, startingPosition: 2, discipline: 'OKKP' as const },
  ]
  for (const data of memberships) {
    await payload.create({ collection: 'squad-members', data })
  }

  console.log(`Seeded active match ${match.id}`)
  console.log(`Log in at /timekeeper with ${ADMIN_EMAIL} / ${ADMIN_PASSWORD}`)
  console.log(`Timer device ID for mqtt-simulator / ble-bridge: ${DEVICE_ID}`)
}

main()
  .catch((error) => {
    console.error(error)
    process.exitCode = 1
  })
  .finally(() => process.exit(process.exitCode ?? 0))
