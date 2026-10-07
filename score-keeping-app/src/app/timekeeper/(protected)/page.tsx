'use client';

import dynamic from 'next/dynamic';

// No server rendering: the browser's Redux state is leading.
const TimekeeperApp = dynamic(
  () => import('@/components/timekeeper/TimekeeperApp').then(module => module.TimekeeperApp),
  { ssr: false }
);

export default function TimekeeperPage() {
  return <TimekeeperApp />;
}
