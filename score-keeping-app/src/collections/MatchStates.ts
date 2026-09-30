import { uuidIdField } from '../fields/uuidId';
import type { CollectionConfig } from 'payload';

/** Backup of the timekeeper browser's Redux match state — the browser is leading, this copy is only read when it has none. */
export const MatchStates: CollectionConfig = {
  slug: 'match-states',
  admin: {
    useAsTitle: 'id',
    defaultColumns: ['match', 'revision', 'updatedAt']
  },
  fields: [
    uuidIdField,
    {
      name: 'match',
      type: 'relationship',
      relationTo: 'matches',
      required: true,
      unique: true
    },
    {
      name: 'revision',
      type: 'number',
      required: true,
      defaultValue: 0
    },
    {
      name: 'state',
      type: 'json',
      required: true
    }
  ]
};
