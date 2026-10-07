'use client';

import { useField, FieldLabel, FieldError } from '@payloadcms/ui';
import type { TextFieldClientComponent } from 'payload';

// Native time input: the field stores a plain "HH:MM" string, with no date to pick.
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
        style={{ width: 'auto' }}
      />
      {showError && <FieldError message={errorMessage} path={path} showError={showError} />}
    </div>
  );
};
