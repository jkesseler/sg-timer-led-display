import { DISCIPLINES } from '../lib/domain/disciplines';
import type { CollectionConfig } from 'payload';

const ROUNDS_PER_MEMBERSHIP = 5;

export const SquadMemberships: CollectionConfig = {
  slug: 'squad-memberships',
  admin: {
    useAsTitle: 'id',
    defaultColumns: ['squad', 'shooter', 'discipline', 'status', 'queuePosition']
  },
  // A shooter appears at most once per (squad, discipline) — the card format
  // is one card per shooter per discipline.
  indexes: [{ fields: ['squad', 'shooter', 'discipline'], unique: true }],
  fields: [
    {
      name: 'squad',
      type: 'relationship',
      relationTo: 'squads',
      required: true
    },
    {
      name: 'shooter',
      type: 'relationship',
      relationTo: 'shooters',
      required: true
    },
    {
      name: 'discipline',
      type: 'select',
      required: true,
      options: DISCIPLINES.map(discipline => ({ label: discipline, value: discipline }))
    },
    {
      name: 'startingPosition',
      type: 'number',
      required: true,
      admin: {
        description: 'Position number from the printed schedule — the starting order only.'
      }
    },
    {
      name: 'queuePosition',
      type: 'number',
      required: true,
      admin: {
        description: 'Current position in the live shooting queue. Mutable mid-match; renumbered on every queue mutation.'
      }
    },
    {
      name: 'status',
      type: 'select',
      required: true,
      defaultValue: 'scheduled',
      options: [
        { label: 'Scheduled', value: 'scheduled' },
        { label: 'Present', value: 'present' },
        { label: 'Absent', value: 'absent' },
        { label: 'Withdrawn', value: 'withdrawn' },
        { label: 'Disqualified', value: 'disqualified' }
      ]
    },
    {
      name: 'disqualifiedReason',
      type: 'textarea',
      admin: {
        description: 'Why the shooter was disqualified — rule breach, unsafe handling. A DQ ends their whole match, across every discipline.'
      }
    },
    {
      name: 'disqualifiedAt',
      type: 'date'
    },
    {
      name: 'reshootTimeMs',
      type: 'number',
      admin: {
        description: 'The one allowed reshoot, in milliseconds. Overwritable before sign-off; blank until taken.'
      }
    },
    {
      name: 'signedOffAt',
      type: 'date',
      admin: {
        description: 'Set manually by the timekeeper once the shooter has physically signed the paper card.'
      }
    }
  ],
  hooks: {
    afterChange: [
      async ({ doc, operation, req }) => {
        if (operation !== 'create') {
          return;
        }

        for (let roundNumber = 1; roundNumber <= ROUNDS_PER_MEMBERSHIP; roundNumber++) {
          await req.payload.create({
            collection: 'round-results',
            data: {
              membership: doc.id,
              roundNumber,
              status: 'pending'
            },
            req
          });
        }
      }
    ]
  }
};
