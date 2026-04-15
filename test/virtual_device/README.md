# Virtual Device Scaffold

This directory contains the profipp-based test device used by the virtual integration stack.

The intended runtime model is one container per device instance. The same binary is configured at runtime via environment variables for station name, interface name, and storage directory.

Current status:

- The current container path builds profipp from source during the image build and installs a repo-local `profipp_virtual_device` binary.
- The device starts with a DAP plus one simple 1-byte input/output module in slot 1/subslot 1.
- The virtual compose stack runs three instances on an isolated bridge network with fixed MAC and IP addresses.
- The scanner-side pytest harness can load the matching profipp-backed profile from `test/profiles/virtual-profipp.json`.
- The lab test image is still Alpine-based, while the virtual-device image uses Debian so profipp and its bundled OSAL build cleanly.
- The virtual-device image keeps the current single-interface bridge-network model so the existing local and remote scanner assertions remain valid.
- The current profipp profile validates three-device local discovery and one-target remote RPC details; topology mode is still expected to report that no links could be resolved.

Current gaps:

- The new runtime currently uses profipp in a single-interface configuration, so topology mode is still conservative and expects no resolved links.
- The compose topology is still a simple bridge network and not yet a true multi-port interface model.
- The container image path still needs end-to-end validation in this repository with a successful Docker build and scanner run.