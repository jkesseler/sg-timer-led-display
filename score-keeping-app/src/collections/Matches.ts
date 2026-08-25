import type { CollectionConfig } from 'payload'

export const Matches: CollectionConfig = {
  slug: 'matches',
  admin: {
    useAsTitle: 'label',
    defaultColumns: ['label', 'device'],
  },
  fields: [
    {
      name: 'label',
      type: 'text',
      admin: {
        description: 'e.g. "Saturday match, 30 August".',
      },
    },
    {
      name: 'device',
      type: 'relationship',
      relationTo: 'devices',
      required: true,
      admin: {
        description: 'The one timer used for every squad rotating through this match.',
      },
    },
    {
      name: 'squads',
      type: 'join',
      collection: 'squads',
      on: 'match',
    },
  ],
}
