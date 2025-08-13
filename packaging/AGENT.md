# AGENT.md

## Purpose
Packaging artifacts such as systemd units, polkit policies, and desktop entries.

## Sibling files
- **systemd/** – user services for the daemon and model server.
- **polkit/** – policies granting screen-cast and device access.

## Integration
Installed by distribution packages or `scripts/setup-arch.sh` to integrate VibeNote with the host system.
