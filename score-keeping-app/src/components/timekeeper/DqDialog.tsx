'use client';

import { useAppDispatch, useAppSelector } from '@/store/store';
import { selectDqDialogForCard } from '@/store/timekeeperSelectors';
import { dqDialogClosed, dqReasonChanged } from '@/store/timekeeperSlice';
import { confirmDisqualification } from '@/store/timekeeperThunks';
import type { FormEvent, KeyboardEvent } from 'react';

interface DqDialogViewProps {
  shooterName: string;
  reason: string;
  onReasonChange: (reason: string) => void;
  onConfirm: () => void;
  onCancel: () => void;
}

export const DqDialogView = ({ shooterName, reason, onReasonChange, onConfirm, onCancel }: DqDialogViewProps) => {
  function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    onConfirm();
  }

  function handleKeyDown(event: KeyboardEvent<HTMLFormElement>) {
    if (event.key === 'Escape') {
      onCancel();
    }
  }

  return (
    <form className="tk-inline-dialog" onSubmit={handleSubmit} onKeyDown={handleKeyDown}>
      <span>
        Disqualify
        {' '}
        <strong>{shooterName}</strong>
        ? This voids all their cards in the match.
      </span>
      <input
        type="text"
        className="tk-input"
        placeholder="Reason"
        aria-label="Disqualification reason"
        value={reason}
        onChange={event => onReasonChange(event.target.value)}
        autoFocus
      />
      <button type="submit" className="tk-button tk-button--small tk-button--danger">Disqualify</button>
      <button type="button" className="tk-button tk-button--small" onClick={onCancel}>Cancel</button>
    </form>
  );
};

/** The DQ confirmation for this card, if it is the one being disqualified. */
export const DqDialog = ({ cardId }: { cardId: string }) => {
  const dispatch = useAppDispatch();
  const dialog = useAppSelector(state => selectDqDialogForCard(state, cardId));

  if (!dialog) {
    return null;
  }

  return (
    <DqDialogView
      shooterName={dialog.shooterName}
      reason={dialog.reason}
      onReasonChange={reason => dispatch(dqReasonChanged(reason))}
      onConfirm={() => dispatch(confirmDisqualification())}
      onCancel={() => dispatch(dqDialogClosed())}
    />
  );
};
