'use client';

import { useSortable } from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';
import objstr from 'obj-str';
import { armTurn, markAbsent, reinstate, signOff, unsign } from '@/store/matchSlice';
import { useAppDispatch, useAppSelector } from '@/store/store';
import { selectCardRowView } from '@/store/timekeeperSelectors';
import { dqDialogOpened, roundEditorToggled, scoreSheetPrintRequested } from '@/store/timekeeperSlice';
import { DqDialog } from './DqDialog';
import { RoundEditor } from './RoundEditor';
import type { CardRowProps, CardRowViewProps, SortableProps } from './types';

export const CardRowView = ({
  card,
  position,
  sortable,
  onArmNextRound,
  onToggleRound,
  onMarkAbsent,
  onDisqualify,
  onReinstate,
  onSignOff,
  onUnsign,
  onReprint,
  children
}: CardRowViewProps) => (
  <div ref={sortable.setNodeRef} style={sortable.style} className="tk-card-row">
    <div className={objstr({
      'tk-queue-row': true,
      'tk-queue-row--active': card.isActive,
      'tk-queue-row--dragging': sortable.isDragging
    })}
    >
      <span className="tk-queue-row__handle" aria-label="Drag to reorder" {...sortable.attributes} {...sortable.listeners}>
        ⠿
      </span>
      <span className="tk-queue-row__position">{position}</span>
      <button
        type="button"
        className="tk-queue-row__name"
        disabled={card.nextRound === null}
        title={card.nextRound === null ? 'Nothing left to shoot' : `Arm round ${card.nextRound}`}
        onClick={onArmNextRound}
      >
        {card.shooterName}
      </button>
      {/* Always rendered so the grid columns after it stay in place. */}
      <span className="tk-queue-row__discipline">{card.discipline}</span>
      <div className="tk-queue-row__rounds">
        {card.rounds.map(round => (
          <button
            key={round.key}
            type="button"
            className={objstr({
              'tk-round-cell': true,
              [`tk-round-cell--${round.modifier}`]: true,
              'tk-round-cell--armed': round.isArmed,
              'tk-round-cell--editing': round.isEditing
            })}
            title={round.heading === 'RS' ? `Reshoot of round ${round.n}: edit` : `Round ${round.n}: edit`}
            aria-expanded={round.isEditing}
            onClick={() => onToggleRound(round.n)}
          >
            <span className="tk-round-cell__number">{round.heading}</span>
            {round.label}
          </button>
        ))}
      </div>
      <span className={objstr({ 'tk-score': true, 'tk-score--dq': card.isDisqualified })} title="Mean of the 3 fastest rounds">
        {card.scoreText}
      </span>
      <div className="tk-queue-row__actions">
        {card.warnings.map(warning => <span key={warning} className="tk-warning">{warning}</span>)}
        <button type="button" className="tk-button tk-button--small" onClick={onMarkAbsent}>Absent</button>
        {card.isDisqualified
          ? <button type="button" className="tk-button tk-button--small" onClick={onReinstate}>Undo DQ</button>
          : <button type="button" className="tk-button tk-button--small tk-button--danger" onClick={onDisqualify}>DQ</button>}
        {card.isSignedOff
          ? (
              <>
                <button type="button" className="tk-button tk-button--small" onClick={onReprint}>Reprint</button>
                <button type="button" className="tk-button tk-button--small" onClick={onUnsign}>Unsign</button>
              </>
            )
          : (
              <button
                type="button"
                className={objstr({ 'tk-button tk-button--small': true, 'tk-button--primary': card.isReadyForSignOff })}
                onClick={onSignOff}
              >
                Signed
              </button>
            )}
      </div>
    </div>
    {children}
  </div>
);

export const CardRow = ({ cardId, position }: CardRowProps) => {
  const dispatch = useAppDispatch();
  const card = useAppSelector(state => selectCardRowView(state, cardId));
  const { attributes, listeners, setNodeRef, transform, transition, isDragging } = useSortable({ id: cardId });

  if (!card) {
    return null;
  }

  const sortable: SortableProps = {
    setNodeRef,
    style: { transform: CSS.Transform.toString(transform), transition },
    attributes,
    listeners,
    isDragging
  };

  return (
    <CardRowView
      card={card}
      position={position}
      sortable={sortable}
      onArmNextRound={() => card.nextRound !== null && dispatch(armTurn({ cardId, round: card.nextRound }))}
      onToggleRound={round => dispatch(roundEditorToggled({ cardId, round }))}
      onMarkAbsent={() => dispatch(markAbsent({ cardId }))}
      onDisqualify={() => dispatch(dqDialogOpened(cardId))}
      onReinstate={() => dispatch(reinstate({ shooterId: card.shooterId }))}
      onSignOff={() => dispatch(signOff({ cardId }))}
      onUnsign={() => dispatch(unsign({ cardId }))}
      onReprint={() => dispatch(scoreSheetPrintRequested(cardId))}
    >
      <DqDialog cardId={cardId} />
      <RoundEditor cardId={cardId} />
    </CardRowView>
  );
};
