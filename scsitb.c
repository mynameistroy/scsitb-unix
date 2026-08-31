#include <iso646.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

#include "include/scsi_device.h"
#include "include/scsitb.h"
#include "include/toolbox_commands.h"

#define CMD_NONE 0
#define CMD_INFO_ALL 1
#define CMD_LSDIR 2
#define CMD_LSCDS 3
#define CMD_GET 4
#define CMD_DEBUG 5
#define CMD_SET_CD 6
#define CMD_PUT 7
#define CMD_INFO_DEVICE 8

void help(char *exe_name);
void cmd_arg_print(int argc, char **argv);
int cmd_info(SCSI_DEVICE *device, SCSI_DEVICE **device_list,
             unsigned int dev_count, int cmd_argc, char **cmd_args);
int cmd_info_device(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);
int cmd_list_files(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);
int cmd_list_cds(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);
int cmd_get_file(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);
int cmd_debug(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);
int cmd_set_cd(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);
int cmd_put_file(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);

int log_level = 0;

void scsitb_log(int line, char *file, int LEVEL, char *fmt, ...)
{
    char log_buffer[128] = {0};
    va_list args;

    if (LEVEL <= log_level)
    {
        va_start(args, fmt);
        vsnprintf(log_buffer, sizeof(log_buffer), fmt, args);
        va_end(args);

        printf("%s:%d %s", file, line, log_buffer);
    }
}

int main(int argc, char **argv)
{
    int command = CMD_NONE;
    int cmd_offset = 3;
    char *cmd_argv = NULL;
    SCSI_DEVICE *target_device = NULL;
    /* handle args */

    /* not enough args or help */
    if (argc < 2)
    {
        help(argv[0]);
        return NO_ERROR;
    }

    for (int i = 0; i < argc; i++)
    {
        cmd_argv = argv[i];

        if (NULL == cmd_argv)
        {
            /* break out */
            break;
        }

        /* check for help arg */
        if (0 == strcasecmp("-h", cmd_argv) ||
            0 == strcasecmp("--help", cmd_argv) ||
            0 == strcasecmp("help", cmd_argv))
        {
            help(argv[0]);
            return NO_ERROR;
        }

        /* check for verbosity */
        if (0 == strcasecmp("-v", cmd_argv))
        {
            log_level++;
        }
    }

    SCSI_DEVICE **device_list = NULL;
    unsigned int dev_count = 0;

    get_scsi_device_list(&device_list, &dev_count);

    if (0 == dev_count)
    {
        printf("Error: No compatible devices found\n");
        return CMD_FAILED;
    }

    if (NULL != argv[2])
    {
        /* verify the specific device asked for exists */
        for (unsigned int i = 0; i < dev_count; i++)
        {
            char *dev_name = device_list[i]->name;
            if (strstr(dev_name, argv[1]))
            {
                target_device = device_list[i];
                break;
            }
        }
    }

    /* first arg is always a command */
    if (0 == strcasecmp("info", argv[1]))
    {
        /* scsitb info <args> */
        command = CMD_INFO_ALL;
    }
    else if (0 == strcasecmp("info", argv[2]))
    {
        command = CMD_INFO_DEVICE;
    }
    else if (0 == strcasecmp("lsdir", argv[2]))
    {
        /* scsitb lsdir <args> */
        command = CMD_LSDIR;
    }
    else if (0 == strcasecmp("lsimg", argv[2]))
    {
        command = CMD_LSCDS;
    }
    else if (0 == strcasecmp("get", argv[2]))
    {
        command = CMD_GET;
    }
    else if (0 == strcasecmp("debug", argv[2]))
    {
        command = CMD_DEBUG;
    }
    else if (0 == strcasecmp("put", argv[2]))
    {
        command = CMD_PUT;
    }
    else if (0 == strcasecmp("setimg", argv[2]))
    {
        command = CMD_SET_CD;
    }
    else
    {
        for (unsigned int i = 0; i < dev_count; i++)
        {
            scsi_close(device_list[i]);
        }
        free(device_list);
        printf("Error: invalid command %s\n", argv[2]);
        return CMD_FAILED;
    }

    /* if cmd is info device is options, otherwise required */
    if ((CMD_INFO_ALL != command) && (argc < 3))
    {
        printf("Error: missing device\n");
        return CMD_FAILED;
    }

    switch (command)
    {
        case CMD_INFO_ALL:
            cmd_info(NULL, device_list, dev_count, argc - cmd_offset,
                     &argv[cmd_offset]);
            break;

        case CMD_INFO_DEVICE:
            cmd_info(target_device, device_list, dev_count, argc - cmd_offset,
                     &argv[cmd_offset]);
            break;

        case CMD_LSDIR:
            cmd_list_files(target_device, argc - cmd_offset, &argv[cmd_offset]);
            break;

        case CMD_LSCDS:
            cmd_list_cds(target_device, argc - cmd_offset, &argv[cmd_offset]);
            break;

        case CMD_GET:
            cmd_get_file(target_device, argc - cmd_offset, &argv[cmd_offset]);
            break;

        case CMD_DEBUG:
            cmd_debug(target_device, argc - cmd_offset, &argv[cmd_offset]);
            break;

        case CMD_PUT:
            cmd_put_file(target_device, argc - cmd_offset, &argv[cmd_offset]);
            break;

        case CMD_SET_CD:
            cmd_set_cd(target_device, argc - cmd_offset, &argv[cmd_offset]);
            break;

        default:
            for (unsigned int i = 0; i < dev_count; i++)
            {
                scsi_close(device_list[i]);
            }
            free(device_list);
            printf("Unknown action (%d), exiting...\n", command);
            return INVALID_ARGS;
    }

    for (unsigned int i = 0; i < dev_count; i++)
    {
        scsi_close(device_list[i]);
    }
    free(device_list);
    return NO_ERROR;
}

void help(char *exe_name)
{
    printf("%s <options> <device> <command> <command args>\n", exe_name);
    printf("\n");
    printf("Commands:\n");
    printf("info                    Provides information for all found "
           "devices\n");
    printf("info <device>           Provides information about a specific "
           "device\n");

    printf("lsdir                   List files in shared directory\n");
    printf("lscds                   List images available to <device>\n");
    printf("get <remote> <local>    Get a file from device. <local> is "
           "optional\n");
    printf("put <local> <remote>    Put a file on the device. <remote> "
           "is optional\n");
    printf("setimg <index>          Set the image of a optical drive via "
           "index\n");
    printf("debug                   Display current debug logging "
           "setting\n");
    printf("debug <0|1>             Set debug valued to 0 = Off or 1 = "
           "On\n");
    printf("\n");

    printf("Options:\n");
    printf("-v                      Verbosity\n");
    printf("-h,--help               This help\n");
}

void cmd_arg_print(int argc, char **argv)
{
    LOG(VERBOSE, "INFO\n");
    LOGF(VERBOSE, "  args (%d):", argc);
    for (int i = 0; i < argc; i++)
    {
        LOGF(VERBOSE, "%s ", argv[i]);
    }
    LOG(VERBOSE, "\n");
}

int cmd_info(SCSI_DEVICE *device, SCSI_DEVICE **device_list,
             unsigned int dev_count, int argc, char **argv)
{
    printf("Addr     Vendor   Model            Type       Adapter            "
           "  Emulation  Dev\n");
    printf("-------------------------------------------------------------------"
           "-----"
           "---------\n");
    if (NULL != device)
    {
        cmd_info_device(device, argc, argv);
    }
    else
    {
        for (unsigned int i = 0; i < dev_count; i++)
        {
            cmd_info_device(device_list[i], argc, argv);
        }
    }

    return NO_ERROR;
}

int cmd_info_device(SCSI_DEVICE *device, int argc, char **argv)
{
    cmd_arg_print(argc, argv);

    char *dev = "XXX";

    printf("%-8s %-8s %-16s %-10s %-20s %-10s %s\n", basename(device->name),
           device->inquiry_data.vendor, device->inquiry_data.product,
           device->device_type_name, device->host_device_name,
           toolbox_s2s_to_str(device->s2s_type), dev);

    return NO_ERROR;
}

int cmd_list_files(SCSI_DEVICE *device, int argc, char **argv)
{
    TOOLBOX_DIR *dir = NULL;

    cmd_arg_print(argc, argv);

    if (toolbox_cmd_list_files(device, &dir))
    {
        printf("Error: Getting file list from device\n");
        return CMD_FAILED;
    }
    printf("dir->count %d\n", dir->count);

    TOOLBOX_FILE *file = dir->entries;
    for (int i = 0; i < dir->count; i++)
    {
        printf("%-2d %c %-33s %8ld B\n", file->index, file->type ? 'F' : 'D',
               file->name, toolbox_get_file_size(file));

        file++;
    }

    toolbox_dir_free(dir);

    return NO_ERROR;
}

int cmd_list_cds(SCSI_DEVICE *device, int argc, char **argv)
{
    TOOLBOX_DIR *dir = NULL;

    cmd_arg_print(argc, argv);

    if (TOOLBOX_DEVICE_TYPE_OPTICAL != device->s2s_type)
    {
        printf("Error: Not an optical device\n");
        return CMD_FAILED;
    }

    if (toolbox_cmd_list_cds(device, &dir))
    {
        printf("Error: Getting image list for device\n");
        return CMD_FAILED;
    }

    TOOLBOX_FILE *file = dir->entries;
    for (int i = 0; i < dir->count; i++)
    {
        printf("%-2d %c %-33s %8ld B\n", file->index, file->type ? 'F' : 'D',
               file->name, toolbox_get_file_size(file));

        file++;
    }

    toolbox_dir_free(dir);

    return NO_ERROR;
}

int cmd_get_file(SCSI_DEVICE *device, int argc, char **argv)
{
    cmd_arg_print(argc, argv);
    /* get the desired remote file */
    if (NULL == argv[0])
    {
        printf("Error: Missing target filename\n");
        return INVALID_ARGS;
    }
    char *remote_file = argv[0];

    /* get the desired local file name (optional) */
    char *local_file = NULL;
    if (NULL != argv[1])
    {
        local_file = argv[1];
    }

    if (toolbox_cmd_get_file(device, remote_file, local_file))
    {
        printf("Error: File transfer failed\n");
        return CMD_FAILED;
    }

    return NO_ERROR;
}

int cmd_debug(SCSI_DEVICE *device, int argc, char **argv)
{
    cmd_arg_print(argc, argv);

    if (NULL == argv[0])
    {
        /* display current debug setting */
        int debug = 0;
        if (toolbox_cmd_get_debug(device, &debug))
        {
            printf("Error: Couldn't get current debug setting\n");
            return CMD_FAILED;
        }
        printf("Debug:%s\n", debug ? "Enabled" : "Disabled");
    }
    else
    {
        /* set debug setting accordingly to parameter */
        int debug = strtol(argv[0], NULL, 0);
        if (toolbox_cmd_set_debug(device, debug))
        {
            printf("Error: Couldn't set debug to new value %d\n", debug);
            return CMD_FAILED;
        }
    }

    return NO_ERROR;
}

int cmd_set_cd(SCSI_DEVICE *device, int argc, char **argv)
{
    cmd_arg_print(argc, argv);

    if (NULL == argv[0])
    {
        printf("Error: Requires an index or filename\n");
        return INVALID_ARGS;
    }

    if (strlen(argv[0]) > 1)
    {
        /* probably an index */
        long index = strtoul(argv[0], NULL, 0);
        if (toolbox_cmd_set_next_cd(device, index))
        {
            printf("Error: Unable to set image\n");
            return CMD_FAILED;
        }
    }
    else
    {
        /* likely a filename */
        printf("Error: Unimplemented, use index values\n");
        return INVALID_ARGS;
    }

    return NO_ERROR;
}

int cmd_put_file(SCSI_DEVICE *device, int argc, char **argv)
{
    cmd_arg_print(argc, argv);

    if (NULL == argv[0])
    {
        printf("Error: Requires a filename\n");
        return INVALID_ARGS;
    }

    if (toolbox_cmd_send_file(device, argv[0], NULL))
    {
        printf("Error: Unable to send file to target\n");
        return CMD_FAILED;
    }

    return NO_ERROR;
}
