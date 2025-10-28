#pragma once

#include "ITimerDevice.h"
#include <Arduino.h>

// Simulator configuration
namespace SimulatorConfig {
  constexpr uint32_t SIMULATION_STEP_MS = 100;      // How often simulator updates
  constexpr uint32_t CONNECTION_DELAY_MS = 2000;    // Time to simulate connection
  constexpr uint32_t SESSION_START_DELAY_MS = 3000; // Delay before starting session
  constexpr uint32_t MIN_SHOT_INTERVAL_MS = 800;    // Minimum time between shots
  constexpr uint32_t MAX_SHOT_INTERVAL_MS = 3000;   // Maximum time between shots
  constexpr uint16_t MAX_SIMULATOR_SHOTS = 20;      // Maximum shots in a session
  constexpr float SESSION_AUTO_STOP_CHANCE = 0.1f;  // Chance per second to auto-stop
}

// Simulation modes
enum class SimulationMode {
  MANUAL,        // Manual control via commands
  AUTO_CONNECT,  // Auto-connect and start session
  AUTO_SHOTS,    // Auto-generate shots at random intervals
  REALISTIC      // Realistic shooting pattern simulation
};

class SGTimerSimulator : public ITimerDevice {
private:
  // Device state
  DeviceConnectionState connectionState;
  bool deviceConnected;
  String deviceName;
  String deviceModel;
  BLEAddress simulatedAddress;

  // Session state
  SessionData currentSession;
  bool sessionActive;
  uint16_t shotCount;
  uint32_t lastShotTime;
  uint32_t sessionStartTime;

  // Simulation control
  SimulationMode simulationMode;
  unsigned long lastUpdateTime;
  unsigned long nextShotTime;
  unsigned long connectionStartTime;
  unsigned long sessionStartTime_sim;
  bool autoSessionStarted;

  // Callbacks
  std::function<void(const NormalizedShotData&)> shotDetectedCallback;
  std::function<void(const SessionData&)> sessionStartedCallback;
  std::function<void(const SessionData&)> sessionStoppedCallback;
  std::function<void(const SessionData&)> sessionSuspendedCallback;
  std::function<void(const SessionData&)> sessionResumedCallback;
  std::function<void(DeviceConnectionState)> connectionStateCallback;

  // Internal methods
  void setConnectionState(DeviceConnectionState newState);
  void simulateConnection();
  void simulateSessionStart();
  void simulateShot();
  void simulateSessionEnd();
  void generateRealisticShotTiming();
  uint32_t generateRandomShotInterval();
  NormalizedShotData createShotData(uint16_t shotNumber, uint32_t absoluteTime);

public:
  SGTimerSimulator(SimulationMode mode = SimulationMode::AUTO_SHOTS);
  virtual ~SGTimerSimulator();

  // ITimerDevice interface implementation
  bool initialize() override;
  bool startScanning() override;
  bool connect(BLEAddress address) override;
  void disconnect() override;
  DeviceConnectionState getConnectionState() const override;
  bool isConnected() const override;

  const char* getDeviceModel() const override;
  const char* getDeviceName() const override;
  BLEAddress getDeviceAddress() const override;

  void onShotDetected(std::function<void(const NormalizedShotData&)> callback) override;
  void onSessionStarted(std::function<void(const SessionData&)> callback) override;
  void onSessionStopped(std::function<void(const SessionData&)> callback) override;
  void onSessionSuspended(std::function<void(const SessionData&)> callback) override;
  void onSessionResumed(std::function<void(const SessionData&)> callback) override;
  void onConnectionStateChanged(std::function<void(DeviceConnectionState)> callback) override;

  bool supportsRemoteStart() const override;
  bool supportsShotList() const override;
  bool supportsSessionControl() const override;

  bool requestShotList(uint32_t sessionId) override;
  bool startSession() override;
  bool stopSession() override;

  void update() override;

  // Simulator-specific methods
  void setSimulationMode(SimulationMode mode);
  void simulateManualShot();
  void simulateManualSessionStart();
  void simulateManualSessionStop();
  void reset();

  // Status methods
  uint16_t getCurrentShotCount() const { return shotCount; }
  bool isSessionActive() const { return sessionActive; }
  SimulationMode getSimulationMode() const { return simulationMode; }
};