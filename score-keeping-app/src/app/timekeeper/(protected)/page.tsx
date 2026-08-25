import Link from 'next/link';
import { listOpenSquads, loadSquadView } from '@/lib/match/loadSquadView';
import { TimekeeperBoard } from '@/components/timekeeper/TimekeeperBoard';

export default async function TimekeeperPage({
  searchParams
}: {
  searchParams: Promise<{ squad?: string }>;
}) {
  const { squad: squadParam } = await searchParams;
  const openSquads = await listOpenSquads();

  const squadId = squadParam ? Number(squadParam) : openSquads.length === 1 ? openSquads[0].id : undefined;

  if (!squadId) {
    return (
      <div className="tk-layout">
        <div className="tk-main">
          <h1 className="tk-squad-title">Select a squad</h1>
          {openSquads.length === 0 && (
            <p style={{ color: 'var(--ink-dim)' }}>
              No squad is currently active or in reshoot-phase. Set one active in /admin.
            </p>
          )}
          <div className="tk-list">
            {openSquads.map(squad => (
              <Link href={`/timekeeper?squad=${squad.id}`} key={squad.id} className="tk-list-row" style={{ textDecoration: 'none' }}>
                <span>{squad.label || `Squad #${squad.id}`}</span>
              </Link>
            ))}
          </div>
        </div>
      </div>
    );
  }

  const view = await loadSquadView(squadId);

  return <TimekeeperBoard view={view} />;
}
