#include <errno.h>
#include <stdio.h>
#include <strings.h>

extern int errno;

#include "include/scsi_device.h"
#include "include/toolbox_commands.h"

unsigned char toolbox_count_files_cdb[12] = {0xD2, 0, 0, 0, 0, 0,
                                             0,    0, 0, 0, 0, 0};

unsigned char toolbox_list_files_cdb[12] = {0xD0, 0, 0, 0, 0, 0,
                                            0,    0, 0, 0, 0, 0};

void toolbox_dir_free(TOOLBOX_DIR *dir)
{
    if (NULL != dir)
    {
        free(dir->entries);
        free(dir);
    }
}

char *toolbox_s2s_to_str(unsigned char s2s_type)
{
    switch (s2s_type)
    {
        case TOOLBOX_DEVICE_TYPE_FIXED:
            return "Fixed";
        case TOOLBOX_DEVICE_TYPE_REMOVABLE:
            return "Removable";
        case TOOLBOX_DEVICE_TYPE_OPTICAL:
            return "Optical";
        case TOOLBOX_DEVICE_TYPE_FLOPPY:
            return "Floppy";
        case TOOLBOX_DEVICE_TYPE_MAGNETO:
            return "Magneto";
        case TOOLBOX_DEVICE_TYPE_TAPE:
            return "Tape";
        case TOOLBOX_DEVICE_TYPE_NETWORK:
            return "Network";
        case TOOLBOX_DEVICE_TYPE_ZIP100:
            return "Zip100";
        case TOOLBOX_DEVICE_TYPE_NONE:
            return "None";
        default:
            return "Unknown";
    }
}

int toolbox_cmd_count_files(SCSI_DEVICE *target)
{

    unsigned char recv_buffer[1] = {0};

    SCSI_CMD cmd;
    memset(&cmd, 0, sizeof(SCSI_CMD));

    memcpy(cmd.cdb, toolbox_count_files_cdb, sizeof(toolbox_count_files_cdb));
    cmd.cdb_len = sizeof(toolbox_count_files_cdb);
    cmd.recv_buffer = &recv_buffer[0];
    cmd.recv_buffer_len = 1;

    SCSI_CMD_RESPONSE response;

    if (NULL == target)
    {
        printf("Invalid SCSI target\n");
        return -1;
    }

    /* perform BLUESCSI_TOOLBOX_COUNT_FILES */
    if (scsi_cmd(target, &cmd, &response))
    {
        printf("COUNT_FILES failed (%d)\n", errno);
        return -1;
    }

    printf("file count %d\n", recv_buffer[0]);

    return recv_buffer[0];
}

int toolbox_cmd_list_files(SCSI_DEVICE *target, TOOLBOX_DIR **dir)
{
    int file_count = 0;
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;

    memset(&cmd, 0, sizeof(SCSI_CMD));
    memset(&response, 0, sizeof(SCSI_CMD_RESPONSE));

    if (NULL != *dir)
    {
        printf("toolbox_cmd_list_files: Invalid arg\n");
        return -1;
    }

    file_count = toolbox_cmd_count_files(target);
    if (0 > file_count)
    {
        printf("Failed to get file count\n");
        return -1;
    }

    TOOLBOX_FILE *file_buffer = malloc(sizeof(TOOLBOX_FILE) * file_count);
    if (NULL == file_buffer)
    {
        printf("Failed to allocate file list buffer\n");
        return -1;
    }

    memcpy(cmd.cdb, toolbox_list_files_cdb, sizeof(toolbox_list_files_cdb));
    cmd.cdb_len = sizeof(toolbox_list_files_cdb);
    cmd.recv_buffer = (unsigned char *)file_buffer;
    cmd.recv_buffer_len = sizeof(TOOLBOX_FILE) * file_count;
    ;
    /* perform BLUESCSI_TOOLBOX_LIST_FILES */
    if (scsi_cmd(target, &cmd, &response))
    {
        printf("tool_cmd_list_files() failed (%s)\n", strerror(errno));
        free(file_buffer);
        return -1;
    }

    *dir = malloc(sizeof(TOOLBOX_DIR));
    if (NULL == *dir)
    {
        printf("alloc error\n");
        free(file_buffer);
        return -1;
    }

    (*dir)->count = file_count;
    (*dir)->entries = file_buffer;
    return 0;
}

int toolbox_cmd_get_metadata(SCSI_DEVICE *target, unsigned char data_type,
                             unsigned char *metadata,
                             unsigned int *metadata_len)
{
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;

    if (NULL == target)
    {
        return -1;
    }

    if (data_type > TOOLBOX_SCSI_META_GET_WORKING_DIR)
    {
        return -1;
    }

    if (NULL == metadata)
    {
        return -1;
    }

    if (NULL == metadata_len || 0 == *metadata_len)
    {
        return -1;
    }

    memset(&cmd, 0, sizeof(SCSI_CMD));
    memset(&response, 0, sizeof(SCSI_CMD_RESPONSE));

    cmd.cdb[0] = TOOLBOX_SCSI_METADATA;
    cmd.cdb[1] = data_type;
    cmd.cdb_len = 10;
    cmd.recv_buffer = metadata;
    cmd.recv_buffer_len = *metadata_len;

    /* get the list of devices */
    if (scsi_cmd(target, &cmd, &response))
    {
        return -1;
    }

    return 0;
}

int toolbox_cmd_list_devices(SCSI_DEVICE *target, unsigned char device_list[])
{
    unsigned int size = 8;
    return toolbox_cmd_get_metadata(target, TOOLBOX_SCSI_META_LIST_DEVICES,
                                    device_list, &size);
}

int toolbox_cmd_get_capabilities(SCSI_DEVICE *target,
                                 unsigned char capabilities[])
{
    unsigned int size = 8;
    return toolbox_cmd_get_metadata(target, TOOLBOX_SCSI_META_GET_CAPABILITIES,
                                    capabilities, &size);
}
