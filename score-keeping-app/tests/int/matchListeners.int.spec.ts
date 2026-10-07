import { beforeEach, describe, expect, it, vi } from 'vitest';
import { armTurn, hydrate } from '@/store/matchSlice';
import { publishMqtt } from '@/store/mqttMiddleware';
import { selectDevice, sessionStopped } from '@/store/mqttSlice';
import { makeStore } from '@/store/store';
import { buildCard, buildState } from './matchFixtures';
import type { MatchState } from '@/lib/match/types';

vi.mock('@/store/mqttMiddleware', async (importOriginal) => ({
  ...await importOriginal<typeof import('@/store/mqttMiddleware')>(),
  publishMqtt: vi.fn()
}));

const sync = { saveMatchState: async () => {}, appendAudit: async () => {} };

function buildStore(match: MatchState) {
  const store = makeStore({ sync });
  store.dispatch(hydrate(match));
  store.dispatch(selectDevice('ABC123'));

  return store;
}

function stopSession(store: ReturnType<typeof buildStore>) {
  store.dispatch(sessionStopped({ sessionId: 7, totalShots: 3, lastShotTimeMs: 4200, timestamp: 0 }));
}

describe('session stopped', () => {
  beforeEach(() => {
    vi.mocked(publishMqtt).mockClear();
  });

  it('publishes next and on deck for the timer once the turn is recorded', () => {
    const store = buildStore(buildState(['a', 'b', 'c'].map((id, index) => buildCard({ id, queuePosition: index }))));
    store.dispatch(armTurn({ cardId: 'a', round: 1 }));
    stopSession(store);

    expect(publishMqtt).toHaveBeenCalledWith('timer/ABC123/session/up-next', { sessionId: 7, next: 'Shooter b', onDeck: 'Shooter c' });
  });

  it('leaves out on deck when only one shooter is left', () => {
    const store = buildStore(buildState(['a', 'b'].map((id, index) => buildCard({ id, queuePosition: index }))));
    store.dispatch(armTurn({ cardId: 'a', round: 1 }));
    stopSession(store);

    expect(publishMqtt).toHaveBeenCalledWith('timer/ABC123/session/up-next', { sessionId: 7, next: 'Shooter b' });
  });

  it('publishes nothing without an armed turn', () => {
    const store = buildStore(buildState([buildCard({ id: 'a' })]));
    stopSession(store);

    expect(publishMqtt).not.toHaveBeenCalled();
  });
});
