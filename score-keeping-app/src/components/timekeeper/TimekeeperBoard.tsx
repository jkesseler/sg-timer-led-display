'use client';

import { SplitList } from '@/components/display/SplitList';
import { useAppSelector } from '@/store/store';
import { selectSplitsView } from '@/store/timekeeperSelectors';
import { AbsentList, LateShooterForm, OutstandingList, UnassignedResults } from './ListSections';
import { Queue } from './Queue';
import { Message, Roster, ScanForm, SquadStatusCard, SquadTabs, StatusLine } from './StatusSections';

const SplitsPane = () => {
  const { shots, highlightExtremes } = useAppSelector(selectSplitsView);

  return (
    <aside className="tk-splits-pane">
      <SplitList shots={shots} highlightExtremes={highlightExtremes} />
    </aside>
  );
};

/** Layout only: every section reads its own data from the store. */
export const TimekeeperBoard = () => (
  <div className="tk-layout">
    <div className="tk-main">
      <StatusLine />
      <SquadTabs />
      <SquadStatusCard />
      <Roster />
      <Message />
      <ScanForm />
      <UnassignedResults />
      <Queue />
      <OutstandingList />
      <AbsentList />
      <LateShooterForm />
    </div>
    <SplitsPane />
  </div>
);
