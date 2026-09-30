import { DISCIPLINES } from '../lib/domain/disciplines';
import { uuidIdField } from '../fields/uuidId';
import type { CollectionConfig } from 'payload';

export const Squads: CollectionConfig = {
  slug: 'squads',
  admin: {
    useAsTitle: 'title',
    defaultColumns: ['label', 'match', 'discipline', 'startTime', 'endTime']
  },
  fields: [
    uuidIdField,
    {
      name: 'title',
      type: 'text',
      // Persisted, not computed on read: the relationship picker fetches only
      // the title field, so an afterRead fallback would see no times there.
      admin: { hidden: true },
      hooks: {
        beforeChange: [
          ({ siblingData }) => {
            const { label, startTime, endTime, discipline } = siblingData as {
              label?: string;
              startTime?: string;
              endTime?: string;
              discipline?: string;
            };
            const name = label || `${startTime} - ${endTime}`;

            return discipline ? `${name} · ${discipline}` : name;
          }
        ]
      }
    },
    {
      name: 'label',
      type: 'text',
      admin: {
        description: 'Optional friendly name, e.g. "08:00 squad". Defaults to the start–end time range when left blank.'
      },
      hooks: {
        afterRead: [
          ({ value, siblingData }) => {
            if (value) {
              return value;
            }
            const { startTime, endTime } = siblingData as { startTime?: string; endTime?: string };

            return startTime && endTime ? `${startTime} - ${endTime}` : value;
          }
        ]
      }
    },
    {
      name: 'match',
      type: 'relationship',
      relationTo: 'matches',
      required: true,
      admin: {
        description: 'The match this squad rotates through — its timer device comes from here, not from the squad.'
      }
    },
    {
      type: 'row',
      fields: [
        {
          name: 'startTime',
          type: 'text',
          required: true,
          admin: {
            description: 'e.g. "08:00"',
            width: '50%',
            components: {
              Field: '/fields/TimeInput#TimeInput'
            }
          },
          validate: (value: string | null | undefined) =>
            typeof value === 'string' && /^\d{2}:\d{2}$/.test(value) ? true : 'Enter a time as HH:MM.'
        },
        {
          name: 'endTime',
          type: 'text',
          required: true,
          admin: {
            description: 'e.g. "09:00"',
            width: '50%',
            components: {
              Field: '/fields/TimeInput#TimeInput'
            }
          },
          validate: (value: string | null | undefined) =>
            typeof value === 'string' && /^\d{2}:\d{2}$/.test(value) ? true : 'Enter a time as HH:MM.'
        }
      ]
    },
    {
      name: 'discipline',
      type: 'select',
      required: true,
      options: DISCIPLINES.map(discipline => ({ label: discipline, value: discipline }))
    },
    {
      name: 'members',
      type: 'join',
      collection: 'squad-members',
      on: 'squad'
    }
  ]
};
