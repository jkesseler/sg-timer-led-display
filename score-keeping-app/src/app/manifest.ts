import type { MetadataRoute } from 'next'

// Replaces pwa-display-app's vite-plugin-pwa config. The only installable
// route in this app is /display (a TV/tablet kiosk); /admin and /timekeeper
// are staff tools, not meant to be "added to home screen".
export default function manifest(): MetadataRoute.Manifest {
  return {
    name: 'SG Timer LED Display',
    short_name: 'Timer Display',
    description: 'Live shot timer scoreboard for TV kiosk display',
    start_url: '/display',
    display: 'standalone',
    orientation: 'landscape',
    background_color: '#0a0d10',
    theme_color: '#0a0d10',
    icons: [
      {
        src: '/display-icon.svg',
        sizes: 'any',
        type: 'image/svg+xml',
      },
    ],
  }
}
