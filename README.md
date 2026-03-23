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
- Automatic RPC endpoint mapper requests for discovered devices
- Human-readable scan summary output to stdout
- Extensive code comments for learning and reference

---

## Usage

Run the scanner from the build directory. Example paths:

```sh
# Linux
./build/SendPacket/pn_scanner

# Windows PowerShell
.\build-windows\SendPacket\pn_scanner.exe
```

The program supports interactive mode (default) and non-interactive CLI mode.

- Interactive mode (default):
  ```sh
  ./build/SendPacket/pn_scanner
  ```
- Show help:
  ```sh
  ./build/SendPacket/pn_scanner --help
  ```
- List interfaces:
  ```sh
  ./build/SendPacket/pn_scanner --list-interfaces
  ```

On Windows, run the same commands against `pn_scanner.exe` in the selected build directory.

---

## CLI Options

```txt
--help
	Show help message and exit.

--list-interfaces
	Print available capture interfaces and exit.

--interface <index|name>
	Select interface by 1-based index (from --list-interfaces) or interface name.

--mode <local|remote>
    Select scan mode: local uses PROFINET DCP (Layer 2) and remote uses DCE/RPC (Layer 3).

--target <a.b.c.d[-e]>
	Remote target IP or range. Required when --mode remote is used.
	Examples: 192.168.0.10, 192.168.0.10-20

--duration <seconds>
  Stop the scan after the given number of seconds (applies to the overall run).

--interactive
	Force prompt-based mode (also the default when no parameters are provided).
```

Examples:

```sh
# Local (DCP) scan (non-interactive)
./build/SendPacket/pn_scanner --interface 1 --mode local

# Real world example
sudo ./build/SendPacket/pn_scanner --interface enp0s31f6 --mode local
Send pn_dcp 

listening on enp0s31f6 for pn_dcp...
23:29:49.003030 len:144  00:07:05:32:7e:aa
  DCP IP: 0.0.0.0
  DCP NameOfStation: testxasensorf28b
  DCP DeviceVendorValue: iTEMP TMT86
  DCP VendorID: 0x0011
  DCP DeviceID: 0xa3ff

# Remote scan (DCE/RPC) (non-interactive)
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
./build-windows/SendPacket/pn_scanner.exe --list-interfaces
```

Notes:

- Use the `MSYS2 UCRT64` shell for configure and build so the correct gcc, pkg-config, and runtime DLL paths are available.
- If `--list-interfaces` reports no suitable Ethernet interfaces, confirm that Npcap is installed and that the machine has a usable wired capture interface for the scan.
- Windows binaries built this way depend on the MSYS2 UCRT runtime and on a local Npcap installation.

---

## Build (Linux)

Install prerequisites:

```sh
sudo apt install libpcap-dev
```

Configure and build:

```sh
cmake -S . -B build        # configure the project and generate build files
cmake --build build -j     # build all targets using parallel jobs
```

The resulting binary will be in `build/SendPacket/pn_scanner`.

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

## Thanks

This project is based on the original Profinet scanner by Eiwanger:
https://github.com/Eiwanger/profinet_scanner_prototype

Special thanks to Eiwanger for the original implementation and documentation.
