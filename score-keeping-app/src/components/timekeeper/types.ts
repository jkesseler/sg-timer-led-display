import type { Discipline } from '@/lib/domain/disciplines';
import type { MatchSquad, RoundStatus, ShooterOption, SquadStatus } from '@/lib/match/types';
import type {
  CardRowView as CardRowData,
  CardShooterView,
  RoundEditorView as RoundEditorData,
  SquadTurnView,
  SyncStatus,
  TimekeeperLoadStatus
} from '@/store/types';
import type { DraggableAttributes, DraggableSyntheticListeners } from '@dnd-kit/core';
import type { CSSProperties, ReactNode } from 'react';

export interface SortableProps {
  setNodeRef: (element: HTMLElement | null) => void;
  style: CSSProperties;
  attributes: DraggableAttributes;
  listeners: DraggableSyntheticListeners;
  isDragging: boolean;
}

export interface CardRowViewProps {
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

export interface CardRowProps {
  cardId: string;
  position: number;
}

export interface DqDialogViewProps {
  shooterName: string;
  reason: string;
  onReasonChange: (reason: string) => void;
  onConfirm: () => void;
  onCancel: () => void;
}

export interface DqDialogProps {
  cardId: string;
}

export interface StatusButton {
  status: RoundStatus;
  label: string;
}

export interface RoundEditorViewProps extends RoundEditorData {
  onStatusChange: (status: RoundStatus) => void;
  onTimeTextChange: (text: string) => void;
  onSaveTime: () => void;
  onClearTime: () => void;
  onReshootTextChange: (text: string) => void;
  onSaveReshoot: () => void;
  onClearReshoot: () => void;
  onArm: () => void;
  onClose: () => void;
}

export interface RoundEditorProps {
  cardId: string;
}

export interface UnassignedResultRow {
  resultId: string;
  timeText: string;
  at: string;
  cardId: string;
  round: number;
}

export interface UnassignedResultsViewProps {
  results: UnassignedResultRow[];
  shooterOptions: CardShooterView[];
  onCardChange: (resultId: string, cardId: string) => void;
  onRoundChange: (resultId: string, round: number) => void;
  onAssign: (resultId: string) => void;
  onDiscard: (resultId: string) => void;
}

export interface OutstandingRow {
  cardId: string;
  shooterName: string;
  round: number;
  kindLabel: string;
}

export interface OutstandingListViewProps {
  items: OutstandingRow[];
  onArm: (cardId: string, round: number) => void;
}

export interface AbsentListViewProps {
  rows: CardShooterView[];
  onMarkPresent: (cardId: string) => void;
}

export interface LateShooterFormViewProps {
  shooters: ShooterOption[];
  shooterId: string;
  discipline: Discipline;
  onShooterChange: (shooterId: string) => void;
  onDisciplineChange: (discipline: Discipline) => void;
  onAdd: () => void;
}

export interface StatusLineViewProps {
  syncStatus: SyncStatus;
  deviceId: string | null;
  isTimerOnline: boolean;
  isBrokerConnected: boolean;
  /** The barcode scanner arms shooters only while nobody is armed. */
  isScannerListening: boolean;
}

export interface SquadTabsViewProps {
  squads: MatchSquad[];
  selectedSquadId: string | null;
  onSelect: (squadId: string) => void;
}

export interface SquadStatusCardViewProps {
  roundLabel: string;
  squadStatus: SquadStatus;
  turn: SquadTurnView | null;
  onSquadStatusChange: (status: SquadStatus) => void;
  onCancelTurn: () => void;
}

export interface RosterViewProps {
  nextName: string | null;
  onDeckName: string | null;
}

export interface MessageViewProps {
  message: string | null;
}

export interface HeaderViewProps {
  userEmail: string | null;
}

export interface MainViewProps {
  loadStatus: TimekeeperLoadStatus;
  loadError: string | null;
  matchLabel: string | null;
}
