import { Inter, JetBrains_Mono } from 'next/font/google';
import type { Metadata } from 'next';
import type { ReactNode } from 'react';
import './styles.css';

// Matches --font-ui / --font-mono already referenced throughout the ported
// display CSS — next/font self-hosts these instead of the external Google
// Fonts <link> tags the original Vite app used (better for a kiosk that
// might have flaky internet).
const interFont = Inter({
  subsets: ['latin'],
  weight: ['400', '500', '600', '700'],
  variable: '--font-ui',
  display: 'swap'
});

const jetBrainsMonoFont = JetBrains_Mono({
  subsets: ['latin'],
  weight: ['500', '600', '700'],
  variable: '--font-mono',
  display: 'swap'
});

export const metadata: Metadata = {
  title: 'J.K. PewPew Timer',
  description: 'Live shot timer scoreboard for TV kiosk display, driven by MQTT',
  manifest: '/manifest.webmanifest'
};

export const viewport = {
  themeColor: '#0a0d10'
};

// Root layout for the /display segment — see src/app/timekeeper/layout.tsx
// for why this is needed (no shared root layout.tsx in this template).
export default function DisplayRootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en" className={`${interFont.variable} ${jetBrainsMonoFont.variable}`}>
      <body style={{ margin: 0, padding: 0, background: '#0a0d10', overflow: 'hidden' }}>{children}</body>
    </html>
  );
}
