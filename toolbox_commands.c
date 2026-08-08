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

void toolbox_dir_free(toolbox_dir *dir)
{
    if (NULL != dir)
    {
        free(dir->entries);
        free(dir);
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

int toolbox_cmd_list_files(SCSI_DEVICE *target, toolbox_dir **dir)
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

    toolbox_file_entry *file_buffer =
        malloc(sizeof(toolbox_file_entry) * file_count);
    if (NULL == file_buffer)
    {
        printf("Failed to allocate file list buffer\n");
        return -1;
    }

    memcpy(cmd.cdb, toolbox_list_files_cdb, sizeof(toolbox_list_files_cdb));
    cmd.cdb_len = sizeof(toolbox_list_files_cdb);
    cmd.recv_buffer = (unsigned char *)file_buffer;
    cmd.recv_buffer_len = sizeof(toolbox_file_entry) * file_count;
    ;
    /* perform BLUESCSI_TOOLBOX_LIST_FILES */
    if (scsi_cmd(target, &cmd, &response))
    {
        printf("tool_cmd_list_files() failed (%s)\n", strerror(errno));
        free(file_buffer);
        return -1;
    }

    *dir = malloc(sizeof(toolbox_dir));
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
