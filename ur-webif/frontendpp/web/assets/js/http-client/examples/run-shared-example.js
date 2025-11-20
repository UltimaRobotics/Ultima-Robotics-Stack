/**
 * Runner script to demonstrate shared client usage across multiple scripts
 * This script runs both Script 1 and Script 2 to show how they share the same HTTP client
 */

import { spawn } from 'child_process';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

async function runScript(scriptName) {
  return new Promise((resolve, reject) => {
    console.log(`\n--- Starting ${scriptName} ---`);
    
    const child = spawn('node', [join(__dirname, scriptName)], {
      stdio: 'inherit',
      shell: true
    });

    child.on('close', (code) => {
      if (code === 0) {
        console.log(`--- ${scriptName} completed successfully ---`);
        resolve();
      } else {
        console.error(`--- ${scriptName} failed with code ${code} ---`);
        reject(new Error(`${scriptName} failed`));
      }
    });

    child.on('error', (error) => {
      console.error(`Failed to start ${scriptName}:`, error);
      reject(error);
    });
  });
}

async function demonstrateSharedClients() {
  console.log('=== Demonstrating Shared HTTP Client Usage ===');
  console.log('This example shows how two scripts can share the same HTTP client instance.\n');

  try {
    // Run both scripts concurrently to demonstrate true sharing
    console.log('Running both scripts concurrently...\n');
    
    const script1Promise = runScript('shared-client-script1.js');
    const script2Promise = runScript('shared-client-script2.js');

    // Wait for both scripts to complete
    await Promise.all([script1Promise, script2Promise]);

    console.log('\n=== Both scripts completed successfully! ===');
    console.log('The shared HTTP client was used by both scripts simultaneously.');

  } catch (error) {
    console.error('Error running shared client demonstration:', error);
  }
}

// Alternative: Run scripts sequentially for clearer output
async function demonstrateSequentially() {
  console.log('=== Demonstrating Shared HTTP Client Usage (Sequential) ===');
  console.log('This example shows how scripts can share the same HTTP client instance when run sequentially.\n');

  try {
    await runScript('shared-client-script1.js');
    await runScript('shared-client-script2.js');

    console.log('\n=== Sequential demonstration completed! ===');
    console.log('Both scripts used the same shared HTTP client instance.');

  } catch (error) {
    console.error('Error running sequential demonstration:', error);
  }
}

// Choose demonstration mode based on command line arguments
const mode = process.argv[2];

if (mode === 'concurrent') {
  demonstrateSharedClients().catch(console.error);
} else if (mode === 'sequential') {
  demonstrateSequentially().catch(console.error);
} else {
  console.log('Usage: node run-shared-example.js [concurrent|sequential]');
  console.log('Running sequential demonstration by default...\n');
  demonstrateSequentially().catch(console.error);
}
