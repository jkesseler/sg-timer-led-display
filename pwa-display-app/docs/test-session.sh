#!/bin/bash
# Complete session simulation for testing PWA display
# Usage: ./test-session.sh [host]

HOST="${1:-localhost}"
TOPIC_PREFIX="timer"

echo "=========================================="
echo "  SG Timer PWA Display - Test Session"
echo "=========================================="
echo "MQTT Broker: $HOST"
echo ""

sleep 1

echo "▶ 1. Device connected..."
mosquitto_pub -h "$HOST" -t "${TOPIC_PREFIX}/connection/state" \
  -m "{\"state\":\"CONNECTED\",\"deviceName\":\"Test Timer\",\"timestamp\":$(date +%s)000}"
sleep 2

echo "▶ 2. Starting session with 3s countdown..."
mosquitto_pub -h "$HOST" -t "${TOPIC_PREFIX}/session/started" \
  -m "{\"sessionId\":99999,\"startDelaySeconds\":3.0,\"timestamp\":$(date +%s)000}"
sleep 4

echo "▶ 3. Firing 10 shots..."
for i in {1..10}; do
  TIME=$((2000 + i * 1500))
  SPLIT=1500
  IS_FIRST="false"

  if [ $i -eq 1 ]; then
    SPLIT=0
    IS_FIRST="true"
  fi

  echo "   Shot #$i at ${TIME}ms"
  mosquitto_pub -h "$HOST" -t "${TOPIC_PREFIX}/shot/detected" \
    -m "{\"sessionId\":99999,\"shotNumber\":$i,\"absoluteTimeMs\":$TIME,\"splitTimeMs\":$SPLIT,\"deviceModel\":\"Test Timer\",\"isFirstShot\":$IS_FIRST,\"timestamp\":$(date +%s)000}"

  sleep 1
done

echo "▶ 4. Ending session..."
sleep 2
mosquitto_pub -h "$HOST" -t "${TOPIC_PREFIX}/session/stopped" \
  -m "{\"sessionId\":99999,\"totalShots\":10,\"timestamp\":$(date +%s)000}"

echo ""
echo "=========================================="
echo "  ✓ Test Complete!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "  - Verify display showed all states"
echo "  - Check shot times were correct"
echo "  - Session should have ended gracefully"
echo ""
