import type { CollectionConfig } from 'payload'

export const Squads: CollectionConfig = {
  slug: 'squads',
  admin: {
    useAsTitle: 'label',
    defaultColumns: ['label', 'match', 'startTime', 'endTime', 'status'],
  },
  fields: [
    {
      name: 'label',
      type: 'text',
      admin: {
        description: 'Optional friendly name, e.g. "08:00 squad".',
      },
    },
    {
      name: 'match',
      type: 'relationship',
      relationTo: 'matches',
      required: true,
      admin: {
        description: 'The match this squad rotates through — its timer device comes from here, not from the squad.',
      },
    },
    {
      type: 'row',
      fields: [
        {
          name: 'startTime',
          type: 'text',
          required: true,
          admin: {
            description: 'e.g. "08:00"',
            width: '50%',
            components: {
              Field: '/fields/TimeInput#TimeInput',
            },
          },
          validate: (value: string | null | undefined) =>
            typeof value === 'string' && /^\d{2}:\d{2}$/.test(value) ? true : 'Enter a time as HH:MM.',
        },
        {
          name: 'endTime',
          type: 'text',
          required: true,
          admin: {
            description: 'e.g. "09:00"',
            width: '50%',
            components: {
              Field: '/fields/TimeInput#TimeInput',
            },
          },
          validate: (value: string | null | undefined) =>
            typeof value === 'string' && /^\d{2}:\d{2}$/.test(value) ? true : 'Enter a time as HH:MM.',
        },
      ],
    },
    {
      name: 'status',
      type: 'select',
      required: true,
      defaultValue: 'scheduled',
      options: [
        { label: 'Scheduled', value: 'scheduled' },
        { label: 'Active', value: 'active' },
        { label: 'Reshoot phase', value: 'reshoot-phase' },
        { label: 'Completed', value: 'completed' },
      ],
    },
    {
      name: 'memberships',
      type: 'join',
      collection: 'squad-memberships',
      on: 'squad',
    },
  ],
}
