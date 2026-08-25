'use server';

import { getPayload } from 'payload';
import { cookies } from 'next/headers';
import { redirect } from 'next/navigation';
import config from '@/payload.config';

// Payload's default auth cookie: `${cookiePrefix}-token`, cookiePrefix
// defaults to 'payload' and isn't overridden in payload.config.ts.
const AUTH_COOKIE_NAME = 'payload-token';
const AUTH_COOKIE_MAX_AGE_SECONDS = 7200;

export async function loginAction(formData: FormData): Promise<void> {
  const email = String(formData.get('email') ?? '');
  const password = String(formData.get('password') ?? '');

  const payload = await getPayload({ config });

  let token: string | undefined;
  try {
    const result = await payload.login({ collection: 'users', data: { email, password } });
    token = result.token;
  } catch {
    redirect('/timekeeper/login?error=1');
  }

  if (!token) {
    redirect('/timekeeper/login?error=1');
  }

  const cookieStore = await cookies();
  cookieStore.set(AUTH_COOKIE_NAME, token, {
    httpOnly: true,
    path: '/',
    maxAge: AUTH_COOKIE_MAX_AGE_SECONDS,
    sameSite: 'lax',
    secure: process.env.NODE_ENV === 'production'
  });

  redirect('/timekeeper');
}

export async function logoutAction(): Promise<void> {
  const cookieStore = await cookies();
  cookieStore.delete(AUTH_COOKIE_NAME);
  redirect('/timekeeper/login');
}
