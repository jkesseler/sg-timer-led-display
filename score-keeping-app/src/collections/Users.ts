import { uuidIdField } from '../fields/uuidId';
import type { CollectionConfig } from 'payload';

export const Users: CollectionConfig = {
  slug: 'users',
  admin: {
    useAsTitle: 'email'
  },
  auth: true,
  fields: [
    uuidIdField,
    // Email added by default
    {
      name: 'role',
      type: 'select',
      required: true,
      defaultValue: 'timekeeper',
      options: [
        { label: 'Admin', value: 'admin' },
        { label: 'Timekeeper', value: 'timekeeper' }
      ]
    }
  ]
};
