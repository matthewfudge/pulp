---
name: ship
description: Sign, notarize, package, and distribute Pulp plugins and apps across macOS, Windows, and Android
triggers:
  - ship
  - sign
  - notarize
  - package
  - appcast
  - keystore
  - code signing
  - Play Store
  - release
  - distribute
  - installer
---

# Ship Skill — Signing, Packaging, and Distribution

## Overview

The `pulp ship` command handles the full distribution pipeline: code signing, Apple notarization, platform-specific packaging, update feed generation, and Android APK/AAB builds.

## Subcommands

| Command | Platform | What It Does |
|---------|----------|-------------|
| `sign` | macOS, Windows, Android | Sign plugin bundles or APK/AAB |
| `notarize` | macOS only | Submit to Apple notarization, poll, staple |
| `package` | All | Create .pkg (macOS), NSIS (Windows), APK+AAB (Android) |
| `appcast` | All | Generate Sparkle-compatible XML update feed |
| `check` | All | Verify signing status of built artifacts |

## Configuration

Settings are resolved in order: **CLI flag > environment variable > ~/.pulp/config.toml**

### Setup

```bash
pulp config init                    # Create config from template
pulp config set signing.apple.identity "Developer ID Application: Name (TEAMID)"
pulp config set signing.apple.team_id "ABCDE12345"
pulp config set signing.apple.apple_id "you@example.com"
pulp config set signing.android.keystore "~/keystores/release.jks"
```

### Config file location

`~/.pulp/config.toml` (or `$PULP_HOME/config.toml`)

See `config.example.toml` in the repo root for all options with documentation.

## Workflows

### macOS: Sign → Notarize → Package

```bash
pulp ship sign                                          # Uses identity from config
pulp ship notarize                                      # Uses apple_id/team_id from config
pulp ship package --version 1.0.0                       # Creates .pkg in artifacts/
pulp ship appcast --url https://example.com/Plugin.pkg --version 1.0.0
```

### Android: Package → Sign → Verify

```bash
pulp ship package --target android --keystore ~/key.jks # Build APK+AAB via Gradle
pulp ship sign --target android                         # Uses keystore from config
pulp ship check --target android                        # Verify APK/AAB signatures
```

### Windows: Sign → Package

```bash
pulp ship sign --identity "Your Company"                # Uses signtool
pulp ship package --version 1.0.0                       # Creates NSIS .exe installer
```

## Android-Specific

### ABI Selection

```bash
--abi arm64-v8a     # Default — ARM64 phones + tablets
--abi x86_64        # Emulator / Chromebook
--abi all           # arm64-v8a + x86_64 + armeabi-v7a
```

### Tablet Support

No special flags needed. ARM64 covers phones and tablets. AAB with split APKs handles screen density automatically.

### Keystore Creation

```bash
keytool -genkey -v -keystore release.jks -keyalg RSA -keysize 2048 -validity 10000
```

### Password Security

Never store passwords in plaintext config. Use environment variable references:
```toml
store_pass = "@env:ANDROID_STORE_PASS"
```

## Common Issues

### "No signing identity specified"

Run `security find-identity -v -p codesigning` (macOS) to find your identity, then:
```bash
pulp config set signing.apple.identity "Developer ID Application: ..."
```

### "Android SDK not found"

Install Android Studio or set `ANDROID_HOME`:
```bash
export ANDROID_HOME=~/Library/Android/sdk  # macOS
```

### "Notarization failed"

- Check that your app-specific password is valid (regenerate at https://appleid.apple.com)
- Ensure the bundle is properly signed with a Developer ID certificate (not just a development cert)
- Check the notarization log: `xcrun notarytool log <UUID>`

### "Gradle build failed"

- Run `pulp doctor` to check Android SDK/NDK/Java versions
- Ensure `android/` project exists (`pulp create --targets android`)
- Check Gradle output in the terminal for specific errors

## Doctor Checks

`pulp doctor` validates Android toolchain:
- Android SDK location
- NDK version (r26+ required for C++20)
- Java version (17+ required for AGP 8+)
- Build-tools availability (apksigner, zipalign)
