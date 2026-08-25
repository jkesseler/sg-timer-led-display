import { headers as getHeaders } from 'next/headers.js'
import { redirect } from 'next/navigation'
import { getPayload } from 'payload'
import config from '@/payload.config'
import { logoutAction } from '../login/actions'

export default async function TimekeeperLayout({ children }: { children: React.ReactNode }) {
  const headers = await getHeaders()
  const payload = await getPayload({ config })
  const { user } = await payload.auth({ headers })

  if (!user) {
    redirect('/timekeeper/login')
  }

  return (
    <div>
      <header style={{ display: 'flex', justifyContent: 'space-between', padding: '0.75rem 1.5rem', borderBottom: '1px solid #ddd' }}>
        <strong>Timekeeper</strong>
        <form action={logoutAction}>
          <span style={{ marginRight: '1rem' }}>{user.email}</span>
          <button type="submit">Log out</button>
        </form>
      </header>
      <main>{children}</main>
    </div>
  )
}
