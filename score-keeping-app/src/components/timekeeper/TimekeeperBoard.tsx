'use client';

import { useEffect, useState } from 'react';
import { DndContext, closestCenter, PointerSensor, TouchSensor, useSensor, useSensors } from '@dnd-kit/core';
import { SortableContext, verticalListSortingStrategy, arrayMove } from '@dnd-kit/sortable';
import { appendAudit, saveMatchState } from '@/app/timekeeper/(protected)/actions';
import { ReduxProvider } from '@/components/display/ReduxProvider';
import SplitList from '@/components/display/SplitList';
import {
  deriveCurrentRound,
  deriveOutstanding,
  deriveUpcomingShooters,
  findCardByKnsa,
  findCurrentSquadId,
  findNextRoundToShoot,
  getSquadCards
} from '@/lib/match/derive';
import { DisplayState } from '@/lib/mqtt/types';
import { armTurn, cancelTurn, hydrate, markPresent, reorderQueue, setSquadStatus } from '@/store/matchSlice';
import { disconnectMqttClient } from '@/store/mqttMiddleware';
import { selectDevice, selectDisplayState, selectIsConnected, selectKnownDevices, selectShots, startConnecting } from '@/store/mqttSlice';
import { useAppDispatch, useAppSelector } from '@/store/store';
import { getLocalMatchState, resolveInitialMatch } from '@/store/syncMiddleware';
import type { MatchState, SquadStatus } from '@/lib/match/types';
import type { MqttServerConfig } from '@/lib/mqtt/config';
import type { SyncStatus } from '@/store/matchSlice';
import type { SyncTransport } from '@/store/syncMiddleware';
import { CardRow } from './CardRow';
import { LateShooterForm, UnassignedResults } from './BoardSections';
import type { FormEvent } from 'react';
import type { DragEndEvent } from '@dnd-kit/core';
import type { ShooterOption } from './BoardSections';

const syncTransport: SyncTransport = { saveMatchState, appendAudit };

const SYNC_LABELS: Record<SyncStatus, string> = {
  synced: 'Saved',
  dirty: 'Saving…',
  error: 'Not saved to server — retrying'
};

const SQUAD_STATUSES: SquadStatus[] = ['scheduled', 'active', 'completed'];

interface TimekeeperBoardProps {
  matchId: string;
  freshState: MatchState;
  serverState: MatchState | null;
  shooters: ShooterOption[];
  mqttConfig: MqttServerConfig;
}

export const TimekeeperBoard = ({ mqttConfig, ...props }: TimekeeperBoardProps) => (
  <ReduxProvider sync={syncTransport} mqttConfig={mqttConfig}>
    <BoardContent {...props} />
  </ReduxProvider>
);

type BoardContentProps = Omit<TimekeeperBoardProps, 'mqttConfig'>;

const BoardContent = ({ matchId, freshState, serverState, shooters }: BoardContentProps) => {
  const dispatch = useAppDispatch();
  const match = useAppSelector(state => state.match.current);
  const syncStatus = useAppSelector(state => state.match.syncStatus);
  const shots = useAppSelector(selectShots);
  const displayState = useAppSelector(selectDisplayState);
  const isBrokerConnected = useAppSelector(selectIsConnected);
  const knownDevices = useAppSelector(selectKnownDevices);

  const [selectedSquadId, setSelectedSquadId] = useState<string | null>(null);
  const [code, setCode] = useState('');
  const [message, setMessage] = useState<string | null>(null);

  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 5 } }),
    useSensor(TouchSensor, { activationConstraint: { delay: 150, tolerance: 8 } })
  );

  const deviceId = match?.deviceId ?? null;

  useEffect(() => {
    dispatch(hydrate(resolveInitialMatch(matchId, getLocalMatchState(), serverState, () => freshState)));
  }, [dispatch, matchId, serverState, freshState]);

  useEffect(() => {
    dispatch(startConnecting());

    return () => disconnectMqttClient();
  }, [dispatch]);

  useEffect(() => {
    if (deviceId) {
      dispatch(selectDevice(deviceId));
    }
  }, [deviceId, dispatch]);

  if (!match) {
    return <div className="tk-layout"><p className="tk-muted">Loading match…</p></div>;
  }

  const squadId = selectedSquadId ?? findCurrentSquadId(match) ?? match.squads[0]?.id ?? null;
  const squad = match.squads.find(candidate => candidate.id === squadId) ?? null;
  const squadCards = squadId ? getSquadCards(match, squadId) : [];
  const presentCards = squadCards.filter(card => card.presence === 'present');
  const absentCards = squadCards.filter(card => card.presence === 'absent');
  const activeTurn = match.activeTurn;
  const activeCard = match.cards.find(card => card.id === activeTurn?.cardId) ?? null;
  const currentRound = deriveCurrentRound(squadCards);
  const { next, onDeck } = deriveUpcomingShooters(squadCards, currentRound, activeTurn?.cardId ?? null);
  const outstanding = currentRound === null ? deriveOutstanding(squadCards) : [];
  const timer = knownDevices.find(device => device.deviceId === deviceId);
  const isTimerOnline = timer?.presence === 'online';
  const hasLiveShots = activeTurn?.phase === 'running' && displayState !== DisplayState.SESSION_ENDED && shots.length > 0;
  const liveTimeMs = hasLiveShots ? shots[shots.length - 1].absoluteTimeMs : null;

  function handleScanSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    const submittedCode = code.trim();
    setCode('');

    if (!match || !submittedCode) {
      return;
    }

    const card = findCardByKnsa(match, submittedCode, squadId);
    if (!card) {
      setMessage(`No shooter with KNSA number ${submittedCode} in this match.`);

      return;
    }

    const round = findNextRoundToShoot(card);
    if (round === null) {
      setMessage(`${card.shooterName} has nothing left to shoot.`);

      return;
    }

    setMessage(null);
    setSelectedSquadId(card.squadId);
    dispatch(armTurn({ cardId: card.id, round }));
  }

  function handleDragEnd(event: DragEndEvent) {
    const { active, over } = event;
    if (!over || active.id === over.id) {
      return;
    }

    const orderedIds = presentCards.map(card => card.id);
    const oldIndex = orderedIds.indexOf(String(active.id));
    const newIndex = orderedIds.indexOf(String(over.id));
    if (oldIndex === -1 || newIndex === -1) {
      return;
    }

    const absentIds = absentCards.map(card => card.id);
    dispatch(reorderQueue({ cardIds: [...arrayMove(orderedIds, oldIndex, newIndex), ...absentIds] }));
  }

  return (
    <div className="tk-layout">
      <div className="tk-main">
        <div className="tk-status-line">
          <span className={`tk-pill tk-pill--${syncStatus}`}>{SYNC_LABELS[syncStatus]}</span>
          <span className={`tk-pill${isTimerOnline ? ' tk-pill--synced' : ' tk-pill--error'}`}>
            Timer
            {' '}
            {deviceId ?? 'none'}
            :
            {' '}
            {isTimerOnline ? 'online' : 'offline'}
          </span>
          <span className={`tk-pill${isBrokerConnected ? ' tk-pill--synced' : ' tk-pill--error'}`}>
            MQTT
            {' '}
            {isBrokerConnected ? 'connected' : 'disconnected'}
          </span>
        </div>

        <nav className="tk-tabs" aria-label="Squads">
          {match.squads.map(candidate => (
            <button
              key={candidate.id}
              type="button"
              className={`tk-tab${candidate.id === squadId ? ' tk-tab--selected' : ''}`}
              onClick={() => setSelectedSquadId(candidate.id)}
            >
              {candidate.label}
              <span className="tk-tab__meta">
                {candidate.discipline}
                {' '}
                ·
                {' '}
                {candidate.status}
              </span>
            </button>
          ))}
        </nav>

        {squad && (
          <section className="tk-card tk-status-card">
            <div>
              <div className="tk-status-card__round">
                {currentRound !== null ? `Round ${currentRound} of 5` : 'Reshoot / catch-up phase'}
              </div>
              <span className={`tk-status-badge${activeTurn ? ' tk-status-badge--active' : ''}`}>
                <span className="tk-status-badge__dot" />
                {activeTurn && activeCard
                  ? (
                      <span>
                        {activeTurn.phase === 'running' ? 'Shooting: ' : 'Armed: '}
                        <strong>{activeCard.shooterName}</strong>
                        {' '}
                        round
                        {' '}
                        {activeTurn.round}
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
                value={squad.status}
                onChange={event => dispatch(setSquadStatus({ squadId: squad.id, status: event.target.value as SquadStatus }))}
              >
                {SQUAD_STATUSES.map(status => <option key={status} value={status}>{status}</option>)}
              </select>
              {activeTurn && (
                <button type="button" className="tk-button tk-button--danger" onClick={() => dispatch(cancelTurn())}>
                  Cancel turn
                </button>
              )}
            </div>
          </section>
        )}

        <section className="tk-roster">
          <div className="tk-roster__item">
            <span className="tk-roster__label">Next</span>
            <span className="tk-roster__name">{next?.shooterName ?? '—'}</span>
          </div>
          <div className="tk-roster__item tk-roster__item--on-deck">
            <span className="tk-roster__label">On deck</span>
            <span className="tk-roster__name">{onDeck?.shooterName ?? '—'}</span>
          </div>
        </section>

        {message && <div className="tk-error">{message}</div>}

        <section className="tk-card">
          <div className="tk-section-title">Scan or enter KNSA number</div>
          <form onSubmit={handleScanSubmit} className="tk-scan-form">
            <input
              type="text"
              className="tk-scan-input"
              value={code}
              onChange={event => setCode(event.target.value)}
              placeholder="scan card or type KNSA number"
              autoFocus
            />
            <button type="submit" className="tk-button tk-button--primary">Arm</button>
          </form>
        </section>

        <UnassignedResults match={match} squadCards={presentCards} />

        <section>
          <div className="tk-section-title">Queue</div>
          <DndContext sensors={sensors} collisionDetection={closestCenter} onDragEnd={handleDragEnd}>
            <SortableContext items={presentCards.map(card => card.id)} strategy={verticalListSortingStrategy}>
              <div className="tk-queue">
                {presentCards.map((card, index) => (
                  <CardRow key={card.id} match={match} card={card} position={index + 1} liveTimeMs={liveTimeMs} />
                ))}
              </div>
            </SortableContext>
          </DndContext>
        </section>

        {outstanding.length > 0 && (
          <section>
            <div className="tk-section-title">Outstanding (reshoots &amp; catch-up rounds)</div>
            <div className="tk-list">
              {outstanding.map(item => (
                <div className="tk-list-row" key={`${item.card.id}-${item.round}`}>
                  <span>
                    {item.card.shooterName}
                    {' '}
                    <span className="tk-list-row__meta">
                      round
                      {' '}
                      {item.round}
                      {' '}
                      ·
                      {' '}
                      {item.kind === 'rs' ? 'reshoot' : 'catch-up'}
                    </span>
                  </span>
                  <button
                    type="button"
                    className="tk-button tk-button--small"
                    onClick={() => dispatch(armTurn({ cardId: item.card.id, round: item.round }))}
                  >
                    Arm
                  </button>
                </div>
              ))}
            </div>
          </section>
        )}

        {absentCards.length > 0 && (
          <section>
            <div className="tk-section-title">Absent</div>
            <div className="tk-list">
              {absentCards.map(card => (
                <div className="tk-list-row" key={card.id}>
                  <span>{card.shooterName}</span>
                  <button type="button" className="tk-button tk-button--small" onClick={() => dispatch(markPresent({ cardId: card.id }))}>
                    Mark present / rejoin
                  </button>
                </div>
              ))}
            </div>
          </section>
        )}

        {squad && <LateShooterForm squadId={squad.id} squadCards={squadCards} shooters={shooters} />}
      </div>

      <aside className="tk-splits-pane">
        <SplitList shots={shots} highlightExtremes={displayState === DisplayState.SESSION_ENDED} />
      </aside>
    </div>
  );
};
