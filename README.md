# Profinet Scanner (Linux Port)

A port of the original Windows-only [Profinet scanner by Eiwanger](https://github.com/Eiwanger/profinet_scanner_prototype), now compatible with Linux using CMake and libpcap.

---

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Usage](#usage)
- [CLI Options](#cli-options)
- [Used PROFINET protocols](#used-profinet-protocols)
- [Build (Linux)](#build-linux)
- [Build (OpenWrt APK)](#build-openwrt-apk)
- [Thanks](#thanks)

---

## Overview
This program scans for Profinet devices in a local subnet (Layer 2) or across a range of IP addresses (Layer 3). It sends Profinet DCP calls, listens for device responses, and performs additional RPC endpoint mapper requests for each discovered device. Results are printed as a human-readable summary to stdout.

Originally created as a Visual Studio Express console application for Windows, this port enables Linux compatibility and uses libpcap for packet capture.

The Linux port and code migration were performed by AI (GitHub Copilot), based on the original implementation and documentation.

---

## Features
- Layer 2 scan: Profinet DCP call in local subnet
- Layer 3 scan: IP range scan with detailed device info
- Automatic RPC endpoint mapper requests for discovered devices
- Human-readable scan summary output to stdout
- Extensive code comments for learning and reference

---

## Usage

Run the scanner from the build directory. Example:

```sh
./build/SendPacket/pn_scanner
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

This port is based on the original Windows-only program by Eiwanger:
https://github.com/Eiwanger/profinet_scanner_prototype

Special thanks to Eiwanger for the original implementation and documentation.
