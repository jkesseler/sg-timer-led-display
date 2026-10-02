import { DISCIPLINES } from '../lib/domain/disciplines';
import { uuidIdField } from '../fields/uuidId';
import type { CollectionConfig } from 'payload';

export const SquadMembers: CollectionConfig = {
  slug: 'squad-members',
  admin: {
    useAsTitle: 'id',
    defaultColumns: ['squad', 'shooter', 'startingPosition', 'discipline']
  },
  fields: [
    uuidIdField,
    {
      name: 'squad',
      type: 'relationship',
      relationTo: 'squads',
      required: true
    },
    {
      name: 'shooter',
      type: 'relationship',
      relationTo: 'shooters',
      required: true
    },
    {
      name: 'startingPosition',
      type: 'number',
      required: true,
      admin: {
        description: 'Position number from the printed schedule — the starting order only.'
      }
    },
    {
      name: 'discipline',
      type: 'select',
      required: true,
      // Per member, not per squad: one squad mixes disciplines (the schedule's "Klasse" column).
      options: DISCIPLINES.map(discipline => ({ label: discipline, value: discipline }))
    }
  ]
};
