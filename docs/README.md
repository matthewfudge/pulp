# Pulp Documentation

This directory is the source of truth for Pulp developer documentation on this branch.

## How It Works

- **Human-readable docs** live in Markdown files under `concepts/`, `guides/`, `reference/`, and `policies/`.
- **Machine-readable status** lives in YAML manifests under `status/`.
- The `pulp docs` CLI commands read these local files directly.
- If a hosted docs site is published later, it mirrors these files. This directory is authoritative.

## Layout

```
docs/
  concepts/         High-level explanations of what Pulp is and how it works
  guides/           Step-by-step instructions for common tasks
  reference/        Detailed reference for CLI, CMake, and modules
  policies/         Code style and contribution rules
  status/           YAML manifests for support levels, module status, CLI commands
```

## Maturity Vocabulary

All status fields use this fixed vocabulary:

| Term           | Meaning |
|----------------|---------|
| `stable`       | Tested, documented, unlikely to change in breaking ways |
| `usable`       | Works for its intended purpose, may have rough edges |
| `experimental` | Under active development, API may change |
| `partial`      | Some functionality implemented, gaps remain |
| `planned`      | Designed but not yet implemented |
| `unsupported`  | Not available on this platform or configuration |

## For Agents

An agent with access to this checkout can answer most questions about Pulp without web calls by reading:

- `docs/status/support-matrix.yaml` for platform and format support
- `docs/status/modules.yaml` for subsystem status
- `docs/status/cli-commands.yaml` for available CLI commands
- `docs/status/cmake-functions.yaml` for build system functions
- `docs/status/style-rules.yaml` for contribution rules

Use `docs/status/docs-index.yaml` as the table of contents.
