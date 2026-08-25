import type { ReactNode } from 'react';

// Root layout for the /dev segment — see src/app/timekeeper/layout.tsx for
// why this is needed (no shared root layout.tsx in this template).
export default function DevRootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
