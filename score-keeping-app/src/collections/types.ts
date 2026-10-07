export interface SquadTimeFields {
  startTime?: string;
  endTime?: string;
}

export interface SquadTitleFields extends SquadTimeFields {
  label?: string;
}

export interface ShooterNameFields {
  firstName?: string;
  lastName?: string;
  knsaNumber?: string;
}
