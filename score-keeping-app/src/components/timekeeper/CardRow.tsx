'use client';

import { useSortable } from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';
import { armTurn, markAbsent, reinstate, signOff, unsign } from '@/store/matchSlice';
import { useAppDispatch, useAppSelector } from '@/store/store';
import { selectCardRowView } from '@/store/timekeeperSelectors';
import { dqDialogOpened, roundEditorToggled, scoreSheetPrintRequested } from '@/store/timekeeperSlice';
import type { CardRowView as CardRowData } from '@/store/timekeeperSelectors';
import { DqDialog } from './DqDialog';
import { RoundEditor } from './RoundEditor';
import type { DraggableAttributes, DraggableSyntheticListeners } from '@dnd-kit/core';
import type { CSSProperties, ReactNode } from 'react';

function joinClassNames(...classNames: (string | false)[]): string {
  return classNames.filter(Boolean).join(' ');
}

export interface SortableProps {
  setNodeRef: (element: HTMLElement | null) => void;
  style: CSSProperties;
  attributes: DraggableAttributes;
  listeners: DraggableSyntheticListeners;
  isDragging: boolean;
}

interface CardRowViewProps {
  card: CardRowData;
  position: number;
  sortable: SortableProps;
  onArmNextRound: () => void;
  onToggleRound: (round: number) => void;
  onMarkAbsent: () => void;
  onDisqualify: () => void;
  onReinstate: () => void;
  onSignOff: () => void;
  onUnsign: () => void;
  onReprint: () => void;
  /** The DQ dialog and round editor, rendered below the row. */
  children?: ReactNode;
}

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
    <div className={joinClassNames('tk-queue-row', card.isActive && 'tk-queue-row--active', sortable.isDragging && 'tk-queue-row--dragging')}>
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
            className={joinClassNames(
              'tk-round-cell',
              `tk-round-cell--${round.modifier}`,
              round.isArmed && 'tk-round-cell--armed',
              round.isEditing && 'tk-round-cell--editing'
            )}
            title={round.heading === 'RS' ? `Reshoot of round ${round.n}: edit` : `Round ${round.n}: edit`}
            aria-expanded={round.isEditing}
            onClick={() => onToggleRound(round.n)}
          >
            <span className="tk-round-cell__number">{round.heading}</span>
            {round.label}
          </button>
        ))}
      </div>
      <span className={joinClassNames('tk-score', card.isDisqualified && 'tk-score--dq')} title="Mean of the 3 fastest rounds">
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
                className={joinClassNames('tk-button tk-button--small', card.isReadyForSignOff && 'tk-button--primary')}
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

export const CardRow = ({ cardId, position }: { cardId: string; position: number }) => {
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
