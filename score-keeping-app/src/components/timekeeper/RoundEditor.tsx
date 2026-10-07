'use client';

import { useId } from 'react';
import { useAppDispatch, useAppSelector } from '@/store/store';
import { selectEditor, selectRoundEditorView } from '@/store/timekeeperSelectors';
import { editorReshootTextChanged, editorTimeTextChanged, roundEditorClosed } from '@/store/timekeeperSlice';
import {
  armEditorRound,
  clearEditorReshoot,
  clearEditorTime,
  saveEditorReshoot,
  saveEditorTime,
  setEditorRoundStatus
} from '@/store/timekeeperThunks';
import type { RoundEditorProps, RoundEditorViewProps, StatusButton } from './types';

const STATUS_BUTTONS: StatusButton[] = [
  { status: 'pending', label: 'Pending' },
  { status: 'timed', label: 'Timed' },
  { status: 'rs', label: 'RS' },
  { status: 'dnf', label: 'DNF' },
  { status: 'skipped', label: 'Skipped' }
];

export const RoundEditorView = ({
  round,
  shooterName,
  status,
  timeText,
  reshootText,
  error,
  onStatusChange,
  onTimeTextChange,
  onSaveTime,
  onClearTime,
  onReshootTextChange,
  onSaveReshoot,
  onClearReshoot,
  onArm,
  onClose
}: RoundEditorViewProps) => {
  const timeInputId = useId();
  const reshootInputId = useId();

  return (
    <div className="tk-round-editor">
      <div className="tk-round-editor__title">{`${shooterName} · round ${round}`}</div>

      <div className="tk-round-editor__group">
        {STATUS_BUTTONS.map(button => (
          <button
            key={button.status}
            type="button"
            className={`tk-button tk-button--small${status === button.status ? ' tk-button--primary' : ''}`}
            onClick={() => onStatusChange(button.status)}
          >
            {button.label}
          </button>
        ))}
      </div>

      <div className="tk-round-editor__group">
        <label className="tk-round-editor__label" htmlFor={timeInputId}>Time (s)</label>
        <input
          id={timeInputId}
          type="text"
          inputMode="decimal"
          className="tk-input tk-input--time"
          value={timeText}
          onChange={event => onTimeTextChange(event.target.value)}
        />
        <button type="button" className="tk-button tk-button--small" onClick={onSaveTime}>Save time</button>
        <button type="button" className="tk-button tk-button--small" onClick={onClearTime}>Clear time</button>
      </div>

      {status === 'rs' && (
        <div className="tk-round-editor__group">
          <label className="tk-round-editor__label" htmlFor={reshootInputId}>Reshoot (s)</label>
          <input
            id={reshootInputId}
            type="text"
            inputMode="decimal"
            className="tk-input tk-input--time"
            value={reshootText}
            onChange={event => onReshootTextChange(event.target.value)}
          />
          <button type="button" className="tk-button tk-button--small" onClick={onSaveReshoot}>Save reshoot</button>
          <button type="button" className="tk-button tk-button--small" onClick={onClearReshoot}>Clear reshoot</button>
        </div>
      )}

      {error && <div className="tk-error">{error}</div>}

      <div className="tk-round-editor__group">
        <button type="button" className="tk-button tk-button--small tk-button--primary" onClick={onArm}>
          Arm this round
        </button>
        <button type="button" className="tk-button tk-button--small" onClick={onClose}>Close</button>
      </div>
    </div>
  );
};

/** The round editor for this card, if the open editor is on it. */
export const RoundEditor = ({ cardId }: RoundEditorProps) => {
  const dispatch = useAppDispatch();
  const editor = useAppSelector(selectEditor);
  const editedRound = editor?.cardId === cardId ? editor.round : null;
  const view = useAppSelector(state => (editedRound === null ? null : selectRoundEditorView(state, cardId, editedRound)));

  if (!view) {
    return null;
  }

  return (
    <RoundEditorView
      {...view}
      onStatusChange={status => dispatch(setEditorRoundStatus(status))}
      onTimeTextChange={text => dispatch(editorTimeTextChanged(text))}
      onSaveTime={() => dispatch(saveEditorTime())}
      onClearTime={() => dispatch(clearEditorTime())}
      onReshootTextChange={text => dispatch(editorReshootTextChanged(text))}
      onSaveReshoot={() => dispatch(saveEditorReshoot())}
      onClearReshoot={() => dispatch(clearEditorReshoot())}
      onArm={() => dispatch(armEditorRound())}
      onClose={() => dispatch(roundEditorClosed())}
    />
  );
};
