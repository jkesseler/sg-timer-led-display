import type { MetadataRoute } from 'next';

// Only /display is installable; /admin and /timekeeper are staff tools.
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
        type: 'image/svg+xml'
      }
    ]
  };
}
