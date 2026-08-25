/**
 * Headless Phase 1 verification: drives mqtt-simulator's TimerSimulator
 * against a real broker and asserts the running dev server's MQTT
 * subscriber (src/lib/mqtt/serverSubscriber.ts) binds sessions to
 * (shooter, discipline, round) correctly via Payload's local API.
 *
 * Prerequisites: `npm run dev` already running (holds the subscriber),
 * and Postgres reachable per .env.
 *
 * Run with: npx tsx scripts/verify-mqtt-binding.ts
 */
import 'dotenv/config'
import { getPayload } from 'payload'
import config from '../src/payload.config.js'
import { TimerSimulator } from '../../mqtt-simulator/src/simulator.js'

const DEVICE_ID = 'PGTEST'
const BROKER_URL = process.env.MQTT_BROKER_URL || 'mqtt://127.0.0.1:1883'

let passed = 0
let failed = 0

function assert(condition: boolean, message: string): void {
  if (condition) {
    passed++
    console.log(`  ✓ ${message}`)
  } else {
    failed++
    console.error(`  ✗ ${message}`)
  }
}

async function waitFor<T>(fn: () => Promise<T | undefined | null | false>, description: string, timeoutMs = 5000): Promise<T> {
  const start = Date.now()
  while (Date.now() - start < timeoutMs) {
    const result = await fn()
    if (result) return result
    await new Promise((resolve) => setTimeout(resolve, 150))
  }
  throw new Error(`Timed out waiting for: ${description}`)
}

async function main(): Promise<void> {
  const payload = await getPayload({ config })

  console.log('Setting up fixtures...')
  const device = await payload.create({ collection: 'devices', data: { deviceId: DEVICE_ID, label: 'Verification device' } })
  const match = await payload.create({ collection: 'matches', data: { label: 'Verification match', device: device.id } })
  const shooter = await payload.create({ collection: 'shooters', data: { firstName: 'Test', lastName: 'Shooter', knsaNumber: '999999' } })
  const squad = await payload.create({
    collection: 'squads',
    data: {
      startTime: '08:00',
      endTime: '09:00',
      match: match.id,
      status: 'active',
    },
  })
  const membership = await payload.create({
    collection: 'squad-memberships',
    data: { squad: squad.id, shooter: shooter.id, discipline: 'OKP', startingPosition: 1, queuePosition: 1, status: 'present' },
  })

  const roundResults = await payload.find({
    collection: 'round-results',
    where: { membership: { equals: membership.id } },
    sort: 'roundNumber',
    limit: 10,
  })
  assert(roundResults.docs.length === 5, `afterChange hook seeded 5 round-results (got ${roundResults.docs.length})`)
  assert(roundResults.docs.every((r) => r.status === 'pending'), 'all seeded round-results start pending')

  const [round1, round2, round3] = roundResults.docs

  const sim = new TimerSimulator({ deviceId: DEVICE_ID, brokerUrl: BROKER_URL })
  await sim.connect()

  try {
    // --- Scenario A: session/started with no pending activation ---
    console.log('\nScenario A: session/started with no pending activation')
    sim.publishSessionStarted(5001, 0)
    await new Promise((resolve) => setTimeout(resolve, 500))
    const strayActive = await payload.find({
      collection: 'match-sessions',
      where: { and: [{ device: { equals: device.id } }, { status: { equals: 'active' } }] },
    })
    assert(strayActive.docs.length === 0, 'no match-session activated when nothing was pending')

    // --- Scenario B: clean round ---
    console.log('\nScenario B: clean round')
    const pendingB = await payload.create({
      collection: 'match-sessions',
      data: { device: device.id, status: 'pending', roundResult: round1.id },
    })
    sim.publishSessionStarted(5002, 0)
    const activeB = await waitFor(
      async () => {
        const doc = await payload.findByID({ collection: 'match-sessions', id: pendingB.id })
        return doc.status === 'active' ? doc : null
      },
      'match-session activated on session/started',
    )
    assert(activeB.timerSessionId === 5002, 'timerSessionId stamped from session/started')

    sim.publishShot(5002, 1, 950, 0, true)
    sim.publishShot(5002, 2, 1820, 870, false)
    sim.publishShot(5002, 3, 2600, 780, false)
    sim.publishSessionStopped(5002, 3)

    const finishedRound1 = await waitFor(async () => {
      const doc = await payload.findByID({ collection: 'round-results', id: round1.id })
      return doc.status === 'timed' ? doc : null
    }, 'round-result 1 marked timed')
    assert(finishedRound1.timeMs === 2600, `round 1 timeMs = last shot's absoluteTimeMs (got ${finishedRound1.timeMs})`)

    const completedB = await payload.findByID({ collection: 'match-sessions', id: pendingB.id })
    assert(completedB.status === 'completed', 'match-session completed after session/stopped')

    // --- Scenario C: session/stopped with zero shots ---
    console.log('\nScenario C: session/stopped with zero shots')
    const pendingC = await payload.create({
      collection: 'match-sessions',
      data: { device: device.id, status: 'pending', roundResult: round2.id },
    })
    sim.publishSessionStarted(5003, 0)
    await waitFor(async () => {
      const doc = await payload.findByID({ collection: 'match-sessions', id: pendingC.id })
      return doc.status === 'active' ? doc : null
    }, 'match-session activated for scenario C')
    sim.publishSessionStopped(5003, 0)

    const abandonedC = await waitFor(async () => {
      const doc = await payload.findByID({ collection: 'match-sessions', id: pendingC.id })
      return doc.status === 'abandoned' ? doc : null
    }, 'match-session abandoned on zero-shot stop')
    assert(abandonedC.status === 'abandoned', 'zero-shot session marked abandoned')
    const round2AfterC = await payload.findByID({ collection: 'round-results', id: round2.id })
    assert(round2AfterC.status === 'pending', 'round 2 stays pending after abandoned turn — shooter can retry')

    // --- Scenario D: RS marker + deferred reshoot ---
    console.log('\nScenario D: RS marker + deferred reshoot')
    await payload.update({ collection: 'round-results', id: round3.id, data: { status: 'rs' } })
    const round3AfterRs = await payload.findByID({ collection: 'round-results', id: round3.id })
    assert(round3AfterRs.status === 'rs', 'round 3 marked as malfunction (RS)')

    let secondRsRejected = false
    try {
      await payload.update({ collection: 'round-results', id: roundResults.docs[3].id, data: { status: 'rs' } })
    } catch {
      secondRsRejected = true
    }
    assert(secondRsRejected, 'a second RS on the same membership is rejected (one reshoot per card)')

    const pendingD = await payload.create({
      collection: 'match-sessions',
      data: { device: device.id, status: 'pending', reshootFor: membership.id },
    })
    sim.publishSessionStarted(5004, 0)
    await waitFor(async () => {
      const doc = await payload.findByID({ collection: 'match-sessions', id: pendingD.id })
      return doc.status === 'active' ? doc : null
    }, 'match-session activated for reshoot')
    sim.publishShot(5004, 1, 1010, 0, true)
    sim.publishSessionStopped(5004, 1)

    const membershipAfterReshoot = await waitFor(async () => {
      const doc = await payload.findByID({ collection: 'squad-memberships', id: membership.id })
      return doc.reshootTimeMs ? doc : null
    }, 'reshootTimeMs set on the membership')
    assert(membershipAfterReshoot.reshootTimeMs === 1010, `reshootTimeMs = reshoot's absoluteTimeMs (got ${membershipAfterReshoot.reshootTimeMs})`)

    const round3AfterReshoot = await payload.findByID({ collection: 'round-results', id: round3.id })
    assert(round3AfterReshoot.status === 'rs' && round3AfterReshoot.timeMs == null, 'round 3 still RS — reshoot never overwrites it')
  } finally {
    await sim.disconnect()
  }

  console.log(`\n${passed} passed, ${failed} failed`)
  if (failed > 0) process.exitCode = 1
}

main()
  .catch((error) => {
    console.error('Verification script crashed:', error)
    process.exitCode = 1
  })
  .finally(() => process.exit(process.exitCode ?? 0))
