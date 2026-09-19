import type { CollectionConfig } from 'payload';

export const Matches: CollectionConfig = {
  slug: 'matches',
  admin: {
    useAsTitle: 'label',
    defaultColumns: ['label', 'device', 'currentSquad']
  },
  fields: [
    {
      name: 'label',
      type: 'text',
      admin: {
        description: 'e.g. "Saturday match, 30 August".'
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
      name: 'currentSquad',
      type: 'relationship',
      relationTo: 'squads',
      admin: {
        description: 'The squad currently on the range for this match\'s timer. Set from the timekeeper\'s squad bar; /display reads it to choose whose roster to show. At most one squad per match is current.'
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
