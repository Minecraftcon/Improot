# Improot 🚀
> **Next-Generation Userspace Virtual Disk & Container Engine**  
> *Boot, execute, and persist Linux virtual disk images (QCOW2, Dynamic VHD, RAW) without root privileges, kernel loop devices, FUSE, or VM hypervisors.*

---

## 📖 Overview

**Improot** is an enhanced, high-performance evolution of **PRoot** integrated with **`libvdisk`** (userspace virtual storage library). It enables unprivileged users to directly mount, inspect, modify, and execute guest Linux distributions packaged as standard virtual disk formats (`.qcow2`, `.vhd`, `.raw`).

### 🌟 Key Innovations

- **Zero-Root Virtual Drive Booting**: Run entire Linux guest disk images without `sudo`, `mount`, `mknod`, or loop devices.
- **Multi-Format Storage Engine**: Native support for QEMU Copy-On-Write (**QCOW2 v2/v3**), Microsoft Virtual Hard Disk (**Dynamic & Fixed VHD**), and **Raw Ext4 Sparse Disks**.
- **Instant Boot & High Fidelity**: Pre-populates filesystem trees and dynamic linkers in < 15 milliseconds.
- **Persistence Write-Back (`--commit` / `--persistent`)**: Automatically commit all installed packages, configuration changes, and file edits back into `.vhd` / `.qcow2` images on session exit.
- **Auto Binary & PATH Discovery (`--setup-paths`, `-se`)**: Recursively scans guest filesystems for binary locations (`/bin`, `/sbin`, `/usr/local/bin`, `~/.local/bin`, `/system/bin`) and sets environment `PATH` automatically.
- **Distro Codename Virtualization**: Intercepts `uname(2)` and `/etc/hostname` dynamically to match the guest distribution (`ubuntu`, `alpine`, `fedora`, `void`, etc.).
- **Complete Terminal & Network Integration**: Automatic `/etc/resolv.conf` DNS bridging, terminal terminfo integration for curses/nano, and root ID faking (`-0`).

---

## 🛠 Command-Line Reference

```
Usage: improot [options] [command]
```

### 1. Virtual Disk (`vdisk`) Options

| Flag | Description | Example |
| :--- | :--- | :--- |
| **`--vdisk=<img[:part#]>`** / **`--disk=...`** | Direct boot from QCOW2, Dynamic VHD, or RAW virtual image. | `--vdisk=alpine.vhd` or `--vdisk=ubuntu.qcow2:1` |
| **`--commit`** / **`--persistent`** | Persist and sync all guest changes back to virtual disk on exit. | `--vdisk=alpine.vhd --commit -0 sh` |
| **`--setup-paths`** / **`--setup-path`** | Auto-scan and export binary directories to `PATH`. | `--vdisk=void.qcow2 --setup-paths -0 bash` |
| **`-se <list>`** / **`--setup-exclude`** | Exclude specific directory paths from auto-discovery. | `-se '[ /usr/local, /opt ]'` |
| **`--setup-paths=off`** | Disable automatic PATH discovery. | `--setup-paths=off` |

### 2. Root & Privilege Emulation

| Flag | Description |
| :--- | :--- |
| **`-0`** / **`--root-id`** | Fake root privileges (`UID=0`, `GID=0`, `groups=0`) and bypass package manager checks (`apk`, `apt`, `dnf`, `xbps`). |
| **`-i <uid:gid>`** | Custom user and group emulation. |
| **`-k <version>`** | Emulate custom Linux kernel release string in `uname(2)`. |

### 3. Filesystem & Bind Mounts

| Flag | Description |
| :--- | :--- |
| **`-r <path>`** | Traditional directory-based guest root filesystem. |
| **`-b <host:guest>`** | Bind-mount host files or directories into guest rootfs. |
| **`-w <dir>`** / **`--pwd`** | Set container initial working directory. |
| **`-R <path>`** | Recommended full host binding mode (`/dev`, `/proc`, `/sys`, `/etc/hosts`, `$HOME`). |
| **`-S <path>`** | Safe package building mode (`-0` + minimal isolated host bindings). |

### 4. Networking & Execution

| Flag | Description |
| :--- | :--- |
| **`-p <in:out>`** | Userspace network port forwarding (e.g. `-p 80:8080`). |
| **`-n`** / **`--netcoop`** | Dynamic port cooperation mode to prevent bind collisions. |
| **`-q <command>`** | Multi-architecture emulation via QEMU user-mode (`qemu-aarch64`, `qemu-riscv64`). |
| **`--kill-on-exit`** | Force-kill orphaned background/daemon processes on exit. |

---

## 🚀 Quickstart & Examples

### 1. Boot an Alpine Linux Dynamic VHD:
```bash
./proot/src/proot --vdisk=examples/alpine_base.vhd -0 sh
```

### 2. Install Packages & Persist to QCOW2 Virtual Drive:
```bash
./proot/src/proot --vdisk=examples/void_base.qcow2 --commit -0 bash
# Inside container:
xbps-install -S curl git nano
exit
# Changes are automatically committed and compressed back into void_base.qcow2!
```

### 3. Run Commands Directly with Auto-PATH & Exclusions:
```bash
./proot/src/proot --vdisk=examples/fedora_base.vhd -se '[ /usr/local ]' -0 dnf --version
```

---

## 🏗 Building Improot

### Prerequisites:
- GCC / Clang
- Make
- Python 3 (for VHD/QCOW2 conversion tools)
- `e2fsprogs` (`mkfs.ext4`)
- `talloc` (`libtalloc-dev`)

### Compile:
```bash
cd proot/src
make -j$(nproc)
```

The resulting binary `./proot` (Improot) is completely self-contained!

---

## 📄 License
Improot is licensed under the **GNU General Public License v2.0 or later (GPL-2.0+)**.
Contains components from PRoot and libvdisk.
