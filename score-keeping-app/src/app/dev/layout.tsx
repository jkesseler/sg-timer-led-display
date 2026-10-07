// No shared root layout: each top-level segment provides its own <html>/<body>.
export default function DevRootLayout({ children }: LayoutProps<'/dev'>) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
