import { Inter, JetBrains_Mono } from 'next/font/google';
import './styles.css';

// Keep in sync with the fonts in src/app/display/layout.tsx.
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

// No shared root layout: each top-level segment provides its own <html>/<body>.
// Auth-gated chrome lives in (protected)/ so /timekeeper/login stays outside it.
export default function TimekeeperRootLayout({ children }: LayoutProps<'/timekeeper'>) {
  return (
    <html lang="en" className={`${interFont.variable} ${jetBrainsMonoFont.variable}`}>
      <body>{children}</body>
    </html>
  );
}
