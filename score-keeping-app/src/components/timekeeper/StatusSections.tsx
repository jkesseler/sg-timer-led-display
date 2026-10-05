'use client';

import { cancelTurn, setSquadStatus } from '@/store/matchSlice';
import { useAppDispatch, useAppSelector } from '@/store/store';
import {
  selectIsScannerListening,
  selectMessage,
  selectRoster,
  selectSelectedSquadId,
  selectSquadStatusView,
  selectSquads,
  selectStatusLine
} from '@/store/timekeeperSelectors';
import { squadSelected } from '@/store/timekeeperSlice';
import type { MatchSquad, SquadStatus } from '@/lib/match/types';
import type { SyncStatus } from '@/store/matchSlice';

const SYNC_LABELS: Record<SyncStatus, string> = {
  synced: 'Saved',
  dirty: 'Saving…',
  error: 'Not saved to server — retrying'
};

const SQUAD_STATUSES: SquadStatus[] = ['scheduled', 'active', 'completed'];

// --- Status line --------------------------------------------------------

interface StatusLineViewProps {
  syncStatus: SyncStatus;
  deviceId: string | null;
  isTimerOnline: boolean;
  isBrokerConnected: boolean;
  /** The barcode scanner arms shooters only while nobody is armed. */
  isScannerListening: boolean;
}

export const StatusLineView = ({ syncStatus, deviceId, isTimerOnline, isBrokerConnected, isScannerListening }: StatusLineViewProps) => (
  <div className="tk-status-line">
    <span className={`tk-pill tk-pill--${syncStatus}`}>{SYNC_LABELS[syncStatus]}</span>
    <span className={`tk-pill${isTimerOnline ? ' tk-pill--synced' : ' tk-pill--error'}`}>
      {`Timer ${deviceId ?? 'none'}: ${isTimerOnline ? 'online' : 'offline'}`}
    </span>
    <span className={`tk-pill${isBrokerConnected ? ' tk-pill--synced' : ' tk-pill--error'}`}>
      {`MQTT ${isBrokerConnected ? 'connected' : 'disconnected'}`}
    </span>
    <span className={`tk-pill${isScannerListening ? ' tk-pill--synced' : ''}`}>
      {isScannerListening ? 'Scanner ready' : 'Scanner paused: shooter armed'}
    </span>
  </div>
);

export const StatusLine = () => (
  <StatusLineView {...useAppSelector(selectStatusLine)} isScannerListening={useAppSelector(selectIsScannerListening)} />
);

// --- Squad tabs ---------------------------------------------------------

interface SquadTabsViewProps {
  squads: MatchSquad[];
  selectedSquadId: string | null;
  onSelect: (squadId: string) => void;
}

export const SquadTabsView = ({ squads, selectedSquadId, onSelect }: SquadTabsViewProps) => (
  <nav className="tk-tabs" aria-label="Squads">
    {squads.map(squad => (
      <button
        key={squad.id}
        type="button"
        className={`tk-tab${squad.id === selectedSquadId ? ' tk-tab--selected' : ''}`}
        onClick={() => onSelect(squad.id)}
      >
        {squad.label}
        <span className="tk-tab__meta">{squad.status}</span>
      </button>
    ))}
  </nav>
);

export const SquadTabs = () => {
  const dispatch = useAppDispatch();

  return (
    <SquadTabsView
      squads={useAppSelector(selectSquads)}
      selectedSquadId={useAppSelector(selectSelectedSquadId)}
      onSelect={squadId => dispatch(squadSelected(squadId))}
    />
  );
};

// --- Squad status -------------------------------------------------------

interface SquadStatusCardViewProps {
  roundLabel: string;
  squadStatus: SquadStatus;
  turn: { verb: string; shooterName: string; round: number } | null;
  onSquadStatusChange: (status: SquadStatus) => void;
  onCancelTurn: () => void;
}

export const SquadStatusCardView = ({ roundLabel, squadStatus, turn, onSquadStatusChange, onCancelTurn }: SquadStatusCardViewProps) => (
  <section className="tk-card tk-status-card">
    <div>
      <div className="tk-status-card__round">{roundLabel}</div>
      <span className={`tk-status-badge${turn ? ' tk-status-badge--active' : ''}`}>
        <span className="tk-status-badge__dot" />
        {turn
          ? (
              <span>
                {`${turn.verb}: `}
                <strong>{turn.shooterName}</strong>
                {` round ${turn.round}`}
              </span>
            )
          : <span>Nobody armed — scan a card or click a name</span>}
      </span>
    </div>
    <div className="tk-status-card__actions">
      <label className="tk-round-editor__label" htmlFor="squad-status">Squad</label>
      <select
        id="squad-status"
        className="tk-input"
        value={squadStatus}
        onChange={event => onSquadStatusChange(event.target.value as SquadStatus)}
      >
        {SQUAD_STATUSES.map(status => <option key={status} value={status}>{status}</option>)}
      </select>
      {turn && (
        <button type="button" className="tk-button tk-button--danger" onClick={onCancelTurn}>
          Cancel turn
        </button>
      )}
    </div>
  </section>
);

export const SquadStatusCard = () => {
  const dispatch = useAppDispatch();
  const view = useAppSelector(selectSquadStatusView);
  if (!view) {
    return null;
  }

  return (
    <SquadStatusCardView
      roundLabel={view.roundLabel}
      squadStatus={view.squadStatus}
      turn={view.turn}
      onSquadStatusChange={status => dispatch(setSquadStatus({ squadId: view.squadId, status }))}
      onCancelTurn={() => dispatch(cancelTurn())}
    />
  );
};

// --- Roster -------------------------------------------------------------

interface RosterViewProps {
  nextName: string | null;
  onDeckName: string | null;
}

export const RosterView = ({ nextName, onDeckName }: RosterViewProps) => (
  <section className="tk-roster">
    <div className="tk-roster__item">
      <span className="tk-roster__label">Next</span>
      <span className="tk-roster__name">{nextName ?? '—'}</span>
    </div>
    <div className="tk-roster__item tk-roster__item--on-deck">
      <span className="tk-roster__label">On deck</span>
      <span className="tk-roster__name">{onDeckName ?? '—'}</span>
    </div>
  </section>
);

export const Roster = () => <RosterView {...useAppSelector(selectRoster)} />;

// --- Message ------------------------------------------------------------

export const MessageView = ({ message }: { message: string | null }) =>
  message ? <div className="tk-error">{message}</div> : null;

export const Message = () => <MessageView message={useAppSelector(selectMessage)} />;
