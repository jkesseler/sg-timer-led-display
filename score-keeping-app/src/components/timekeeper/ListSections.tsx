'use client';

import { DISCIPLINES } from '@/lib/domain/disciplines';
import { ROUNDS_PER_CARD } from '@/lib/match/types';
import { armTurn, discardUnassignedResult, markPresent } from '@/store/matchSlice';
import { useAppDispatch, useAppSelector } from '@/store/store';
import { selectAbsentRows, selectLateShooterView, selectOutstanding, selectUnassignedView } from '@/store/timekeeperSelectors';
import { lateShooterChanged, unassignedDraftChanged } from '@/store/timekeeperSlice';
import { addLateShooter, assignUnassignedResult } from '@/store/timekeeperThunks';
import type { Discipline } from '@/lib/domain/disciplines';
import type { ShooterOption } from '@/lib/match/bootstrap';

const ROUND_NUMBERS = Array.from({ length: ROUNDS_PER_CARD }, (_, index) => index + 1);

// --- Unassigned results -------------------------------------------------

interface UnassignedResultRow {
  resultId: string;
  timeText: string;
  at: string;
  cardId: string;
  round: number;
}

interface UnassignedResultsViewProps {
  results: UnassignedResultRow[];
  shooterOptions: { cardId: string; shooterName: string }[];
  onCardChange: (resultId: string, cardId: string) => void;
  onRoundChange: (resultId: string, round: number) => void;
  onAssign: (resultId: string) => void;
  onDiscard: (resultId: string) => void;
}

/** Timer results that arrived with nobody armed; the timekeeper puts each on the right round. */
export const UnassignedResultsView = ({ results, shooterOptions, onCardChange, onRoundChange, onAssign, onDiscard }: UnassignedResultsViewProps) => {
  if (results.length === 0) {
    return null;
  }

  return (
    <section className="tk-card tk-card--warning">
      <div className="tk-section-title">Unassigned results</div>
      <div className="tk-list">
        {results.map(result => (
          <div className="tk-list-row" key={result.resultId}>
            <span>
              <strong>{result.timeText}</strong>
              {' '}
              <span className="tk-list-row__meta">{new Date(result.at).toLocaleTimeString()}</span>
            </span>
            <div className="tk-inline-form">
              <select className="tk-input" value={result.cardId} onChange={event => onCardChange(result.resultId, event.target.value)} aria-label="Shooter">
                {shooterOptions.map(option => <option key={option.cardId} value={option.cardId}>{option.shooterName}</option>)}
              </select>
              <select className="tk-input" value={result.round} onChange={event => onRoundChange(result.resultId, Number(event.target.value))} aria-label="Round">
                {ROUND_NUMBERS.map(round => <option key={round} value={round}>{`Round ${round}`}</option>)}
              </select>
              <button
                type="button"
                className="tk-button tk-button--small tk-button--primary"
                disabled={!result.cardId}
                onClick={() => onAssign(result.resultId)}
              >
                Assign
              </button>
              <button type="button" className="tk-button tk-button--small" onClick={() => onDiscard(result.resultId)}>Discard</button>
            </div>
          </div>
        ))}
      </div>
    </section>
  );
};

export const UnassignedResults = () => {
  const dispatch = useAppDispatch();
  const { results, shooterOptions } = useAppSelector(selectUnassignedView);

  return (
    <UnassignedResultsView
      results={results}
      shooterOptions={shooterOptions}
      onCardChange={(resultId, cardId) => dispatch(unassignedDraftChanged({ resultId, cardId }))}
      onRoundChange={(resultId, round) => dispatch(unassignedDraftChanged({ resultId, round }))}
      onAssign={resultId => dispatch(assignUnassignedResult(resultId))}
      onDiscard={resultId => dispatch(discardUnassignedResult({ resultId }))}
    />
  );
};

// --- Outstanding --------------------------------------------------------

interface OutstandingRow {
  cardId: string;
  shooterName: string;
  round: number;
  kindLabel: string;
}

interface OutstandingListViewProps {
  items: OutstandingRow[];
  onArm: (cardId: string, round: number) => void;
}

export const OutstandingListView = ({ items, onArm }: OutstandingListViewProps) => {
  if (items.length === 0) {
    return null;
  }

  return (
    <section>
      <div className="tk-section-title">Outstanding (reshoots &amp; catch-up rounds)</div>
      <div className="tk-list">
        {items.map(item => (
          <div className="tk-list-row" key={`${item.cardId}-${item.round}`}>
            <span>
              {item.shooterName}
              {' '}
              <span className="tk-list-row__meta">{`round ${item.round} · ${item.kindLabel}`}</span>
            </span>
            <button type="button" className="tk-button tk-button--small" onClick={() => onArm(item.cardId, item.round)}>Arm</button>
          </div>
        ))}
      </div>
    </section>
  );
};

export const OutstandingList = () => {
  const dispatch = useAppDispatch();

  return (
    <OutstandingListView
      items={useAppSelector(selectOutstanding)}
      onArm={(cardId, round) => dispatch(armTurn({ cardId, round }))}
    />
  );
};

// --- Absent -------------------------------------------------------------

interface AbsentListViewProps {
  rows: { cardId: string; shooterName: string }[];
  onMarkPresent: (cardId: string) => void;
}

export const AbsentListView = ({ rows, onMarkPresent }: AbsentListViewProps) => {
  if (rows.length === 0) {
    return null;
  }

  return (
    <section>
      <div className="tk-section-title">Absent</div>
      <div className="tk-list">
        {rows.map(row => (
          <div className="tk-list-row" key={row.cardId}>
            <span>{row.shooterName}</span>
            <button type="button" className="tk-button tk-button--small" onClick={() => onMarkPresent(row.cardId)}>
              Mark present / rejoin
            </button>
          </div>
        ))}
      </div>
    </section>
  );
};

export const AbsentList = () => {
  const dispatch = useAppDispatch();

  return (
    <AbsentListView
      rows={useAppSelector(selectAbsentRows)}
      onMarkPresent={cardId => dispatch(markPresent({ cardId }))}
    />
  );
};

// --- Late shooter -------------------------------------------------------

interface LateShooterFormViewProps {
  shooters: ShooterOption[];
  shooterId: string;
  discipline: Discipline;
  onShooterChange: (shooterId: string) => void;
  onDisciplineChange: (discipline: Discipline) => void;
  onAdd: () => void;
}

export const LateShooterFormView = ({ shooters, shooterId, discipline, onShooterChange, onDisciplineChange, onAdd }: LateShooterFormViewProps) => (
  <section className="tk-card">
    <div className="tk-section-title">Add a late shooter to this squad</div>
    <div className="tk-inline-form">
      <select className="tk-input" value={shooterId} onChange={event => onShooterChange(event.target.value)} aria-label="Shooter">
        <option value="">Choose a shooter…</option>
        {shooters.map(shooter => <option key={shooter.id} value={shooter.id}>{shooter.name}</option>)}
      </select>
      <select
        className="tk-input"
        value={discipline}
        onChange={event => onDisciplineChange(event.target.value as Discipline)}
        aria-label="Discipline"
      >
        {DISCIPLINES.map(option => <option key={option} value={option}>{option}</option>)}
      </select>
      <button type="button" className="tk-button tk-button--small tk-button--primary" disabled={!shooterId} onClick={onAdd}>
        Add
      </button>
    </div>
  </section>
);

export const LateShooterForm = () => {
  const dispatch = useAppDispatch();
  const { squadId, shooters, shooterId, discipline } = useAppSelector(selectLateShooterView);

  if (!squadId) {
    return null;
  }

  return (
    <LateShooterFormView
      shooters={shooters}
      shooterId={shooterId}
      discipline={discipline}
      onShooterChange={nextShooterId => dispatch(lateShooterChanged({ shooterId: nextShooterId }))}
      onDisciplineChange={nextDiscipline => dispatch(lateShooterChanged({ discipline: nextDiscipline }))}
      onAdd={() => dispatch(addLateShooter())}
    />
  );
};
