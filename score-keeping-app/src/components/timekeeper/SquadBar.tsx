'use client';

import { useEffect, useState, useTransition } from 'react';
import { useRouter } from 'next/navigation';
import { setCurrentSquadAction } from '@/app/timekeeper/(protected)/actions';

export interface SquadOption {
  id: number;
  label: string;
}

/**
 * Fixed bottom bar for the timekeeper screen showing which squad is loaded,
 * with a modal to pick any squad. Picking one points its match's currentSquad
 * at it (via setCurrentSquadAction), then navigates to /timekeeper?squad=<id>.
 */
export function SquadBar({ options, activeSquadId }: { options: SquadOption[]; activeSquadId: number | null }) {
  const router = useRouter();
  const [isOpen, setIsOpen] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [isPending, startTransition] = useTransition();

  const activeLabel
    = options.find(option => option.id === activeSquadId)?.label
      ?? (activeSquadId !== null ? `Squad #${activeSquadId}` : null);

  useEffect(() => {
    if (!isOpen) {
      return;
    }

    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        setIsOpen(false);
      }
    };

    window.addEventListener('keydown', handleKeyDown);

    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [isOpen]);

  function handleSelect(squadId: number) {
    if (squadId === activeSquadId) {
      setIsOpen(false);

      return;
    }

    setError(null);
    startTransition(async () => {
      const result = await setCurrentSquadAction(squadId);
      if (!result.ok) {
        setError(result.error ?? 'Could not switch to that squad.');

        return;
      }

      setIsOpen(false);
      router.push(`/timekeeper?squad=${squadId}`);
    });
  }

  return (
    <>
      <div className="tk-squad-bar">
        <button
          type="button"
          className="tk-squad-bar__toggle"
          aria-haspopup="dialog"
          aria-expanded={isOpen}
          onClick={() => setIsOpen(open => !open)}
        >
          <span className="tk-squad-bar__label">Active squad</span>
          <span className="tk-squad-bar__name">{activeLabel ?? 'Choose a squad'}</span>
          <span
            className={`tk-squad-bar__chevron${isOpen ? ' tk-squad-bar__chevron--open' : ''}`}
            aria-hidden="true"
          >
            ▾
          </span>
        </button>
      </div>

      {isOpen && (
        <div className="tk-squad-modal__overlay" onClick={() => setIsOpen(false)}>
          <div
            className="tk-squad-modal"
            role="dialog"
            aria-modal="true"
            aria-labelledby="tk-squad-modal-title"
            onClick={event => event.stopPropagation()}
          >
            <div className="tk-squad-modal__header">
              <h2 id="tk-squad-modal-title">Choose a squad</h2>
              <button
                type="button"
                className="tk-squad-modal__close"
                aria-label="Close"
                onClick={() => setIsOpen(false)}
              >
                ×
              </button>
            </div>

            {error && <p className="tk-squad-modal__error">{error}</p>}

            <div className="tk-squad-modal__list">
              {options.length === 0 && (
                <p className="tk-squad-modal__empty">No squads exist yet. Create one in /admin.</p>
              )}
              {options.map(option => (
                <button
                  key={option.id}
                  type="button"
                  className={`tk-squad-modal__row${option.id === activeSquadId ? ' tk-squad-modal__row--current' : ''}`}
                  aria-current={option.id === activeSquadId ? 'true' : undefined}
                  disabled={isPending}
                  onClick={() => handleSelect(option.id)}
                >
                  <span>{option.label}</span>
                  {option.id === activeSquadId && <span className="tk-squad-modal__tag">current</span>}
                </button>
              ))}
            </div>
          </div>
        </div>
      )}
    </>
  );
}

export default SquadBar;
