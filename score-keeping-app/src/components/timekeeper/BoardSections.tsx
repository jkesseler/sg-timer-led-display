'use client';

import { useState } from 'react';
import { formatRoundTimeMs } from '@/lib/match/derive';
import { addCard, assignResult, discardUnassignedResult } from '@/store/matchSlice';
import { useAppDispatch } from '@/store/store';
import type { Card, MatchState, UnassignedResult } from '@/lib/match/types';

export interface ShooterOption {
  id: string;
  name: string;
  knsaNumber: string | null;
}

interface UnassignedResultsProps {
  match: MatchState;
  squadCards: Card[];
}

/** Timer results that arrived with nobody armed; the timekeeper puts each on the right round. */
export const UnassignedResults = ({ match, squadCards }: UnassignedResultsProps) => {
  if (match.unassignedResults.length === 0) {
    return null;
  }

  return (
    <section className="tk-card tk-card--warning">
      <div className="tk-section-title">Unassigned results</div>
      <div className="tk-list">
        {match.unassignedResults.map(result => (
          <UnassignedResultRow key={result.id} result={result} squadCards={squadCards} />
        ))}
      </div>
    </section>
  );
};

interface UnassignedResultRowProps {
  result: UnassignedResult;
  squadCards: Card[];
}

const UnassignedResultRow = ({ result, squadCards }: UnassignedResultRowProps) => {
  const dispatch = useAppDispatch();
  const [cardId, setCardId] = useState(squadCards[0]?.id ?? '');
  const [round, setRound] = useState(1);

  return (
    <div className="tk-list-row">
      <span>
        <strong>{formatRoundTimeMs(result.timeMs)}</strong>
        {' '}
        <span className="tk-list-row__meta">{new Date(result.at).toLocaleTimeString()}</span>
      </span>
      <div className="tk-inline-form">
        <select className="tk-input" value={cardId} onChange={event => setCardId(event.target.value)} aria-label="Shooter">
          {squadCards.map(card => <option key={card.id} value={card.id}>{card.shooterName}</option>)}
        </select>
        <select className="tk-input" value={round} onChange={event => setRound(Number(event.target.value))} aria-label="Round">
          {[1, 2, 3, 4, 5].map(roundNumber => <option key={roundNumber} value={roundNumber}>{`Round ${roundNumber}`}</option>)}
        </select>
        <button
          type="button"
          className="tk-button tk-button--small tk-button--primary"
          disabled={!cardId}
          onClick={() => dispatch(assignResult({ resultId: result.id, cardId, round }))}
        >
          Assign
        </button>
        <button type="button" className="tk-button tk-button--small" onClick={() => dispatch(discardUnassignedResult({ resultId: result.id }))}>
          Discard
        </button>
      </div>
    </div>
  );
};

interface LateShooterFormProps {
  squadId: string;
  squadCards: Card[];
  shooters: ShooterOption[];
}

export const LateShooterForm = ({ squadId, squadCards, shooters }: LateShooterFormProps) => {
  const dispatch = useAppDispatch();
  const [shooterId, setShooterId] = useState('');

  const available = shooters.filter(shooter => !squadCards.some(card => card.shooterId === shooter.id));
  const selected = available.find(shooter => shooter.id === shooterId);

  function handleAdd() {
    if (!selected) {
      return;
    }

    dispatch(addCard({ squadId, shooterId: selected.id, shooterName: selected.name, knsaNumber: selected.knsaNumber }));
    setShooterId('');
  }

  return (
    <section className="tk-card">
      <div className="tk-section-title">Add a late shooter to this squad</div>
      <div className="tk-inline-form">
        <select className="tk-input" value={shooterId} onChange={event => setShooterId(event.target.value)} aria-label="Shooter">
          <option value="">Choose a shooter…</option>
          {available.map(shooter => <option key={shooter.id} value={shooter.id}>{shooter.name}</option>)}
        </select>
        <button type="button" className="tk-button tk-button--small tk-button--primary" disabled={!selected} onClick={handleAdd}>
          Add
        </button>
      </div>
    </section>
  );
};
