# System Architecture - PWA Display

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         SHOOTING RANGE                          │
│                                                                 │
│  ┌──────────────┐                                               │
│  │  Shot Timer  │  (SG Timer Sport / Special Pie)               │
│  │   Device     │                                               │
│  └──────┬───────┘                                               │
│         │ BLE                                                   │
│         │                                                       │
│  ┌──────▼───────┐                                               │
│  │   ESP32-S3   │  WiFi Bridge (optional LED display)           │
│  │    Bridge    │                                               │
│  └──────┬───────┘                                               │
│         │ WiFi                                                  │
│         │                                                       │
│  ┌──────▼───────┐                                               │
│  │ MQTT Broker  │  (Mosquitto on local network)                 │
│  │  (WebSocket) │                                               │
│  └──────┬───────┘                                               │
│         │ ws://                                                 │
└─────────┼─────────────────────────────────────────────────────┘
          │
          │ Internet (optional)
          │
┌─────────▼─────────────────────────────────────────────────────┐
│                        DISPLAY DEVICES                          │
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   Tablet     │  │   Laptop     │  │      TV      │          │
│  │  (Browser)   │  │  (Browser)   │  │  (Browser)   │          │
│  │              │  │              │  │              │          │
│  │  PWA Display │  │  PWA Display │  │  PWA Display │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Component Architecture

### PWA Application Structure

```
┌────────────────────────────────────────────────────────────┐
│                      Browser Window                        │
│                                                            │
│  ┌──────────────────────────────────────────────────────┐ │
│  │                   App Component                       │ │
│  │                                                       │ │
│  │  ┌─────────────┐  ┌──────────────┐  ┌─────────────┐ │ │
│  │  │ Status Bar  │  │  Settings    │  │   Controls  │ │ │
│  │  │             │  │   Modal      │  │             │ │ │
│  │  └─────────────┘  └──────────────┘  └─────────────┘ │ │
│  │                                                       │ │
│  │  ┌──────────────────────────────────────────────┐    │ │
│  │  │         LEDMatrix Component                  │    │ │
│  │  │                                              │    │ │
│  │  │   ┌──────────────────────────────────┐      │    │ │
│  │  │   │      Canvas Element              │      │    │ │
│  │  │   │      (128x32 pixels)             │      │    │ │
│  │  │   │                                  │      │    │ │
│  │  │   │    Rendered Display States:     │      │    │ │
│  │  │   │    - Startup                    │      │    │ │
│  │  │   │    - Connected                  │      │    │ │
│  │  │   │    - Shot Display               │      │    │ │
│  │  │   │    - Session End                │      │    │ │
│  │  │   └──────────────────────────────────┘      │    │ │
│  │  │                                              │    │ │
│  │  └──────────────────────────────────────────────┘    │ │
│  │                                                       │ │
│  └───────────────────────────────────────────────────────┘ │
│                                                            │
│  ┌───────────────────────────────────────────────────┐    │
│  │             Service Worker (PWA)                  │    │
│  │  - Offline caching                                │    │
│  │  - Asset management                               │    │
│  └───────────────────────────────────────────────────┘    │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

---

## Data Flow Architecture

### MQTT Message Flow

```
ESP32 Bridge                MQTT Broker              PWA Display
┌─────────┐                ┌──────────┐             ┌──────────┐
│         │                │          │             │          │
│  BLE    │   WiFi/MQTT    │  Topics  │  WebSocket  │  React   │
│ Device  ├───────────────>│  Queue   ├────────────>│   App    │
│         │                │          │             │          │
└─────────┘                └──────────┘             └──────────┘
    │                           │                        │
    │ Shot Detected             │ timer/shot/detected    │
    ├──────────────────────────>│                        │
    │                           ├───────────────────────>│
    │                           │                        │
    │                           │                        ▼
    │                           │               ┌────────────────┐
    │                           │               │ useMqtt Hook   │
    │                           │               │ - Parse JSON   │
    │                           │               │ - Update State │
    │                           │               └────────┬───────┘
    │                           │                        │
    │                           │                        ▼
    │                           │               ┌────────────────┐
    │                           │               │  App State     │
    │                           │               │  - shotData    │
    │                           │               │  - displayState│
    │                           │               └────────┬───────┘
    │                           │                        │
    │                           │                        ▼
    │                           │               ┌────────────────┐
    │                           │               │  LEDMatrix     │
    │                           │               │  - Render      │
    │                           │               │  - Canvas      │
    │                           │               └────────────────┘
```

---

## State Machine

### Display States

```
                    ┌──────────┐
                    │  STARTUP │
                    └────┬─────┘
                         │ (3s timeout)
                         ▼
                ┌────────────────┐
        ┌──────>│ DISCONNECTED   │<──────┐
        │       └────────┬───────┘       │
        │                │                │
        │        (scanning)               │
        │                ▼                │
        │       ┌────────────────┐        │
        │       │    SCANNING    │        │
        │       └────────┬───────┘        │
        │                │                │
        │        (connecting)             │
        │                ▼                │
        │       ┌────────────────┐        │
        │       │   CONNECTING   │        │
        │       └────────┬───────┘        │
        │                │                │
        │         (connected)          (error)
        │                ▼                │
        │       ┌────────────────┐        │
        └───────┤   CONNECTED    │────────┘
                └────────┬───────┘
                         │
                 (session started)
                         ▼
                ┌────────────────┐
                │   COUNTDOWN    │
                └────────┬───────┘
                         │ (countdown complete)
                         ▼
                ┌────────────────┐
         ┌─────>│ WAITING_FOR_   │
         │      │     SHOTS      │
         │      └────────┬───────┘
         │               │
         │        (shot detected)
         │               ▼
         │      ┌────────────────┐
         └──────┤  SHOWING_SHOT  │
                └────────┬───────┘
                         │
                  (session stopped)
                         ▼
                ┌────────────────┐
                │ SESSION_ENDED  │
                └────────┬───────┘
                         │ (10s timeout)
                         ▼
                    [CONNECTED]
```

---

## React Component Hierarchy

```
App.jsx
│
├── State Management
│   ├── displayState
│   ├── shotData
│   ├── sessionData
│   ├── deviceName
│   └── brightness
│
├── Hooks
│   └── useMqtt()
│       ├── connect()
│       ├── disconnect()
│       ├── onMessage()
│       └── updateSettings()
│
└── Components
    │
    ├── StatusBar
    │   ├── Connection indicator
    │   ├── Error display
    │   └── Broker URL
    │
    ├── LEDMatrix (Canvas)
    │   ├── Props: displayState, shotData, etc.
    │   ├── Responsive sizing
    │   ├── Render functions per state
    │   └── Canvas 2D context
    │
    ├── Settings (Modal)
    │   ├── MQTT configuration
    │   ├── Brightness slider
    │   ├── Connection status
    │   └── Save/Cancel actions
    │
    ├── Controls (Buttons)
    │   ├── Settings button
    │   ├── Status toggle
    │   └── Brightness +/-
    │
    └── Instructions
        └── Keyboard hints
```

---

## File Organization

```
pwa-display-app/
│
├── Configuration Layer
│   ├── package.json          (Dependencies)
│   ├── vite.config.js        (Build + PWA)
│   ├── index.html            (Entry point)
│   └── .gitignore            (Git exclusions)
│
├── Source Code Layer
│   ├── src/
│   │   ├── main.jsx          (React initialization)
│   │   ├── App.jsx           (Main component)
│   │   ├── constants.js      (States, colors, topics)
│   │   ├── utils.js          (Helper functions)
│   │   │
│   │   ├── components/
│   │   │   ├── LEDMatrix.jsx (Display component)
│   │   │   └── Settings.jsx  (Settings modal)
│   │   │
│   │   └── hooks/
│   │       └── useMqtt.js    (MQTT hook)
│   │
│   └── Styling
│       ├── index.css         (Global)
│       ├── App.css           (App-specific)
│       ├── LEDMatrix.css     (Display)
│       └── Settings.css      (Modal)
│
├── Documentation Layer
│   ├── README.md             (Overview)
│   ├── QUICKSTART.md         (Setup guide)
│   ├── PROJECT-SUMMARY.md    (This file)
│   │
│   └── docs/
│       ├── DEVELOPMENT.md    (Dev guide)
│       ├── TESTING.md        (Test guide)
│       ├── ESP32-MQTT-BRIDGE.md (Integration)
│       ├── test-session.sh   (Test script)
│       └── test-session.ps1  (Test script)
│
└── Static Assets Layer
    └── public/
        ├── vite.svg          (Icon)
        └── manifest.json     (PWA manifest)
```

---

## Technology Stack Layers

```
┌─────────────────────────────────────────────────────┐
│              User Interface Layer                   │
│  - React Components (JSX)                           │
│  - CSS Styling                                      │
│  - Canvas API (Display rendering)                   │
└─────────────────┬───────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────┐
│           State Management Layer                    │
│  - React useState                                   │
│  - Custom hooks (useMqtt)                           │
│  - localStorage (persistence)                       │
└─────────────────┬───────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────┐
│          Communication Layer                        │
│  - MQTT.js (WebSocket client)                       │
│  - JSON message parsing                             │
│  - Topic subscription                               │
└─────────────────┬───────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────┐
│            Network Layer                            │
│  - WebSocket Protocol                               │
│  - MQTT Protocol                                    │
│  - HTTP (asset loading)                             │
└─────────────────┬───────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────┐
│          Browser Platform Layer                     │
│  - Service Worker API (PWA)                         │
│  - Web App Manifest                                 │
│  - localStorage API                                 │
└─────────────────────────────────────────────────────┘
```

---

## Deployment Architecture

### Development Environment

```
Developer Machine
├── Node.js → Vite Dev Server (localhost:3000)
├── Browser → Hot Module Replacement
└── MQTT Broker → Local Mosquitto
```

### Production Environment

```
                    ┌──────────────┐
                    │ CDN/Static   │
                    │   Hosting    │
                    │              │
                    │ - index.html │
                    │ - assets/    │
                    │ - service-   │
                    │   worker.js  │
                    └──────┬───────┘
                           │ HTTPS
                           │
┌──────────────────────────▼────────────────────────┐
│                  User Devices                     │
│                                                   │
│  Browser (Chrome/Safari/Firefox)                  │
│  ├── Service Worker (offline cache)               │
│  ├── PWA (installed app)                          │
│  └── WebSocket → MQTT Broker                      │
│                                                   │
└───────────────────────────────────────────────────┘
```

---

## Security Architecture

```
┌────────────────────────────────────────────────────┐
│                  Security Layers                   │
│                                                    │
│  1. Transport Layer                                │
│     ├── WSS:// (WebSocket Secure)                  │
│     └── TLS/SSL certificates                       │
│                                                    │
│  2. Authentication Layer                           │
│     ├── MQTT username/password                     │
│     └── Optional: OAuth/JWT tokens                 │
│                                                    │
│  3. Authorization Layer                            │
│     ├── MQTT ACL (topic permissions)               │
│     └── Read-only subscriptions                    │
│                                                    │
│  4. Data Validation Layer                          │
│     ├── JSON schema validation                     │
│     ├── Input sanitization                         │
│     └── Error handling                             │
│                                                    │
│  5. Storage Layer                                  │
│     ├── localStorage (client-side only)            │
│     └── No sensitive data stored                   │
│                                                    │
└────────────────────────────────────────────────────┘
```

---

## Performance Considerations

### Optimization Strategies

```
┌────────────────────────────────────────┐
│         Performance Optimizations      │
│                                        │
│  Frontend                              │
│  ├── Canvas rendering (hardware accel) │
│  ├── React memo (prevent re-renders)   │
│  ├── Debounced updates                 │
│  └── Responsive image sizing           │
│                                        │
│  Network                               │
│  ├── WebSocket (persistent connection) │
│  ├── Message buffering                 │
│  ├── Reconnection strategy             │
│  └── Auto-retry logic                  │
│                                        │
│  Caching                               │
│  ├── Service worker (offline assets)   │
│  ├── localStorage (settings)           │
│  └── Browser cache (static resources)  │
│                                        │
│  Build                                 │
│  ├── Vite bundling                     │
│  ├── Code splitting                    │
│  ├── Minification                      │
│  └── Tree shaking                      │
│                                        │
└────────────────────────────────────────┘
```

---

## Summary

This architecture provides:

✅ **Separation of Concerns** - Clear layers and responsibilities
✅ **Scalability** - Multiple displays from one source
✅ **Flexibility** - Works with any MQTT broker
✅ **Reliability** - Auto-reconnect, error handling
✅ **Performance** - Optimized rendering and caching
✅ **Security** - Encrypted transport, authentication
✅ **Maintainability** - Modular components, clear structure
✅ **Extensibility** - Easy to add features, customize

The system mirrors the ESP32 firmware architecture while leveraging web technologies for broader device compatibility and easier deployment.
