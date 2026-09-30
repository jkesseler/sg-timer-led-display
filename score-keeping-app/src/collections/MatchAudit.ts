import { uuidIdField } from '../fields/uuidId';
import type { CollectionConfig } from 'payload';

/** Append-only log of every match change, written by the timekeeper's sync middleware. */
export const MatchAudit: CollectionConfig = {
  slug: 'match-audit',
  admin: {
    useAsTitle: 'type',
    defaultColumns: ['at', 'type', 'by', 'match']
  },
  access: {
    read: ({ req }) => Boolean(req.user),
    create: ({ req }) => Boolean(req.user),
    update: () => false,
    delete: () => false
  },
  hooks: {
    beforeChange: [
      ({ data, req }) => ({ ...data, by: req.user?.id ?? null })
    ]
  },
  fields: [
    uuidIdField,
    {
      name: 'actionId',
      type: 'text',
      required: true,
      unique: true,
      admin: {
        description: 'The Redux action id; makes retried writes idempotent.'
      }
    },
    {
      name: 'match',
      type: 'relationship',
      relationTo: 'matches',
      required: true,
      index: true
    },
    {
      name: 'at',
      type: 'date',
      required: true
    },
    {
      name: 'by',
      type: 'relationship',
      relationTo: 'users'
    },
    {
      name: 'type',
      type: 'text',
      required: true
    },
    {
      name: 'payload',
      type: 'json'
    }
  ]
};
