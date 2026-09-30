// @vitest-environment node
import { getPayload } from 'payload';
import { afterAll, beforeAll, describe, it, expect } from 'vitest';
import config from '@/payload.config';
import type { Payload } from 'payload';
import type { User } from '@/payload-types';

const UUID_PATTERN = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;

let payload: Payload;
let user: User;

beforeAll(async () => {
  payload = await getPayload({ config });
  user = await payload.create({
    collection: 'users',
    data: { email: 'timekeeper@example.test', password: 'secret123', role: 'timekeeper' }
  });
});

// Dropping only at the end: Payload builds the unique indexes once, at init.
afterAll(async () => {
  await payload.db.connection.dropDatabase();
  await payload.db.destroy?.();
});

async function createMatch() {
  const device = await payload.create({ collection: 'devices', data: { deviceId: `D${Date.now()}`, label: 'Lane' } });

  return payload.create({ collection: 'matches', data: { label: 'M', date: new Date().toISOString(), device: device.id, active: true } });
}

describe('collections on Mongo', () => {
  it('gives every document a UUID id and keeps relationships working', async () => {
    const match = await createMatch();
    const squad = await payload.create({ collection: 'squads', data: { match: match.id, startTime: '08:00', endTime: '09:00', discipline: 'OKP' } });
    const shooter = await payload.create({ collection: 'shooters', data: { firstName: 'Jan', lastName: 'Jansen', knsaNumber: `K${Date.now()}` } });
    const member = await payload.create({ collection: 'squad-members', data: { squad: squad.id, shooter: shooter.id, startingPosition: 1 } });

    const loaded = await payload.findByID({ collection: 'squad-members', id: member.id, depth: 2 });

    for (const id of [user.id, match.id, squad.id, shooter.id, member.id]) {
      expect(id).toMatch(UUID_PATTERN);
    }
    expect(typeof loaded.squad === 'object' && typeof loaded.squad.match === 'object').toBe(true);
    expect(squad.label).toBe('08:00 - 09:00');
  });

  it('lets a user with a UUID id log in', async () => {
    const result = await payload.login({ collection: 'users', data: { email: 'timekeeper@example.test', password: 'secret123' } });

    expect(result.user?.id).toBe(user.id);
    expect(result.token).toBeTruthy();
  });

  it('sets the audit author from the logged-in user and rejects a duplicate action id', async () => {
    const match = await createMatch();
    const data = { actionId: crypto.randomUUID(), match: match.id, at: new Date().toISOString(), type: 'match/armTurn', payload: {} };

    const entry = await payload.create({ collection: 'match-audit', data: { ...data, by: 'someone-else' }, user, overrideAccess: false });

    expect(typeof entry.by === 'object' ? entry.by?.id : entry.by).toBe(user.id);
    await expect(payload.create({ collection: 'match-audit', data, user, overrideAccess: false })).rejects.toThrow();
  });

  it('never lets a user change or delete audit entries', async () => {
    const match = await createMatch();
    const entry = await payload.create({
      collection: 'match-audit',
      data: { actionId: crypto.randomUUID(), match: match.id, at: new Date().toISOString(), type: 'match/flagDnf' },
      user,
      overrideAccess: false
    });

    await expect(payload.update({ collection: 'match-audit', id: entry.id, data: { type: 'x' }, user, overrideAccess: false })).rejects.toThrow();
    await expect(payload.delete({ collection: 'match-audit', id: entry.id, user, overrideAccess: false })).rejects.toThrow();
  });
});
