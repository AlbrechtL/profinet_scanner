# Profinet Scanner

A CMake-based build of the original [Profinet scanner by Eiwanger](https://github.com/Eiwanger/profinet_scanner_prototype), updated to build with gcc on Windows through MSYS2 and on Linux through the usual system toolchain.

---

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Usage](#usage)
- [CLI Options](#cli-options)
- [Used PROFINET protocols](#used-profinet-protocols)
- [Build (Windows, MSYS2)](#build-windows-msys2)
- [Build (Linux)](#build-linux)
- [Build (OpenWrt APK)](#build-openwrt-apk)
- [Thanks](#thanks)

---

## Overview
This program scans for Profinet devices in a local subnet (Layer 2) or across a range of IP addresses (Layer 3). It sends Profinet DCP calls, listens for device responses, and performs additional RPC endpoint mapper requests for each discovered device. Results are printed as a human-readable summary to stdout.

The repository no longer uses Visual Studio project files. Supported builds are driven through CMake:

- Windows: MSYS2 UCRT64 gcc, CMake, Ninja, libpcap headers, and the Npcap runtime.
- Linux: system gcc/clang, CMake, and libpcap development headers.

The code migration and build cleanup were performed by AI (GitHub Copilot), based on the original implementation and documentation.

---

## Features
- Layer 2 scan: Profinet DCP call in local subnet
- Layer 3 scan: IP range scan with detailed device info
- Topology scan: PROFINET RPC peer-link resolution on the selected interface
- Automatic RPC endpoint mapper requests for discovered devices
- Human-readable scan summary output to stdout
- Extensive code comments for learning and reference

---

## Usage

Run the scanner from the build directory. Example paths:

```sh
# Linux
./build/SendPacket/pn_scanner --help

# Windows PowerShell
.\build-windows\SendPacket\pn_scanner.exe --help
```

The scanner is CLI-only. Running it without scan arguments prints the help text and exits.

- Show help:
  ```sh
  ./build/SendPacket/pn_scanner --help
  ```

On Windows, run the same commands against `pn_scanner.exe` in the selected build directory.

---

## CLI Options

```txt
--help
	Show help message and exit.

--interface <name>
  Select interface by name.

--mode <local|remote|topology>
  Select scan mode: local uses PROFINET DCP (Layer 2), remote uses DCE/RPC (Layer 3),
  and topology combines PROFINET DCP with RPC-based per-port peer data.

  Output behavior:
  - local prints DCP discovery output and the final device summary.
  - remote prints RPC discovery output and the final device summary.
  - topology suppresses DCP/RPC chatter and prints only the final topology summary.

--target <a.b.c.d[-e]>
	Remote target IP or range. Required when --mode remote is used.
	Examples: 192.168.0.10, 192.168.0.10-20

--duration <seconds>
  Stop the scan after the given number of seconds (applies to the overall run).
```

Examples:

```sh
# Local (DCP) scan
./build/SendPacket/pn_scanner --interface eth0 --mode local

# Topology scan on the selected Ethernet interface
doas ./build/SendPacket/pn_scanner --interface eth0 --mode topology --duration 60

# Real world example
doas ./build/SendPacket/pn_scanner --interface enp0s31f6 --mode local
Send pn_dcp 

listening on enp0s31f6 for pn_dcp...
23:29:49.003030 len:144  00:07:05:32:7e:aa
  DCP IP: 0.0.0.0
  DCP NameOfStation: testxasensorf28b
  DCP DeviceVendorValue: iTEMP TMT86
  DCP VendorID: 0x0011
  DCP DeviceID: 0xa3ff

# Remote scan (DCE/RPC)
./build/SendPacket/pn_scanner --interface eth0 --mode remote --target 192.168.0.10-20

# Real world example
./build/SendPacket/pn_scanner --interface eth0 --mode remote --target 192.168.1.110 --duration 2
Send RPC lookup endpointmapper first call 

listening on eth0 for IP/RPC...
10:47:08.420661 len:338  192.168.1.110

Scan duration reached; stopping early.

Scan results (stdout):

Device
  IP: 192.168.1.110
  MAC: 00:15:5d:67:b6:76
  Name: 
  Type: Switch series IE-SW-ALM
  Annotation: Switch series IE-SW-ALM   2682370000               1 V  1 37  0
  Order ID: 2682370000
  SW Version: V 1.37.0
  HW Revision: 1
  Vendor ID: 0x0000
  Device ID: 0x0000
  UDP Port: 34964

# Topology scan output
./build/SendPacket/pn_scanner --interface eth0 --mode topology
Topology results (stdout):

Topology source: PROFINET RPC peer data.

Topology chain:
  testxasensorf28b --[port-001 <-> port-009]-- ie-sw-al24m-16gt-8gesfp --[port-011 <-> port-001]-- desktop-sj9ndpi
```

---

## Used PROFINET protocols
- Profinet DCP (Layer 2)
- Ethernet
- IP
- UDP
- DCE/RPC (Layer 3)

---

## Build (Windows, MSYS2)

Install prerequisites:

1. Install MSYS2.
2. Open the `MSYS2 UCRT64` shell.
3. Install the required packages:

```sh
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-libpcap
```

4. Install Npcap on Windows.
     The Windows executable loads Npcap's `Packet.dll` and `wpcap.dll` at runtime to enumerate interfaces and capture packets.

Configure and build from the `MSYS2 UCRT64` shell:

```sh
cmake -S . -B build-windows -G Ninja
cmake --build build-windows -j
```

The resulting binary will be in `build-windows/SendPacket/pn_scanner.exe`.

Example commands:

```sh
./build-windows/SendPacket/pn_scanner.exe --help
./build-windows/SendPacket/pn_scanner.exe --interface <name> --mode local
```

Notes:

- Use the `MSYS2 UCRT64` shell for configure and build so the correct gcc, pkg-config, and runtime DLL paths are available.
- If the scanner reports an unknown interface name, confirm that Npcap is installed and that the machine has a usable wired capture interface for the scan.
- Windows binaries built this way depend on the MSYS2 UCRT runtime and on a local Npcap installation.

---

## Build (Linux)

Install prerequisites:

```sh
# Alpine Linux
doas apk add build-base cmake libpcap-dev

# Debian/Ubuntu
doas apt install libpcap-dev
```

Configure and build:

```sh
cmake -S . -B build        # configure the project and generate build files
cmake --build build -j     # build all targets using parallel jobs
```

The resulting binary will be in `build/SendPacket/pn_scanner`.

Topology note:

- Topology mode currently relies on PROFINET RPC peer data gathered during the follow-up RPC phases.
- The initial DCP pass discovers reachable devices, and the later RPC reads extract local port IDs, peer MAC addresses, peer chassis IDs, and peer port IDs where the device supports them.
- A topology link can only be shown if the device exposes this peer data through the PROFINET RPC reads.

---

## Build (OpenWrt APK)

This repository now contains a local OpenWrt feed package in `openwrt-feed/pn-scanner/`.
The intended workflow is to keep the package files here and let an OpenWrt buildroot
consume them through a linked local feed configured in the OpenWrt tree.

Example OpenWrt tree used during development:

```sh
~/src/openwrt
```

Create or edit `feeds.conf` in the OpenWrt tree and add a local linked feed:

```sh
cd ~/src/openwrt
cp -n feeds.conf.default feeds.conf
printf '\nsrc-link profinet ~/src/profinet_scanner_prototype/openwrt-feed\n' >> feeds.conf
```

Update the feed metadata and install the package into the OpenWrt package tree:

```sh
cd ~/src/openwrt
./scripts/feeds update profinet
./scripts/feeds install -f -p profinet pn-scanner
```

For local development builds from the current checkout, use the package-local
source override supported by `openwrt-feed/pn-scanner/Makefile`:

```sh
cd ~/src/openwrt
make package/feeds/profinet/pn-scanner/clean V=s
make package/feeds/profinet/pn-scanner/compile LOCAL_SOURCE_DIR=~/src/profinet_scanner_prototype V=s
```

This compiles the package from the current repository checkout instead of downloading
the pinned git source.

For normal OpenWrt package generation, enable `pn-scanner` in `make menuconfig`
or pass the package selection on the command line, then run the usual package build:

```sh
cd ~/src/openwrt
make menuconfig

# Network  ---> pn-scanner

make package/compile V=s
```

If the package is selected in the OpenWrt configuration and `CONFIG_USE_APK=y` is enabled,
OpenWrt will generate an `.apk` in the package output directories under `bin/packages/`.

Notes:

- The package definition is stored in `openwrt-feed/pn-scanner/Makefile`.
- `LOCAL_SOURCE_DIR=~/src/profinet_scanner_prototype` is only for local development.
  If it is omitted, OpenWrt uses the pinned upstream git source declared in the package Makefile.
- The package builds from a dedicated `openwrt-build/` subdirectory in this repository
  when `LOCAL_SOURCE_DIR` is active.
- If an earlier failed `USE_SOURCE_DIR` run polluted dependency build directories, clean the
  affected package in the OpenWrt tree before rebuilding.

---

## Disclaimer

This project is being used as a sandbox for experimenting with AI-assisted programming. The code migration, build system updates, and documentation have been performed with AI assistance (GitHub Copilot) to explore modern development workflows and tooling.

---

## Thanks

This project is based on the original Profinet scanner by Eiwanger:
https://github.com/Eiwanger/profinet_scanner_prototype

Special thanks to Eiwanger for the original implementation and documentation.
