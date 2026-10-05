import { describe, expect, it } from 'vitest';
import { addCard, armTurn, cancelTurn, disqualify, hydrate, setRoundTime, signOff } from '@/store/matchSlice';
import { makeStore } from '@/store/store';
import {
  selectCardRowView,
  selectIsScannerListening,
  selectLateShooterView,
  selectPresentCardIds,
  selectRoundEditorView,
  selectRoundTimeText,
  selectScoreSheet,
  selectSelectedSquadId
} from '@/store/timekeeperSelectors';
import {
  editorTimeTextChanged,
  lateShooterChanged,
  loaded,
  roundEditorToggled,
  scoreSheetPrinted,
  scoreSheetPrintRequested
} from '@/store/timekeeperSlice';
import { handleScannedCard, moveQueueCard, saveEditorTime } from '@/store/timekeeperThunks';
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

  it('shows a reshoot as RS in its round and its time in an extra RS cell', () => {
    const store = buildStore(buildState([buildCard({
      id: 'a',
      rounds: [
        { status: 'timed', timeMs: 10110 },
        { status: 'rs', timeMs: 9500, reshootTimeMs: 8990 },
        { status: 'timed', timeMs: 10000 },
        { status: 'timed', timeMs: 9860 },
        { status: 'timed', timeMs: 9800 }
      ]
    })]));

    const cells = selectCardRowView(store.getState(), 'a')?.rounds;

    expect(cells?.map(cell => `${cell.heading} ${cell.label}`)).toEqual(['R1 10.11', 'R2 RS', 'R3 10.00', 'R4 09.86', 'R5 09.80', 'RS 08.99']);
    expect(cells?.[5].n).toBe(2);
  });

  it('shows an armed reshoot on the RS cell, not on the round it replaces', () => {
    const store = buildStore(buildState([buildCard({
      id: 'a',
      rounds: [{ status: 'timed', timeMs: 10110 }, { status: 'rs', timeMs: 9500 }, { status: 'pending' }, { status: 'pending' }, { status: 'pending' }]
    })]));

    store.dispatch(armTurn({ cardId: 'a', round: 2 }));

    const cells = selectCardRowView(store.getState(), 'a')?.rounds;
    expect(cells?.map(cell => [cell.heading, cell.label, cell.isArmed])).toEqual([
      ['R1', '10.11', false],
      ['R2', 'RS', false],
      ['R3', '—', false],
      ['R4', '—', false],
      ['R5', '—', false],
      ['RS', '—', true]
    ]);
  });

  it('marks the round being edited', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(roundEditorToggled({ cardId: 'a', round: 3 }));

    expect(selectCardRowView(store.getState(), 'a')?.rounds.map(round => round.isEditing)).toEqual([false, false, true, false, false]);
    expect(selectCardRowView(store.getState(), 'b')?.rounds.every(round => !round.isEditing)).toBe(true);
  });
});

describe('scan', () => {
  it('arms the next round of the scanned shooter', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(handleScannedCard(' 111 '));

    const state = store.getState();
    expect(state.match.current?.activeTurn).toMatchObject({ cardId: 'a', round: 2 });
    expect(state.timekeeper.message).toBeNull();
    expect(selectSelectedSquadId(state)).toBe('squad-a');
  });

  it('reports an unknown KNSA number', () => {
    const store = buildStore(buildTwoCardMatch());

    store.dispatch(handleScannedCard('999'));

    expect(store.getState().timekeeper.message).toBe('No shooter with KNSA number 999 in this match.');
    expect(store.getState().match.current?.activeTurn).toBeNull();
  });
});

describe('barcode scanner', () => {
  it('arms the scanned shooter while nobody is armed', () => {
    const store = buildStore(buildTwoCardMatch());
    expect(selectIsScannerListening(store.getState())).toBe(true);

    store.dispatch(handleScannedCard('222'));

    expect(store.getState().match.current?.activeTurn).toMatchObject({ cardId: 'b', round: 1 });
    expect(selectIsScannerListening(store.getState())).toBe(false);
  });

  it('does not replace the armed shooter', () => {
    const store = buildStore(buildTwoCardMatch());
    store.dispatch(armTurn({ cardId: 'a', round: 2 }));

    store.dispatch(handleScannedCard('222'));

    expect(store.getState().match.current?.activeTurn).toMatchObject({ cardId: 'a', round: 2 });
  });

  it('listens again once the turn is cancelled', () => {
    const store = buildStore(buildTwoCardMatch());
    store.dispatch(armTurn({ cardId: 'a', round: 2 }));

    store.dispatch(cancelTurn());

    expect(selectIsScannerListening(store.getState())).toBe(true);
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

describe('score sheet printing', () => {
  function buildFinishedMatch() {
    return buildState([
      buildCard({
        id: 'a',
        knsaNumber: '111',
        discipline: 'SKP',
        rounds: [
          { status: 'timed', timeMs: 5000 },
          { status: 'rs', timeMs: 3000, reshootTimeMs: 4000 },
          { status: 'timed', timeMs: 6000 },
          { status: 'dnf' },
          { status: 'timed', timeMs: 7000 }
        ]
      }),
      buildCard({ id: 'b', shooterId: 'shooter-a' })
    ]);
  }

  it('requests a print when a card is signed off', () => {
    const store = buildStore(buildFinishedMatch());

    store.dispatch(signOff({ cardId: 'a' }));

    expect(store.getState().timekeeper.printRequest).toEqual({ cardId: 'a', requestNumber: 1 });
  });

  it('makes a reprint of the same card a new request', () => {
    const store = buildStore(buildFinishedMatch());
    store.dispatch(signOff({ cardId: 'a' }));
    store.dispatch(scoreSheetPrinted(1));

    store.dispatch(scoreSheetPrintRequested('a'));

    expect(store.getState().timekeeper.printRequest).toEqual({ cardId: 'a', requestNumber: 2 });
  });

  it('keeps a newer request when an older print finishes', () => {
    const store = buildStore(buildFinishedMatch());
    store.dispatch(signOff({ cardId: 'a' }));
    store.dispatch(scoreSheetPrintRequested('b'));

    store.dispatch(scoreSheetPrinted(1));

    expect(store.getState().timekeeper.printRequest).toEqual({ cardId: 'b', requestNumber: 2 });
  });

  it('shows the rounds, the three counted rounds and the score', () => {
    const store = buildStore(buildFinishedMatch());
    store.dispatch(loaded({ userEmail: 'tk@example.com', matchLabel: 'Twente Shoot', shooters: [] }));
    store.dispatch(signOff({ cardId: 'a' }));

    const sheet = selectScoreSheet(store.getState(), 'a');

    expect(sheet).toMatchObject({
      matchLabel: 'Twente Shoot',
      squadLabel: 'A',
      squadTimes: '08:00 - 09:00',
      knsaNumber: '111',
      discipline: 'SKP',
      scoreText: '05.00',
      dqReason: null
    });
    expect(sheet?.rounds.map(round => [round.heading, round.label, round.isCounted])).toEqual([
      ['Round 1', '05.00', true],
      ['Round 2', 'RS', false],
      ['Round 3', '06.00', true],
      ['Round 4', 'DNF', false],
      ['Round 5', '07.00', false],
      ['RS', '04.00', true]
    ]);
    expect(sheet?.signedOffText).not.toBeNull();
  });

  it('marks a sheet voided by a DQ on another card of the shooter', () => {
    const store = buildStore(buildFinishedMatch());

    store.dispatch(disqualify({ cardId: 'b', reason: 'unsafe gun handling' }));

    const sheet = selectScoreSheet(store.getState(), 'a');
    expect(sheet?.scoreText).toBe('DQ');
    expect(sheet?.dqReason).toBe('unsafe gun handling');
    expect(sheet?.rounds.some(round => round.isCounted)).toBe(false);
  });
});
