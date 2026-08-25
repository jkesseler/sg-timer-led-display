import 'dotenv/config'
import { getPayload } from 'payload'
import config from '../src/payload.config.js'

async function main(): Promise<void> {
  const payload = await getPayload({ config })

  const device = await payload.create({ collection: 'devices', data: { deviceId: 'DISPLAY1', label: 'Display verification lane' } })
  const shooterA = await payload.create({ collection: 'shooters', data: { firstName: 'Casey', lastName: 'Nguyen', knsaNumber: '444444' } })
  const shooterB = await payload.create({ collection: 'shooters', data: { firstName: 'Drew', lastName: 'Park', knsaNumber: '555555' } })

  const squad = await payload.create({
    collection: 'squads',
    data: {
      label: 'Display verification squad',
      startTime: new Date().toISOString(),
      endTime: new Date(Date.now() + 3600_000).toISOString(),
      device: device.id,
      status: 'active',
    },
  })

  const membershipA = await payload.create({
    collection: 'squad-memberships',
    data: { squad: squad.id, shooter: shooterA.id, discipline: 'OKP', startingPosition: 1, queuePosition: 1, status: 'present' },
  })
  const membershipB = await payload.create({
    collection: 'squad-memberships',
    data: { squad: squad.id, shooter: shooterB.id, discipline: 'OKP', startingPosition: 2, queuePosition: 2, status: 'present' },
  })

  console.log('Device: DISPLAY1')
  console.log('Squad:', squad.id, 'memberships:', membershipA.id, membershipB.id)
}

main()
  .catch((error) => {
    console.error(error)
    process.exitCode = 1
  })
  .finally(() => process.exit(process.exitCode ?? 0))
