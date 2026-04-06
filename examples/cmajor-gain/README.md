# Cmajor Gain

This is a **source-only** example for Pulp's MIT-safe Cmajor support lane.

What it is:

- a tiny `.cmajor` / `.cmajorpatch` pair
- suitable for validating Pulp's external-toolchain workflow
- intentionally not built by Pulp's normal CMake examples list

What it is not:

- a checked-in generated Cmajor artifact
- proof that Pulp ships the Cmajor runtime
- a finished `Processor` adapter in Pulp core

## Validate The Layout

```bash
python3 tools/scripts/cmajor_external.py doctor \
  --patch examples/cmajor-gain/CmajorGain.cmajorpatch
```

## Generate With Your Own `cmaj`

```bash
python3 tools/scripts/cmajor_external.py generate \
  --patch examples/cmajor-gain/CmajorGain.cmajorpatch \
  --target cpp \
  --output /tmp/CmajorGain.cpp
```

For other upstream targets:

```bash
python3 tools/scripts/cmajor_external.py generate \
  --patch examples/cmajor-gain/CmajorGain.cmajorpatch \
  --target clap \
  --output /tmp/CmajorGainClap \
  --arg --clapIncludePath=/path/to/clap/include
```
