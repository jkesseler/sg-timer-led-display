import type { CollectionConfig, Where } from 'payload'

export const RoundResults: CollectionConfig = {
  slug: 'round-results',
  admin: {
    useAsTitle: 'id',
    defaultColumns: ['membership', 'roundNumber', 'status', 'timeMs'],
  },
  indexes: [{ fields: ['membership', 'roundNumber'], unique: true }],
  fields: [
    {
      name: 'membership',
      type: 'relationship',
      relationTo: 'squad-memberships',
      required: true,
    },
    {
      name: 'roundNumber',
      type: 'number',
      required: true,
      min: 1,
      max: 5,
    },
    {
      name: 'status',
      type: 'select',
      required: true,
      defaultValue: 'pending',
      options: [
        { label: 'Pending', value: 'pending' },
        { label: 'Timed', value: 'timed' },
        { label: 'Malfunction (RS)', value: 'rs' },
        { label: 'Skipped', value: 'skipped' },
      ],
    },
    {
      name: 'timeMs',
      type: 'number',
      admin: {
        description: 'Full-precision recorded time in milliseconds, from session/stopped.lastShotTimeMs.',
      },
    },
    {
      name: 'timerSessionId',
      type: 'number',
      admin: {
        description: 'The MQTT session/started.sessionId this round was bound to.',
      },
    },
    {
      name: 'startedAtMs',
      type: 'number',
    },
    {
      name: 'stoppedAtMs',
      type: 'number',
    },
    {
      name: 'device',
      type: 'relationship',
      relationTo: 'devices',
    },
  ],
  hooks: {
    beforeChange: [
      async ({ data, originalDoc, req }) => {
        if (data.status !== 'rs') {
          return data
        }

        const membershipId =
          typeof data.membership === 'object' && data.membership !== null
            ? data.membership.id
            : (data.membership ?? originalDoc?.membership)

        if (membershipId == null) {
          return data
        }

        // The card has a single Reshoot: field — at most one round per
        // membership may carry the permanent RS marker.
        const conditions: Where[] = [
          { membership: { equals: membershipId } },
          { status: { equals: 'rs' } },
        ]

        if (originalDoc?.id != null) {
          conditions.push({ id: { not_equals: originalDoc.id } })
        }

        const existingRs = await req.payload.find({
          collection: 'round-results',
          where: { and: conditions },
          limit: 1,
          req,
        })

        if (existingRs.docs.length > 0) {
          throw new Error(
            'This shooter already has an outstanding RS round — only one reshoot is allowed per card.',
          )
        }

        return data
      },
    ],
  },
}
