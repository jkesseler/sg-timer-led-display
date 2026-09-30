import { getPayload } from 'payload';
import { TimekeeperBoard } from '@/components/timekeeper/TimekeeperBoard';
import { buildFreshMatchState, findActiveMatch, findMatchState } from '@/lib/match/loadActiveMatch';
import { getMqttConfig } from '@/lib/mqtt/config';
import config from '@/payload.config';

export default async function TimekeeperPage() {
  const payload = await getPayload({ config });
  const match = await findActiveMatch(payload);

  if (!match) {
    return (
      <div className="tk-layout">
        <div className="tk-main">
          <h1 className="tk-squad-title">No active match</h1>
          <p className="tk-muted">Tick "active" on a match in /admin.</p>
        </div>
      </div>
    );
  }

  const [freshState, serverState, shooters, mqttConfig] = await Promise.all([
    buildFreshMatchState(payload, match),
    findMatchState(payload, match.id),
    payload.find({ collection: 'shooters', sort: 'lastName', depth: 0, pagination: false }),
    getMqttConfig()
  ]);

  return (
    <>
      <h1 className="tk-match-title">{match.label || 'Match'}</h1>
      <TimekeeperBoard
        matchId={match.id}
        freshState={freshState}
        serverState={serverState}
        shooters={shooters.docs.map(shooter => ({
          id: shooter.id,
          name: `${shooter.firstName} ${shooter.lastName}`,
          knsaNumber: shooter.knsaNumber ?? null
        }))}
        mqttConfig={mqttConfig}
      />
    </>
  );
}
