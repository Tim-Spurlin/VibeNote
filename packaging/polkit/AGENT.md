# AGENT.md

## Purpose
Polkit policy files granting the daemon permission to access screen-cast and system resources.

## Integration
Installed by setup scripts to `/usr/share/polkit-1/actions/` so the daemon can request PipeWire capture without root.
