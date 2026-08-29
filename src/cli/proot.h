#ifndef PROOT_CLI_H
#define PROOT_CLI_H

#include "cli/cli.h"
#include "extension/vdisk/vdisk.h"

#define S_HELP(value) static int handle_option_##value(Tracee *tracee, const Cli *cli, const char *value)

S_HELP(r);
S_HELP(b);
S_HELP(q);
S_HELP(w);
S_HELP(v);
S_HELP(V);
S_HELP(h);
S_HELP(k);
S_HELP(i);
S_HELP(p);
S_HELP(n);
S_HELP(vdisk);
S_HELP(mixed_mode);
#ifdef HAVE_PYTHON_EXTENSION
static int handle_option_P(Tracee *tracee, const Cli *cli, const char *value);
#endif
static int handle_option_0(Tracee *tracee, const Cli *cli, const char *value);
static int handle_option_l(Tracee *tracee, const Cli *cli, const char *value);
static int handle_option_R(Tracee *tracee, const Cli *cli, const char *value);
static int handle_option_S(Tracee *tracee, const Cli *cli, const char *value);
static int handle_option_kill_on_exit(Tracee *tracee, const Cli *cli, const char *value);
static int handle_option_persistent(Tracee *tracee, const Cli *cli, const char *value);
static int handle_option_setup_paths(Tracee *tracee, const Cli *cli, const char *value);
static int handle_option_setup_exclude(Tracee *tracee, const Cli *cli, const char *value);

static const char *recommended_bindings[] = {
	"/etc/host.conf",
	"/etc/hosts",
	"/etc/hosts.equiv",
	"/etc/mtab",
	"/etc/netgroup",
	"/etc/networks",
	"/etc/passwd",
	"/etc/group",
	"/etc/nsswitch.conf",
	"/etc/resolv.conf",
	"/etc/localtime",
	"/dev/",
	"/sys/",
	"/proc/",
	"/tmp/",
	"/run/",
	"/var/run/dbus/system_bus_socket",
	"$HOME",
	NULL
};

static const char *recommended_su_bindings[] = {
	"/etc/host.conf",
	"/etc/hosts",
	"/etc/nsswitch.conf",
	"/etc/resolv.conf",
	"/dev/",
	"/sys/",
	"/proc/",
	"/tmp/",
	"/run/shm",
	"$HOME",
	NULL
};

static int pre_initialize_bindings(Tracee *, const Cli *, size_t, char *const *, size_t);
static int post_initialize_exe(Tracee *, const Cli *, size_t, char *const *, size_t);

static Cli proot_cli = {
	.version  = VERSION,
	.name     = "improot",
	.subtitle = "Userspace virtual disk booter (QCOW2/VHD/RAW), chroot & mount engine without root",
	.synopsis = "improot [options] [command]",
	.colophon = "Improot: High-Performance Userspace Virtual Disk & Container Engine\n\
Based on PRoot (GPL v2 or later). Includes libvdisk userspace storage layer.",
	.logo = "\
  ___                              _   \n\
 |_ _| _ __ ___   _ __  _ __  ___ | |_ \n\
  | | | '_ ` _ \\ | '_ \\| '__|/ _ \\| __|\n\
  | | | | | | | || |_) | |  | (_) | |_ \n\
 |___||_| |_| |_|| .__/|_|   \\___/ \\__|\n\
                 |_|                   ",

	.pre_initialize_bindings = pre_initialize_bindings,
	.post_initialize_exe = post_initialize_exe,

	.options = {
	/* =========================================================================
	 * Virtual Disk (vdisk) Options
	 * ========================================================================= */
	{ .class = "Virtual Disk (vdisk) Options",
	  .arguments = {
		{ .name = "--vdisk", .separator = '=', .value = "image[:part#]" },
		{ .name = "--disk",  .separator = '=', .value = "image[:part#]" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_vdisk,
	  .description = "Directly boot from a QCOW2, Dynamic VHD, or RAW virtual drive.",
	  .detail = "\tMounts and boots directly from a QCOW2, VHD, or RAW image file without\n\
\trequiring root privileges, kernel loop devices, FUSE, or guest VM overhead.\n\
\tOptionally specify a partition index (e.g. image.qcow2:1).",
	},
	{ .class = "Virtual Disk (vdisk) Options",
	  .arguments = {
		{ .name = "--commit",           .separator = '\0', .value = NULL },
		{ .name = "--persistent",       .separator = '\0', .value = NULL },
		{ .name = "--vdisk-persistent", .separator = '\0', .value = NULL },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_persistent,
	  .description = "Persist and sync all guest changes back to the virtual disk image on exit.",
	  .detail = "\tWhen enabled, all installed packages, file modifications, and created files\n\
\tare automatically committed back to the virtual drive upon session exit.",
	},
	{ .class = "Virtual Disk (vdisk) Options",
	  .arguments = {
		{ .name = "--setup-paths",      .separator = '=',  .value = "mode" },
		{ .name = "--setup-path",       .separator = '=',  .value = "mode" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_setup_paths,
	  .description = "Auto-scan and export binary directories (bin, sbin, xbin, .bin) to PATH.",
	  .detail = "\tScans the virtual filesystem for all binary directories and automatically\n\
\texports them into $PATH and /etc/profile.d/00-vdisk-paths.sh.\n\
\tPass --setup-paths=[/] or --setup-paths=off to disable.",
	},
	{ .class = "Virtual Disk (vdisk) Options",
	  .arguments = {
		{ .name = "--setup-paths",      .separator = '\0', .value = NULL },
		{ .name = "--setup-path",       .separator = '\0', .value = NULL },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_setup_paths,
	  .description = "Auto-scan and export binary directories (bin, sbin, xbin, .bin) to PATH.",
	  .detail = "\tScans the virtual filesystem for all binary directories and exports them to PATH.",
	},
	{ .class = "Virtual Disk (vdisk) Options",
	  .arguments = {
		{ .name = "-se",                .separator = ' ',  .value = "exclusions" },
		{ .name = "-se",                .separator = '=',  .value = "exclusions" },
		{ .name = "--setup-exclude",    .separator = '=',  .value = "exclusions" },
		{ .name = "--exclude-path",     .separator = '=',  .value = "exclusions" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_setup_exclude,
	  .description = "Exclude directory paths from the auto-scanned PATH list.",
	  .detail = "\tSpecify a comma-separated or bracketed list of paths to exclude\n\
\t(e.g., -se '[ /usr/local, /opt ]' or -se /usr/local,/opt).",
	},

	/* =========================================================================
	 * Root & Privilege Emulation Options
	 * ========================================================================= */
	{ .class = "Root & Identity Emulation Options",
	  .arguments = {
		{ .name = "-0", .separator = '\0', .value = NULL },
		{ .name = "--root-id", .separator = '\0', .value = NULL },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_0,
	  .description = "Fakes root privileges (UID=0, GID=0, groups=0) and successful chown/chmod.",
	  .detail = "\tBypasses root-only limitations in package managers (apt, apk, dnf, xbps)\n\
\tand system tools by intercepting getuid, getgid, getgroups, and setuid syscalls.",
	},
	{ .class = "Root & Identity Emulation Options",
	  .arguments = {
		{ .name = "-i", .separator = ' ', .value = "uid:gid" },
		{ .name = "--change-id", .separator = '=', .value = "uid:gid" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_i,
	  .description = "Make current user and group appear as custom \"uid:gid\".",
	  .detail = "\tEmulates specified user and group identity inside the guest environment.",
	},
	{ .class = "Root & Identity Emulation Options",
	  .arguments = {
		{ .name = "-k", .separator = ' ', .value = "release_string" },
		{ .name = "--kernel-release", .separator = '=', .value = "release_string" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_k,
	  .description = "Make current kernel appear as custom kernel release version.",
	  .detail = "\tOverrides uname release string if guest binaries require a specific kernel.",
	},

	/* =========================================================================
	 * Filesystem & Mount Binding Options
	 * ========================================================================= */
	{ .class = "Filesystem & Mount Options",
	  .arguments = {
		{ .name = "-r", .separator = ' ', .value = "path" },
		{ .name = "--rootfs", .separator = '=', .value = "path" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_r,
	  .description = "Use host *path* as guest root file-system (directory rootfs).",
	  .detail = "\tSpecifies a traditional directory tree as the guest rootfs.",
	},
	{ .class = "Filesystem & Mount Options",
	  .arguments = {
		{ .name = "-b", .separator = ' ', .value = "path" },
		{ .name = "--bind", .separator = '=', .value = "path" },
		{ .name = "-m", .separator = ' ', .value = "path" },
		{ .name = "--mount", .separator = '=', .value = "path" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_b,
	  .description = "Bind-mount host *path* into the guest rootfs.",
	  .detail = "\tMakes host files/directories accessible inside the guest.\n\
\tSyntax: -b host_path:guest_location.",
	},
	{ .class = "Filesystem & Mount Options",
	  .arguments = {
		{ .name = "-w", .separator = ' ', .value = "path" },
		{ .name = "--pwd", .separator = '=', .value = "path" },
		{ .name = "--cwd", .separator = '=', .value = "path" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_w,
	  .description = "Set the initial guest working directory (default: /root or /).",
	  .detail = "\tSets the working directory before executing the guest command.",
	},
	{ .class = "Filesystem & Mount Options",
	  .arguments = {
		{ .name = "-R", .separator = ' ', .value = "path" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_R,
	  .description = "Alias: -r *path* + recommended host bindings (/etc/hosts, /dev, /proc, etc.).",
	  .detail = "\tStandard container mode with host system bindings.",
	},
	{ .class = "Filesystem & Mount Options",
	  .arguments = {
		{ .name = "-S", .separator = ' ', .value = "path" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_S,
	  .description = "Alias: -0 -r *path* + minimal safe host bindings for package building.",
	  .detail = "\tSafe package installation mode with minimal host path exposure.",
	},

	/* =========================================================================
	 * Networking Options
	 * ========================================================================= */
	{ .class = "Networking Options",
	  .arguments = {
		{ .name = "-p", .separator = ' ', .value = "port_in:port_out" },
		{ .name = "--port", .separator = '=', .value = "port_in:port_out" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_p,
	  .description = "Map ports without root privileges (e.g. -p 80:8080).",
	  .detail = "\tIntercepts bind() and connect() syscalls to redirect network ports.",
	},
	{ .class = "Networking Options",
	  .arguments = {
		{ .name = "-n", .separator = '\0', .value = NULL },
		{ .name = "--netcoop", .separator = '\0', .value = NULL },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_n,
	  .description = "Enable dynamic port cooperation mode to prevent bind port collisions.",
	  .detail = "\tIntercepts bind() to assign dynamic ephemeral ports automatically.",
	},

	/* =========================================================================
	 * Execution & Architecture Options
	 * ========================================================================= */
	{ .class = "Execution & Emulation Options",
	  .arguments = {
		{ .name = "-q", .separator = ' ', .value = "command" },
		{ .name = "--qemu", .separator = '=', .value = "command" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_q,
	  .description = "Execute guest programs through QEMU user-mode emulation.",
	  .detail = "\tRuns multi-architecture guest binaries (ARM64, RISC-V) via QEMU user.",
	},
	{ .class = "Execution & Emulation Options",
	  .arguments = {
		{ .name = "--kill-on-exit", .separator = '\0', .value = NULL },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_kill_on_exit,
	  .description = "Kill all background/orphaned child processes when main command exits.",
	  .detail = "\tEnsures all daemon processes spawned in the container terminate on exit.",
	},
	{ .class = "Execution & Emulation Options",
	  .arguments = {
		{ .name = "-l", .separator = '\0', .value = NULL },
		{ .name = "--link2symlink", .separator = '\0', .value = NULL },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_l,
	  .description = "Emulate hardlinks as symlinks for environments without hardlink support.",
	  .detail = "\tTranslates link() system calls to symlink() for restricted host storage.",
	},
	{ .class = "Execution & Emulation Options",
	  .arguments = {
		{ .name = "--mixed-mode", .separator = '=', .value = "mode" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_mixed_mode,
	  .description = "Control mixed-execution mode for host native executables.",
	  .detail = "\tConfigures execution behavior when host ELF binaries are invoked.",
	},

	/* =========================================================================
	 * General & Information Options
	 * ========================================================================= */
	{ .class = "General & Information Options",
	  .arguments = {
		{ .name = "-v", .separator = ' ', .value = "level" },
		{ .name = "--verbose", .separator = '=', .value = "level" },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_v,
	  .description = "Set debug verbosity level (higher integer = more detail).",
	  .detail = "\tOutputs tracing and diagnostic messages to stderr.",
	},
	{ .class = "General & Information Options",
	  .arguments = {
		{ .name = "-V", .separator = '\0', .value = NULL },
		{ .name = "--version", .separator = '\0', .value = NULL },
		{ .name = "--about",   .separator = '\0', .value = NULL },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_V,
	  .description = "Print version, copyright, and license information, then exit.",
	  .detail = "\tDisplays detailed version and author credits.",
	},
	{ .class = "General & Information Options",
	  .arguments = {
		{ .name = "-h", .separator = '\0', .value = NULL },
		{ .name = "--help", .separator = '\0', .value = NULL },
		{ .name = "--usage", .separator = '\0', .value = NULL },
		{ .name = NULL, .separator = '\0', .value = NULL } },
	  .handler = handle_option_h,
	  .description = "Print this help message and exit.",
	  .detail = "\tShows syntax, available flags, and usage documentation.",
	},

	END_OF_OPTIONS
	},
};

#endif /* PROOT_CLI_H */
