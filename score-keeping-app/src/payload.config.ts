import path from 'path';
import { fileURLToPath } from 'url';
import { postgresAdapter } from '@payloadcms/db-postgres';
import { lexicalEditor } from '@payloadcms/richtext-lexical';
import { buildConfig } from 'payload';
import { Users } from './collections/Users';
import { Shooters } from './collections/Shooters';
import { Devices } from './collections/Devices';
import { Matches } from './collections/Matches';
import { Squads } from './collections/Squads';
import { SquadMemberships } from './collections/SquadMemberships';
import { RoundResults } from './collections/RoundResults';
import { MatchSessions } from './collections/MatchSessions';

const filename = fileURLToPath(import.meta.url);
const dirname = path.dirname(filename);

export default buildConfig({
  admin: {
    user: Users.slug,
    importMap: {
      baseDir: path.resolve(dirname)
    }
  },
  collections: [Users, Shooters, Devices, Matches, Squads, SquadMemberships, RoundResults, MatchSessions],
  editor: lexicalEditor(),
  secret: process.env.PAYLOAD_SECRET || '',
  typescript: {
    outputFile: path.resolve(dirname, 'payload-types.ts')
  },
  db: postgresAdapter({
    pool: {
      connectionString: process.env.DATABASE_URL || ''
    }
  }),
  plugins: []
});
