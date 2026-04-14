## Plan: Fork-Backed Virtual Topology

Replace the released single-port p-net SDK path with a source build from the fork and make the repository’s custom virtual device the active runtime. Then give each virtual device a real Linux multi-port topology, let p-net expose real PDPort peer data over RPC, and only then flip the virtual test profile from “no links expected” to a concrete topology chain and fix any remaining failures.

**Steps**
1. Phase 1: Validate the fork as a source dependency. Clone the fork with submodules, confirm it exports the Linux CMake packages expected by `/home/albrecht/src/profinet_scanner/test/virtual_device/CMakeLists.txt`, and verify it can be built with `PNET_MAX_PHYSICAL_PORTS=2` and `PNET_MAX_SUBSLOTS>=4`. This blocks all later steps.
2. Phase 1: Decide the runtime base. Recommended path: build `/home/albrecht/src/profinet_scanner/test/virtual_device/pnet_virtual_device.c` against the fork instead of adapting the fork’s `pn_dev` sample, because the repo-specific env-driven station/IP/peer configuration already lives in the custom app while the current binary path ignores the topology env vars. This depends on step 1.
3. Phase 2: Replace the SDK-download image path. Update `/home/albrecht/src/profinet_scanner/test/docker/Dockerfile.virtual-device` so it no longer downloads the public x86_64 SDK zip or copies a prebuilt `pn_dev`. Instead, clone or mount the fork source, build the custom virtual device against it, and install that binary as the container entrypoint. This depends on step 2.
4. Phase 2: Keep `/home/albrecht/src/profinet_scanner/test/virtual_device/CMakeLists.txt` as the source-build entrypoint, but adjust it if the fork’s exported package names, CMake config locations, or required compile definitions differ from the currently assumed `cmake/PNetConfig.cmake` and `cmake/OsalConfig.cmake`. This can run in parallel with step 3 after step 2.
5. Phase 3: Redesign the Docker topology so each device has real multi-port Linux interfaces. Replace the current single shared bridge model in `/home/albrecht/src/profinet_scanner/test/docker/docker-compose.virtual.yml` with a per-link network model that gives each device distinct interfaces for each physical port plus a management bridge (`br0`). Add container bootstrap logic to create `br0`, enslave the physical interfaces, and make the interface names predictable. This depends on step 3.
6. Phase 3: Update the custom virtual device to use the real interface model. In `/home/albrecht/src/profinet_scanner/test/virtual_device/pnet_virtual_device.c`, remove the hard-coded `eth0` assumptions, stop binding both physical ports to the same interface, and pass distinct management/physical interfaces into `pnet_cfg.if_cfg`. Preserve the existing two-port DAP submodule setup and `num_physical_ports=2`. This depends on step 5.
7. Phase 3: Treat port naming pragmatically. The fork already defaults ports to names such as `port-001` and `port-002`, which matches the current test profile. Do not spend time on arbitrary custom port naming unless the scanner output proves it is needed. If the existing `DEVICE_PORT_NAME_*` env vars stay, either wire them into real port naming or remove them from the compose/profile to avoid dead configuration. This depends on step 6.
8. Phase 4: Make peer topology data real rather than synthetic. The scanner topology mode consumes PDPort peer data from PROFINET RPC responses, not the printed env vars. Ensure the fork-backed device plus the real interface topology causes RPC reads to return peer MAC, peer port ID, peer chassis/station, and own port ID as parsed by `getSubmodulPDRealData()` in `/home/albrecht/src/profinet_scanner/SendPacket/packetCapture.c` and printed by `printTopologyToStdout()` in `/home/albrecht/src/profinet_scanner/SendPacket/main.c`. This depends on step 6.
9. Phase 4: If real LLDP-driven PDPort data does not appear in containers, instrument before patching. First add targeted runtime logging around LLDP and PDPort state in the virtual device and compare it to the fields the scanner expects. Only if the fork plus real interfaces still does not expose peer data should the custom device be extended to persist or publish the minimum missing PDPort peer information. This depends on step 8.
10. Phase 5: Update the virtual test profile after the device returns real topology data. In `/home/albrecht/src/profinet_scanner/test/profiles/virtual-pnet.json`, flip `topology_links_expected` to true and set `topology_chain` to the actual chain produced by the scanner. Keep the current local and remote expectations unless the runtime behavior changes. This depends on step 8.
11. Phase 5: Keep `/home/albrecht/src/profinet_scanner/test/python/lab/test_modes.py` mostly unchanged. The branching already supports both “no links” and “links expected”; only adjust assertions if the final, verified scanner output differs from the current normalization logic or surrounding banner text. This depends on step 10.
12. Phase 6: Run and fix the test in dependency order. First build and start only the virtual-device services and confirm the containers expose `br0` and two distinct physical ports. Next run local mode and remote mode to ensure the fork migration did not regress discovery or RPC basics. Finally run topology mode and fix failures from the lowest broken layer upward: fork build, interface bootstrap, LLDP/PDPort exposure, profile expectations, then pytest assertions. This depends on step 11.
13. Phase 6: Update `/home/albrecht/src/profinet_scanner/test/virtual_device/README.md` with the fork source requirement, the multi-interface bridge model, the build/run commands, the expected compose topology, and any remaining virtual-lab limitations. This can run in parallel with step 12 once the implementation is stable.

**Relevant files**
- `/home/albrecht/src/profinet_scanner/test/docker/Dockerfile.virtual-device` — Replace the SDK zip download and `pn_dev` install path with a fork source build and custom runtime binary.
- `/home/albrecht/src/profinet_scanner/test/docker/docker-compose.virtual.yml` — Replace the single shared bridge layout with a real multi-interface chain and any required bootstrap/env wiring.
- `/home/albrecht/src/profinet_scanner/test/virtual_device/pnet_virtual_device.c` — Activate this as the runtime, replace hard-coded interface bindings, and verify the two-port DAP and PDPort behavior.
- `/home/albrecht/src/profinet_scanner/test/virtual_device/CMakeLists.txt` — Reuse as the source-build integration point for the fork, adjusting package discovery only if necessary.
- `/home/albrecht/src/profinet_scanner/test/virtual_device/README.md` — Document the new fork-backed workflow and topology model.
- `/home/albrecht/src/profinet_scanner/test/profiles/virtual-pnet.json` — Switch topology expectations from unresolved links to the verified device chain.
- `/home/albrecht/src/profinet_scanner/test/python/lab/test_modes.py` — Reuse the existing topology assertion branching and only adjust if actual output shape changes.
- `/home/albrecht/src/profinet_scanner/test/python/lab/conftest.py` — Reuse the current profile loading and scanner invocation path unless new profile keys or durations become necessary.
- `/home/albrecht/src/profinet_scanner/SendPacket/main.c` — Use `printTopologyToStdout()` and `printResolvedTopology()` as the output contract for the test profile.
- `/home/albrecht/src/profinet_scanner/SendPacket/packetCapture.c` — Use `getSubmodulPDRealData()` to verify exactly which PDPort peer fields must be returned by the device.
- `/home/albrecht/src/profinet_scanner/SendPacket/remoteScan.c` — Use the existing implicit-read request path when debugging topology RPC behavior.

**Verification**
1. Clone the fork with submodules and verify a clean p-net source build with `PNET_MAX_PHYSICAL_PORTS=2` and matching subslot settings.
2. Build the virtual-device image and start only the device services; verify in container logs and with `ip link` that each container exposes `br0` and distinct physical interfaces.
3. Confirm the device startup logs report two ports instead of the current single-port behavior.
4. Run the local-mode lab test and confirm all expected station names are still discovered.
5. Run the remote-mode lab test and confirm the target IP, type, and UDP port output still satisfy the existing assertions.
6. Run topology mode and confirm the output includes `Topology source: PROFINET RPC peer data.` and a resolved chain before changing the profile.
7. Run `pytest -c test/pytest.ini test/python/lab/test_modes.py -vv --lab-profile test/profiles/virtual-pnet.json` and iterate until all relevant tests pass.
8. If topology still fails, compare the returned PDPort fields against `/home/albrecht/src/profinet_scanner/SendPacket/packetCapture.c` and fix the device side before changing tests again.

**Decisions**
- Recommended approach: make `/home/albrecht/src/profinet_scanner/test/virtual_device/pnet_virtual_device.c` the active runtime instead of adapting the fork’s `pn_dev` sample.
- Recommended approach: use real interface/LLDP discovery to populate topology data rather than hard-coding scanner output or bypassing the RPC path.
- Included scope: fork integration, Docker-based virtual lab networking, topology RPC behavior, and the existing lab tests.
- Excluded scope: unrelated scanner feature work, non-virtual lab environments, and cosmetic output changes unless they directly block the test.

**Further Considerations**
1. Keep a fallback single-port path behind a separate compose file or build arg during the migration so local and remote smoke tests remain runnable while topology work is in progress.
2. Start by cloning the fork at image-build time to prove the path, then pin a commit SHA or vendor the source only after the fork-backed test flow is stable.
