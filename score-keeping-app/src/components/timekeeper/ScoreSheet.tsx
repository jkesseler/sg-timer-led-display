'use client';

import { useEffect } from 'react';
import { createPortal } from 'react-dom';
import { useAppDispatch, useAppSelector } from '@/store/store';
import { selectPrintRequest, selectScoreSheet } from '@/store/timekeeperSelectors';
import { scoreSheetPrinted } from '@/store/timekeeperSlice';
import type { ScoreSheetView as ScoreSheetData } from '@/store/types';

/** One A6 score sheet; only visible when printing (see the print rules in timekeeper.css). */
export const ScoreSheetView = ({
  matchLabel,
  squadLabel,
  squadTimes,
  shooterName,
  knsaNumber,
  discipline,
  rounds,
  scoreText,
  dqReason,
  warnings,
  signedOffText
}: ScoreSheetData) => (
  <article className="tk-sheet">
    <header className="tk-sheet__header">
      <div className="tk-sheet__match">{matchLabel}</div>
      <div className="tk-sheet__squad">{[squadLabel, squadTimes].filter(Boolean).join(' · ')}</div>
    </header>

    <section className="tk-sheet__shooter">
      <div className="tk-sheet__name">{shooterName}</div>
      <div className="tk-sheet__meta">
        {knsaNumber && <span>{`KNSA ${knsaNumber}`}</span>}
        {discipline && <span className="tk-sheet__discipline">{discipline}</span>}
      </div>
    </section>

    <table className="tk-sheet__rounds">
      <tbody>
        {rounds.map(round => (
          <tr key={round.key} className={round.isCounted ? 'tk-sheet__round--counted' : undefined}>
            <th scope="row">{round.heading}</th>
            <td>{round.label}</td>
            <td className="tk-sheet__counted-mark">{round.isCounted ? '★' : ''}</td>
          </tr>
        ))}
      </tbody>
    </table>

    <section className="tk-sheet__score">
      <span>Score</span>
      <strong>{scoreText}</strong>
    </section>
    <p className="tk-sheet__note">Score: mean of the 3 fastest rounds (★)</p>

    {dqReason && <p className="tk-sheet__dq">{`DISQUALIFIED: ${dqReason}`}</p>}
    {warnings.map(warning => <p key={warning} className="tk-sheet__warning">{`Note: ${warning}`}</p>)}

    <footer className="tk-sheet__footer">
      {signedOffText && <div className="tk-sheet__signed">{`Signed off ${signedOffText}`}</div>}
      <div className="tk-sheet__signatures">
        <div className="tk-sheet__signature">Shooter</div>
        <div className="tk-sheet__signature">Timekeeper</div>
      </div>
    </footer>
  </article>
);

/** With silent printing enabled in the browser, the sheet goes straight to the label printer. */
export const ScoreSheetPrinter = () => {
  const dispatch = useAppDispatch();
  const printRequest = useAppSelector(selectPrintRequest);
  const sheet = useAppSelector(state => (printRequest ? selectScoreSheet(state, printRequest.cardId) : null));
  const requestNumber = printRequest?.requestNumber ?? null;
  const hasSheet = sheet !== null;

  useEffect(() => {
    if (requestNumber === null) {
      return;
    }
    if (!hasSheet) {
      dispatch(scoreSheetPrinted(requestNumber));

      return;
    }

    const handleAfterPrint = () => dispatch(scoreSheetPrinted(requestNumber));
    window.addEventListener('afterprint', handleAfterPrint, { once: true });
    // One frame so the sheet is laid out before the browser takes its print snapshot.
    const frameId = window.requestAnimationFrame(() => window.print());

    return () => {
      window.cancelAnimationFrame(frameId);
      window.removeEventListener('afterprint', handleAfterPrint);
    };
  }, [requestNumber, hasSheet, dispatch]);

  if (!sheet) {
    return null;
  }

  return createPortal(
    <div className="tk-print-root">
      <ScoreSheetView {...sheet} />
    </div>,
    document.body
  );
};
