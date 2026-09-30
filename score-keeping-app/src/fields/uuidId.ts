import type { TextField } from 'payload';

/** Replaces Mongo's ObjectId: every document id in this app is a UUID. */
export const uuidIdField: TextField = {
  name: 'id',
  type: 'text',
  required: true,
  defaultValue: () => crypto.randomUUID(),
  admin: {
    hidden: true
  },
  hooks: {
    beforeValidate: [({ value }) => value || crypto.randomUUID()]
  }
};
