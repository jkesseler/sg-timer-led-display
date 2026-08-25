import type { CollectionConfig } from 'payload'

export const Shooters: CollectionConfig = {
  slug: 'shooters',
  admin: {
    useAsTitle: 'lastName',
    defaultColumns: ['lastName', 'firstName', 'knsaNumber', 'asnNumber'],
  },
  fields: [
    {
      name: 'firstName',
      type: 'text',
      required: true,
    },
    {
      name: 'lastName',
      type: 'text',
      required: true,
    },
    {
      name: 'asnNumber',
      type: 'text',
      // Some shooters have no ASN membership number on record.
    },
    {
      name: 'knsaNumber',
      type: 'text',
      unique: true,
      // Optional: not every shooter has a barcode card to scan, in which
      // case they can only be selected manually by the timekeeper.
      admin: {
        description: 'Barcode scan lookup key. Leave blank if the shooter has no card.',
      },
    },
  ],
}
