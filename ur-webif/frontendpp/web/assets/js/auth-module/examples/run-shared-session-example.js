/**
 * Runner script to demonstrate shared JWT session usage across multiple scripts
 * This script runs both Script 1 and Script 2 to show how they share the same authentication session
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

async function demonstrateSharedSessions() {
  console.log('=== Demonstrating Shared JWT Session Usage ===');
  console.log('This example shows how two scripts can share the same JWT authentication session.\n');

  try {
    // Run both scripts concurrently to demonstrate true sharing
    console.log('Running both scripts concurrently...\n');
    
    const script1Promise = runScript('shared-session-script1.js');
    const script2Promise = runScript('shared-session-script2.js');

    // Wait for both scripts to complete
    await Promise.all([script1Promise, script2Promise]);

    console.log('\n=== Both scripts completed successfully! ===');
    console.log('The shared JWT authentication session was used by both scripts simultaneously.');

  } catch (error) {
    console.error('Error running shared session demonstration:', error);
  }
}

// Alternative: Run scripts sequentially for clearer output
async function demonstrateSequentially() {
  console.log('=== Demonstrating Shared JWT Session Usage (Sequential) ===');
  console.log('This example shows how scripts can share the same JWT authentication session when run sequentially.\n');

  try {
    await runScript('shared-session-script1.js');
    await runScript('shared-session-script2.js');

    console.log('\n=== Sequential demonstration completed! ===');
    console.log('Both scripts used the same shared JWT authentication session.');

  } catch (error) {
    console.error('Error running sequential demonstration:', error);
  }
}

// Clean up any existing sessions before starting
async function cleanupSessions() {
  console.log('Cleaning up existing sessions...');
  try {
    const { JwtAuthManager } = await import('../src/JwtAuthManager.js');
    const cleanedUp = JwtAuthManager.cleanupExpiredSessions();
    const inactiveCleanedUp = JwtAuthManager.cleanupInactiveSessions(0);
    console.log(`Cleaned up ${cleanedUp.length + inactiveCleanedUp.length} sessions`);
  } catch (error) {
    console.log('Cleanup error (can be ignored):', error.message);
  }
}

// Choose demonstration mode based on command line arguments
const mode = process.argv[2];

async function main() {
  await cleanupSessions();

  if (mode === 'concurrent') {
    await demonstrateSharedSessions();
  } else if (mode === 'sequential') {
    await demonstrateSequentially();
  } else {
    console.log('Usage: node run-shared-session-example.js [concurrent|sequential]');
    console.log('Running sequential demonstration by default...\n');
    await demonstrateSequentially();
  }
}

// Execute demonstration
main().catch(console.error);
