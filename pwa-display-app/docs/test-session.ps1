# Complete session simulation for testing PWA display
# Usage: .\test-session.ps1 [host]

param(
    [string]$Host = "localhost"
)

$TOPIC_PREFIX = "timer"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  SG Timer PWA Display - Test Session" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "MQTT Broker: $Host"
Write-Host ""

Start-Sleep -Seconds 1

Write-Host "▶ 1. Device connected..." -ForegroundColor Green
mosquitto_pub -h $Host -t "$TOPIC_PREFIX/connection/state" `
  -m "{`"state`":`"CONNECTED`",`"deviceName`":`"Test Timer`",`"timestamp`":$([DateTimeOffset]::Now.ToUnixTimeMilliseconds())}"
Start-Sleep -Seconds 2

Write-Host "▶ 2. Starting session with 3s countdown..." -ForegroundColor Green
mosquitto_pub -h $Host -t "$TOPIC_PREFIX/session/started" `
  -m "{`"sessionId`":99999,`"startDelaySeconds`":3.0,`"timestamp`":$([DateTimeOffset]::Now.ToUnixTimeMilliseconds())}"
Start-Sleep -Seconds 4

Write-Host "▶ 3. Firing 10 shots..." -ForegroundColor Green
for ($i = 1; $i -le 10; $i++) {
    $TIME = 2000 + ($i * 1500)
    $SPLIT = if ($i -eq 1) { 0 } else { 1500 }
    $IS_FIRST = if ($i -eq 1) { "true" } else { "false" }

    Write-Host "   Shot #$i at ${TIME}ms" -ForegroundColor Yellow
    mosquitto_pub -h $Host -t "$TOPIC_PREFIX/shot/detected" `
      -m "{`"sessionId`":99999,`"shotNumber`":$i,`"absoluteTimeMs`":$TIME,`"splitTimeMs`":$SPLIT,`"deviceModel`":`"Test Timer`",`"isFirstShot`":$IS_FIRST,`"timestamp`":$([DateTimeOffset]::Now.ToUnixTimeMilliseconds())}"

    Start-Sleep -Seconds 1
}

Write-Host "▶ 4. Ending session..." -ForegroundColor Green
Start-Sleep -Seconds 2
mosquitto_pub -h $Host -t "$TOPIC_PREFIX/session/stopped" `
  -m "{`"sessionId`":99999,`"totalShots`":10,`"timestamp`":$([DateTimeOffset]::Now.ToUnixTimeMilliseconds())}"

Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  ✓ Test Complete!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:"
Write-Host "  - Verify display showed all states"
Write-Host "  - Check shot times were correct"
Write-Host "  - Session should have ended gracefully"
Write-Host ""
