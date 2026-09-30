'use client';

import { useState } from 'react';
import { parseTimeInput } from '@/lib/match/derive';
import { armTurn, flagDnf, flagRs, setReshootTime, setRoundStatus, setRoundTime } from '@/store/matchSlice';
import { useAppDispatch } from '@/store/store';
import type { Card, RoundStatus } from '@/lib/match/types';

const STATUS_BUTTONS: { status: RoundStatus; label: string }[] = [
  { status: 'pending', label: 'Pending' },
  { status: 'timed', label: 'Timed' },
  { status: 'rs', label: 'RS' },
  { status: 'dnf', label: 'DNF' },
  { status: 'skipped', label: 'Skipped' }
];

// Seconds with full ms precision, so saving an untouched value never changes it.
function toSecondsText(timeMs: number | null): string {
  return timeMs === null ? '' : String(timeMs / 1000);
}

interface RoundEditorProps {
  card: Card;
  roundNumber: number;
  onClose: () => void;
}

export const RoundEditor = ({ card, roundNumber, onClose }: RoundEditorProps) => {
  const dispatch = useAppDispatch();
  const round = card.rounds.find(candidate => candidate.n === roundNumber);
  const [timeText, setTimeText] = useState(toSecondsText(round?.timeMs ?? null));
  const [reshootText, setReshootText] = useState(toSecondsText(round?.reshootTimeMs ?? null));
  const [error, setError] = useState<string | null>(null);

  if (!round) {
    return null;
  }

  const ref = { cardId: card.id, round: round.n };

  function handleSaveTime() {
    const timeMs = parseTimeInput(timeText);
    if (timeMs === null) {
      setError('Enter a time in seconds, e.g. 12.34');

      return;
    }
    setError(null);
    dispatch(setRoundTime({ ...ref, timeMs }));
  }

  function handleSaveReshoot() {
    const reshootTimeMs = parseTimeInput(reshootText);
    if (reshootTimeMs === null) {
      setError('Enter a time in seconds, e.g. 12.34');

      return;
    }
    setError(null);
    dispatch(setReshootTime({ ...ref, reshootTimeMs }));
  }

  function handleSetStatus(status: RoundStatus) {
    if (status === 'rs') {
      dispatch(flagRs(ref));

      return;
    }
    if (status === 'dnf') {
      dispatch(flagDnf(ref));

      return;
    }
    dispatch(setRoundStatus({ ...ref, status }));
  }

  return (
    <div className="tk-round-editor">
      <div className="tk-round-editor__title">
        {card.shooterName}
        {' '}
        · round
        {' '}
        {round.n}
      </div>

      <div className="tk-round-editor__group">
        {STATUS_BUTTONS.map(({ status, label }) => (
          <button
            key={status}
            type="button"
            className={`tk-button tk-button--small${round.status === status ? ' tk-button--primary' : ''}`}
            onClick={() => handleSetStatus(status)}
          >
            {label}
          </button>
        ))}
      </div>

      <div className="tk-round-editor__group">
        <label className="tk-round-editor__label" htmlFor={`time-${card.id}-${round.n}`}>Time (s)</label>
        <input
          id={`time-${card.id}-${round.n}`}
          type="text"
          inputMode="decimal"
          className="tk-input tk-input--time"
          value={timeText}
          onChange={event => setTimeText(event.target.value)}
        />
        <button type="button" className="tk-button tk-button--small" onClick={handleSaveTime}>Save time</button>
        <button
          type="button"
          className="tk-button tk-button--small"
          onClick={() => {
            setTimeText('');
            dispatch(setRoundTime({ ...ref, timeMs: null }));
          }}
        >
          Clear time
        </button>
      </div>

      {round.status === 'rs' && (
        <div className="tk-round-editor__group">
          <label className="tk-round-editor__label" htmlFor={`reshoot-${card.id}-${round.n}`}>Reshoot (s)</label>
          <input
            id={`reshoot-${card.id}-${round.n}`}
            type="text"
            inputMode="decimal"
            className="tk-input tk-input--time"
            value={reshootText}
            onChange={event => setReshootText(event.target.value)}
          />
          <button type="button" className="tk-button tk-button--small" onClick={handleSaveReshoot}>Save reshoot</button>
          <button
            type="button"
            className="tk-button tk-button--small"
            onClick={() => {
              setReshootText('');
              dispatch(setReshootTime({ ...ref, reshootTimeMs: null }));
            }}
          >
            Clear reshoot
          </button>
        </div>
      )}

      {error && <div className="tk-error">{error}</div>}

      <div className="tk-round-editor__group">
        <button type="button" className="tk-button tk-button--small tk-button--primary" onClick={() => dispatch(armTurn(ref))}>
          Arm this round
        </button>
        <button type="button" className="tk-button tk-button--small" onClick={onClose}>
          Close
        </button>
      </div>
    </div>
  );
};
