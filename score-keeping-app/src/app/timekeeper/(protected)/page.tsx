'use client';

import dynamic from 'next/dynamic';

// No server rendering: the timekeeper is a browser app whose Redux state is
// leading. It loads its data from /timekeeper/bootstrap, which also checks
// the login and sends the browser to /timekeeper/login when there is none.
const TimekeeperApp = dynamic(
  () => import('@/components/timekeeper/TimekeeperApp').then(module => module.TimekeeperApp),
  { ssr: false }
);

export default function TimekeeperPage() {
  return <TimekeeperApp />;
}
