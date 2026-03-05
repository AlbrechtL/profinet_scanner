# Profinet Scanner (Linux Port)

A port of the original Windows-only [Profinet scanner by Eiwanger](https://github.com/Eiwanger/profinet_scanner_prototype), now compatible with Linux using CMake and libpcap.

---

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Build (Linux)](#build-linux)
- [Usage](#usage)
- [CLI Options](#cli-options)
- [Protocols](#protocols)
- [TODO](#todo)
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

--interactive
	Force prompt-based mode (also the default when no parameters are provided).
```

Examples:

```sh
# Local scan (non-interactive)
./build/SendPacket/pn_scanner --interface 1 --mode local

# Remote scan (non-interactive)
./build/SendPacket/pn_scanner --interface eth0 --mode remote --target 192.168.0.10-20

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
```

---

## Protocols
- Profinet DCP (Layer 2)
- Ethernet
- IP
- UDP
- DCE/RPC (Layer 3)

---

## TODO (from original [Profinet scanner by Eiwanger (https://github.com/Eiwanger/profinet_scanner_prototype))
- Add error enum
- Change functions to return an error type instead of int or void
- Refactor packet_handlerIP/packet_handlerIP_rem (difference: linked list data comparison)

---

## Thanks

This port is based on the original Windows-only program by Eiwanger:
https://github.com/Eiwanger/profinet_scanner_prototype

Special thanks to Eiwanger for the original implementation and documentation.
