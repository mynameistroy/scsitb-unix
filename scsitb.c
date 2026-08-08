#include <stdio.h>
#include <strings.h>

#include "include/scsi_device.h"
#include "include/toolbox_commands.h"

#define CMD_NONE 0
#define CMD_INFO 1
#define CMD_LSDIR 2

void help(void);
void cmd_arg_print(int argc, char **argv);
int cmd_info(char *device, int cmd_argc, char **cmd_args);
int cmd_list_files(char *device, int cmd_argc, char **cmd_args);

int main(int argc, char **argv)
{
    int command = CMD_NONE;
    int cmd_offset = 0;
    char *cmd_argv = NULL;
    char *device = NULL;
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
        cmd_offset = i + 1;

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

        /* scsitb info <args> */
        if (0 == strcasecmp("info", cmd_argv))
        {
            command = CMD_INFO;
        }

        /* scsitb lsdir <args> */
        if (0 == strcasecmp("lsdir", cmd_argv))
        {
            command = CMD_LSDIR;
        }

        /* specified device */
        if (0 == strcasecmp("-d", cmd_argv))
        {
            device = argv[i + 1];
            i++;
            printf("device:%s\n", device);
        }
    }

    switch (command)
    {
        case CMD_INFO:
            return cmd_info(device, argc - cmd_offset, &argv[cmd_offset]);

        case CMD_LSDIR:
            return cmd_list_files(device, argc - cmd_offset, &argv[cmd_offset]);

        default:
            printf("Unknown action (%d), exiting...\n", command);
            return 1;
    }
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

int cmd_info(char *device, int argc, char **argv)
{
    cmd_arg_print(argc, argv);
    SCSI_DEVICE *d = scsi_open(device);
    if (NULL == d)
    {
        printf("Error opening %s\n", device);
        return -1;
    }

    scsi_inquiry(d);
    char *emulation = "XXXXX";
    char *dev = "XXX";

    printf("Addr     Vendor   Model            Type       Adapter            "
           "  Emulation  Dev\n");
    printf("-------------------------------------------------------------------"
           "-----"
           "---------\n");
    printf("%-8s %-8s %-16s %-10s %-20s %-10s %s", d->addr,
           d->inquiry_data.vendor, d->inquiry_data.product, d->device_type_name,
           d->host_device_name, emulation, dev);

    scsi_close(d);
    return 0;
}

int cmd_list_files(char *device, int argc, char **argv)
{
    toolbox_dir *dir = NULL;
    printf("LSIMG\n");

    SCSI_DEVICE *d = scsi_open(device);
    if (NULL == d)
    {
        printf("Error opening %s\n", device);
        return -1;
    }

    if (toolbox_cmd_list_files(d, &dir))
    {
        printf("Error listing files\n");
        return -1;
    }

    toolbox_file_entry *file = dir->entries;
    for (int i = 0; i < dir->count; i++)
    {
        long size = 0;
        size |= file->size[0];
        size = size << 1;
        size |= file->size[1];
        size = size << 1;
        size |= file->size[2];
        size = size << 1;
        size |= file->size[3];
        size = size << 1;
        size |= file->size[4];

        printf("%d %c %s %ld\n", file->index, file->type ? 'F' : 'D',
               file->name, size);

        file++;
    }

    toolbox_dir_free(dir);
    scsi_close(d);

    return 0;
}
