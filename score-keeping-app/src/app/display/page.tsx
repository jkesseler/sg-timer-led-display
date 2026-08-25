import { ReduxProvider } from '@/components/display/ReduxProvider';
import DisplayApp from '@/components/display/DisplayApp';

export default function DisplayPage() {
  return (
    <ReduxProvider>
      <DisplayApp />
    </ReduxProvider>
  );
}
