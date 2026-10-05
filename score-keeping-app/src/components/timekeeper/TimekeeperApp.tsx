'use client';

import { useEffect } from 'react';
import { appendAudit, saveMatchState } from '@/app/timekeeper/(protected)/actions';
import { logoutAction } from '@/app/timekeeper/login/actions';
import { ReduxProvider } from '@/components/display/ReduxProvider';
import { disconnectMqttClient } from '@/store/mqttMiddleware';
import { useAppDispatch, useAppSelector } from '@/store/store';
import { selectLoadError, selectLoadStatus, selectMatchLabel, selectUserEmail } from '@/store/timekeeperSelectors';
import { timekeeperReset } from '@/store/timekeeperSlice';
import { loadTimekeeper } from '@/store/timekeeperThunks';
import type { TimekeeperLoadStatus } from '@/store/timekeeperSlice';
import type { SyncTransport } from '@/store/syncMiddleware';
import { TimekeeperBoard } from './TimekeeperBoard';

const syncTransport: SyncTransport = { saveMatchState, appendAudit };

interface HeaderViewProps {
  userEmail: string | null;
}

export const HeaderView = ({ userEmail }: HeaderViewProps) => (
  <header className="tk-header">
    <span className="tk-header__title">Timekeeper</span>
    <form action={logoutAction} className="tk-header__account">
      {userEmail && <span className="tk-header__email">{userEmail}</span>}
      <button type="submit" className="tk-button tk-button--small">Log out</button>
    </form>
  </header>
);

interface MainViewProps {
  loadStatus: TimekeeperLoadStatus;
  loadError: string | null;
  matchLabel: string | null;
}

export const MainView = ({ loadStatus, loadError, matchLabel }: MainViewProps) => {
  switch (loadStatus) {
    case 'loading':
      return <div className="tk-layout"><p className="tk-muted">Loading match…</p></div>;
    case 'error':
      return <div className="tk-layout"><div className="tk-error">{loadError}</div></div>;
    case 'no-match':
      return (
        <div className="tk-layout">
          <div className="tk-main">
            <h1 className="tk-squad-title">No active match</h1>
            <p className="tk-muted">Tick &quot;active&quot; on a match in /admin.</p>
          </div>
        </div>
      );
    case 'ready':
      return (
        <>
          <h1 className="tk-match-title">{matchLabel}</h1>
          <TimekeeperBoard />
        </>
      );
  }
};

const TimekeeperShell = () => {
  const dispatch = useAppDispatch();

  useEffect(() => {
    dispatch(loadTimekeeper());

    return () => {
      disconnectMqttClient();
      dispatch(timekeeperReset());
    };
  }, [dispatch]);

  return (
    <div>
      <HeaderView userEmail={useAppSelector(selectUserEmail)} />
      <main>
        <MainView
          loadStatus={useAppSelector(selectLoadStatus)}
          loadError={useAppSelector(selectLoadError)}
          matchLabel={useAppSelector(selectMatchLabel)}
        />
      </main>
    </div>
  );
};

/** Client-only: the page renders nothing on the server and fetches its data from /timekeeper/bootstrap. */
export const TimekeeperApp = () => (
  <ReduxProvider sync={syncTransport}>
    <TimekeeperShell />
  </ReduxProvider>
);
