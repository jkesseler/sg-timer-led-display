import type { CollectionConfig } from 'payload'

export const Shooters: CollectionConfig = {
  slug: 'shooters',
  admin: {
    useAsTitle: 'displayName',
    defaultColumns: ['lastName', 'firstName', 'knsaNumber', 'asnNumber'],
    listSearchableFields: ['firstName', 'lastName', 'knsaNumber', 'asnNumber'],
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
      name: 'displayName',
      type: 'text',
      // Used as the relationship-select and title label ("Firstname Lastname - KNSA").
      // Persisted via beforeChange (not computed on read) because the
      // relationship-field option list fetches a narrowed `select` that omits
      // firstName/lastName/knsaNumber — an afterRead-only computation would see
      // no sibling data there and fall back to "Untitled".
      admin: { hidden: true },
      hooks: {
        beforeChange: [
          ({ siblingData }) => {
            const { firstName, lastName, knsaNumber } = siblingData as {
              firstName?: string
              lastName?: string
              knsaNumber?: string
            }
            const name = [firstName, lastName].filter(Boolean).join(' ')
            return knsaNumber ? `${name} - ${knsaNumber}` : name
          },
        ],
      },
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
