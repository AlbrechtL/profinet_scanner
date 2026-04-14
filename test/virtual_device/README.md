# Virtual Device Scaffold

This directory contains the P-Net based test device used by the virtual integration stack.

The intended runtime model is one container per device instance. The same binary is configured at runtime via environment variables for station name, IP address, MAC address, local port names, and expected peer links.

Current status:

- The current container path uses the public P-Net Linux SDK `pn_dev` binary directly so the image can stay `alpine:latest` only.
- The device starts with a DAP plus one simple 1-byte input/output module in slot 1/subslot 1.
- The virtual compose stack runs three instances on an isolated bridge network with fixed MAC and IP addresses.
- The scanner-side pytest harness can load the matching virtual profile from `test/profiles/virtual-pnet.json`.
- The device image downloads the public Linux x86_64 evaluation SDK from the `rtlabs-com/p-net` release assets during build.
- Both test container images are based on `alpine:latest`.
- The virtual-device image runs the glibc-linked SDK binary on Alpine via `gcompat` and ships no-op Linux helper scripts expected by the sample runtime.
- The current virtual profile validates three-device local discovery and one-target remote RPC details; topology mode is currently expected to report that no links could be resolved.

Current gaps:

- The public GitHub `rtlabs-com/p-net` repository is still evaluation-only and not usable as a Linux device source tree, so the container path depends on the released SDK package instead.
- The current image path is pinned to the public Linux x86_64 SDK release artifact and has not yet been generalized for other host architectures.
- The Alpine-only image uses the upstream sample binary, so the custom `pnet_virtual_device.c` code is no longer the active runtime path inside the container.
- The public Linux x86_64 SDK is built with `PNET_MAX_PHYSICAL_PORTS=1`, so the current container build path is limited to single-port runtime behaviour and does not yet satisfy the planned multi-port topology simulation.
- The topology peer chain env vars in the compose file are only consumed by the inactive custom `pnet_virtual_device.c` path; the active upstream `pn_dev` runtime does not emit the matching PDPort/LLDP peer data.
- The topology peer chain is therefore not yet driven by validated PDPort/LLDP data, so topology mode currently reports no resolved links instead of a three-device chain.
- The container image path still needs end-to-end validation in this repository with a successful Docker build and scanner run.
- The app currently focuses on DCP discovery and baseline RPC/device behaviour rather than full controller-driven cyclic IO test coverage.