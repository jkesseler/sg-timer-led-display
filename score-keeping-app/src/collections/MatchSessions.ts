import type { CollectionConfig, Where } from 'payload'

const LIVE_STATUSES = ['pending', 'active'] as const

export const MatchSessions: CollectionConfig = {
  slug: 'match-sessions',
  admin: {
    useAsTitle: 'id',
    defaultColumns: ['device', 'status', 'timerSessionId', 'roundResult'],
  },
  fields: [
    {
      name: 'device',
      type: 'relationship',
      relationTo: 'devices',
      required: true,
    },
    {
      name: 'status',
      type: 'select',
      required: true,
      defaultValue: 'pending',
      options: [
        { label: 'Pending (awaiting timer start)', value: 'pending' },
        { label: 'Active', value: 'active' },
        { label: 'Completed', value: 'completed' },
        { label: 'Abandoned', value: 'abandoned' },
      ],
    },
    {
      name: 'timerSessionId',
      type: 'number',
      admin: {
        description: 'Stamped from session/started.sessionId once the range officer presses Start.',
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
      name: 'roundResult',
      type: 'relationship',
      relationTo: 'round-results',
      admin: {
        description:
          'The round-result this session is bound to — set at activation, the durable (shooter, discipline, round) binding. Exactly one of roundResult / reshootFor is set per session.',
      },
    },
    {
      name: 'reshootFor',
      type: 'relationship',
      relationTo: 'squad-memberships',
      admin: {
        description:
          "Set instead of roundResult when this session is a deferred reshoot: the result goes to the membership's reshootTimeMs field, never back into the RS-marked round.",
      },
    },
  ],
  hooks: {
    beforeChange: [
      async ({ data, originalDoc, req }) => {
        const nextStatus = data.status ?? originalDoc?.status
        if (!nextStatus || !LIVE_STATUSES.includes(nextStatus as (typeof LIVE_STATUSES)[number])) {
          return data
        }

        const deviceId =
          typeof data.device === 'object' && data.device !== null
            ? data.device.id
            : (data.device ?? originalDoc?.device)

        if (deviceId == null) {
          return data
        }

        // Server-side mirror of the "no active session" guard: at most one
        // pending/active match-session per device at a time.
        const conditions: Where[] = [
          { device: { equals: deviceId } },
          { status: { in: LIVE_STATUSES as unknown as string[] } },
        ]

        if (originalDoc?.id != null) {
          conditions.push({ id: { not_equals: originalDoc.id } })
        }

        const existingLive = await req.payload.find({
          collection: 'match-sessions',
          where: { and: conditions },
          limit: 1,
          req,
        })

        if (existingLive.docs.length > 0) {
          throw new Error('This device already has a pending or active match session.')
        }

        return data
      },
    ],
  },
}
