import path from 'path';
import { fileURLToPath } from 'url';
import { mongooseAdapter } from '@payloadcms/db-mongodb';
import { lexicalEditor } from '@payloadcms/richtext-lexical';
import { buildConfig } from 'payload';
import { Devices } from './collections/Devices';
import { MatchAudit } from './collections/MatchAudit';
import { Matches } from './collections/Matches';
import { MatchStates } from './collections/MatchStates';
import { Shooters } from './collections/Shooters';
import { SquadMembers } from './collections/SquadMembers';
import { Squads } from './collections/Squads';
import { Users } from './collections/Users';

const filename = fileURLToPath(import.meta.url);
const dirname = path.dirname(filename);

export default buildConfig({
  admin: {
    user: Users.slug,
    importMap: {
      baseDir: path.resolve(dirname)
    }
  },
  collections: [Users, Shooters, Devices, Matches, Squads, SquadMembers, MatchStates, MatchAudit],
  editor: lexicalEditor(),
  secret: process.env.PAYLOAD_SECRET || '',
  typescript: {
    outputFile: path.resolve(dirname, 'payload-types.ts')
  },
  // Standalone Mongo (no replica set) cannot run transactions. The unique
  // actionId index must exist before the first audit write to dedupe retries.
  db: mongooseAdapter({
    url: process.env.DATABASE_URI || '',
    transactionOptions: false,
    ensureIndexes: true
  }),
  plugins: []
});
