import type { ReactNode } from 'react'
import { Inter, JetBrains_Mono } from 'next/font/google'
import './styles.css'

// Same fonts as /display (src/app/display/layout.tsx), self-hosted via
// next/font instead of an external Google Fonts request.
const interFont = Inter({
  subsets: ['latin'],
  weight: ['400', '500', '600', '700'],
  variable: '--font-ui',
  display: 'swap',
})

const jetBrainsMonoFont = JetBrains_Mono({
  subsets: ['latin'],
  weight: ['500', '600', '700'],
  variable: '--font-mono',
  display: 'swap',
})

// Root layout for the /timekeeper segment — this repo has no shared root
// layout.tsx, so each top-level route group/segment provides its own
// <html>/<body> (matching the pattern in (frontend)/layout.tsx and
// (payload)/layout.tsx). The auth-gated chrome lives one level down, in
// (protected)/layout.tsx, so /timekeeper/login stays outside it.
export default function TimekeeperRootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en" className={`${interFont.variable} ${jetBrainsMonoFont.variable}`}>
      <body>{children}</body>
    </html>
  )
}
