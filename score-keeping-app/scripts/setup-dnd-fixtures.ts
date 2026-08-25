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

  const device = await findOrCreateDevice(payload, 'DNDTEST', 'DnD verification lane')
  const match = await payload.create({ collection: 'matches', data: { label: 'DnD verification match', device: device.id } })
  const names = [
    ['Priya', 'Shah'],
    ['Owen', 'Clarke'],
    ['Mika', 'Tanaka'],
  ]
  const shooterIds: number[] = []
  for (const [firstName, lastName] of names) {
    const shooter = await findOrCreateShooter(payload, firstName, lastName, `7${shooterIds.length}${shooterIds.length}${shooterIds.length}${shooterIds.length}${shooterIds.length}`)
    shooterIds.push(shooter.id)
  }

  const squad = await payload.create({
    collection: 'squads',
    data: { label: 'DnD verification squad', startTime: '08:00', endTime: '09:00', match: match.id, status: 'active' },
  })

  for (let i = 0; i < shooterIds.length; i++) {
    await payload.create({
      collection: 'squad-memberships',
      data: { squad: squad.id, shooter: shooterIds[i], discipline: 'OKP', startingPosition: i + 1, queuePosition: i + 1, status: 'present' },
    })
  }

  console.log('Squad URL: /timekeeper?squad=' + squad.id)
  console.log('Order: Priya Shah, Owen Clarke, Mika Tanaka')
}

main()
  .catch((error) => {
    console.error(error)
    process.exitCode = 1
  })
  .finally(() => process.exit(process.exitCode ?? 0))
