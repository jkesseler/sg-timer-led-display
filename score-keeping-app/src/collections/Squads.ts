import type { CollectionConfig } from 'payload'

export const Squads: CollectionConfig = {
  slug: 'squads',
  admin: {
    useAsTitle: 'label',
    defaultColumns: ['label', 'startTime', 'endTime', 'status', 'device'],
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
      name: 'startTime',
      type: 'date',
      required: true,
    },
    {
      name: 'endTime',
      type: 'date',
      required: true,
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
      name: 'device',
      type: 'relationship',
      relationTo: 'devices',
      admin: {
        description: 'The timer device bound to this squad, chosen by the timekeeper when the squad starts.',
      },
    },
  ],
}
