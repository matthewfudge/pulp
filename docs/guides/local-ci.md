# Local CI

Local CI lets you validate branches on your Mac and cross-platform VMs before merging, without spending money on cloud CI minutes.

## Why local instead of cloud

Pulp has GitHub Actions workflows for CI, but running them on every branch costs money. Local CI is free and faster for iterative development — you get results in minutes from machines you already own or have running locally. Cloud CI remains available for release branches and public PRs where you want the full matrix on neutral hardware.

## How it works

When you run `pulp ci-local`, it:

1. Validates locally on Mac via `./validate-build.sh`
2. For each SSH target in `config.json`: SSHes in and runs `./validate-build.sh` in the configured repo path
3. If an SSH target is unreachable, it tries to start the corresponding UTM VM, waits for it to boot, then retries the SSH connection
4. Jobs queue on disk and drain on login or wake if you install the launchd agent

Mac validation always runs. SSH targets are skipped if disabled in config.

## Prerequisites

- [UTM](https://docs.getutm.app) — free VM manager for macOS (Apple Silicon and Intel)
- SSH key access to your VMs (password auth is not supported)
- The Pulp repo cloned on each VM at the path specified in `config.json`

UTM is the simplest option, but any SSH-reachable host works: Proxmox, a cloud VM (Azure/AWS/GCP), or a physical machine on your network. Cloud VMs cost money to run but are otherwise fully supported.

## Setup

### 1. Create your config

```bash
cp tools/local-ci/config.example.json tools/local-ci/config.json
```

Edit `config.json` and fill in your SSH hostnames and repo paths. The `utm_fallback` block is optional — remove it if you're not using UTM.

```json
{
  "targets": {
    "mac": {
      "type": "local",
      "enabled": true
    },
    "ubuntu": {
      "type": "ssh",
      "host": "ubuntu",
      "repo_path": "/home/yourname/Code/pulp-validate",
      "utm_fallback": {
        "vm_name": "Ubuntu 24.04",
        "boot_wait_secs": 30,
        "ssh_retry_secs": 60
      }
    },
    "windows": {
      "type": "ssh",
      "host": "win2",
      "repo_path": "C:\\Users\\yourname\\pulp-validate",
      "utm_fallback": {
        "vm_name": "Windows 11",
        "boot_wait_secs": 60,
        "ssh_retry_secs": 120
      }
    }
  }
}
```

SSH host aliases come from `~/.ssh/config`. Set them up there rather than putting raw IPs in this file.

### 2. Set up SSH keys

Each VM needs your public key in its `authorized_keys`. Standard procedure:

```bash
ssh-copy-id ubuntu    # or whatever your host alias is
ssh-copy-id win2
```

Test that passwordless login works before proceeding:

```bash
ssh ubuntu exit && echo "ok"
ssh win2 exit && echo "ok"
```

### 3. Clone the repo on each VM

The runner does a `git fetch` + checkout on the target, so the repo must already exist at the configured `repo_path`.

```bash
# On each VM:
git clone https://github.com/your-org/pulp.git ~/Code/pulp-validate
```

### 4. (Optional) Install the launchd drain agent

To automatically drain the queue on login and every 30 minutes:

```bash
cp tools/local-ci/dev.pulp.local-ci.plist ~/Library/LaunchAgents/
launchctl load ~/Library/LaunchAgents/dev.pulp.local-ci.plist
```

Edit the plist first if your repo is at a different path. To remove:

```bash
launchctl unload ~/Library/LaunchAgents/dev.pulp.local-ci.plist
rm ~/Library/LaunchAgents/dev.pulp.local-ci.plist
```

## Usage

```bash
# Enqueue the current branch and run immediately
pulp ci-local run

# Just enqueue (drain will happen later)
pulp ci-local enqueue

# Drain all pending jobs now
pulp ci-local drain

# Show queue, recent results, and VM status
pulp ci-local status
```

`pulp ci-local run` is the most common command — it enqueues the current branch and drains immediately, blocking until all targets finish.

Results are written to `tools/local-ci/results/` and summarized in the terminal. A non-zero exit means at least one target failed.

## Running Mac-only

If you don't have VMs set up, disable the SSH targets in `config.json`:

```json
"ubuntu": {
  "type": "ssh",
  "enabled": false,
  ...
}
```

Mac validation still runs. You get single-platform coverage, which is better than nothing for catching build breaks before pushing.

## For contributors

You don't need the same VM setup as the original developer. Options:

- **Mac-only**: Disable all SSH targets. Fast, free, covers the primary development platform.
- **UTM VMs**: Free. Requires ~40 GB of disk for both VMs. UTM images can be created from ISO or from the UTM gallery.
- **Cloud VMs**: Works with any SSH-accessible host. Costs money while running — stop them when not in use.
- **Physical machines**: A spare Linux box or Windows machine on your network works fine.

`config.json` is gitignored. Your host topology stays local.
