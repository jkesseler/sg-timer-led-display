import { headers as getHeaders } from 'next/headers.js'
import { redirect } from 'next/navigation'
import { getPayload } from 'payload'
import config from '@/payload.config'
import { logoutAction } from '../login/actions'
import '../timekeeper.css'

export default async function TimekeeperLayout({ children }: { children: React.ReactNode }) {
  const headers = await getHeaders()
  const payload = await getPayload({ config })
  const { user } = await payload.auth({ headers })

  if (!user) {
    redirect('/timekeeper/login')
  }

  return (
    <div>
      <header className="tk-header">
        <span className="tk-header__title">Timekeeper</span>
        <form action={logoutAction} className="tk-header__account">
          <span className="tk-header__email">{user.email}</span>
          <button type="submit" className="tk-button tk-button--small">
            Log out
          </button>
        </form>
      </header>
      <main>{children}</main>
    </div>
  )
}
