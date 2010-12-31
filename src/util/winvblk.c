/**
 * Copyright (C) 2009-2010, Shao Miller <shao.miller@yrdsb.edu.on.ca>.
 * Copyright 2006-2008, V.
 * For WinAoE contact information, see http://winaoe.org/
 *
 * This file is part of WinVBlock, derived from WinAoE.
 *
 * WinVBlock is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WinVBlock is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WinVBlock.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file
 *
 * WinVBlock user-mode utility.
 */

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <malloc.h>

#include "winvblock.h"
#include "portable.h"
#include "mount.h"
#include "aoe.h"

/**
 * Command routine.
 *
 * @v boot_bus          A handle to the boot bus device.
 */
#define command_decl( x )       \
                                \
int STDCALL                     \
x (                             \
  HANDLE boot_bus               \
 )
/* Function pointer for a command routine. */
typedef command_decl(( *command_routine ));

winvblock__def_struct(option) {
    char *name;
    char *value;
    int has_arg;
  };

static option opt_h1 = {
    "HELP", NULL, 0
  };

static option opt_h2 = {
    "?", NULL, 0
  };

static option opt_cmd = {
    "CMD", NULL, 1
  };

static option opt_cyls = {
    "C", NULL, 1
  };

static option opt_heads = {
    "H", NULL, 1
  };

static option opt_spt = {
    "S", NULL, 1
  };

static option opt_disknum = {
    "D", NULL, 1
  };

static option opt_media = {
    "M", NULL, 1
  };

static option opt_uri = {
    "U", NULL, 1
  };

static option opt_mac = {
    "MAC", NULL, 1
  };

static option *options[] = {
    &opt_h1,
    &opt_h2,
    &opt_cmd,
    &opt_cyls,
    &opt_heads,
    &opt_spt,
    &opt_disknum,
    &opt_media,
    &opt_uri,
    &opt_mac,
  };

static char present[] = "";
static char *invalid_opt = NULL;

static void cmdline_options(int argc, char **argv) {
    int cur = 1;

    while (cur < argc) {
        char *cur_arg = argv[cur];
        int opt;

        /* Check if the argument is an option.  Look for '-', '--', or '/'. */
        switch (*cur_arg) {
            case '-':
              cur_arg++;
              if (*cur_arg == '-')
                cur_arg++;
              break;

            case '/':
              cur_arg++;
              break;

            default:
              invalid_opt = cur_arg;
              return;
          }
        /* Convert possible option to upper-case. */
        {   char *walker = cur_arg;

            while (*walker) {
                *walker = toupper(*walker);
                walker++;
              }
          }
        /* Check if the argument is a _valid_ option. */
        opt = 0;
        while (opt < sizeof (options) / sizeof (option *)) {
            if (strcmp(cur_arg, options[opt]->name) == 0) {
                /*
                 * We have a match.
                 * Check if the option was already specified.
                 */
                if (options[opt]->value != NULL) {
                    printf(
                        "%s specified more than once, making it invalid.\n",
                        cur_arg
                      );
                    invalid_opt = cur_arg;
                    return;
                  }
                /*
                 * The option's value is the next argument.  For boolean
                 * options, we ignore the value anyway, but need to know the
                 * option is present.
                 */
                if (cur == argc - 1)
                  options[opt]->value = present;
                  else
                  options[opt]->value = argv[cur + 1];
                cur += options[opt]->has_arg;
                break;
              }
            opt++;
          }
        /* Did we find no valid option match? */
        if (opt == sizeof (options) / sizeof (option *)) {
            invalid_opt = cur_arg;
            return;
          }
        cur++;
      }
  }

static command_decl(cmd_help) {
    char help_text[] = "\n\
WinVBlock user-land utility for disk control. (C) 2006-2008 V.,\n\
                                              (C) 2009-2010 Shao Miller\n\
Usage:\n\
  winvblk -cmd <command> [-d <disk number>] [-m <media>] [-u <uri or path>]\n\
    [-mac <client mac address>] [-c <cyls>] [-h <heads>] [-s <sects per track>]\n\
  winvblk -?\n\
\n\
Parameters:\n\
  <command> is one of:\n\
    scan   - Shows the reachable AoE targets.\n\
    show   - Shows the mounted AoE targets.\n\
    mount  - Mounts an AoE target.  Requires -mac and -u\n\
    umount - Unmounts an AoE disk.  Requires -d\n\
    attach - Attaches <filepath> disk image file.  Requires -u and -m.\n\
             -c, -h, -s are optional.\n\
    detach - Detaches file-backed disk.  Requires -d\n\
  <uri or path> is something like:\n\
    aoe:eX.Y        - Where X is the \"major\" (shelf) and Y is\n\
                      the \"minor\" (slot)\n\
    c:\\my_disk.hdd - The path to a disk image file or .ISO\n\
  <media> is one of 'c' for CD/DVD, 'f' for floppy, 'h' for hard disk drive\n\
\n";
    printf(help_text);
    return 1;
  }

static command_decl(cmd_scan) {
    aoe__mount_targets_ptr targets;
    DWORD bytes_returned;
    winvblock__uint32 i;
    winvblock__uint8 string[256];
    int status = 2;

    targets = malloc(
        sizeof (aoe__mount_targets) +
        (32 * sizeof (aoe__mount_target))
      );
    if (targets == NULL) {
        printf("Out of memory\n");
        goto err_alloc;
      }

    if (!DeviceIoControl(
        boot_bus,
        IOCTL_AOE_SCAN,
        NULL,
        0,
        targets,
        sizeof (aoe__mount_targets) + (32 * sizeof (aoe__mount_target)),
        &bytes_returned,
        (LPOVERLAPPED) NULL
      )) {
        printf("DeviceIoControl (%d)\n", (int) GetLastError());
        status = 2;
        goto err_ioctl;
      }
    if (targets->Count == 0) {
        printf("No AoE targets found.\n");
        goto err_no_targets;
      }
    printf("Client NIC          Target      Server MAC         Size\n");
    for (i = 0; i < targets->Count && i < 10; i++) {
        sprintf(
            string,
            "e%lu.%lu      ",
            targets->Target[i].Major,
            targets->Target[i].Minor
          );
        string[10] = 0;
        printf(
            " %02x:%02x:%02x:%02x:%02x:%02x  %s "
              " %02x:%02x:%02x:%02x:%02x:%02x  %I64uM\n",
            targets->Target[i].ClientMac[0],
            targets->Target[i].ClientMac[1],
            targets->Target[i].ClientMac[2],
            targets->Target[i].ClientMac[3],
            targets->Target[i].ClientMac[4],
            targets->Target[i].ClientMac[5],
            string,
            targets->Target[i].ServerMac[0],
            targets->Target[i].ServerMac[1],
            targets->Target[i].ServerMac[2],
            targets->Target[i].ServerMac[3],
            targets->Target[i].ServerMac[4],
            targets->Target[i].ServerMac[5],
            targets->Target[i].LBASize / 2048
          );
      } /* for */

    err_no_targets:

    status = 0;

    err_ioctl:

    free(targets);
    err_alloc:

    return status;
  }

static command_decl(cmd_show) {
    aoe__mount_disks_ptr mounted_disks;
    DWORD bytes_returned;
    winvblock__uint32 i;
    winvblock__uint8 string[256];
    int status = 2;

    mounted_disks = malloc(
        sizeof (aoe__mount_disks) +
        (32 * sizeof (aoe__mount_disk)) 
      );
    if (mounted_disks == NULL) {
        printf("Out of memory\n");
        goto err_alloc;
      }
  if (!DeviceIoControl(
      boot_bus,
      IOCTL_AOE_SHOW,
      NULL,
      0,
      mounted_disks,
      sizeof (aoe__mount_disks) + (32 * sizeof (aoe__mount_disk)),
      &bytes_returned,
      (LPOVERLAPPED) NULL
    )) {
      printf("DeviceIoControl (%d)\n", (int) GetLastError());
      goto err_ioctl;
    }

    status = 0;

    if (mounted_disks->Count == 0) {
        printf("No AoE disks mounted.\n");
        goto err_no_disks;
      }
    printf("Disk  Client NIC         Server MAC         Target      Size\n");
    for (i = 0; i < mounted_disks->Count && i < 10; i++) {
        sprintf(
            string,
            "e%lu.%lu      ",
            mounted_disks->Disk[i].Major,
            mounted_disks->Disk[i].Minor
          );
        string[10] = 0;
        printf(
            " %-4lu %02x:%02x:%02x:%02x:%02x:%02x  "
              "%02x:%02x:%02x:%02x:%02x:%02x  %s  %I64uM\n",
            mounted_disks->Disk[i].Disk,
            mounted_disks->Disk[i].ClientMac[0],
            mounted_disks->Disk[i].ClientMac[1],
            mounted_disks->Disk[i].ClientMac[2],
            mounted_disks->Disk[i].ClientMac[3],
            mounted_disks->Disk[i].ClientMac[4],
            mounted_disks->Disk[i].ClientMac[5],
            mounted_disks->Disk[i].ServerMac[0],
            mounted_disks->Disk[i].ServerMac[1],
            mounted_disks->Disk[i].ServerMac[2],
            mounted_disks->Disk[i].ServerMac[3],
            mounted_disks->Disk[i].ServerMac[4],
            mounted_disks->Disk[i].ServerMac[5],
            string,
            mounted_disks->Disk[i].LBASize / 2048
          );
      }

    err_no_disks:

    err_ioctl:

    free(mounted_disks);
    err_alloc:

    return status;
  }

static command_decl(cmd_mount) {
    winvblock__uint8 mac_addr[6];
    winvblock__uint32 ver_major, ver_minor;
    winvblock__uint8 in_buf[sizeof (mount__filedisk) + 1024];
    DWORD bytes_returned;

    if (opt_mac.value == NULL || opt_uri.value == NULL) {
        printf("-mac and -u options required.  See -? for help.\n");
        return 1;
      }
    sscanf(
        opt_mac.value,
        "%02x:%02x:%02x:%02x:%02x:%02x",
        (int *) (mac_addr + 0),
        (int *) (mac_addr + 1),
        (int *) (mac_addr + 2),
        (int *) (mac_addr + 3),
        (int *) (mac_addr + 4),
        (int *) (mac_addr + 5)
      );
    sscanf(
        opt_uri.value,
        "aoe:e%lu.%lu",
        (int *) &ver_major,
        (int *) &ver_minor
      );
    printf(
        "mounting e%lu.%lu from %02x:%02x:%02x:%02x:%02x:%02x\n",
        (int) ver_major,
        (int) ver_minor,
        mac_addr[0],
        mac_addr[1],
        mac_addr[2],
        mac_addr[3],
        mac_addr[4],
        mac_addr[5]
      );
    memcpy(in_buf, mac_addr, 6);
    *((winvblock__uint16_ptr) (in_buf + 6)) = (winvblock__uint16) ver_major;
    *((winvblock__uint8_ptr) (in_buf + 8)) = (winvblock__uint8) ver_minor;
    if (!DeviceIoControl(
        boot_bus,
        IOCTL_AOE_MOUNT,
        in_buf,
        sizeof (in_buf),
        NULL,
        0,
        &bytes_returned,
        (LPOVERLAPPED) NULL
      )) {
        printf("DeviceIoControl (%d)\n", (int) GetLastError());
        return 2;
      }
    return 0;
  }

static command_decl(cmd_umount) {
    winvblock__uint32 disk_num;
    winvblock__uint8 in_buf[sizeof (mount__filedisk) + 1024];
    DWORD bytes_returned;

    if (opt_disknum.value == NULL) {
        printf("-d option required.  See -? for help.\n");
        return 1;
      }
    sscanf(opt_disknum.value, "%d", (int *) &disk_num);
    printf("unmounting disk %d\n", (int) disk_num);
    memcpy(in_buf, &disk_num, 4);
    if (!DeviceIoControl(
        boot_bus,
        IOCTL_AOE_UMOUNT,
        in_buf,
        sizeof (in_buf),
        NULL,
        0,
        &bytes_returned,
        (LPOVERLAPPED) NULL
      )) {
        printf("DeviceIoControl (%d)\n", (int) GetLastError());
        return 2;
      }
    return 0;
  }

static command_decl(cmd_attach) {
    mount__filedisk filedisk;
    char obj_path_prefix[] = "\\??\\";
    winvblock__uint8 in_buf[sizeof (mount__filedisk) + 1024];
    DWORD bytes_returned;

    if (opt_uri.value == NULL || opt_media.value == NULL) {
        printf("-u and -m options required.  See -? for help.\n");
        return 1;
      }
    filedisk.type = opt_media.value[0];
    if (opt_cyls.value != NULL)
      sscanf(opt_cyls.value, "%d", (int *) &filedisk.cylinders);
    if (opt_heads.value != NULL)
      sscanf(opt_heads.value, "%d", (int *) &filedisk.heads);
    if (opt_spt.value != NULL)
      sscanf(opt_spt.value, "%d", (int *) &filedisk.sectors);
    memcpy(in_buf, &filedisk, sizeof (mount__filedisk));
    memcpy(
        in_buf + sizeof (mount__filedisk),
        obj_path_prefix,
        sizeof (obj_path_prefix)
      );
    memcpy(
        in_buf + sizeof (mount__filedisk) + sizeof (obj_path_prefix) - 1,
        opt_uri.value,
        strlen(opt_uri.value) + 1
      );
    if (!DeviceIoControl(
        boot_bus,
        IOCTL_FILE_ATTACH,
        in_buf,
        sizeof (in_buf),
        NULL,
        0,
        &bytes_returned,
        (LPOVERLAPPED) NULL
      )) {
        printf("DeviceIoControl (%d)\n", (int) GetLastError());
        return 2;
      }
    return 0;
  }

static command_decl(cmd_detach) {
    winvblock__uint32 disk_num;
    winvblock__uint8 in_buf[sizeof (mount__filedisk) + 1024];
    DWORD bytes_returned;

    if (opt_disknum.value == NULL) {
        printf("-d option required.  See -? for help.\n");
        return 1;
      }
    sscanf(opt_disknum.value, "%d", (int *) &disk_num);
    printf("Detaching file-backed disk %d\n", (int) disk_num);
    memcpy(in_buf, &disk_num, 4);
    if (!DeviceIoControl(
        boot_bus,
        IOCTL_FILE_DETACH,
        in_buf,
        sizeof (in_buf),
        NULL,
        0,
        &bytes_returned,
        (LPOVERLAPPED) NULL
      )) {
        printf("DeviceIoControl (%d)\n", (int) GetLastError());
        return 2;
      }
    return 0;
  }

int main(int argc, char **argv, char **envp) {
    command_routine cmd = cmd_help;
    HANDLE boot_bus = NULL;
    int status = 1;

    cmdline_options(argc, argv);
    /* Check for invalid option. */
    if (invalid_opt != NULL) {
        printf("Use -? for help.  Invalid option: %s\n", invalid_opt);
        goto err_bad_cmd;
      }
    /* Check for cry for help. */
    if (opt_h1.value || opt_h2.value)
      goto do_cmd;
    /* Check for no command. */
    if (opt_cmd.value == NULL)
      goto do_cmd;
    /* Check given command. */
    if (strcmp(opt_cmd.value, "scan") == 0)
      cmd = cmd_scan;
    if (strcmp(opt_cmd.value, "show") == 0)
      cmd = cmd_show;
    if (strcmp(opt_cmd.value, "mount" ) == 0)
      cmd = cmd_mount;
    if (strcmp(opt_cmd.value, "umount") == 0)
      cmd = cmd_umount;
    if (strcmp(opt_cmd.value, "attach") == 0)
      cmd = cmd_attach;
    if (strcmp(opt_cmd.value, "detach") == 0)
      cmd = cmd_detach;
    /* Check for invalid command. */
    if (cmd == cmd_help)
      goto do_cmd;

    boot_bus = CreateFile(
        "\\\\.\\" winvblock__literal,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
      );
    if (boot_bus == INVALID_HANDLE_VALUE) {
        printf("CreateFile (%d)\n", (int) GetLastError());
        goto err_handle;
      }

    do_cmd:
    status = cmd(boot_bus);

    if (boot_bus != NULL)
      CloseHandle(boot_bus);
    err_handle:

    err_bad_cmd:

    return status;
  }
