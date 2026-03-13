# PWA Display App - Documentation Index

Welcome to the SG Timer PWA Display documentation! This index will help you find the information you need.

---

## 📚 Quick Navigation

### For New Users
1. **[README.md](README.md)** - Project overview and features
2. **[QUICKSTART.md](QUICKSTART.md)** - Get started in 5 minutes
3. **[docs/TESTING.md](docs/TESTING.md)** - Test the display with sample data

### For Developers
1. **[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)** - Development workflow and customization
2. **[ARCHITECTURE.md](ARCHITECTURE.md)** - System design and architecture
3. **[PROJECT-SUMMARY.md](PROJECT-SUMMARY.md)** - Complete project summary

### For ESP32 Integration
1. **[docs/ESP32-MQTT-BRIDGE.md](docs/ESP32-MQTT-BRIDGE.md)** - Implement MQTT bridge in firmware

---

## 📖 Documentation by Topic

### Getting Started

| Document | Description | Time Required |
|----------|-------------|---------------|
| [README.md](README.md) | Project overview, features, architecture | 5 min read |
| [QUICKSTART.md](QUICKSTART.md) | Installation and setup guide | 5 min setup |
| [package.json](package.json) | Dependencies and npm scripts | Reference |

**Start here:** If you're new to the project, read README.md first, then follow QUICKSTART.md.

---

### Development

| Document | Description | Audience |
|----------|-------------|----------|
| [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) | Development workflow, customization, deployment | Developers |
| [ARCHITECTURE.md](ARCHITECTURE.md) | System architecture, data flow, component hierarchy | Architects |
| [PROJECT-SUMMARY.md](PROJECT-SUMMARY.md) | What was created, file structure, features | Team leads |
| [vite.config.js](vite.config.js) | Build configuration | DevOps |

**For developers:** Start with DEVELOPMENT.md for practical coding information.

---

### Testing

| Document | Description | Type |
|----------|-------------|------|
| [docs/TESTING.md](docs/TESTING.md) | Testing procedures, scenarios, checklist | Guide |
| [docs/test-session.sh](docs/test-session.sh) | Automated test script (Bash) | Script |
| [docs/test-session.ps1](docs/test-session.ps1) | Automated test script (PowerShell) | Script |

**For QA:** Use TESTING.md for comprehensive test scenarios and automated scripts.

---

### Integration

| Document | Description | Scope |
|----------|-------------|-------|
| [docs/ESP32-MQTT-BRIDGE.md](docs/ESP32-MQTT-BRIDGE.md) | ESP32 firmware MQTT implementation | Firmware |
| [src/constants.js](src/constants.js) | MQTT topics and message formats | API Contract |

**For hardware integration:** Follow ESP32-MQTT-BRIDGE.md for complete firmware implementation.

---

### Source Code

| File/Directory | Description | Language |
|----------------|-------------|----------|
| [src/App.jsx](src/App.jsx) | Main application component | React/JSX |
| [src/components/LEDMatrix.jsx](src/components/LEDMatrix.jsx) | Canvas-based display component | React/JSX |
| [src/components/Settings.jsx](src/components/Settings.jsx) | Settings modal component | React/JSX |
| [src/hooks/useMqtt.js](src/hooks/useMqtt.js) | MQTT connection hook | JavaScript |
| [src/constants.js](src/constants.js) | Constants and configuration | JavaScript |
| [src/utils.js](src/utils.js) | Helper functions | JavaScript |

**For code exploration:** Start with App.jsx to understand the overall structure.

---

## 🎯 Use Case Navigation

### "I want to..."

#### ...get the app running quickly
→ [QUICKSTART.md](QUICKSTART.md)

#### ...understand what this project does
→ [README.md](README.md)

#### ...see the system architecture
→ [ARCHITECTURE.md](ARCHITECTURE.md)

#### ...develop and customize the PWA
→ [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)

#### ...test the display without hardware
→ [docs/TESTING.md](docs/TESTING.md) + test scripts

#### ...integrate with ESP32 firmware
→ [docs/ESP32-MQTT-BRIDGE.md](docs/ESP32-MQTT-BRIDGE.md)

#### ...understand MQTT message formats
→ [src/constants.js](src/constants.js) + [docs/DEVELOPMENT.md#mqtt-message-format](docs/DEVELOPMENT.md)

#### ...deploy to production
→ [docs/DEVELOPMENT.md#deployment](docs/DEVELOPMENT.md)

#### ...modify the LED display appearance
→ [src/components/LEDMatrix.jsx](src/components/LEDMatrix.jsx) + [docs/DEVELOPMENT.md#customization](docs/DEVELOPMENT.md)

#### ...troubleshoot issues
→ [QUICKSTART.md#troubleshooting](QUICKSTART.md) + [docs/DEVELOPMENT.md#troubleshooting](docs/DEVELOPMENT.md)

---

## 📂 File Structure Reference

```
pwa-display-app/
│
├── 📘 Documentation (You are here!)
│   ├── README.md                    ← Project overview
│   ├── QUICKSTART.md                ← 5-minute setup
│   ├── ARCHITECTURE.md              ← System design
│   ├── PROJECT-SUMMARY.md           ← Complete summary
│   ├── INDEX.md                     ← This file
│   │
│   └── docs/
│       ├── DEVELOPMENT.md           ← Dev workflow
│       ├── TESTING.md               ← Test procedures
│       ├── ESP32-MQTT-BRIDGE.md     ← Firmware integration
│       ├── test-session.sh          ← Test script (Bash)
│       └── test-session.ps1         ← Test script (PowerShell)
│
├── 📦 Configuration
│   ├── package.json                 ← Dependencies
│   ├── vite.config.js               ← Build config
│   ├── index.html                   ← HTML entry
│   ├── .gitignore                   ← Git exclusions
│   └── .editorconfig                ← Editor settings
│
├── 💻 Source Code
│   └── src/
│       ├── main.jsx                 ← React entry
│       ├── App.jsx                  ← Main component
│       ├── App.css                  ← App styling
│       ├── index.css                ← Global styles
│       ├── constants.js             ← Config/constants
│       ├── utils.js                 ← Utilities
│       │
│       ├── components/
│       │   ├── LEDMatrix.jsx        ← Display component
│       │   ├── LEDMatrix.css        ← Display styles
│       │   ├── Settings.jsx         ← Settings modal
│       │   └── Settings.css         ← Modal styles
│       │
│       └── hooks/
│           └── useMqtt.js           ← MQTT hook
│
└── 🎨 Static Assets
    └── public/
        ├── vite.svg                 ← App icon
        └── manifest.json            ← PWA manifest
```

---

## 🔍 Topic Index

### MQTT
- Topics reference: [src/constants.js](src/constants.js)
- Message formats: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)
- Connection setup: [QUICKSTART.md](QUICKSTART.md)
- Hook implementation: [src/hooks/useMqtt.js](src/hooks/useMqtt.js)
- ESP32 bridge: [docs/ESP32-MQTT-BRIDGE.md](docs/ESP32-MQTT-BRIDGE.md)

### Display States
- State definitions: [src/constants.js](src/constants.js)
- Rendering logic: [src/components/LEDMatrix.jsx](src/components/LEDMatrix.jsx)
- State machine: [ARCHITECTURE.md](ARCHITECTURE.md)
- ESP32 comparison: [PROJECT-SUMMARY.md](PROJECT-SUMMARY.md)

### PWA Features
- Configuration: [vite.config.js](vite.config.js)
- Manifest: [public/manifest.json](public/manifest.json)
- Service worker: Auto-generated by vite-plugin-pwa
- Installation: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)

### React Components
- App component: [src/App.jsx](src/App.jsx)
- LED matrix: [src/components/LEDMatrix.jsx](src/components/LEDMatrix.jsx)
- Settings: [src/components/Settings.jsx](src/components/Settings.jsx)
- Hierarchy: [ARCHITECTURE.md](ARCHITECTURE.md)

### Testing
- Test guide: [docs/TESTING.md](docs/TESTING.md)
- Bash script: [docs/test-session.sh](docs/test-session.sh)
- PowerShell script: [docs/test-session.ps1](docs/test-session.ps1)
- Manual tests: [docs/TESTING.md](docs/TESTING.md)

### Deployment
- Build process: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)
- Static hosting: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)
- Local network: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)
- HTTPS setup: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)

---

## 🎓 Learning Path

### Beginner (Never used this before)
1. Read [README.md](README.md) - Understand what it does
2. Follow [QUICKSTART.md](QUICKSTART.md) - Get it running
3. Run [docs/test-session.sh](docs/test-session.sh) - See it work
4. Experiment with settings UI

**Time:** 30 minutes

### Intermediate (Want to customize)
1. Complete beginner path
2. Read [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) - Learn customization
3. Review [src/components/LEDMatrix.jsx](src/components/LEDMatrix.jsx) - Display logic
4. Modify colors, text, or layouts
5. Test changes with `npm run dev`

**Time:** 2-3 hours

### Advanced (Want to integrate with ESP32)
1. Complete intermediate path
2. Read [ARCHITECTURE.md](ARCHITECTURE.md) - System design
3. Follow [docs/ESP32-MQTT-BRIDGE.md](docs/ESP32-MQTT-BRIDGE.md) - Firmware
4. Implement MQTT publishing in ESP32
5. Test with real hardware

**Time:** 1-2 days

---

## 📝 Contributing

When adding documentation:
1. Update this index with new files
2. Add cross-references to related docs
3. Keep QUICKSTART.md focused on getting started
4. Put detailed info in docs/DEVELOPMENT.md
5. Technical details in ARCHITECTURE.md

---

## 🆘 Getting Help

### Common Questions

**Q: How do I get started?**
A: Follow [QUICKSTART.md](QUICKSTART.md)

**Q: MQTT won't connect**
A: See [QUICKSTART.md#troubleshooting](QUICKSTART.md)

**Q: How do I customize the display?**
A: See [docs/DEVELOPMENT.md#customization](docs/DEVELOPMENT.md)

**Q: Can I use this without ESP32?**
A: Yes! Use test scripts in [docs/TESTING.md](docs/TESTING.md)

**Q: How do I deploy to production?**
A: See [docs/DEVELOPMENT.md#deployment](docs/DEVELOPMENT.md)

**Q: What MQTT broker should I use?**
A: Mosquitto for local, HiveMQ Cloud for internet. See [QUICKSTART.md](QUICKSTART.md)

---

## 📊 Documentation Stats

- **Total Documents:** 10 files
- **Lines of Documentation:** ~3,000+ lines
- **Code Examples:** 50+
- **Diagrams:** 15+
- **Test Scripts:** 2 (Bash + PowerShell)

---

## 🔄 Last Updated

- **Date:** December 11, 2025
- **Version:** 1.0.0
- **Status:** Complete and ready for use

---

## 📞 Support Resources

- **Main Project:** [../README.md](../README.md)
- **ESP32 Firmware:** [../ESP32-S3-firmware/](../ESP32-S3-firmware/)
- **Memory Bank:** [../memory-bank/](../memory-bank/)

---

**Happy developing! 🚀**

Start with [QUICKSTART.md](QUICKSTART.md) and you'll be up and running in 5 minutes.
