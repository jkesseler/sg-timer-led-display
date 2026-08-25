'use client'

import { useState, useTransition, type CSSProperties, type FormEvent } from 'react'
import { useRouter } from 'next/navigation'
import type { SquadView } from '@/lib/match/loadSquadView'
import { deriveUpcomingShooters, deriveOutstanding, isReadyForSignOff, formatRoundTimeMs, type MembershipView } from '@/lib/match/matchState'
import {
  activateMembershipAction,
  activateByKnsaAction,
  sendToBackAction,
  markAbsentAction,
  markPresentAction,
  offerCatchUpAction,
  markSignedOffAction,
  type ActionResult,
} from '@/app/timekeeper/(protected)/actions'

const ROUND_NUMBERS = [1, 2, 3, 4, 5] as const

function shooterName(view: MembershipView): string {
  const shooter = view.membership.shooter
  return typeof shooter === 'object' ? `${shooter.firstName} ${shooter.lastName}` : `Shooter #${shooter}`
}

const cellStyle: CSSProperties = { border: '1px solid #ddd', padding: '0.4rem 0.6rem', textAlign: 'left' }

export function TimekeeperBoard({ view }: { view: SquadView }) {
  const router = useRouter()
  const [isPending, startTransition] = useTransition()
  const [error, setError] = useState<string | null>(null)
  const [code, setCode] = useState('')

  const present = view.memberships
    .filter((m) => m.membership.status === 'present')
    .sort((a, b) => a.membership.queuePosition - b.membership.queuePosition)
  const absent = view.memberships.filter((m) => m.membership.status === 'absent')
  const activeMembership = view.memberships.find((m) => m.membership.id === view.activeMembershipId) ?? null

  const { next, onDeck } = deriveUpcomingShooters(view.memberships, view.currentRound, view.activeMembershipId)
  const outstanding = view.currentRound === null ? deriveOutstanding(view.memberships) : []

  const controlsDisabled = view.isSessionActive || isPending

  function runAction(action: () => Promise<ActionResult>) {
    setError(null)
    startTransition(async () => {
      const result = await action()
      if (!result.ok) {
        setError(result.error ?? 'Something went wrong.')
      }
      router.refresh()
    })
  }

  function handleScanSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    const submittedCode = code
    setCode('')
    runAction(() => activateByKnsaAction(view.squad.id, submittedCode))
  }

  return (
    <div style={{ padding: '2rem', display: 'flex', flexDirection: 'column', gap: '1.5rem', fontFamily: 'sans-serif' }}>
      <h1>{view.squad.label || `Squad #${view.squad.id}`}</h1>

      <section>
        <h2>{view.currentRound !== null ? `Round ${view.currentRound} of 5` : 'Reshoot / catch-up phase'}</h2>
        {view.isSessionActive ? (
          <p>
            <strong>Turn in progress:</strong> {activeMembership ? shooterName(activeMembership) : 'unknown shooter'} —
            waiting for the range officer to stop the timer.
          </p>
        ) : (
          <p>Scanner and manual selection are armed — ready for the next shooter.</p>
        )}
        <button type="button" onClick={() => router.refresh()} disabled={isPending}>
          End Turn / Refresh
        </button>
      </section>

      <section>
        <p style={{ fontSize: '1.4em', margin: 0 }}>
          Next: <strong>{next ? shooterName(next) : '—'}</strong>
        </p>
        <p style={{ fontSize: '1em', margin: 0, opacity: 0.75 }}>On deck: {onDeck ? shooterName(onDeck) : '—'}</p>
      </section>

      {error && <p style={{ color: 'crimson' }}>{error}</p>}

      <section>
        <h3>Scan or enter KNSA number</h3>
        <form onSubmit={handleScanSubmit} style={{ display: 'flex', gap: '0.5rem' }}>
          <input
            type="text"
            value={code}
            onChange={(event) => setCode(event.target.value)}
            placeholder="scan card or type KNSA number"
            disabled={controlsDisabled}
            autoFocus
            style={{ fontFamily: 'monospace', fontSize: '1rem', padding: '0.4rem', width: '16rem' }}
          />
          <button type="submit" disabled={controlsDisabled}>
            Activate
          </button>
        </form>
      </section>

      <section>
        <h3>Queue</h3>
        <table style={{ borderCollapse: 'collapse', width: '100%' }}>
          <thead>
            <tr>
              <th style={cellStyle}>#</th>
              <th style={cellStyle}>Shooter</th>
              <th style={cellStyle}>Discipline</th>
              {ROUND_NUMBERS.map((round) => (
                <th key={round} style={cellStyle}>
                  R{round}
                </th>
              ))}
              <th style={cellStyle}>Reshoot</th>
              <th style={cellStyle}>Actions</th>
            </tr>
          </thead>
          <tbody>
            {present.map((m) => {
              const isActive = m.membership.id === view.activeMembershipId
              return (
                <tr key={m.membership.id} style={{ background: isActive ? '#fffae0' : undefined }}>
                  <td style={cellStyle}>{m.membership.queuePosition}</td>
                  <td style={cellStyle}>
                    <button
                      type="button"
                      disabled={controlsDisabled}
                      onClick={() => runAction(() => activateMembershipAction(view.squad.id, m.membership.id))}
                    >
                      {shooterName(m)}
                    </button>
                  </td>
                  <td style={cellStyle}>{m.membership.discipline}</td>
                  {ROUND_NUMBERS.map((round) => {
                    const result = m.roundResults.find((r) => r.roundNumber === round)
                    const label = !result || result.status === 'pending'
                      ? '—'
                      : result.status === 'rs'
                        ? 'RS'
                        : result.status === 'skipped'
                          ? 'X'
                          : result.timeMs != null
                            ? formatRoundTimeMs(result.timeMs)
                            : '—'
                    return (
                      <td key={round} style={cellStyle}>
                        {label}
                      </td>
                    )
                  })}
                  <td style={cellStyle}>
                    {m.membership.reshootTimeMs != null ? formatRoundTimeMs(m.membership.reshootTimeMs) : ''}
                  </td>
                  <td style={cellStyle}>
                    <button type="button" disabled={controlsDisabled} onClick={() => runAction(() => sendToBackAction(view.squad.id, m.membership.id))}>
                      Send to back
                    </button>{' '}
                    <button type="button" disabled={controlsDisabled} onClick={() => runAction(() => markAbsentAction(view.squad.id, m.membership.id))}>
                      Mark absent
                    </button>{' '}
                    {isReadyForSignOff(m) && !m.membership.signedOffAt && (
                      <button type="button" disabled={isPending} onClick={() => runAction(() => markSignedOffAction(m.membership.id))}>
                        Mark signed
                      </button>
                    )}
                    {m.membership.signedOffAt && <em>signed</em>}
                  </td>
                </tr>
              )
            })}
          </tbody>
        </table>
      </section>

      {outstanding.length > 0 && (
        <section>
          <h3>Outstanding (reshoots &amp; catch-up rounds)</h3>
          <ul>
            {outstanding.map((item) => (
              <li key={`${item.membership.membership.id}-${item.roundNumber}`}>
                {shooterName(item.membership)} — round {item.roundNumber} ({item.kind === 'rs' ? 'reshoot' : 'catch-up'}){' '}
                <button
                  type="button"
                  disabled={controlsDisabled}
                  onClick={() =>
                    runAction(() =>
                      item.kind === 'rs'
                        ? activateMembershipAction(view.squad.id, item.membership.membership.id)
                        : offerCatchUpAction(view.squad.id, item.roundResultId),
                    )
                  }
                >
                  {item.kind === 'rs' ? 'Shoot reshoot' : 'Shoot catch-up round'}
                </button>
              </li>
            ))}
          </ul>
        </section>
      )}

      {absent.length > 0 && (
        <section>
          <h3>Absent</h3>
          <ul>
            {absent.map((m) => (
              <li key={m.membership.id}>
                {shooterName(m)}{' '}
                <button type="button" disabled={controlsDisabled} onClick={() => runAction(() => markPresentAction(view.squad.id, m.membership.id))}>
                  Mark present / rejoin
                </button>
              </li>
            ))}
          </ul>
        </section>
      )}
    </div>
  )
}
