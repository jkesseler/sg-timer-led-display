import 'dotenv/config'
import { getPayload } from 'payload'
import config from '../src/payload.config.js'

async function findOrCreateDevice(payload: Awaited<ReturnType<typeof getPayload>>, deviceId: string, label: string) {
  const existing = await payload.find({ collection: 'devices', where: { deviceId: { equals: deviceId } }, limit: 1 })
  if (existing.docs[0]) return existing.docs[0]
  return payload.create({ collection: 'devices', data: { deviceId, label } })
}

async function findOrCreateShooter(
  payload: Awaited<ReturnType<typeof getPayload>>,
  firstName: string,
  lastName: string,
  knsaNumber: string,
) {
  const existing = await payload.find({ collection: 'shooters', where: { knsaNumber: { equals: knsaNumber } }, limit: 1 })
  if (existing.docs[0]) return existing.docs[0]
  return payload.create({ collection: 'shooters', data: { firstName, lastName, knsaNumber } })
}

async function main(): Promise<void> {
  const payload = await getPayload({ config })

  const email = 'admin@timer.tsd'
  const password = 'qazwsx123'

  const existing = await payload.find({ collection: 'users', where: { email: { equals: email } }, limit: 1 })
  if (existing.docs.length === 0) {
    await payload.create({ collection: 'users', data: { email, password, role: 'admin' } })
    console.log(`Created admin user ${email} / ${password}`)
  } else {
    console.log(`Admin user ${email} already exists`)
  }

  const device = await findOrCreateDevice(payload, 'TKUI01', 'Timekeeper UI test lane')
  console.log('Device', device.id, device.deviceId)

  const match = await payload.create({ collection: 'matches', data: { label: 'UI verification match', device: device.id } })
  console.log('Created match', match.id)

  const shooterA = await findOrCreateShooter(payload, 'Alex', 'Rivera', '111111')
  const shooterB = await findOrCreateShooter(payload, 'Jamie', 'Chen', '222222')
  const shooterC = await findOrCreateShooter(payload, 'Morgan', 'Blake', '333333')
  console.log('Shooters', shooterA.id, shooterB.id, shooterC.id)

  const squad = await payload.create({
    collection: 'squads',
    data: {
      label: 'UI verification squad',
      startTime: '08:00',
      endTime: '09:00',
      match: match.id,
      status: 'active',
    },
  })
  console.log('Created squad', squad.id)

  const membershipA = await payload.create({
    collection: 'squad-memberships',
    data: { squad: squad.id, shooter: shooterA.id, discipline: 'OKP', startingPosition: 1, queuePosition: 1, status: 'present' },
  })
  const membershipB = await payload.create({
    collection: 'squad-memberships',
    data: { squad: squad.id, shooter: shooterB.id, discipline: 'OKP', startingPosition: 2, queuePosition: 2, status: 'present' },
  })
  const membershipC = await payload.create({
    collection: 'squad-memberships',
    data: { squad: squad.id, shooter: shooterC.id, discipline: 'SKP', startingPosition: 3, queuePosition: 3, status: 'present' },
  })
  console.log('Created memberships', membershipA.id, membershipB.id, membershipC.id)

  console.log('\nDone. Squad URL: /timekeeper?squad=' + squad.id)
  console.log('Device ID for mqtt-simulator: TKUI01')
}

main()
  .catch((error) => {
    console.error(error)
    process.exitCode = 1
  })
  .finally(() => process.exit(process.exitCode ?? 0))
