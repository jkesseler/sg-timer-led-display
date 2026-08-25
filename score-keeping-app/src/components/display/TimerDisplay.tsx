import { useSelector } from 'react-redux';
import { DisplayState } from '@/lib/mqtt/types';
import { STARTUP_TEXT } from '@/lib/display/constants';
import { formatCountdown, formatTimeValue } from '@/lib/display/utils';
import {
  selectDisplayState,
  selectShotData,
  selectSessionData,
  selectShots,
  selectDeviceName,
  selectCountdownRemainingMs,
} from '@/store/mqttSlice';
import type { ShotData, SessionData } from '@/lib/mqtt/types';
import type { RosterInfo } from '@/app/display/actions';
import SplitList from './SplitList';
import './TimerDisplay.css';

const SESSION_DISPLAY_STATES: DisplayState[] = [
  DisplayState.COUNTDOWN,
  DisplayState.WAITING_FOR_SHOTS,
  DisplayState.SHOWING_SHOT,
  DisplayState.SESSION_ENDED,
];

type BeaconTone = 'neutral' | 'searching' | 'ready';

interface IdleCardContent {
  tone: BeaconTone;
  eyebrow: string;
  title: string;
  detail?: string;
}

function resolveIdleContent(displayState: DisplayState, deviceName: string | null): IdleCardContent {
  switch (displayState) {
    case DisplayState.SCANNING:
      return { tone: 'searching', eyebrow: 'Bluetooth', title: 'Scanning for timer…', detail: 'Looking for a paired shot timer' };
    case DisplayState.CONNECTING:
      return { tone: 'searching', eyebrow: 'Bluetooth', title: 'Connecting…', detail: deviceName ?? undefined };
    case DisplayState.CONNECTED:
      return { tone: 'ready', eyebrow: 'Timer ready', title: deviceName ?? 'Connected', detail: 'Waiting for the next stage to start' };
    case DisplayState.DISCONNECTED:
    default:
      return { tone: 'neutral', eyebrow: 'No timer', title: 'No timer connected', detail: 'Waiting for a bridge device' };
  }
}

const Beacon = ({ tone }: { tone: BeaconTone }) => (
  <div className={`beacon beacon--${tone}`} aria-hidden="true">
    <span className="beacon__ring beacon__ring--outer" />
    <span className="beacon__ring beacon__ring--inner" />
    <span className="beacon__core" />
  </div>
);

const StartupScreen = () => (
  <div className="startup-screen">
    <span className="startup-screen__mark">{STARTUP_TEXT}</span>
    <span className="startup-screen__rule" aria-hidden="true" />
  </div>
);

const IdleCard = ({ tone, eyebrow, title, detail }: IdleCardContent) => (
  <div className="idle-card">
    <Beacon tone={tone} />
    <span className="idle-card__eyebrow">{eyebrow}</span>
    <span className="idle-card__title">{title}</span>
    {detail && <span className="idle-card__detail">{detail}</span>}
  </div>
);

const CountdownHero = ({ countdownRemainingMs }: { countdownRemainingMs: number }) => {
  const remainingSeconds = countdownRemainingMs / 1000;
  const tone = remainingSeconds > 3 ? 'ready' : remainingSeconds > 1 ? 'live' : 'stop';

  return (
    <div className="hero hero-countdown">
      <span className="hero__eyebrow">Stage starting</span>
      <span className={`hero-countdown__value hero-countdown__value--${tone}`}>
        {formatCountdown(remainingSeconds)}
      </span>
      <span className="hero__meta">Stand by for the buzzer</span>
    </div>
  );
};

const WaitingHero = ({ sessionData }: { sessionData: SessionData | null }) => (
  <div className="hero hero-waiting">
    <div className="listening-bars" aria-hidden="true">
      <span />
      <span />
      <span />
      <span />
      <span />
    </div>
    <span className="hero__eyebrow">Listening for shots</span>
    {sessionData && <span className="hero__meta">Session #{sessionData.sessionId}</span>}
  </div>
);

const ShotHero = ({ shotData }: { shotData: ShotData }) => {
  const isDraw = shotData.isFirstShot;

  return (
    <div className="hero hero-shot">
      <span className="hero__eyebrow">Shot {shotData.shotNumber}</span>
      <span className="hero-shot__value">{formatTimeValue(shotData.absoluteTimeMs)}</span>
      <span className="hero__meta">{isDraw ? 'Draw' : `Split ${formatTimeValue(shotData.splitTimeMs)}`}</span>
    </div>
  );
};

const ShooterHeader = ({ name }: { name: string }) => (
  <div className="shooter-header">
    <span className="shooter-header__label">Current shooter</span>
    <span className="shooter-header__name">{name}</span>
  </div>
);

const ShooterQueue = ({ next, onDeck }: { next: string; onDeck: string }) => (
  <div className="shooter-queue">
    <div className="shooter-queue__item">
      <span className="shooter-queue__label">Next</span>
      <span className="shooter-queue__name">{next}</span>
    </div>
    <div className="shooter-queue__item">
      <span className="shooter-queue__label">On deck</span>
      <span className="shooter-queue__name">{onDeck}</span>
    </div>
  </div>
);

const SessionEndedHero = ({
  shotData,
  sessionData,
  shots,
}: {
  shotData: ShotData | null;
  sessionData: SessionData | null;
  shots: ShotData[];
}) => {
  const finalTimeMs = shotData?.absoluteTimeMs ?? 0;
  const totalShots = sessionData?.totalShots || shotData?.shotNumber || shots.length;

  const timedSplits = shots.filter((shot) => !shot.isFirstShot).map((shot) => shot.splitTimeMs);
  const fastestMs = timedSplits.length ? Math.min(...timedSplits) : null;
  const slowestMs = timedSplits.length ? Math.max(...timedSplits) : null;
  const averageMs = timedSplits.length
    ? timedSplits.reduce((sum, split) => sum + split, 0) / timedSplits.length
    : null;

  return (
    <div className="hero hero-ended">
      <span className="hero__eyebrow hero__eyebrow--stop">Stage complete</span>
      <span className="hero-ended__value">{formatTimeValue(finalTimeMs)}</span>
      <span className="hero__meta">{totalShots === 1 ? '1 shot' : `${totalShots} shots`}</span>
      <div className="hero-ended__stats">
        <div className="stat">
          <span className="stat__label">Fastest</span>
          <span className="stat__value stat__value--ready">{fastestMs != null ? formatTimeValue(fastestMs) : '—'}</span>
        </div>
        <div className="stat">
          <span className="stat__label">Average</span>
          <span className="stat__value">{averageMs != null ? formatTimeValue(averageMs) : '—'}</span>
        </div>
        <div className="stat">
          <span className="stat__label">Slowest</span>
          <span className="stat__value stat__value--stop">{slowestMs != null ? formatTimeValue(slowestMs) : '—'}</span>
        </div>
      </div>
    </div>
  );
};

export const TimerDisplay = ({ roster }: { roster: RosterInfo | null }) => {
  const displayState = useSelector(selectDisplayState);
  const shotData = useSelector(selectShotData);
  const sessionData = useSelector(selectSessionData);
  const shots = useSelector(selectShots);
  const deviceName = useSelector(selectDeviceName);
  const countdownRemainingMs = useSelector(selectCountdownRemainingMs);

  if (displayState === DisplayState.STARTUP) {
    return <StartupScreen />;
  }

  if (!SESSION_DISPLAY_STATES.includes(displayState)) {
    return <IdleCard {...resolveIdleContent(displayState, deviceName)} />;
  }

  return (
    <div className="session-view">
      <div className="hero-pane">
        {roster?.current && <ShooterHeader name={roster.current} />}
        <div className="hero-pane__content">
          {displayState === DisplayState.COUNTDOWN && (
            <CountdownHero countdownRemainingMs={countdownRemainingMs} />
          )}
          {displayState === DisplayState.WAITING_FOR_SHOTS && <WaitingHero sessionData={sessionData} />}
          {displayState === DisplayState.SHOWING_SHOT && shotData && <ShotHero shotData={shotData} />}
          {displayState === DisplayState.SESSION_ENDED && (
            <SessionEndedHero shotData={shotData} sessionData={sessionData} shots={shots} />
          )}
        </div>
        {(roster?.next || roster?.onDeck) && (
          <ShooterQueue next={roster.next ?? '—'} onDeck={roster.onDeck ?? '—'} />
        )}
      </div>
      <div className="splits-pane">
        <SplitList shots={shots} highlightExtremes={displayState === DisplayState.SESSION_ENDED} />
      </div>
    </div>
  );
};

export default TimerDisplay;
