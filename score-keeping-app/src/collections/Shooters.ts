import { uuidIdField } from '../fields/uuidId';
import type { ShooterNameFields } from './types';
import type { CollectionConfig } from 'payload';

export const Shooters: CollectionConfig = {
  slug: 'shooters',
  admin: {
    useAsTitle: 'displayName',
    defaultColumns: ['lastName', 'firstName', 'knsaNumber', 'asnNumber'],
    listSearchableFields: ['firstName', 'lastName', 'knsaNumber', 'asnNumber']
  },
  fields: [
    uuidIdField,
    {
      name: 'firstName',
      type: 'text',
      required: true
    },
    {
      name: 'lastName',
      type: 'text',
      required: true
    },
    {
      name: 'displayName',
      type: 'text',
      // Persisted, not computed on read: the relationship picker fetches only
      // the title field, so an afterRead fallback would see no names there.
      admin: { hidden: true },
      hooks: {
        beforeChange: [
          ({ siblingData }) => {
            const { firstName, lastName, knsaNumber } = siblingData as ShooterNameFields;
            const name = [firstName, lastName].filter(Boolean).join(' ');

            return knsaNumber ? `${name} - ${knsaNumber}` : name;
          }
        ]
      }
    },
    {
      name: 'asnNumber',
      type: 'text'
    },
    {
      name: 'knsaNumber',
      type: 'text',
      unique: true,
      admin: {
        description: 'Barcode scan lookup key. Leave blank if the shooter has no card.'
      }
    }
  ]
};
