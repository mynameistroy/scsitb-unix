#include <libgen.h>
#include <stdio.h>
#include <strings.h>

#include "include/scsi_device.h"
#include "include/toolbox_commands.h"

#define CMD_NONE 0
#define CMD_INFO 1
#define CMD_LSDIR 2
#define CMD_LSCDS 3
#define CMD_GET 4
#define CMD_DEBUG 5

void help(void);
void cmd_arg_print(int argc, char **argv);
int cmd_info(char *device, SCSI_DEVICE **device_list, unsigned int dev_count,
             int cmd_argc, char **cmd_args);
int cmd_info_device(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);
int cmd_list_files(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);
int cmd_list_cds(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);
int cmd_get_file(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);
int cmd_debug(SCSI_DEVICE *device, int cmd_argc, char **cmd_args);

int main(int argc, char **argv)
{
    int command = CMD_NONE;
    int cmd_offset = 3;
    char *cmd_argv = NULL;
    SCSI_DEVICE *target_device = NULL;
    /* handle args */

    /* not enough args or help */
    if (argc < 1)
    {
        help();
        return 1;
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
            0 == strcasecmp("--help", cmd_argv))
        {
            help();
            return 1;
        }
    }

    if (argc < 1)
    {
        printf("Error: missing command\n");
        return -1;
    }

    /* first arg is always a command */
    if (0 == strcasecmp("info", argv[1]))
    {
        /* scsitb info <args> */
        command = CMD_INFO;
    }
    else if (0 == strcasecmp("lsdir", argv[1]))
    {
        /* scsitb lsdir <args> */
        command = CMD_LSDIR;
    }
    else if (0 == strcasecmp("lsimg", argv[1]))
    {
        command = CMD_LSCDS;
    }
    else if (0 == strcasecmp("get", argv[1]))
    {
        command = CMD_GET;
    }
    else if (0 == strcasecmp("debug", argv[1]))
    {
        command = CMD_DEBUG;
    }
    else
    {
        printf("Error: invalid command %s\n", argv[1]);
        return -1;
    }

    /* second arg is device */
    /* if cmd is info device is options, otherwise required */
    if ((CMD_INFO != command) && (NULL == argv[2]))
    {
        printf("Error: missing device\n");
        return -1;
    }

    SCSI_DEVICE **device_list = NULL;
    unsigned int dev_count = 0;

    get_scsi_device_list(&device_list, &dev_count);

    if (0 == dev_count)
    {
        printf("No compatible devices found\n");
        return -1;
    }

    if (NULL != argv[2])
    {
        /* verify the specific device asked for exists */
        for (unsigned int i = i; i < dev_count; i++)
        {
            char *dev_name = device_list[i]->name;
            if (strstr(dev_name, argv[2]))
            {
                target_device = device_list[i];
                break;
            }
        }
    }

    switch (command)
    {
        case CMD_INFO:
            cmd_info(argv[2], device_list, dev_count, argc - cmd_offset,
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

        default:
            printf("Unknown action (%d), exiting...\n", command);
            return 1;
    }

    for (unsigned int i = 0; i < dev_count; i++)
    {
        scsi_close(device_list[i]);
    }
    free(device_list);
    return 0;
}

void help(void)
{
    printf("scsitb\n\n");
    printf("\n");
}

void cmd_arg_print(int argc, char **argv)
{
    printf("INFO\n");
    printf("  args (%d):", argc);
    for (int i = 0; i < argc; i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\n");
}

int cmd_info(char *device, SCSI_DEVICE **device_list, unsigned int dev_count,
             int argc, char **argv)
{
    printf("Addr     Vendor   Model            Type       Adapter            "
           "  Emulation  Dev\n");
    printf("-------------------------------------------------------------------"
           "-----"
           "---------\n");
    if (NULL != device)
    {
        char full_dev_path[64] = {0};
        snprintf(full_dev_path, sizeof(full_dev_path), "/dev/%s", device);
        SCSI_DEVICE *d = scsi_open(full_dev_path);
        if (NULL == d)
        {
            printf("Error opening %s\n", device);
            return -1;
        }
        cmd_info_device(d, argc, argv);
        scsi_close(d);
    }
    else
    {
        for (unsigned int i = 0; i < dev_count; i++)
        {
            cmd_info_device(device_list[i], argc, argv);
        }
    }

    return 0;
}

int cmd_info_device(SCSI_DEVICE *device, int argc, char **argv)
{
    cmd_arg_print(argc, argv);

    char *dev = "XXX";

    printf("%-8s %-8s %-16s %-10s %-20s %-10s %s", basename(device->name),
           device->inquiry_data.vendor, device->inquiry_data.product,
           device->device_type_name, device->host_device_name,
           toolbox_s2s_to_str(device->s2s_type), dev);

    return 0;
}

int cmd_list_files(SCSI_DEVICE *device, int argc, char **argv)
{
    TOOLBOX_DIR *dir = NULL;

    cmd_arg_print(argc, argv);

    if (toolbox_cmd_list_files(device, &dir))
    {
        printf("Error listing files\n");
        return -1;
    }

    TOOLBOX_FILE *file = dir->entries;
    for (int i = 0; i < dir->count; i++)
    {
        printf("%-2d %c %-33s %8ld B\n", file->index, file->type ? 'F' : 'D',
               file->name, toolbox_get_file_size(file));

        file++;
    }

    toolbox_dir_free(dir);

    return 0;
}

int cmd_list_cds(SCSI_DEVICE *device, int argc, char **argv)
{
    TOOLBOX_DIR *dir = NULL;

    cmd_arg_print(argc, argv);

    if (TOOLBOX_DEVICE_TYPE_OPTICAL != device->s2s_type)
    {
        printf("Not a CD device\n");
        return -1;
    }

    if (toolbox_cmd_list_cds(device, &dir))
    {
        printf("Error listing CDs\n");
        return -1;
    }

    TOOLBOX_FILE *file = dir->entries;
    for (int i = 0; i < dir->count; i++)
    {
        printf("%-2d %c %-33s %8ld B\n", file->index, file->type ? 'F' : 'D',
               file->name, toolbox_get_file_size(file));

        file++;
    }

    toolbox_dir_free(dir);

    return 0;
}

int cmd_get_file(SCSI_DEVICE *device, int argc, char **argv)
{
    cmd_arg_print(argc, argv);
    /* get the desired remote file */
    if (NULL == argv[0])
    {
        printf("No file to get\n");
        return -1;
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
        printf("File transfer failed\n");
        return -1;
    }

    return 0;
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
            printf("get debug failed\n");
            return -1;
        }
        printf("Debug:%s\n", debug ? "Enabled" : "Disabled");
    }
    else
    {
        /* set debug setting accordingly to parameter */
        int debug = strtol(argv[0], NULL, 0);
        if (toolbox_cmd_set_debug(device, debug))
        {
            printf("set debug failed\n");
            return -1;
        }
    }

    return 0;
}
