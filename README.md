# template
Template repository with common workflows for future clone

## Platform Support

The dependency bootstrap and L1 runner scripts currently support Ubuntu only.
On other platforms, they exit with:
`ERROR: Platform not supported. This script supports Ubuntu only.`

For local build and test steps, use an Ubuntu environment (native machine,
VM, container, or CI runner).

## Logging Configuration

OpenCDM provides runtime-configurable logging to aid in field debugging and development.

### Log Levels

The logging system supports four levels (in order of increasing verbosity):

| Level | String | Numeric | Description |
|-------|--------|---------|-------------|
| ERROR | `err` | 1 | Critical failures and errors only (default: ERROR only) |
| WARNING | `warn` | 2 | **Default**: Errors + warnings useful for field debugging (session creation, failures, setup events) |
| DEBUG | `debug` | 3 | Errors + warnings + debug logs (validation checks, routine operations) |
| INFO | `info` | 4 | Errors + warnings + debug + info logs (all messages) |

**Example log output:**
```
[OpenCDM][err] Failed to get type from SVP header
[OpenCDM][warn] Created a Session, result 0x7f1a2b3c, 0
[OpenCDM][warn] Initialized SVP context for server side ID = 1A2B
[OpenCDM][debug] Secure Memory Preallocation disabled, Setting TokenType to InPlace
[OpenCDM][debug] opencdm_gstreamer_session_decrypt_buffer: Subsample mapping size mismatch
```

### Enabling Logs via Configuration File

Create or edit the configuration file on your device:

**File location:** `/opt/rdk_adapter_logging.conf`

#### Option 1: Set using string names

```bash
# Enable WARNING (default) + ERROR logs only
echo "level=warning" > /opt/rdk_adapter_logging.conf

# Enable DEBUG logs (WARNING + ERROR + DEBUG)
echo "level=debug" > /opt/rdk_adapter_logging.conf

# Enable all logs (WARNING + ERROR + DEBUG + INFO)
echo "level=info" > /opt/rdk_adapter_logging.conf

# Enable ERROR logs only
echo "level=error" > /opt/rdk_adapter_logging.conf
```

#### Option 2: Set using numeric values

```bash
# Enable ERROR only (1)
echo "level=1" > /opt/rdk_adapter_logging.conf

# Enable WARNING + ERROR (2) - DEFAULT
echo "level=2" > /opt/rdk_adapter_logging.conf

# Enable DEBUG + WARNING + ERROR (3)
echo "level=3" > /opt/rdk_adapter_logging.conf

# Enable all logs (4)
echo "level=4" > /opt/rdk_adapter_logging.conf
```

#### Reverting to Default

If the configuration file doesn't exist, the default level (WARNING, level=2) is used:

```bash
# Remove config file to revert to default
rm /opt/rdk_adapter_logging.conf
```

### Enabling Logs Programmatically (Runtime)

If you need to enable/change logging at runtime without restarting:

```cpp
#include "helpers/OpenCDMLogger.h"

// Enable DEBUG logs
OpenCDM::Logging::SetLogLevel(OpenCDM::Logging::DEBUG);  // or SetLogLevel(3)

// Enable WARNING logs (default)
OpenCDM::Logging::SetLogLevel(OpenCDM::Logging::WARNING);  // or SetLogLevel(2)

// Get current log level
int currentLevel = OpenCDM::Logging::GetLogLevel();
```

### Enabling Warning and Debug Together

To see both WARNING (debugging-useful status) and DEBUG (routine operations) logs:

```bash
# Via config file
echo "level=3" > /opt/rdk_adapter_logging.conf
echo "level=debug" > /opt/rdk_adapter_logging.conf
```

You will see all three log types: ERROR, WARNING, and DEBUG.

### Performance Considerations

**Default configuration (WARNING, level=2):**
- Minimal performance impact
- Shows critical errors and important status messages
- Recommended for production deployments

**With DEBUG enabled (level=3):**
- Increased stderr I/O (logs printed instead of filtered)
- Acceptable for field debugging and development
- **Not recommended for long-term production use**

**With INFO enabled (level=4):**
- Maximum verbosity and highest I/O overhead
- Use only for comprehensive debugging
- **Not suitable for production**

### Workflow: Debugging Issues in Field

1. **Enable DEBUG logs** to collect additional diagnostic information:
   ```bash
   echo "level=debug" > /opt/rdk_adapter_logging.conf
   systemctl restart opencdmi  # or applicable service
   ```

2. **Reproduce the issue** while collecting logs (output to stderr/console)

3. **Analyze the logs** (search for `[err]`, `[warn]`, `[debug]`)

4. **Revert to default** when debugging is complete:
   ```bash
   rm /opt/rdk_adapter_logging.conf
   systemctl restart opencdmi
   ```
