'use client';

import { useState, type KeyboardEvent } from 'react';

// Temporary hardware verification harness (Phase 1).
// Not part of the planned route structure — safe to delete once the
// timekeeper screen's own scan handling is built and tested in Phase 2.
export default function ScannerTestPage() {
  const [scans, setScans] = useState<{ code: string; at: string }[]>([]);
  const [value, setValue] = useState('');
  const [keyLog, setKeyLog] = useState<string[]>([]);

  // KNOWN BUG (unresolved, deferred): the scanner's terminating Enter
  // reliably fires as a real keydown with key="Enter" (confirmed against a
  // plain vanilla <textarea> via MDN's own KeyboardEvent.key demo — the
  // hardware and browser side are both fine) but this handler still doesn't
  // reach the Enter branch reliably when the input is fed by the physical
  // scanner. Not yet root-caused. Revisit before wiring scanning into the
  // real timekeeper screen — src/lib/scanner/scanCapture.ts has the same
  // open issue. Manual shooter selection is unaffected and stays the
  // primary supported path in the meantime.
  function handleKeyDown(event: KeyboardEvent<HTMLInputElement>) {
    setKeyLog(prev =>
      [`key=${JSON.stringify(event.key)} code=${event.code} keyCode=${event.keyCode}`, ...prev].slice(0, 40)
    );

    if (event.key !== 'Enter') {
      return;
    }
    event.preventDefault();
    const code = event.currentTarget.value;
    setValue('');
    if (code.length === 0) {
      return;
    }
    setScans(prev => [{ code, at: new Date().toLocaleTimeString() }, ...prev]);
  }

  return (
    <div style={{ padding: '2rem', fontFamily: 'monospace' }}>
      <h1>Scanner capture test (listener on the input)</h1>
      <p>Click into the field below, then scan.</p>
      <input
        type="text"
        value={value}
        onChange={event => setValue(event.target.value)}
        onKeyDown={handleKeyDown}
        placeholder="click here, then scan"
        style={{ fontFamily: 'monospace', fontSize: '1rem', padding: '0.4rem', width: '20rem' }}
        autoFocus
      />
      {scans.length === 0 && <p>No scans yet.</p>}
      <ul>
        {scans.map((scan, index) => (
          <li key={`${scan.at}-${index}`}>
            code=
            <strong>{scan.code}</strong>
            {' '}
            at
            {' '}
            {scan.at}
          </li>
        ))}
      </ul>
      <h2>Raw key log (newest first)</h2>
      <pre style={{ background: '#f0f0f0', padding: '1rem', maxHeight: '20rem', overflow: 'auto' }}>
        {keyLog.length === 0 ? '(nothing yet)' : keyLog.join('\n')}
      </pre>
    </div>
  );
}
