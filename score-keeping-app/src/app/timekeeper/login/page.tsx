import { loginAction } from './actions'
import '../timekeeper.css'

export default async function TimekeeperLoginPage({
  searchParams,
}: {
  searchParams: Promise<{ error?: string }>
}) {
  const { error } = await searchParams

  return (
    <div className="tk-login">
      <div className="tk-login-card">
        <span className="tk-login-card__title">Timekeeper login</span>
        {error && <p className="tk-login-error">Invalid email or password.</p>}
        <form action={loginAction} style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
          <div className="tk-field">
            <label htmlFor="email">Email</label>
            <input type="email" id="email" name="email" required />
          </div>
          <div className="tk-field">
            <label htmlFor="password">Password</label>
            <input type="password" id="password" name="password" required />
          </div>
          <button type="submit" className="tk-button tk-button--primary">
            Log in
          </button>
        </form>
      </div>
    </div>
  )
}
