import { Inter, JetBrains_Mono } from 'next/font/google';
import type { Metadata } from 'next';
import './styles.css';

// Self-hosted by next/font: the kiosk may have flaky internet.
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

// No shared root layout: each top-level segment provides its own <html>/<body>.
export default function DisplayRootLayout({ children }: LayoutProps<'/display'>) {
  return (
    <html lang="en" className={`${interFont.variable} ${jetBrainsMonoFont.variable}`}>
      <body style={{ margin: 0, padding: 0, background: '#0a0d10', overflow: 'hidden' }}>{children}</body>
    </html>
  );
}
