import Link from 'next/link';
import { listOpenSquads, listSelectableSquads, loadSquadView } from '@/lib/match/loadSquadView';
import { TimekeeperBoard } from '@/components/timekeeper/TimekeeperBoard';
import { SquadBar } from '@/components/timekeeper/SquadBar';

export default async function TimekeeperPage({
  searchParams
}: {
  searchParams: Promise<{ squad?: string }>;
}) {
  const { squad: squadParam } = await searchParams;
  const openSquads = await listOpenSquads();
  const selectableSquads = await listSelectableSquads();

  const squadId = squadParam ? Number(squadParam) : openSquads.length === 1 ? openSquads[0].id : undefined;

  const squadOptions = selectableSquads.map(squad => ({
    id: squad.id,
    label: squad.label || `Squad #${squad.id}`
  }));

  if (!squadId) {
    return (
      <>
        <div className="tk-layout">
          <div className="tk-main">
            <h1 className="tk-squad-title">Select a squad</h1>
            <p style={{ color: 'var(--ink-dim)' }}>
              {selectableSquads.length === 0
                ? 'No squads exist yet. Create one in /admin.'
                : 'Pick a squad from the bar at the bottom of the screen.'}
            </p>
            {openSquads.length > 0 && (
              <div className="tk-list">
                {openSquads.map(squad => (
                  <Link href={`/timekeeper?squad=${squad.id}`} key={squad.id} className="tk-list-row" style={{ textDecoration: 'none' }}>
                    <span>{squad.label || `Squad #${squad.id}`}</span>
                  </Link>
                ))}
              </div>
            )}
          </div>
        </div>
        <SquadBar options={squadOptions} activeSquadId={null} />
      </>
    );
  }

  const view = await loadSquadView(squadId);

  return (
    <>
      <TimekeeperBoard view={view} />
      <SquadBar options={squadOptions} activeSquadId={squadId} />
    </>
  );
}
