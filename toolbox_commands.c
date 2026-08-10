#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

extern int errno;

#include "include/scsi_device.h"
#include "include/toolbox_commands.h"

unsigned char toolbox_count_files_cdb[12] = {0xD2, 0, 0, 0, 0, 0,
                                             0,    0, 0, 0, 0, 0};

unsigned char toolbox_list_files_cdb[12] = {0xD0, 0, 0, 0, 0, 0,
                                            0,    0, 0, 0, 0, 0};

int toolbox_cmd_count(SCSI_DEVICE *target, unsigned char toolbox_cmd);
int toolbox_cmd_list(SCSI_DEVICE *target, unsigned char toolbox_cmd,
                     TOOLBOX_DIR **dir);

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

long toolbox_get_file_size(TOOLBOX_FILE *file)
{
    long size = 0;
    for (int i = 0; i < sizeof(file->size); i++)
    {
        size = (size * 256) + file->size[i];
    }
    return size;
}

int toolbox_cmd_count_files(SCSI_DEVICE *target)
{
    return toolbox_cmd_count(target, TOOLBOX_SCSI_COUNT_FILES);
}

int toolbox_cmd_count_cds(SCSI_DEVICE *target)
{
    return toolbox_cmd_count(target, TOOLBOX_SCSI_COUNT_CDS);
}

int toolbox_cmd_count(SCSI_DEVICE *target, unsigned char toolbox_cmd)
{

    unsigned char recv_buffer[1] = {0};

    SCSI_CMD cmd;
    memset(&cmd, 0, sizeof(SCSI_CMD));

    cmd.cdb[0] = toolbox_cmd;
    cmd.cdb_len = 10;
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

    return recv_buffer[0];
}

int toolbox_cmd_list_files(SCSI_DEVICE *target, TOOLBOX_DIR **dir)
{
    return toolbox_cmd_list(target, TOOLBOX_SCSI_LIST_FILES, dir);
}

int toolbox_cmd_list_cds(SCSI_DEVICE *target, TOOLBOX_DIR **dir)
{
    return toolbox_cmd_list(target, TOOLBOX_SCSI_LIST_CDS, dir);
}

int toolbox_cmd_list(SCSI_DEVICE *target, unsigned char toolbox_cmd,
                     TOOLBOX_DIR **dir)
{
    int file_count = 0;
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;
    unsigned char count_cmd = 0;

    memset(&cmd, 0, sizeof(SCSI_CMD));
    memset(&response, 0, sizeof(SCSI_CMD_RESPONSE));

    if (NULL != *dir)
    {
        printf("toolbox_cmd_list_files: Invalid arg\n");
        return -1;
    }

    if (TOOLBOX_SCSI_LIST_FILES == toolbox_cmd)
    {
        count_cmd = TOOLBOX_SCSI_COUNT_FILES;
    }
    else
    {
        count_cmd = TOOLBOX_SCSI_COUNT_CDS;
    }

    file_count = toolbox_cmd_count(target, count_cmd);
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

    cmd.cdb[0] = toolbox_cmd;
    cmd.cdb_len = 10;
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

int toolbox_cmd_get_file(SCSI_DEVICE *target, char *src, char *dst)
{
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;
    unsigned char buffer[4096] = {0};
    TOOLBOX_DIR *dir = NULL;
    TOOLBOX_FILE *f = NULL;
    int blocks = 0;
    int last_block_size = 0;
    long file_size = 0;

    if (NULL == target)
    {
        return -1;
    }
    if (NULL == src)
    {
        return -1;
    }

    memset(&cmd, 0, sizeof(SCSI_CMD));
    memset(&response, 0, sizeof(SCSI_CMD_RESPONSE));

    if (toolbox_cmd_list_files(target, &dir))
    {
        printf("list files failed\n");
        return -1;
    }

    for (int i = 0; i < dir->count; i++)
    {
        if (1 != dir->entries[i].type)
        {
            continue;
        }
        f = &dir->entries[i];
        printf("src %s name %s\n", src, f->name);
        if (0 == strncmp(src, f->name, strlen(f->name)))
        {
            break;
        }
    }

    if (NULL == f)
    {
        printf("couldn't find %s\n", src);
        toolbox_dir_free(dir);
        return -1;
    }

    file_size = toolbox_get_file_size(f);
    blocks = file_size / 4096;
    last_block_size = file_size % 4096;
    printf("name %s size %ld blocks %d last_block_size %d\n", f->name,
           file_size, blocks, last_block_size);

    cmd.cdb[0] = TOOLBOX_SCSI_GET_FILE;
    cmd.cdb[1] = f->index;
    cmd.cdb_len = 10;
    cmd.recv_buffer = buffer;
    cmd.recv_buffer_len = sizeof(buffer);

    FILE *local = NULL;
    if (NULL == dst)
    {
        local = fopen(src, "wb");
    }
    else
    {
        local = fopen(dst, "wb");
    }
    /* tranfer 4k blocks */
    long blocks_transferred = 0;
    while (blocks_transferred != blocks)
    {
        memset(buffer, 0, sizeof(buffer));
        for (int i = 2; i < 6; i++)
        {
            printf("%X ", cmd.cdb[i]);
        }
        printf(" %ld / %ld\n", blocks_transferred * 4096, file_size);
        if (scsi_cmd(target, &cmd, &response))
        {
            printf("failed at block %ld\n", blocks_transferred);
            fclose(local);
            toolbox_dir_free(dir);
            return -1;
        }
        fwrite(buffer, sizeof(unsigned char), sizeof(buffer), local);
        blocks_transferred++;
        cmd.cdb[2] = (unsigned char)((blocks_transferred) >> 24) & 0xff;
        cmd.cdb[3] = (unsigned char)((blocks_transferred) >> 16) & 0xff;
        cmd.cdb[4] = (unsigned char)((blocks_transferred) >> 8) & 0xff;
        cmd.cdb[5] = (unsigned char)(blocks_transferred) & 0xff;
    }
    if (0 != last_block_size)
    {
        memset(buffer, 0, sizeof(buffer));
        cmd.recv_buffer_len = last_block_size;
        /* transfer the last block */
        if (scsi_cmd(target, &cmd, &response))
        {
            printf("block transfer failed\n");
            fclose(local);
            toolbox_dir_free(dir);
            return -1;
        }

        fwrite(buffer, sizeof(unsigned char), last_block_size, local);
    }

    fclose(local);
    return 0;
}

int toolbox_cmd_get_debug(SCSI_DEVICE *target, int *debug)
{
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;
    unsigned char buffer[1] = {0};

    if (NULL == target || NULL == debug)
    {
        return -1;
    }

    memset(&cmd, 0, sizeof(SCSI_CMD));
    memset(&response, 0, sizeof(SCSI_CMD_RESPONSE));

    cmd.cdb[0] = TOOLBOX_SCSI_TOGGLE_DEBUG;
    cmd.cdb[1] = TOOLBOX_SCSI_DEBUG_GET;
    cmd.cdb_len = 10;
    cmd.recv_buffer = buffer;
    cmd.recv_buffer_len = sizeof(buffer);

    if (scsi_cmd(target, &cmd, &response))
    {
        return -1;
    }

    *debug = buffer[0];

    return 0;
}

int toolbox_cmd_set_debug(SCSI_DEVICE *target, int debug)
{
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;

    if (NULL == target)
    {
        return -1;
    }

    memset(&cmd, 0, sizeof(SCSI_CMD));
    memset(&response, 0, sizeof(SCSI_CMD_RESPONSE));

    cmd.cdb[0] = TOOLBOX_SCSI_TOGGLE_DEBUG;
    cmd.cdb[1] = TOOLBOX_SCSI_DEBUG_SET;
    cmd.cdb[2] = debug;
    cmd.cdb_len = 10;

    if (scsi_cmd(target, &cmd, &response))
    {
        return -1;
    }

    return 0;
}
