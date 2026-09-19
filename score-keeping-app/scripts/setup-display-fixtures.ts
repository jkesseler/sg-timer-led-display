import 'dotenv/config'
import { getPayload } from 'payload'
import config from '../src/payload.config.js'

// Keep in sync with mqtt-simulator's DEFAULT_SIMULATOR_DEVICE_ID so
// `npx tsx scripts/setup-display-fixtures.ts` and a plain `npm run simulate`
// line up with no extra flags. Pass a different id as argv[2] to match a real
// board (and run the simulator with `-- --device-id <id>` to match).
const DEVICE_ID = process.argv[2] ?? 'SIM001'

const ROSTER = [
  { firstName: 'Casey', lastName: 'Nguyen', knsaNumber: '444444' },
  { firstName: 'Drew', lastName: 'Park', knsaNumber: '555555' },
  { firstName: 'Emerson', lastName: 'Lopez', knsaNumber: '666666' },
]

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

  const device = await findOrCreateDevice(payload, DEVICE_ID, 'Display verification lane')
  const match = await payload.create({ collection: 'matches', data: { label: 'Display verification match', device: device.id } })

  const squad = await payload.create({
    collection: 'squads',
    data: {
      label: 'Display verification squad',
      startTime: '08:00',
      endTime: '09:00',
      match: match.id,
    },
  })
  await payload.update({ collection: 'matches', id: match.id, data: { currentSquad: squad.id } })

  // Creating a squad-membership fires an afterChange hook that seeds five
  // pending round-results for it, so deriveCurrentRound() resolves to round 1
  // and the Next: / On deck: callouts have a queue to derive from.
  const membershipIds: number[] = []
  for (const [index, entry] of ROSTER.entries()) {
    const position = index + 1
    const shooter = await findOrCreateShooter(payload, entry.firstName, entry.lastName, entry.knsaNumber)
    const membership = await payload.create({
      collection: 'squad-memberships',
      data: {
        squad: squad.id,
        shooter: shooter.id,
        discipline: 'OKP',
        queuePosition: position,
        status: 'present',
      },
    })
    membershipIds.push(membership.id)
  }

  // A pending match-session bound to the first shooter's round 1 makes them
  // the "current shooter": loadSquadView reads activeMembershipId from any
  // pending/active session on the device. When the simulator later publishes
  // session/started, the server subscriber flips this same row to active.
  const round1 = await payload.find({
    collection: 'round-results',
    where: { and: [{ membership: { equals: membershipIds[0] } }, { roundNumber: { equals: 1 } }] },
    limit: 1,
  })
  const round1Id = round1.docs[0]?.id

  const liveSession = await payload.find({
    collection: 'match-sessions',
    where: { and: [{ device: { equals: device.id } }, { status: { in: ['pending', 'active'] } }] },
    limit: 1,
  })
  if (liveSession.docs.length === 0 && round1Id != null) {
    await payload.create({
      collection: 'match-sessions',
      data: { device: device.id, status: 'pending', roundResult: round1Id },
    })
  }

  const simulateHint = DEVICE_ID === 'SIM001' ? 'npm run simulate' : `npm run simulate -- --device-id ${DEVICE_ID}`

  console.log('')
  console.log(`Device:      ${DEVICE_ID}`)
  console.log(`Squad:       ${squad.id} (active), memberships ${membershipIds.join(', ')}`)
  console.log('Roster:      current Casey Nguyen / next Drew Park / on deck Emerson Lopez')
  console.log('')
  console.log('Then, to drive the timer UI:')
  console.log(`  cd ../mqtt-simulator && ${simulateHint}`)
  console.log('  open http://localhost:3000/display')
}

main()
  .catch((error) => {
    console.error(error)
    process.exitCode = 1
  })
  .finally(() => process.exit(process.exitCode ?? 0))
