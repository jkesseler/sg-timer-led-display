import React, { useRef, useEffect, useState } from 'react';
import { DisplayConfig, DisplayColors, DisplayState } from '../constants';
import { formatCountdown } from '../utils';
import type { LEDMatrixProps, ShotData, SessionData, CanvasRenderingContext } from '../types';
import './LEDMatrix.css';

// ESP32-style time formatting (matches firmware format: ss:cc where cc is centiseconds)
function formatTimeESP32(timeMs: number): string {
  const totalSeconds = Math.floor(timeMs / 1000);
  const centiseconds = Math.floor((timeMs % 1000) / 10);
  return `${totalSeconds.toString().padStart(2, '0')}:${centiseconds.toString().padStart(2, '0')}`;
}

function formatSplitTimeESP32(timeMs: number): string {
  const totalSeconds = Math.floor(timeMs / 1000);
  const centiseconds = Math.floor((timeMs % 1000) / 10);
  if (totalSeconds < 10) {
    return `${totalSeconds}:${centiseconds.toString().padStart(2, '0')}`;
  }
  return `${totalSeconds.toString().padStart(2, '0')}:${centiseconds.toString().padStart(2, '0')}`;
}

/**
 * LED Matrix Display Component
 * Mimics the 128x32 HUB75 LED matrix display from the ESP32 firmware
 */
const LEDMatrix: React.FC<LEDMatrixProps> = ({
  displayState,
  shotData,
  sessionData,
  deviceName,
  brightness = 200
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [scrollOffset, setScrollOffset] = useState<number>(0);
  const [pixelSize, setPixelSize] = useState<number>(DisplayConfig.PIXEL_SIZE);

  // Calculate responsive pixel size
  useEffect(() => {
    const updatePixelSize = () => {
      const maxWidth = window.innerWidth * 0.9;
      const maxHeight = window.innerHeight * 0.8;
      const widthScale = maxWidth / DisplayConfig.TOTAL_WIDTH;
      const heightScale = maxHeight / DisplayConfig.TOTAL_HEIGHT;
      const newPixelSize = Math.floor(Math.min(widthScale, heightScale, 8));
      setPixelSize(Math.max(2, newPixelSize)); // Minimum 2px per LED pixel
    };

    updatePixelSize();
    window.addEventListener('resize', updatePixelSize);
    return () => window.removeEventListener('resize', updatePixelSize);
  }, []);

  // Scrolling text animation for long messages
  useEffect(() => {
    if (displayState === DisplayState.STARTUP ||
        displayState === DisplayState.CONNECTED) {
      const interval = setInterval(() => {
        setScrollOffset(prev => (prev + 1) % 500);
      }, DisplayConfig.SCROLL_SPEED_MS);
      return () => clearInterval(interval);
    } else {
      setScrollOffset(0);
    }
  }, [displayState]);

  // Render display on canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const width = DisplayConfig.TOTAL_WIDTH;
    const height = DisplayConfig.TOTAL_HEIGHT;

    // Clear canvas
    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, width, height);

    // Apply brightness by adjusting alpha
    const alpha = brightness / 255;
    ctx.globalAlpha = alpha;

    // Render based on state
    switch (displayState) {
      case DisplayState.STARTUP:
        renderStartup(ctx, width, height, scrollOffset);
        break;
      case DisplayState.DISCONNECTED:
        renderDisconnected(ctx, width, height);
        break;
      case DisplayState.SCANNING:
        renderScanning(ctx, width, height);
        break;
      case DisplayState.CONNECTING:
        renderConnecting(ctx, width, height);
        break;
      case DisplayState.CONNECTED:
        renderConnected(ctx, width, height, deviceName, scrollOffset);
        break;
      case DisplayState.COUNTDOWN:
        renderCountdown(ctx, width, height, sessionData);
        break;
      case DisplayState.WAITING_FOR_SHOTS:
        renderWaitingForShots(ctx, width, height);
        break;
      case DisplayState.SHOWING_SHOT:
        renderShotData(ctx, width, height, shotData);
        break;
      case DisplayState.SESSION_ENDED:
        renderSessionEnd(ctx, width, height, sessionData, shotData);
        break;
      default:
        break;
    }

    ctx.globalAlpha = 1.0;
  }, [displayState, shotData, sessionData, deviceName, brightness, scrollOffset]);

  const canvasWidth = DisplayConfig.TOTAL_WIDTH;
  const canvasHeight = DisplayConfig.TOTAL_HEIGHT;
  const displayWidth = canvasWidth * pixelSize;
  const displayHeight = canvasHeight * pixelSize;

  return (
    <div className="led-matrix-container">
      <canvas
        ref={canvasRef}
        width={canvasWidth}
        height={canvasHeight}
        style={{
          width: `${displayWidth}px`,
          height: `${displayHeight}px`,
          imageRendering: 'pixelated',
          border: '2px solid #333'
        }}
      />
    </div>
  );
};

// Render functions for each display state

function renderStartup(ctx: CanvasRenderingContext, width: number, height: number, scrollOffset: number): void {
  const text = DisplayConfig.STARTUP_TEXT;
  ctx.fillStyle = DisplayColors.GREEN;
  ctx.font = 'bold 18px monospace';
  ctx.textBaseline = 'middle';

  const textWidth = ctx.measureText(text).width;

  if (textWidth <= width - 8) {
    // Text fits, center it
    const x = (width - textWidth) / 2;
    ctx.fillText(text, x, height / 2);
  } else {
    // Scroll the text
    const x = -scrollOffset % (textWidth + 60);
    ctx.fillText(text, x, height / 2);
    ctx.fillText(text, x + textWidth + 60, height / 2);
  }
}

function renderDisconnected(ctx: CanvasRenderingContext, _width: number, _height: number): void {
  ctx.fillStyle = DisplayColors.RED;
  ctx.font = 'bold 10px monospace';
  ctx.textBaseline = 'top';
  ctx.fillText('NO DEVICE', 0, 4);
}

function renderScanning(ctx: CanvasRenderingContext, _width: number, _height: number): void {
  ctx.fillStyle = DisplayColors.YELLOW;
  ctx.font = 'bold 10px monospace';
  ctx.textBaseline = 'top';
  ctx.fillText('SCANNING...', 0, 4);
}

function renderConnecting(ctx: CanvasRenderingContext, _width: number, _height: number): void {
  ctx.fillStyle = DisplayColors.BLUE;
  ctx.font = 'bold 10px monospace';
  ctx.textBaseline = 'top';
  ctx.fillText('CONNECTING...', 0, 4);
}

function renderConnected(
  ctx: CanvasRenderingContext,
  width: number,
  _height: number,
  deviceName: string | null,
  scrollOffset: number
): void {
  ctx.fillStyle = DisplayColors.GREEN;
  ctx.font = 'bold 10px monospace';
  ctx.textBaseline = 'top';
  ctx.fillText('CONNECTED', 0, 4);

  if (deviceName) {
    ctx.fillStyle = DisplayColors.WHITE;
    ctx.font = '10px monospace';
    const textWidth = ctx.measureText(deviceName).width;

    if (textWidth <= width - 8) {
      ctx.fillText(deviceName, 2, 20);
    } else {
      // Scroll device name
      const x = -scrollOffset % (textWidth + 60);
      ctx.fillText(deviceName, x, 20);
      ctx.fillText(deviceName, x + textWidth + 60, 20);
    }
  }
}

function renderCountdown(
  ctx: CanvasRenderingContext,
  width: number,
  _height: number,
  sessionData: SessionData | null
): void {
  if (!sessionData) return;

  // Calculate remaining time
  const elapsed = (Date.now() - sessionData.countdownStartTime) / 1000;
  const remaining = Math.max(0, sessionData.startDelaySeconds - elapsed);

  ctx.fillStyle = DisplayColors.YELLOW;
  ctx.font = 'bold 10px monospace';
  ctx.textBaseline = 'top';
  ctx.fillText('READY', 0, 4);

  // Color changes based on remaining time
  let countdownColor = DisplayColors.GREEN;
  if (remaining <= 3.0 && remaining > 1.0) {
    countdownColor = DisplayColors.YELLOW;
  } else if (remaining <= 1.0) {
    countdownColor = DisplayColors.RED;
  }

  ctx.fillStyle = countdownColor;
  ctx.font = 'bold 18px monospace';
  const countdownText = formatCountdown(remaining);
  const textWidth = ctx.measureText(countdownText).width;
  ctx.fillText(countdownText, (width - textWidth) / 2, 8);
}

function renderWaitingForShots(ctx: CanvasRenderingContext, _width: number, _height: number): void {
  ctx.fillStyle = DisplayColors.WHITE;
  ctx.font = '10px monospace';
  ctx.textBaseline = 'top';
  ctx.fillText('Shots: 0', 0, 4);
  ctx.fillText('Split: 0:00', 0, 20);

  ctx.font = 'bold 18px monospace';
  ctx.fillText('00:00', 65, 17);
}

function renderShotData(
  ctx: CanvasRenderingContext,
  _width: number,
  _height: number,
  shotData: ShotData | null
): void {
  if (!shotData) return;

  // Format times using ESP32 format (ss:cc where cc is centiseconds)
  const timeText = formatTimeESP32(shotData.absoluteTimeMs);
  const splitText = formatSplitTimeESP32(shotData.splitTimeMs);

  // Shot count (top left, yellow)
  ctx.fillStyle = DisplayColors.YELLOW;
  ctx.font = '10px monospace';
  ctx.textBaseline = 'top';
  ctx.fillText(`Shots: ${shotData.shotNumber}`, 0, 4);

  // Split time (middle left)
  ctx.fillText(`Split: ${splitText}`, 0, 20);

  // Absolute time (right side, green, large)
  ctx.fillStyle = DisplayColors.GREEN;
  ctx.font = 'bold 18px monospace';
  ctx.fillText(timeText, 65, 17);
}

function renderSessionEnd(
  ctx: CanvasRenderingContext,
  _width: number,
  _height: number,
  sessionData: SessionData | null,
  shotData: ShotData | null
): void {
  const timeText = shotData ? formatTimeESP32(shotData.absoluteTimeMs) : '00:00';
  const shotCount = shotData ? shotData.shotNumber : (sessionData?.totalShots || 0);

  ctx.fillStyle = DisplayColors.RED;
  ctx.font = '10px monospace';
  ctx.textBaseline = 'top';
  ctx.fillText('ENDED', 0, 4);
  ctx.fillText(`Shots: ${shotCount}`, 0, 20);

  ctx.font = 'bold 18px monospace';
  ctx.fillText(timeText, 65, 17);
}

export default LEDMatrix;
