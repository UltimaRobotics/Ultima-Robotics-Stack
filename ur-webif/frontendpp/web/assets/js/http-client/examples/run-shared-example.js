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
    
    const child = spawn('node', [join(__dirname, scriptName)], {
      stdio: 'inherit',
      shell: true
    });

    child.on('close', (code) => {
      if (code === 0) {
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

  try {
    // Run both scripts concurrently to demonstrate true sharing
    
    const script1Promise = runScript('shared-client-script1.js');
    const script2Promise = runScript('shared-client-script2.js');

    // Wait for both scripts to complete
    await Promise.all([script1Promise, script2Promise]);


  } catch (error) {
    console.error('Error running shared client demonstration:', error);
  }
}

// Alternative: Run scripts sequentially for clearer output
async function demonstrateSequentially() {

  try {
    await runScript('shared-client-script1.js');
    await runScript('shared-client-script2.js');


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
  demonstrateSequentially().catch(console.error);
}
