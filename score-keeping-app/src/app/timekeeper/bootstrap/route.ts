import { getPayload } from 'payload';
import { buildFreshMatchState, findActiveMatch, findMatchState } from '@/lib/match/loadActiveMatch';
import { getMqttConfig } from '@/lib/mqtt/config';
import config from '@/payload.config';
import type { TimekeeperBootstrap } from '@/lib/match/bootstrap';

/** Everything the client-only timekeeper needs on load; it renders nothing on the server. */
export async function GET(request: Request) {
  const payload = await getPayload({ config });
  const { user } = await payload.auth({ headers: request.headers });

  if (!user) {
    return Response.json({ error: 'Not logged in.' }, { status: 401 });
  }

  const [match, mqttConfig] = await Promise.all([findActiveMatch(payload), getMqttConfig()]);

  if (!match) {
    return Response.json({ userEmail: user.email, mqttConfig, match: null } satisfies TimekeeperBootstrap);
  }

  const [freshState, serverState, shooters] = await Promise.all([
    buildFreshMatchState(payload, match),
    findMatchState(payload, match.id),
    payload.find({ collection: 'shooters', sort: 'lastName', depth: 0, pagination: false })
  ]);

  return Response.json({
    userEmail: user.email,
    mqttConfig,
    match: {
      id: match.id,
      label: match.label || 'Match',
      freshState,
      serverState,
      shooters: shooters.docs.map(shooter => ({
        id: shooter.id,
        name: `${shooter.firstName} ${shooter.lastName}`,
        knsaNumber: shooter.knsaNumber ?? null
      }))
    }
  } satisfies TimekeeperBootstrap);
}
