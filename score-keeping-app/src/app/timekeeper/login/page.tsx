import { loginAction } from './actions'

export default async function TimekeeperLoginPage({
  searchParams,
}: {
  searchParams: Promise<{ error?: string }>
}) {
  const { error } = await searchParams

  return (
    <div style={{ maxWidth: '24rem', margin: '4rem auto', padding: '0 1rem' }}>
      <h1>Timekeeper login</h1>
      {error && <p style={{ color: 'crimson' }}>Invalid email or password.</p>}
      <form action={loginAction} style={{ display: 'flex', flexDirection: 'column', gap: '0.75rem' }}>
        <label>
          Email
          <input type="email" name="email" required style={{ display: 'block', width: '100%' }} />
        </label>
        <label>
          Password
          <input type="password" name="password" required style={{ display: 'block', width: '100%' }} />
        </label>
        <button type="submit">Log in</button>
      </form>
    </div>
  )
}
