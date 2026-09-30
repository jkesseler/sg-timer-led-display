import dotenv from 'dotenv'

// test.env first: tests must never write to the dev database from .env.
dotenv.config({ path: ['test.env', '.env'], quiet: true })
