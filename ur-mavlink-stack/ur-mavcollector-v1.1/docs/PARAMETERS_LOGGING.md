
# Parameters Logging

## Overview
The MAVLink Collector can log all collected parameters to a JSON file when the application stops. This feature is controlled by the `mav_stats_counter` configuration field.

## Configuration

### Enable/Disable
Set `mav_stats_counter` to `true` in `config.json` to enable parameter logging:

```json
{
  "mav_stats_counter": true,
  "params_log_file": "runtime-data/params_collected.json"
}
```

### Output File
The collected parameters are saved to the file specified by `params_log_file` (default: `runtime-data/params_collected.json`).

## Output Format

The parameters log file contains:
- **total_parameters_collected**: Number of unique parameters collected
- **collection_timestamp**: Unix timestamp when the file was saved
- **parameters**: Array of parameter objects

Each parameter object includes:
- **name**: Parameter name (e.g., "ARMING_CHECK", "WPNAV_SPEED")
- **value**: Parameter value (float)
- **type**: MAVLink parameter type (uint8_t)
- **timestamp**: Unix timestamp when parameter was received
- **timestamp_readable**: Human-readable timestamp (YYYY-MM-DD HH:MM:SS)

## Example Output

```json
{
  "total_parameters_collected": 245,
  "collection_timestamp": 1704067200,
  "parameters": [
    {
      "name": "ARMING_CHECK",
      "value": 1.0,
      "type": 2,
      "timestamp": 1704067150,
      "timestamp_readable": "2024-01-01 12:00:50"
    },
    {
      "name": "WPNAV_SPEED",
      "value": 500.0,
      "type": 9,
      "timestamp": 1704067151,
      "timestamp_readable": "2024-01-01 12:00:51"
    }
  ]
}
```

## Behavior

1. **During Operation**: As PARAM_VALUE messages are received, parameters are stored in memory (only when `mav_stats_counter` is enabled)
2. **On Stop**: When the collector stops (Ctrl+C or normal shutdown), all collected parameters are written to the log file
3. **Duplicate Handling**: If the same parameter is received multiple times, only the most recent value and timestamp are kept

## Use Cases

- **Parameter Backup**: Save all vehicle parameters for documentation or restoration
- **Configuration Audit**: Track what parameters were set during a mission
- **Debugging**: Review parameter values that were active during flight
- **Comparison**: Compare parameters across different sessions

## Notes

- Parameters are only logged if `mav_stats_counter` is `true`
- The log file is only written when the application stops gracefully
- Each parameter appears only once (most recent value)
- The file is overwritten on each run (not appended)
