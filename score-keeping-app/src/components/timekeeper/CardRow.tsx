'use client';

import { useState } from 'react';
import { useSortable } from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';
import { findNextRoundToShoot, formatRoundTimeMs, formatScore, getCardWarnings, isReadyForSignOff } from '@/lib/match/derive';
import { cardScore, isShooterDisqualified } from '@/lib/match/score';
import { armTurn, disqualify, markAbsent, reinstate, signOff, unsign } from '@/store/matchSlice';
import { useAppDispatch } from '@/store/store';
import type { CardWarning } from '@/lib/match/derive';
import type { Card, MatchState, Round } from '@/lib/match/types';
import { RoundEditor } from './RoundEditor';

const WARNING_LABELS: Record<CardWarning, string> = {
  'multiple-rs': 'more than one RS',
  'signed-with-open-rounds': 'signed with open rounds'
};

function describeRound(round: Round, liveTimeMs: number | null): { label: string; modifier: string } {
  if (liveTimeMs !== null) {
    return { label: formatRoundTimeMs(liveTimeMs), modifier: 'live' };
  }
  if (round.status === 'timed') {
    return { label: round.timeMs !== null ? formatRoundTimeMs(round.timeMs) : '—', modifier: 'timed' };
  }
  if (round.status === 'rs') {
    return { label: round.reshootTimeMs !== null ? `RS ${formatRoundTimeMs(round.reshootTimeMs)}` : 'RS', modifier: 'rs' };
  }
  if (round.status === 'dnf') {
    return { label: '--:--', modifier: 'dnf' };
  }
  if (round.status === 'skipped') {
    return { label: '—', modifier: 'skipped' };
  }

  return { label: '—', modifier: 'pending' };
}

interface CardRowProps {
  match: MatchState;
  card: Card;
  position: number;
  liveTimeMs: number | null;
}

export const CardRow = ({ match, card, position, liveTimeMs }: CardRowProps) => {
  const dispatch = useAppDispatch();
  const { attributes, listeners, setNodeRef, transform, transition, isDragging } = useSortable({ id: card.id });
  const [editedRound, setEditedRound] = useState<number | null>(null);
  const [isDqDialogOpen, setIsDqDialogOpen] = useState(false);
  const [dqReason, setDqReason] = useState('');

  const activeTurn = match.activeTurn;
  const isActive = activeTurn?.cardId === card.id;
  const isDisqualified = isShooterDisqualified(match, card.shooterId);
  const warnings = getCardWarnings(card);
  const nextRound = findNextRoundToShoot(card);
  const style = { transform: CSS.Transform.toString(transform), transition };

  function handleConfirmDq() {
    dispatch(disqualify({ cardId: card.id, reason: dqReason.trim() }));
    setIsDqDialogOpen(false);
    setDqReason('');
  }

  const rowClassName = ['tk-queue-row', isActive && 'tk-queue-row--active', isDragging && 'tk-queue-row--dragging']
    .filter(Boolean)
    .join(' ');

  return (
    <div ref={setNodeRef} style={style} className="tk-card-row">
      <div className={rowClassName}>
        <span className="tk-queue-row__handle" aria-label="Drag to reorder" {...attributes} {...listeners}>
          ⠿
        </span>
        <span className="tk-queue-row__position">{position}</span>
        <button
          type="button"
          className="tk-queue-row__name"
          disabled={nextRound === null}
          title={nextRound === null ? 'Nothing left to shoot' : `Arm round ${nextRound}`}
          onClick={() => nextRound !== null && dispatch(armTurn({ cardId: card.id, round: nextRound }))}
        >
          {card.shooterName}
        </button>
        <div className="tk-queue-row__rounds">
          {card.rounds.map((round) => {
            const isLiveRound = isActive && activeTurn?.phase === 'running' && activeTurn.round === round.n;
            const { label, modifier } = describeRound(round, isLiveRound ? liveTimeMs : null);
            const isArmedRound = isActive && activeTurn?.round === round.n;

            return (
              <button
                key={round.n}
                type="button"
                className={`tk-round-cell tk-round-cell--${modifier}${isArmedRound ? ' tk-round-cell--armed' : ''}${editedRound === round.n ? ' tk-round-cell--editing' : ''}`}
                title={`Round ${round.n}: edit`}
                onClick={() => setEditedRound(editedRound === round.n ? null : round.n)}
              >
                <span className="tk-round-cell__number">
                  R
                  {round.n}
                </span>
                {label}
              </button>
            );
          })}
        </div>
        <span className={`tk-score${isDisqualified ? ' tk-score--dq' : ''}`} title="Mean of the 3 fastest rounds">
          {formatScore(cardScore(match, card))}
        </span>
        <div className="tk-queue-row__spacer" />
        <div className="tk-queue-row__actions">
          {warnings.map(warning => (
            <span key={warning} className="tk-warning">{WARNING_LABELS[warning]}</span>
          ))}
          <button type="button" className="tk-button tk-button--small" onClick={() => dispatch(markAbsent({ cardId: card.id }))}>
            Absent
          </button>
          {isDisqualified
            ? (
                <button type="button" className="tk-button tk-button--small" onClick={() => dispatch(reinstate({ shooterId: card.shooterId }))}>
                  Undo DQ
                </button>
              )
            : (
                <button type="button" className="tk-button tk-button--small tk-button--danger" onClick={() => setIsDqDialogOpen(true)}>
                  DQ
                </button>
              )}
          {card.signedOffAt === null
            ? (
                <button
                  type="button"
                  className={`tk-button tk-button--small${isReadyForSignOff(card) ? ' tk-button--primary' : ''}`}
                  onClick={() => dispatch(signOff({ cardId: card.id }))}
                >
                  Signed
                </button>
              )
            : (
                <button type="button" className="tk-button tk-button--small" onClick={() => dispatch(unsign({ cardId: card.id }))}>
                  Unsign
                </button>
              )}
        </div>
      </div>

      {isDqDialogOpen && (
        <div className="tk-inline-dialog">
          <span>
            Disqualify
            {' '}
            <strong>{card.shooterName}</strong>
            ? This voids all their cards in the match.
          </span>
          <input
            type="text"
            className="tk-input"
            placeholder="Reason"
            value={dqReason}
            onChange={event => setDqReason(event.target.value)}
            autoFocus
          />
          <button type="button" className="tk-button tk-button--small tk-button--danger" onClick={handleConfirmDq}>
            Disqualify
          </button>
          <button type="button" className="tk-button tk-button--small" onClick={() => setIsDqDialogOpen(false)}>
            Cancel
          </button>
        </div>
      )}

      {editedRound !== null && (
        <RoundEditor card={card} roundNumber={editedRound} onClose={() => setEditedRound(null)} />
      )}
    </div>
  );
};
