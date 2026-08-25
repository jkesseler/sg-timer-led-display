import { useMemo } from 'react';
import { formatTimeValue } from '@/lib/display/utils';
import type { ShotData } from '@/lib/mqtt/types';
import './SplitList.css';

interface SplitListProps {
  shots: ShotData[];
  /** Tag the fastest/slowest split — only meaningful once a run is complete. */
  highlightExtremes?: boolean;
}

interface Extremes {
  fastestShotNumber: number | null;
  slowestShotNumber: number | null;
}

function findExtremes(shots: ShotData[]): Extremes {
  const timedShots = shots.filter((shot) => !shot.isFirstShot);
  if (timedShots.length < 2) {
    return { fastestShotNumber: null, slowestShotNumber: null };
  }

  let fastest = timedShots[0];
  let slowest = timedShots[0];
  for (const shot of timedShots) {
    if (shot.splitTimeMs < fastest.splitTimeMs) fastest = shot;
    if (shot.splitTimeMs > slowest.splitTimeMs) slowest = shot;
  }

  return { fastestShotNumber: fastest.shotNumber, slowestShotNumber: slowest.shotNumber };
}

export const SplitList = ({ shots, highlightExtremes = false }: SplitListProps) => {
  const maxSplitMs = useMemo(() => {
    const timedSplits = shots.filter((shot) => !shot.isFirstShot).map((shot) => shot.splitTimeMs);
    return Math.max(1, ...timedSplits);
  }, [shots]);

  const extremes = useMemo(
    () => (highlightExtremes ? findExtremes(shots) : { fastestShotNumber: null, slowestShotNumber: null }),
    [shots, highlightExtremes],
  );

  const newestFirst = [...shots].reverse();

  return (
    <div className="split-list">
      <div className="split-list__header">
        <span className="split-list__title">Splits</span>
        <span className="split-list__count">{shots.length === 1 ? '1 shot' : `${shots.length} shots`}</span>
      </div>

      {newestFirst.length === 0 ? (
        <div className="split-list__empty">Splits will appear here once shots are fired.</div>
      ) : (
        <ul className="split-list__rows" role="list">
          {newestFirst.map((shot, index) => {
            const barPercent = shot.isFirstShot
              ? 0
              : Math.min(100, Math.max(8, (shot.splitTimeMs / maxSplitMs) * 100));
            const isFastest = highlightExtremes && shot.shotNumber === extremes.fastestShotNumber;
            const isSlowest = highlightExtremes && shot.shotNumber === extremes.slowestShotNumber;

            const rowClassName = [
              'split-row',
              index === 0 && 'split-row--latest',
              isFastest && 'split-row--fastest',
              isSlowest && 'split-row--slowest',
            ]
              .filter(Boolean)
              .join(' ');

            return (
              <li className={rowClassName} key={shot.shotNumber}>
                <span className="split-row__number">{shot.shotNumber}</span>
                <span className="split-row__bar-track">
                  {!shot.isFirstShot && (
                    <span className="split-row__bar-fill" style={{ width: `${barPercent}%` }} />
                  )}
                </span>
                <span className="split-row__split">
                  {shot.isFirstShot ? 'draw' : formatTimeValue(shot.splitTimeMs)}
                </span>
                <span className="split-row__absolute">{formatTimeValue(shot.absoluteTimeMs)}</span>
              </li>
            );
          })}
        </ul>
      )}
    </div>
  );
};

export default SplitList;
