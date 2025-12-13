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

async function demonstrateSharedSessions() {

  try {
    // Run both scripts concurrently to demonstrate true sharing
    
    const script1Promise = runScript('shared-session-script1.js');
    const script2Promise = runScript('shared-session-script2.js');

    // Wait for both scripts to complete
    await Promise.all([script1Promise, script2Promise]);


  } catch (error) {
    console.error('Error running shared session demonstration:', error);
  }
}

// Alternative: Run scripts sequentially for clearer output
async function demonstrateSequentially() {

  try {
    await runScript('shared-session-script1.js');
    await runScript('shared-session-script2.js');


  } catch (error) {
    console.error('Error running sequential demonstration:', error);
  }
}

// Clean up any existing sessions before starting
async function cleanupSessions() {
  try {
    const { JwtAuthManager } = await import('../src/JwtAuthManager.js');
    const cleanedUp = JwtAuthManager.cleanupExpiredSessions();
    const inactiveCleanedUp = JwtAuthManager.cleanupInactiveSessions(0);
  } catch (error) {
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
    await demonstrateSequentially();
  }
}

// Execute demonstration
main().catch(console.error);
