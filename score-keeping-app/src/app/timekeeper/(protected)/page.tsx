import Link from 'next/link'
import { listOpenSquads, loadSquadView } from '@/lib/match/loadSquadView'
import { TimekeeperBoard } from '@/components/timekeeper/TimekeeperBoard'

export default async function TimekeeperPage({
  searchParams,
}: {
  searchParams: Promise<{ squad?: string }>
}) {
  const { squad: squadParam } = await searchParams
  const openSquads = await listOpenSquads()

  const squadId = squadParam ? Number(squadParam) : openSquads.length === 1 ? openSquads[0].id : undefined

  if (!squadId) {
    return (
      <div style={{ padding: '2rem' }}>
        <h1>Select a squad</h1>
        {openSquads.length === 0 && <p>No squad is currently active or in reshoot-phase. Set one active in /admin.</p>}
        <ul>
          {openSquads.map((squad) => (
            <li key={squad.id}>
              <Link href={`/timekeeper?squad=${squad.id}`}>{squad.label || `Squad #${squad.id}`}</Link>
            </li>
          ))}
        </ul>
      </div>
    )
  }

  const view = await loadSquadView(squadId)
  return <TimekeeperBoard view={view} />
}
