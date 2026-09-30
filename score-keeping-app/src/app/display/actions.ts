'use server';

import { getPayload } from 'payload';
import { deriveRoster } from '@/lib/match/derive';
import { findActiveMatch, findMatchState, getMatchDeviceId } from '@/lib/match/loadActiveMatch';
import config from '@/payload.config';
import type { RosterInfo } from '@/lib/match/derive';

/**
 * The Next:/On deck: callouts for the active match, from the timekeeper's
 * last synced snapshot. Polled by /display, which has no login, so it only
 * ever returns names. Empty when the timer shown is not the match's timer.
 */
export async function getRosterForDevice(deviceId: string): Promise<RosterInfo> {
  const empty: RosterInfo = { current: null, next: null, onDeck: null };
  if (!deviceId) {
    return empty;
  }

  const payload = await getPayload({ config });
  const match = await findActiveMatch(payload);
  if (!match || getMatchDeviceId(match) !== deviceId) {
    return empty;
  }

  const state = await findMatchState(payload, match.id);

  return state ? deriveRoster(state) : empty;
}
