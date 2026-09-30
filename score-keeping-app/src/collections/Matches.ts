import { uuidIdField } from '../fields/uuidId';
import type { CollectionConfig } from 'payload';

export const Matches: CollectionConfig = {
  slug: 'matches',
  admin: {
    useAsTitle: 'label',
    defaultColumns: ['label', 'date', 'device', 'active']
  },
  fields: [
    uuidIdField,
    {
      name: 'label',
      type: 'text',
      admin: {
        description: 'e.g. "Saturday match, 30 August".'
      }
    },
    {
      name: 'date',
      type: 'date',
      required: true,
      admin: {
        date: { pickerAppearance: 'dayOnly' }
      }
    },
    {
      name: 'device',
      type: 'relationship',
      relationTo: 'devices',
      required: true,
      admin: {
        description: 'The one timer used for every squad rotating through this match.'
      }
    },
    {
      name: 'active',
      type: 'checkbox',
      defaultValue: false,
      admin: {
        description: 'The timekeeper and /display work on the active match. If several are ticked, the newest date wins.'
      }
    },
    {
      name: 'squads',
      type: 'join',
      collection: 'squads',
      on: 'match'
    }
  ]
};
