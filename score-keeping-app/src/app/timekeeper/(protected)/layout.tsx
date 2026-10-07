import '../timekeeper.css';

// The login check lives in /timekeeper/bootstrap: the page itself is client-only.
export default function TimekeeperLayout({ children }: LayoutProps<'/timekeeper'>) {
  return children;
}
