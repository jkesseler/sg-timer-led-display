import DisplayApp from '@/components/display/DisplayApp';
import { ReduxProvider } from '@/components/display/ReduxProvider';
import { getMqttConfig } from '@/lib/mqtt/config';

export default async function DisplayPage() {
  const mqttConfig = await getMqttConfig();

  return (
    <ReduxProvider mqttConfig={mqttConfig}>
      <DisplayApp />
    </ReduxProvider>
  );
}
