# WiFi Configuration System

This document explains how the environment-based WiFi configuration system works in the DensePose-ESP32 project.

## Overview

The WiFi configuration system allows you to securely store WiFi credentials outside of version control while making them easy to use during development. It uses a `.env` file for local credentials and ESP-IDF's Kconfig system to inject them into the firmware at build time.

## How It Works

### 1. Environment File (.env)

Create a `.env` file in the project root with your WiFi credentials:

```bash
WIFI_SSID=YourNetworkName
WIFI_PASSWORD=YourNetworkPassword
```

This file is:
- **Git-ignored:** Never committed to version control (listed in `.gitignore`)
- **Local only:** Each developer has their own `.env` with their network credentials
- **Simple format:** Plain `KEY=VALUE` pairs

### 2. Build-Time Integration

When you build the firmware, the system works as follows:

```
.env file → Build script → Environment variables → Kconfig → sdkconfig → Compiled firmware
```

**Step-by-step:**

1. **Build script** (`build.sh`) reads `.env` and exports variables:
   ```bash
   export WIFI_SSID=YourNetworkName
   export WIFI_PASSWORD=YourNetworkPassword
   ```

2. **Kconfig** (`firmware/main/Kconfig.projbuild`) uses these environment variables as defaults:
   ```kconfig
   config WIFI_SSID
       string "WiFi SSID"
       default "$WIFI_SSID" if WIFI_SSID != ""
       default "YOUR_WIFI_SSID"
   ```

3. **ESP-IDF** generates `sdkconfig` with the values from Kconfig

4. **Compiler** accesses these as `CONFIG_WIFI_SSID` and `CONFIG_WIFI_PASSWORD` macros

5. **Firmware** uses the macros in `main.c`:
   ```c
   #define WIFI_SSID CONFIG_WIFI_SSID
   #define WIFI_PASSWORD CONFIG_WIFI_PASSWORD
   ```

### 3. Three Ways to Configure

You have three options for setting WiFi credentials, in order of priority:

#### Option A: .env file (Recommended for development)

```bash
# Create .env from template
cp .env.example .env

# Edit with your credentials
nano .env

# Build (automatically loads .env)
./build.sh
```

**Pros:**
- Simple and quick
- Credentials never in git
- Easy to switch networks (just edit .env)
- No manual menuconfig needed

**Cons:**
- Need to remember to create .env file
- Different .env per developer

#### Option B: menuconfig (Manual configuration)

```bash
cd firmware
idf.py menuconfig
# Navigate to: DensePose WiFi Configuration
# Set WIFI_SSID and WIFI_PASSWORD
idf.py build
```

**Pros:**
- Don't need .env file
- Can set all ESP-IDF options at once
- Visual configuration interface

**Cons:**
- More steps to change credentials
- sdkconfig might be committed accidentally

#### Option C: Environment variables (CI/CD)

```bash
export WIFI_SSID=MyNetwork
export WIFI_PASSWORD=MyPassword
cd firmware
idf.py build
```

**Pros:**
- Good for CI/CD pipelines
- Scriptable
- No files to manage

**Cons:**
- Manual export each session
- Variables visible in shell history

## File Reference

### .env.example
Template file showing the required format. Safe to commit to git.

```bash
WIFI_SSID=YourNetworkName
WIFI_PASSWORD=YourNetworkPassword
```

### .env
Your actual credentials. **Never commit this file!**

### .gitignore
Ensures `.env` is never committed:

```gitignore
# Environment variables (contains sensitive WiFi credentials)
.env
```

### build.sh
Build script that loads `.env` before building:

```bash
#!/bin/bash
# Load environment variables from .env
if [ -f .env ]; then
    while IFS='=' read -r key value; do
        export "$key=$value"
    done < .env
fi

cd firmware
idf.py build
```

### firmware/main/Kconfig.projbuild
Kconfig configuration that uses environment variables:

```kconfig
config WIFI_SSID
    string "WiFi SSID"
    default "$WIFI_SSID" if WIFI_SSID != ""
    default "YOUR_WIFI_SSID"
```

### firmware/main/main.c
C code that uses the configuration:

```c
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD

wifi_config_t wifi_config = {
    .sta = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    },
};
```

## Security Considerations

### What's Protected

- **WiFi credentials** are never in version control
- **Password** is masked in build output (`***`)
- **.env** is in `.gitignore` by default

### What to Watch For

1. **Don't commit sdkconfig:** It contains your credentials after building. Already in `.gitignore`.

2. **Check before pushing:** Always verify `.gitignore` is working:
   ```bash
   git status  # Should NOT show .env
   ```

3. **CI/CD credentials:** Use secrets management (GitHub Secrets, GitLab CI Variables, etc.) instead of committing .env

4. **Shared devices:** If multiple people use the same development machine, be aware .env contains your network password in plain text.

## Troubleshooting

### "WiFi credentials not found"

**Symptom:** Build succeeds but firmware uses default "YOUR_WIFI_SSID"

**Solution:**
1. Check `.env` exists: `ls -la .env`
2. Verify format: `cat .env`
3. Use build script: `./build.sh` (not `idf.py build` directly)

### "Build fails with permission denied"

**Symptom:** `bash: ./build.sh: Permission denied`

**Solution:**
```bash
chmod +x build.sh
```

### "WiFi connects to wrong network"

**Symptom:** Connects to old network even after changing .env

**Solution:**
1. Clean build: `cd firmware && idf.py fullclean`
2. Rebuild: `cd .. && ./build.sh`

### "Variable not found in environment"

**Symptom:** Kconfig can't find WIFI_SSID environment variable

**Solution:**
Use the build script instead of calling idf.py directly:
```bash
# DON'T: cd firmware && idf.py build
# DO: ./build.sh
```

## For Different Environments

### Home Network
```bash
# .env
WIFI_SSID=HomeNetwork
WIFI_PASSWORD=home123
```

### Office Network
```bash
# .env
WIFI_SSID=OfficeWiFi
WIFI_PASSWORD=office456
```

### Testing/Lab Network
```bash
# .env
WIFI_SSID=Lab5G
WIFI_PASSWORD=lab789
```

Just keep different `.env` files (like `.env.home`, `.env.office`) and copy the one you need:
```bash
cp .env.home .env
./build.sh
```

## Advanced: Multiple Configurations

If you work with multiple networks frequently:

```bash
# Create named configs
cp .env.example .env.home
cp .env.example .env.office
cp .env.example .env.lab

# Edit each one
nano .env.home
nano .env.office
nano .env.lab

# Use the one you need
ln -sf .env.home .env
./build.sh

# Switch to office
ln -sf .env.office .env
./build.sh
```

Or use a helper script:

```bash
#!/bin/bash
# switch-wifi.sh
if [ -z "$1" ]; then
    echo "Usage: ./switch-wifi.sh [home|office|lab]"
    exit 1
fi

cp .env.$1 .env
echo "Switched to $1 network configuration"
./build.sh
```

## Summary

The WiFi configuration system:
- ✅ Keeps credentials out of git
- ✅ Makes it easy to change networks
- ✅ Works with ESP-IDF's native Kconfig
- ✅ Supports multiple configuration methods
- ✅ Provides good security defaults

Always use `.env` for local development, and never commit credentials to version control!
