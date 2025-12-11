#!/usr/bin/env node
import { TimerSimulator } from './simulator.js';
import { DeviceModels } from './types.js';

// Color codes for terminal output
const colors = {
  reset: '\x1b[0m',
  bright: '\x1b[1m',
  green: '\x1b[32m',
  yellow: '\x1b[33m',
  blue: '\x1b[34m',
  cyan: '\x1b[36m'
};

/**
 * Print banner
 */
function printBanner() {
  console.log(colors.cyan + colors.bright);
  console.log('╔════════════════════════════════════════════╗');
  console.log('║   SG Timer MQTT Simulator                  ║');
  console.log('║   Shot Timer Event Publisher               ║');
  console.log('╚════════════════════════════════════════════╝');
  console.log(colors.reset);
}

/**
 * Print usage
 */
function printUsage() {
  console.log(`
${colors.bright}Usage:${colors.reset}
  bun run src/index.ts [options] [mode]

${colors.bright}Modes:${colors.reset}
  simple       Simple session with configured shots (default)
  competitive  Realistic competitive shooting stage simulation
  continuous   Run sessions continuously with delays

${colors.bright}Options:${colors.reset}
  --broker <url>      MQTT broker URL (default: ws://localhost:9001)
  --shots <number>    Number of shots per session (default: 10)
  --delay <seconds>   Start delay in seconds (default: 3.0)
  --interval <ms>     Time between shots in ms (default: 1500)
  --device <name>     Device name (default: "Simulated SG Timer")
  --model <model>     Device model (sg-sport, sg-go, special-pie, simulated)
  --help, -h          Show this help

${colors.bright}Examples:${colors.reset}
  ${colors.green}# Run simple simulation${colors.reset}
  bun run src/index.ts

  ${colors.green}# Competitive stage simulation${colors.reset}
  bun run src/index.ts competitive

  ${colors.green}# Custom configuration${colors.reset}
  bun run src/index.ts --shots 15 --delay 5 --interval 1000

  ${colors.green}# Different device${colors.reset}
  bun run src/index.ts --model sg-sport --device "SG Timer Sport #123"

  ${colors.green}# Continuous mode${colors.reset}
  bun run src/index.ts continuous
`);
}

/**
 * Parse command line arguments
 */
function parseArgs() {
  const args = process.argv.slice(2);

  const config = {
    brokerUrl: 'ws://localhost:9001',
    deviceName: 'Simulated SG Timer',
    deviceModel: DeviceModels.SIMULATED as string,
    startDelay: 3.0,
    shotCount: 10,
    shotInterval: 1500,
    splitTimeVariation: 200,
    autoReconnect: true
  };

  let mode = 'simple';

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];

    switch (arg) {
      case '--help':
      case '-h':
        printUsage();
        process.exit(0);
        break;

      case '--broker':
        config.brokerUrl = args[++i];
        break;

      case '--shots':
        config.shotCount = parseInt(args[++i]);
        break;

      case '--delay':
        config.startDelay = parseFloat(args[++i]);
        break;

      case '--interval':
        config.shotInterval = parseInt(args[++i]);
        break;

      case '--device':
        config.deviceName = args[++i];
        break;

      case '--model':
        const model = args[++i];
        switch (model.toLowerCase()) {
          case 'sg-sport':
            config.deviceModel = DeviceModels.SG_TIMER_SPORT;
            break;
          case 'sg-go':
            config.deviceModel = DeviceModels.SG_TIMER_GO;
            break;
          case 'special-pie':
            config.deviceModel = DeviceModels.SPECIAL_PIE_M1A2;
            break;
          case 'simulated':
            config.deviceModel = DeviceModels.SIMULATED;
            break;
          default:
            console.error(`Unknown model: ${model}`);
            process.exit(1);
        }
        break;

      default:
        if (!arg.startsWith('--')) {
          mode = arg;
        }
    }
  }

  return { config, mode };
}

/**
 * Main function
 */
async function main() {
  printBanner();

  const { config, mode } = parseArgs();
  const simulator = new TimerSimulator(config);

  console.log(`${colors.bright}Configuration:${colors.reset}`);
  console.log(`  Broker:   ${config.brokerUrl}`);
  console.log(`  Device:   ${config.deviceName}`);
  console.log(`  Model:    ${config.deviceModel}`);
  console.log(`  Mode:     ${mode}`);
  console.log();

  // Handle Ctrl+C gracefully
  process.on('SIGINT', async () => {
    console.log('\n\n⚠️  Received interrupt signal...');
    await simulator.disconnect();
    process.exit(0);
  });

  try {
    // Connect to broker
    await simulator.connect();
    await sleep(500);

    // Simulate device connection
    await simulator.simulateConnection();
    await sleep(2000);

    // Run based on mode
    switch (mode) {
      case 'simple':
        await simulator.simulateSession();
        break;

      case 'competitive':
        await simulator.simulateCompetitiveStage();
        break;

      case 'continuous':
        console.log(`${colors.yellow}Running in continuous mode. Press Ctrl+C to stop.${colors.reset}\n`);
        while (true) {
          await simulator.simulateCompetitiveStage();
          console.log('\n⏸️  Waiting 5 seconds before next session...\n');
          await sleep(5000);
        }
        break;

      default:
        console.error(`Unknown mode: ${mode}`);
        process.exit(1);
    }

    // Disconnect
    await sleep(2000);
    await simulator.disconnect();

    console.log(`${colors.green}${colors.bright}✅ Simulation complete!${colors.reset}\n`);
    process.exit(0);

  } catch (error) {
    console.error(`${colors.reset}\n❌ Error:`, error);
    await simulator.disconnect();
    process.exit(1);
  }
}

/**
 * Helper: sleep
 */
function sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

// Run main
main();
