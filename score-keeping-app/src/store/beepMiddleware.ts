import { createListenerMiddleware } from '@reduxjs/toolkit';
import { countdownComplete } from './mqttSlice';

let audioCtx: AudioContext | null = null;

function ensureAudioContext(): AudioContext | null {
  if (audioCtx && audioCtx.state !== 'closed') {
    return audioCtx;
  }
  try {
    audioCtx = new AudioContext();

    return audioCtx;
  } catch {
    console.warn('Web Audio API not available');

    return null;
  }
}

function playBeep() {
  const ctx = ensureAudioContext();
  if (!ctx) {
    return;
  }

  // Browsers suspend audio created before a user gesture (autoplay policy).
  if (ctx.state === 'suspended') {
    ctx.resume().catch(() => {});
  }

  const oscillator = ctx.createOscillator();
  const gain = ctx.createGain();

  oscillator.type = 'square';
  oscillator.frequency.setValueAtTime(880, ctx.currentTime);

  gain.gain.setValueAtTime(0.3, ctx.currentTime);
  gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + 0.2);

  oscillator.connect(gain);
  gain.connect(ctx.destination);

  oscillator.start(ctx.currentTime);
  oscillator.stop(ctx.currentTime + 0.2);
}

export const beepMiddleware = createListenerMiddleware();

beepMiddleware.startListening({
  actionCreator: countdownComplete,
  effect: async (_action, _listenerApi) => {
    try {
      playBeep();
      console.log('Countdown complete - beep played');
    } catch (error) {
      console.error('Failed to play countdown beep:', error);
    }
  }
});
