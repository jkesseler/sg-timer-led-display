import 'dotenv/config'
import { getPayload } from 'payload'
import config from '../src/payload.config.js'

async function main(): Promise<void> {
  const payload = await getPayload({ config })

  const email = 'timekeeper-test@example.com'
  const password = 'test-password-123'

  const existing = await payload.find({ collection: 'users', where: { email: { equals: email } }, limit: 1 })
  if (existing.docs.length === 0) {
    await payload.create({ collection: 'users', data: { email, password, role: 'admin' } })
    console.log(`Created admin user ${email} / ${password}`)
  } else {
    console.log(`Admin user ${email} already exists`)
  }

  const device = await payload.create({ collection: 'devices', data: { deviceId: 'TKUI01', label: 'Timekeeper UI test lane' } })
  console.log('Created device', device.id, device.deviceId)

  const shooterA = await payload.create({ collection: 'shooters', data: { firstName: 'Alex', lastName: 'Rivera', knsaNumber: '111111' } })
  const shooterB = await payload.create({ collection: 'shooters', data: { firstName: 'Jamie', lastName: 'Chen', knsaNumber: '222222' } })
  const shooterC = await payload.create({ collection: 'shooters', data: { firstName: 'Morgan', lastName: 'Blake', knsaNumber: '333333' } })
  console.log('Created shooters', shooterA.id, shooterB.id, shooterC.id)

  const squad = await payload.create({
    collection: 'squads',
    data: {
      label: 'UI verification squad',
      startTime: new Date().toISOString(),
      endTime: new Date(Date.now() + 3600_000).toISOString(),
      device: device.id,
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
