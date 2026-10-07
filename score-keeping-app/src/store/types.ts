import type { Discipline } from '@/lib/domain/disciplines';
import type { MatchState, RoundStatus, ShooterOption, SquadStatus } from '@/lib/match/types';
import type { DeviceInfoMessage, DevicePresence, DisplayState, KnownDevice, MqttServerConfig, SessionData, ShotData } from '@/lib/mqtt/types';
import type { Action, PayloadAction } from '@reduxjs/toolkit';

export interface StoreOptions {
  /** Only the timekeeper persists match changes; /display never writes. */
  sync?: SyncTransport;
  mqttConfig?: MqttServerConfig;
}

export interface MatchActionMeta {
  id: string;
  at: string;
}

export type MetaAction<P> = PayloadAction<P, string, MatchActionMeta>;

export interface MaybeMetaAction extends Action {
  meta?: MatchActionMeta;
}

export type SyncStatus = 'synced' | 'dirty' | 'error';

export interface MatchSliceState {
  current: MatchState | null;
  syncStatus: SyncStatus;
}

export interface MatchRootState {
  match: MatchSliceState;
}

export interface CardRef {
  cardId: string;
}

export interface RoundRef extends CardRef {
  round: number;
}

export interface ResultRef {
  resultId: string;
}

export interface ShooterRef {
  shooterId: string;
}

export interface NewCard {
  squadId: string;
  shooterId: string;
  shooterName: string;
  knsaNumber: string | null;
  discipline: Discipline;
}

export interface AddCardPayload extends NewCard {
  cardId: string;
}

export interface TurnStoppedPayload {
  lastShotTimeMs: number | null;
}

export interface RoundTimePayload extends RoundRef {
  timeMs: number | null;
}

export interface RoundStatusPayload extends RoundRef {
  status: RoundStatus;
}

export interface ReshootTimePayload extends RoundRef {
  reshootTimeMs: number | null;
}

export interface DisqualifyPayload extends CardRef {
  reason: string;
}

export interface ReorderQueuePayload {
  cardIds: string[];
}

export interface SquadStatusPayload {
  squadId: string;
  status: SquadStatus;
}

export interface UnassignedResultPayload {
  timeMs: number;
}

export interface AssignResultPayload extends RoundRef {
  resultId: string;
}

export interface AuditEntry {
  actionId: string;
  matchId: string;
  at: string;
  type: string;
  payload: Record<string, unknown> | null;
}

/** The server side of the sync. Both calls must be idempotent: a retry after a lost response resends the same data. */
export interface SyncTransport {
  saveMatchState: (state: MatchState) => Promise<void>;
  appendAudit: (entries: AuditEntry[]) => Promise<void>;
}

export interface SyncStorage {
  get: <T>(key: string, defaultValue: T) => T;
  set: <T>(key: string, value: T) => boolean;
}

export interface SyncOptions {
  transport: SyncTransport;
  localStore?: SyncStorage;
  debounceMs?: number;
  maxRetryDelayMs?: number;
}

export interface MqttState {
  isConnecting: boolean;
  isConnected: boolean;
  connectionError: string | null;
  displayState: DisplayState;
  shotData: ShotData | null;
  sessionData: SessionData | null;
  shots: ShotData[];
  deviceName: string | null;
  knownDevices: KnownDevice[];
  selectedDeviceId: string | null;
  countdownRemainingMs: number;
}

export interface DevicePresenceUpdate {
  deviceId: string;
  presence: DevicePresence;
}

export interface DeviceInfoUpdate {
  deviceId: string;
  info: DeviceInfoMessage;
}

export interface BrokerDefaults {
  broker: string;
  username: string;
  password: string;
}

export interface SettingsState {
  broker: string;
  username: string;
  password: string;
  clientId: string;
  brightness: number;
  defaults: BrokerDefaults;
}

export type TimekeeperLoadStatus = 'loading' | 'ready' | 'no-match' | 'error';

/** Text typed into the round editor; `null` means untouched, so the input shows the stored round value. */
export interface RoundEditorState {
  cardId: string;
  round: number;
  timeText: string | null;
  reshootText: string | null;
  error: string | null;
}

export interface DqDialogState {
  cardId: string;
  reason: string;
}

/** Absent fields fall back to defaults in the selectors. */
export interface UnassignedDraft {
  cardId?: string;
  round?: number;
}

export interface UnassignedDraftChange extends UnassignedDraft {
  resultId: string;
}

export interface LateShooterDraft {
  shooterId: string;
  discipline: Discipline;
}

/** `requestNumber` grows with every request, so reprinting the same card is a new request too. */
export interface PrintRequest {
  cardId: string;
  requestNumber: number;
}

export interface TimekeeperState {
  loadStatus: TimekeeperLoadStatus;
  loadError: string | null;
  userEmail: string | null;
  matchLabel: string | null;
  shooters: ShooterOption[];
  /** The tab the timekeeper picked; null follows the squad in play. */
  selectedSquadId: string | null;
  message: string | null;
  editor: RoundEditorState | null;
  dqDialog: DqDialogState | null;
  lateShooter: LateShooterDraft;
  unassignedDrafts: Record<string, UnassignedDraft>;
  printRequest: PrintRequest | null;
  printRequestCount: number;
}

export interface TimekeeperLoadedPayload {
  userEmail: string;
  matchLabel: string | null;
  shooters: ShooterOption[];
}

export interface SquadTurnView {
  verb: string;
  shooterName: string;
  round: number;
}

export interface CardShooterView {
  cardId: string;
  shooterName: string;
}

/** R1-R5, then one "RS" cell per reshoot; the round the reshoot replaces shows only "RS". */
export interface RoundCellView {
  key: string;
  heading: string;
  /** A reshoot cell edits the round it was requested in. */
  n: number;
  label: string;
  modifier: RoundStatus | 'live';
  isArmed: boolean;
  isEditing: boolean;
}

export interface CellState {
  armedRound: number | null;
  liveRound: number | null;
  liveTimeMs: number | null;
  editedRound: number | null;
}

export interface CardRowView {
  cardId: string;
  shooterId: string;
  shooterName: string;
  discipline: string | null;
  nextRound: number | null;
  isActive: boolean;
  isDisqualified: boolean;
  isSignedOff: boolean;
  isReadyForSignOff: boolean;
  scoreText: string;
  warnings: string[];
  rounds: RoundCellView[];
}

export interface RoundEditorView {
  cardId: string;
  round: number;
  shooterName: string;
  status: RoundStatus;
  timeText: string;
  reshootText: string;
  error: string | null;
}

export interface CountableRound {
  n: number;
  timeMs: number;
}

/** A sheet row: Round 1-5, then one "RS" row per reshoot (as in the card row). */
export interface ScoreSheetRound {
  key: string;
  heading: string;
  label: string;
  /** One of the three fastest rounds that make up the score. */
  isCounted: boolean;
}

export interface ScoreSheetView {
  matchLabel: string;
  squadLabel: string;
  squadTimes: string;
  shooterName: string;
  knsaNumber: string | null;
  discipline: string | null;
  rounds: ScoreSheetRound[];
  scoreText: string;
  dqReason: string | null;
  warnings: string[];
  signedOffText: string | null;
}
