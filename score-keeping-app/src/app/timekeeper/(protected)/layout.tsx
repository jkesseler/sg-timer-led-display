import type { ReactNode } from 'react';
import '../timekeeper.css';

// The login check lives in /timekeeper/bootstrap: the page itself is client-only.
export default function TimekeeperLayout({ children }: { children: ReactNode }) {
  return children;
}
