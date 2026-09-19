import 'dotenv/config'
import { Client } from 'pg'

/**
 * One-off: the squad-membership `status` field dropped its `scheduled` option
 * (there is no check-in step — everyone shoots by default). Any row still on
 * the old value is moved to `present` so Payload's schema sync can drop the
 * enum value. Raw SQL because Payload can't init while rows hold a value its
 * config no longer knows. Idempotent.
 */
async function main(): Promise<void> {
  const connectionString = process.env.DATABASE_URL
  if (!connectionString) {
    throw new Error('DATABASE_URL is not set')
  }

  const client = new Client({ connectionString })
  await client.connect()
  try {
    const result = await client.query(
      "UPDATE squad_memberships SET status = 'present' WHERE status = 'scheduled'",
    )
    console.log(`Moved ${result.rowCount ?? 0} membership(s) from "scheduled" to "present".`)
  } finally {
    await client.end()
  }
}

main().catch((error) => {
  console.error(error)
  process.exit(1)
})
