'use client';

import { useEffect, useState, useTransition, type FormEvent, type ReactNode } from 'react';
import { useRouter } from 'next/navigation';
import { useDispatch, useSelector } from 'react-redux';
import {
  DndContext,
  closestCenter,
  PointerSensor,
  TouchSensor,
  useSensor,
  useSensors,
  type DragEndEvent
} from '@dnd-kit/core';
import { SortableContext, verticalListSortingStrategy, useSortable, arrayMove } from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';
import type { SquadView } from '@/lib/match/loadSquadView';
import {
  deriveUpcomingShooters,
  deriveOutstanding,
  isReadyForSignOff,
  formatRoundTimeMs,
  resolveSquadFirmwareDeviceId,
  type MembershipView
} from '@/lib/match/matchState';
import {
  activateMembershipAction,
  activateByKnsaAction,
  cancelActivationAction,
  reorderQueueAction,
  markAbsentAction,
  markPresentAction,
  offerCatchUpAction,
  markSignedOffAction,
  type ActionResult
} from '@/app/timekeeper/(protected)/actions';
import { ReduxProvider } from '@/components/display/ReduxProvider';
import SplitList from '@/components/display/SplitList';
import { startConnecting, selectDevice, selectShots, selectDisplayState } from '@/store/mqttSlice';
import { disconnectMqttClient } from '@/store/mqttMiddleware';
import { DisplayState } from '@/lib/mqtt/types';

const ROUND_NUMBERS = [1, 2, 3, 4, 5] as const;
const LIVE_SHOT_STATES: DisplayState[] = [DisplayState.WAITING_FOR_SHOTS, DisplayState.SHOWING_SHOT];

function shooterName(view: MembershipView): string {
  const shooter = view.membership.shooter;

  return typeof shooter === 'object' ? `${shooter.firstName} ${shooter.lastName}` : `Shooter #${shooter}`;
}

function roundCell(view: MembershipView, round: number, liveTimeMs: number | null): ReactNode {
  const result = view.roundResults.find(r => r.roundNumber === round);

  if (liveTimeMs != null && result?.status === 'pending') {
    return (
      <span className="tk-round-cell tk-round-cell--live">
        R
        {round}
        {' '}
        {formatRoundTimeMs(liveTimeMs)}
      </span>
    );
  }
  if (!result || result.status === 'pending') {
    return (
      <span className="tk-round-cell">
        R
        {round}
        {' '}
        —
      </span>
    );
  }
  if (result.status === 'rs') {
    return (
      <span className="tk-round-cell tk-round-cell--rs">
        R
        {round}
        {' '}
        RS
      </span>
    );
  }
  if (result.status === 'skipped') {
    return (
      <span className="tk-round-cell tk-round-cell--skipped">
        R
        {round}
        {' '}
        —
      </span>
    );
  }

  return (
    <span className="tk-round-cell tk-round-cell--timed">
      R
      {round}
      {' '}
      {result.timeMs != null ? formatRoundTimeMs(result.timeMs) : '—'}
    </span>
  );
}

function SortableQueueRow({
  id,
  disabled,
  active,
  children
}: {
  id: number;
  disabled: boolean;
  active: boolean;
  children: ReactNode;
}) {
  const { attributes, listeners, setNodeRef, transform, transition, isDragging } = useSortable({ id, disabled });
  const style = { transform: CSS.Transform.toString(transform), transition };

  return (
    <div
      ref={setNodeRef}
      style={style}
      className={`tk-queue-row${active ? ' tk-queue-row--active' : ''}${isDragging ? ' tk-queue-row--dragging' : ''}`}
    >
      <span className="tk-queue-row__handle" aria-label="Drag to reorder" {...(disabled ? {} : { ...attributes, ...listeners })}>
        ⠿
      </span>
      {children}
    </div>
  );
}

export function TimekeeperBoard({ view }: { view: SquadView }) {
  return (
    <ReduxProvider>
      <TimekeeperBoardInner view={view} />
    </ReduxProvider>
  );
}

function TimekeeperBoardInner({ view }: { view: SquadView }) {
  const router = useRouter();
  const dispatch = useDispatch();
  const shots = useSelector(selectShots);
  const displayState = useSelector(selectDisplayState);

  const [isPending, startTransition] = useTransition();
  const [error, setError] = useState<string | null>(null);
  const [code, setCode] = useState('');
  const [localOrder, setLocalOrder] = useState<number[] | null>(null);

  const deviceId = resolveSquadFirmwareDeviceId(view.squad);

  // A live MQTT connection, pinned to this squad's device — reused from the
  // /display route's store so shot data streams in during a turn without
  // waiting for the server-side subscriber to write it to Payload.
  useEffect(() => {
    dispatch(startConnecting());

    return () => disconnectMqttClient();
  }, []);

  useEffect(() => {
    if (deviceId) {
      dispatch(selectDevice(deviceId));
    }
  }, [deviceId, dispatch]);

  // The server-provided view always wins once it arrives (after a refresh) —
  // drop the optimistic drag order so future divergences reflect fresh data.
  useEffect(() => {
    setLocalOrder(null);
  }, [view]);

  // Auto-advance once a turn ends, without waiting for the timekeeper to
  // click End Turn / Refresh. The browser's own MQTT feed sees
  // session/stopped the moment it happens, but the round-result write is
  // done by a separate server-side subscriber on its own subscription to
  // the same broker — there's no ordering guarantee between the two, so
  // poll briefly rather than assuming one refresh lands after the DB write.
  // Stops itself the instant the server confirms the turn is over.
  useEffect(() => {
    if (displayState !== DisplayState.SESSION_ENDED || !view.isSessionActive) {
      return;
    }

    const interval = setInterval(() => router.refresh(), 700);

    return () => clearInterval(interval);
  }, [displayState, view.isSessionActive, router]);

  const serverPresent = view.memberships
    .filter(m => m.membership.status === 'present')
    .sort((a, b) => a.membership.queuePosition - b.membership.queuePosition);
  const serverIds = serverPresent.map(m => m.membership.id);
  const orderedIds = localOrder && localOrder.length === serverIds.length ? localOrder : serverIds;
  const present = orderedIds
    .map(id => serverPresent.find(m => m.membership.id === id))
    .filter((m): m is MembershipView => m != null);

  const absent = view.memberships.filter(m => m.membership.status === 'absent');
  const activeMembership = view.memberships.find(m => m.membership.id === view.activeMembershipId) ?? null;

  const { next, onDeck } = deriveUpcomingShooters(view.memberships, view.currentRound, view.activeMembershipId);
  const outstanding = view.currentRound === null ? deriveOutstanding(view.memberships) : [];

  // The round currently being shot, live, straight off the MQTT feed — not
  // yet written to Payload (that only happens once session/stopped arrives
  // at the server). Only trusted once shots are actually streaming in for
  // this activation, so a stale previous session's shots can't leak in
  // during the gap between the timekeeper's click and the range officer's
  // physical Start press.
  const liveRoundNumber
    = view.isSessionActive && activeMembership
      ? (activeMembership.roundResults.find(r => r.status === 'pending')?.roundNumber ?? null)
      : null;
  const hasLiveShots = view.isSessionActive && LIVE_SHOT_STATES.includes(displayState) && shots.length > 0;
  const liveTimeMs = hasLiveShots ? shots[shots.length - 1].absoluteTimeMs : null;

  const controlsDisabled = view.isSessionActive || isPending;

  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 5 } }),
    useSensor(TouchSensor, { activationConstraint: { delay: 150, tolerance: 8 } })
  );

  function runAction(action: () => Promise<ActionResult>) {
    setError(null);
    startTransition(async () => {
      const result = await action();
      if (!result.ok) {
        setError(result.error ?? 'Something went wrong.');
      }
      router.refresh();
    });
  }

  function handleCancelActivation() {
    // A pending activation (before the range officer has pressed Start) is
    // always safe to cancel outright — nothing has happened yet. Once shots
    // may already be in flight (status active), confirm first.
    const liveStatus = view.liveSessions[0]?.status;
    if (liveStatus === 'active' && !window.confirm('A turn may already be in progress on the timer. Cancel this activation anyway?')) {
      return;
    }
    runAction(() => cancelActivationAction(view.squad.id));
  }

  function handleDragEnd(event: DragEndEvent) {
    const { active, over } = event;
    if (!over || active.id === over.id) {
      return;
    }

    const oldIndex = orderedIds.indexOf(Number(active.id));
    const newIndex = orderedIds.indexOf(Number(over.id));
    if (oldIndex === -1 || newIndex === -1) {
      return;
    }

    const newOrder = arrayMove(orderedIds, oldIndex, newIndex);
    setLocalOrder(newOrder);
    runAction(() => reorderQueueAction(view.squad.id, newOrder));
  }

  function handleScanSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    const submittedCode = code;
    setCode('');
    runAction(() => activateByKnsaAction(view.squad.id, submittedCode));
  }

  return (
    <div className="tk-layout">
      <div className="tk-main">
        <h1 className="tk-squad-title">{view.squad.label || `Squad #${view.squad.id}`}</h1>

        <section className="tk-card tk-status-card">
          <div>
            <div className="tk-status-card__round">
              {view.currentRound !== null ? `Round ${view.currentRound} of 5` : 'Reshoot / catch-up phase'}
            </div>
            <span className={`tk-status-badge${view.isSessionActive ? ' tk-status-badge--active' : ''}`}>
              <span className="tk-status-badge__dot" />
              {view.isSessionActive
                ? (
                    <span>
                      Turn in progress:
                      {' '}
                      <strong>{activeMembership ? shooterName(activeMembership) : 'unknown shooter'}</strong>
                    </span>
                  )
                : (
                    <span>Scanner and manual selection armed</span>
                  )}
            </span>
          </div>
          <div style={{ display: 'flex', gap: '0.6rem' }}>
            {view.isSessionActive && (
              <button
                type="button"
                className="tk-button tk-button--danger"
                disabled={isPending}
                onClick={handleCancelActivation}
              >
                Cancel activation
              </button>
            )}
            <button type="button" className="tk-button" onClick={() => router.refresh()} disabled={isPending}>
              End Turn / Refresh
            </button>
          </div>
        </section>

        <section className="tk-roster">
          <div className="tk-roster__item">
            <span className="tk-roster__label">Next</span>
            <span className="tk-roster__name">{next ? shooterName(next) : '—'}</span>
          </div>
          <div className="tk-roster__item tk-roster__item--on-deck">
            <span className="tk-roster__label">On deck</span>
            <span className="tk-roster__name">{onDeck ? shooterName(onDeck) : '—'}</span>
          </div>
        </section>

        {error && <div className="tk-error">{error}</div>}

        <section className="tk-card">
          <div className="tk-section-title">Scan or enter KNSA number</div>
          <form onSubmit={handleScanSubmit} className="tk-scan-form">
            <input
              type="text"
              className="tk-scan-input"
              value={code}
              onChange={event => setCode(event.target.value)}
              placeholder="scan card or type KNSA number"
              disabled={controlsDisabled}
              autoFocus
            />
            <button type="submit" className="tk-button tk-button--primary" disabled={controlsDisabled}>
              Activate
            </button>
          </form>
        </section>

        <section>
          <div className="tk-section-title">Queue</div>
          <DndContext sensors={sensors} collisionDetection={closestCenter} onDragEnd={handleDragEnd}>
            <SortableContext items={orderedIds} strategy={verticalListSortingStrategy}>
              <div className="tk-queue">
                {present.map((m, index) => {
                  const isActive = m.membership.id === view.activeMembershipId;

                  return (
                    <SortableQueueRow key={m.membership.id} id={m.membership.id} disabled={controlsDisabled} active={isActive}>
                      <span className="tk-queue-row__position">{index + 1}</span>
                      <button
                        type="button"
                        className="tk-queue-row__name"
                        disabled={controlsDisabled}
                        onClick={() => runAction(() => activateMembershipAction(view.squad.id, m.membership.id))}
                      >
                        {shooterName(m)}
                      </button>
                      <span className="tk-queue-row__discipline">{m.membership.discipline}</span>
                      <div className="tk-queue-row__rounds">
                        {ROUND_NUMBERS.map(round => (
                          <span key={round}>
                            {roundCell(m, round, isActive && round === liveRoundNumber ? liveTimeMs : null)}
                          </span>
                        ))}
                        {m.membership.reshootTimeMs != null && (
                          <span className="tk-round-cell tk-round-cell--reshoot">
                            RS→
                            {formatRoundTimeMs(m.membership.reshootTimeMs)}
                          </span>
                        )}
                      </div>
                      <div className="tk-queue-row__spacer" />
                      <div className="tk-queue-row__actions">
                        <button
                          type="button"
                          className="tk-button tk-button--small"
                          disabled={controlsDisabled}
                          onClick={() => runAction(() => markAbsentAction(view.squad.id, m.membership.id))}
                        >
                          Mark absent
                        </button>
                        {isReadyForSignOff(m) && !m.membership.signedOffAt && (
                          <button
                            type="button"
                            className="tk-button tk-button--small tk-button--primary"
                            disabled={isPending}
                            onClick={() => runAction(() => markSignedOffAction(m.membership.id))}
                          >
                            Mark signed
                          </button>
                        )}
                        {m.membership.signedOffAt && <span className="tk-signed-tag">signed</span>}
                      </div>
                    </SortableQueueRow>
                  );
                })}
              </div>
            </SortableContext>
          </DndContext>
        </section>

        {outstanding.length > 0 && (
          <section>
            <div className="tk-section-title">Outstanding (reshoots &amp; catch-up rounds)</div>
            <div className="tk-list">
              {outstanding.map(item => (
                <div className="tk-list-row" key={`${item.membership.membership.id}-${item.roundNumber}`}>
                  <span>
                    {shooterName(item.membership)}
                    {' '}
                    <span className="tk-list-row__meta">
                      round
                      {' '}
                      {item.roundNumber}
                      {' '}
                      ·
                      {' '}
                      {item.kind === 'rs' ? 'reshoot' : 'catch-up'}
                    </span>
                  </span>
                  <button
                    type="button"
                    className="tk-button tk-button--small"
                    disabled={controlsDisabled}
                    onClick={() =>
                      runAction(() =>
                        item.kind === 'rs'
                          ? activateMembershipAction(view.squad.id, item.membership.membership.id)
                          : offerCatchUpAction(view.squad.id, item.roundResultId)
                      )}
                  >
                    {item.kind === 'rs' ? 'Shoot reshoot' : 'Shoot catch-up round'}
                  </button>
                </div>
              ))}
            </div>
          </section>
        )}

        {absent.length > 0 && (
          <section>
            <div className="tk-section-title">Absent</div>
            <div className="tk-list">
              {absent.map(m => (
                <div className="tk-list-row" key={m.membership.id}>
                  <span>{shooterName(m)}</span>
                  <button
                    type="button"
                    className="tk-button tk-button--small"
                    disabled={controlsDisabled}
                    onClick={() => runAction(() => markPresentAction(view.squad.id, m.membership.id))}
                  >
                    Mark present / rejoin
                  </button>
                </div>
              ))}
            </div>
          </section>
        )}
      </div>

      <aside className="tk-splits-pane">
        <SplitList shots={shots} highlightExtremes={displayState === DisplayState.SESSION_ENDED} />
      </aside>
    </div>
  );
}
