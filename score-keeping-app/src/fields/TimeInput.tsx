'use client';

import { useField, FieldLabel, FieldError } from '@payloadcms/ui';
import type { TextFieldClientComponent } from 'payload';

// A native <input type="time"> instead of Payload's default date-picker
// widget — squads.startTime/endTime store a plain "HH:MM" string (see
// Squads.ts), so there's no calendar date to combine with it, and no
// custom JS picker component beyond this thin wrapper Payload's admin
// architecture requires to swap in a native element.
export const TimeInput: TextFieldClientComponent = ({ field, path: pathFromProps }) => {
  const {
    value,
    setValue,
    path,
    showError,
    errorMessage
  } = useField<string>({ potentiallyStalePath: pathFromProps });

  return (
    <div className="field-type text">
      <FieldLabel htmlFor={path} label={field.label} required={field.required} />
      <input
        type="time"
        id={path}
        name={path}
        value={value ?? ''}
        onChange={event => setValue(event.target.value)}
        // en-GB renders the native time control in 24-hour format —
        // there's no dedicated HTML attribute for this, browsers derive
        // it from locale.
        lang="en-GB"
        style={{ width: 'auto' }}
      />
      {showError && <FieldError message={errorMessage} path={path} showError={showError} />}
    </div>
  );
};
