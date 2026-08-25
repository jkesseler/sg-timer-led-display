import type { CollectionConfig } from 'payload'

export const Devices: CollectionConfig = {
  slug: 'devices',
  admin: {
    useAsTitle: 'label',
    defaultColumns: ['label', 'deviceId'],
  },
  fields: [
    {
      name: 'deviceId',
      type: 'text',
      required: true,
      unique: true,
      admin: {
        description: "The firmware's 6-character device ID, as published on timer/<deviceId>/... MQTT topics.",
      },
    },
    {
      name: 'label',
      type: 'text',
      required: true,
      admin: {
        description: 'Friendly name shown when the timekeeper picks a device, e.g. "Lane 3".',
      },
    },
  ],
}
