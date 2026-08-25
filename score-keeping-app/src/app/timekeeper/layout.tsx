import type { ReactNode } from 'react'

// Root layout for the /timekeeper segment — this repo has no shared root
// layout.tsx, so each top-level route group/segment provides its own
// <html>/<body> (matching the pattern in (frontend)/layout.tsx and
// (payload)/layout.tsx). The auth-gated chrome lives one level down, in
// (protected)/layout.tsx, so /timekeeper/login stays outside it.
export default function TimekeeperRootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  )
}
