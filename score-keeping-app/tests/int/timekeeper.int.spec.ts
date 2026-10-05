import { describe, expect, it } from 'vitest';
import { addCard, hydrate, setRoundTime } from '@/store/matchSlice';
import { makeStore } from '@/store/store';
import {
  selectCardRowView,
  selectLateShooterView,
  selectPresentCardIds,
  selectRoundEditorView,
  selectRoundTimeText,
  selectSelectedSquadId
} from '@/store/timekeeperSelectors';
import { editorTimeTextChanged, lateShooterChanged, loaded, roundEditorToggled, scanCodeChanged } from '@/store/timekeeperSlice';
import { moveQueueCard, saveEditorTime, submitScan } from '@/store/timekeeperThunks';
import { buildCard, buildState } from './matchFixtures';
import type { MatchState } from '@/lib/match/types';

function buildStore(match: MatchState) {
  const store = makeStore();
  store.dispatch(hydrate(match));

  return store;
}

function buildTwoCardMatch() {
  return buildState([
    buildCard({ id: 'a', queuePosition: 0, knsaNumber: '111', rounds: [{ status: 'timed', timeMs: 1150 }, { status: 'pending' }, { status: 'pending' }, { status: 'pending' }, { status: 'pending' }] }),
    buildCard({ id: 'b', queuePosition: 1, knsaNumber: '222' })
  ]);
}

describe('round editor input', () => {
  it('shows the value of the round it is opened on, not the previous one', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(roundEditorToggled({ cardId: 'a', round: 1 }));
    expect(selectRoundEditorView(store.getState(), 'a', 1)?.timeText).toBe('1.15');

    store.dispatch(roundEditorToggled({ cardId: 'a', round: 2 }));
    expect(selectRoundEditorView(store.getState(), 'a', 2)?.timeText).toBe('');
  });

  it('drops a draft typed on one round when another round is opened', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(roundEditorToggled({ cardId: 'a', round: 1 }));
    store.dispatch(editorTimeTextChanged('9.99'));
    store.dispatch(roundEditorToggled({ cardId: 'a', round: 2 }));
    store.dispatch(roundEditorToggled({ cardId: 'a', round: 1 }));

    expect(selectRoundTimeText(store.getState(), 'a', 1)).toBe('1.15');
  });

  it('saves the draft and then shows the stored value', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(roundEditorToggled({ cardId: 'a', round: 2 }));
    store.dispatch(editorTimeTextChanged('12,34'));
    store.dispatch(saveEditorTime());

    const state = store.getState();
    expect(state.match.current?.cards[0].rounds[1]).toMatchObject({ status: 'timed', timeMs: 12340 });
    expect(state.timekeeper.editor?.timeText).toBeNull();
    expect(selectRoundTimeText(state, 'a', 2)).toBe('12.34');
  });

  it('keeps an invalid draft and shows an error', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(roundEditorToggled({ cardId: 'a', round: 2 }));
    store.dispatch(editorTimeTextChanged('abc'));
    store.dispatch(saveEditorTime());

    const view = selectRoundEditorView(store.getState(), 'a', 2);
    expect(view?.timeText).toBe('abc');
    expect(view?.error).toMatch(/seconds/);
    expect(store.getState().match.current?.cards[0].rounds[1].status).toBe('pending');
  });

  it('follows a timer result while the input is untouched', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(roundEditorToggled({ cardId: 'b', round: 1 }));
    store.dispatch(setRoundTime({ cardId: 'b', round: 1, timeMs: 8000 }));

    expect(selectRoundTimeText(store.getState(), 'b', 1)).toBe('8');
  });
});

describe('card row view', () => {
  it('stays the same object when another card changes', () => {
    const store = buildStore(buildTwoCardMatch());
    const before = selectCardRowView(store.getState(), 'a');

    store.dispatch(setRoundTime({ cardId: 'b', round: 1, timeMs: 5000 }));

    expect(selectCardRowView(store.getState(), 'a')).toBe(before);
    expect(selectCardRowView(store.getState(), 'b')?.rounds[0]).toMatchObject({ label: '05.00', modifier: 'timed' });
  });

  it('marks the round being edited', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(roundEditorToggled({ cardId: 'a', round: 3 }));

    expect(selectCardRowView(store.getState(), 'a')?.rounds.map(round => round.isEditing)).toEqual([false, false, true, false, false]);
    expect(selectCardRowView(store.getState(), 'b')?.rounds.every(round => !round.isEditing)).toBe(true);
  });
});

describe('scan', () => {
  it('arms the next round of the scanned shooter and clears the input', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(scanCodeChanged(' 111 '));
    store.dispatch(submitScan());

    const state = store.getState();
    expect(state.match.current?.activeTurn).toMatchObject({ cardId: 'a', round: 2 });
    expect(state.timekeeper.scanCode).toBe('');
    expect(state.timekeeper.message).toBeNull();
    expect(selectSelectedSquadId(state)).toBe('squad-a');
  });

  it('reports an unknown KNSA number', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(scanCodeChanged('999'));
    store.dispatch(submitScan());

    expect(store.getState().timekeeper.message).toBe('No shooter with KNSA number 999 in this match.');
    expect(store.getState().match.current?.activeTurn).toBeNull();
  });
});

describe('queue', () => {
  it('moves a card by drag and drop', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(moveQueueCard('b', 'a'));

    expect(selectPresentCardIds(store.getState())).toEqual(['b', 'a']);
  });
});

describe('late shooter form', () => {
  it('offers a shooter again only in a discipline they do not shoot in this squad', () => {
    const store = buildStore(buildState([buildCard({ id: 'a', shooterId: 'jan', discipline: 'OKP' })]));
    store.dispatch(loaded({
      userEmail: 'tk@example.com',
      matchLabel: 'M',
      shooters: [{ id: 'jan', name: 'Jan', knsaNumber: null }, { id: 'piet', name: 'Piet', knsaNumber: null }]
    }));

    store.dispatch(lateShooterChanged({ discipline: 'OKP' }));
    expect(selectLateShooterView(store.getState()).shooters.map(shooter => shooter.id)).toEqual(['piet']);

    store.dispatch(lateShooterChanged({ discipline: 'SKP' }));
    expect(selectLateShooterView(store.getState()).shooters.map(shooter => shooter.id)).toEqual(['jan', 'piet']);
  });

  it('clears the picked shooter once added', () => {
    const store = buildStore(buildState([]));
    store.dispatch(lateShooterChanged({ shooterId: 'piet' }));

    store.dispatch(addCard({ squadId: 'squad-a', shooterId: 'piet', shooterName: 'Piet', knsaNumber: null, discipline: 'OKP' }));

    expect(store.getState().timekeeper.lateShooter.shooterId).toBe('');
  });
});
