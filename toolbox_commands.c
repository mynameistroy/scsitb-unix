#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

extern int errno;

#include "include/scsi_device.h"
#include "include/scsitb.h"
#include "include/toolbox_commands.h"

unsigned char toolbox_count_files_cdb[12] = {0xD2, 0, 0, 0, 0, 0,
                                             0,    0, 0, 0, 0, 0};

unsigned char toolbox_list_files_cdb[12] = {0xD0, 0, 0, 0, 0, 0,
                                            0,    0, 0, 0, 0, 0};

int toolbox_cmd_count(SCSI_DEVICE *target, unsigned char toolbox_cmd);
int toolbox_cmd_list(SCSI_DEVICE *target, unsigned char toolbox_cmd,
                     TOOLBOX_DIR **dir);

/* Helper to free a TOOLBOX_DIR */
void toolbox_dir_free(TOOLBOX_DIR *dir)
{
    if (NULL != dir)
    {
        free(dir->entries);
        free(dir);
    }
}

/* Helper to convert the S2S type value to a string */
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

/* Helper to convert the size from big endian to little endian */
long toolbox_get_file_size(TOOLBOX_FILE *file)
{
    long size = 0;
    for (size_t i = 0; i < sizeof(file->size); i++)
    {
        size = (size * 256) + file->size[i];
    }
    return size;
}

/* Toolbox command to return the file count on device */
int toolbox_cmd_count_files(SCSI_DEVICE *target)
{
    return toolbox_cmd_count(target, TOOLBOX_SCSI_COUNT_FILES);
}

/* Toolbox command to return the CD count for a target */
int toolbox_cmd_count_cds(SCSI_DEVICE *target)
{
    return toolbox_cmd_count(target, TOOLBOX_SCSI_COUNT_CDS);
}

/* Toolbox command to select the CD image to load */
int toolbox_cmd_set_next_cd(SCSI_DEVICE *target, unsigned char image_index)
{
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;

    if (NULL == target)
    {
        printf("toolbox_cmd_set_next_cd() Invalid SCSI target\n");
        return INVALID_ARGS;
    }

    /* Setup up SCSI command */
    memset(&cmd, 0, sizeof(SCSI_CMD));
    cmd.cdb[0] = TOOLBOX_SCSI_SET_NEXT_CD;
    cmd.cdb[1] = image_index;

    return scsi_cmd(target, &cmd, &response);
}

/* Toolbox function to handle either CD or file count */
int toolbox_cmd_count(SCSI_DEVICE *target, unsigned char toolbox_cmd)
{
    int retval = NO_ERROR;
    unsigned char recv_buffer[1] = {0};
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;

    /* Setup SCSI command */
    memset(&cmd, 0, sizeof(SCSI_CMD));
    cmd.cdb[0] = toolbox_cmd;
    cmd.cdb_len = 10;
    cmd.recv_buffer = recv_buffer;
    cmd.recv_buffer_len = 1;

    if (NULL == target)
    {
        printf("toolbox_cmd_count() Invalid SCSI target\n");
        return INVALID_ARGS;
    }

    /* perform BLUESCSI_TOOLBOX_COUNT_FILES */
    retval = scsi_cmd(target, &cmd, &response);
    if (0 < retval)
    {
        return retval;
    }

    return recv_buffer[0];
}

/* Toolbox function to list files on device */
int toolbox_cmd_list_files(SCSI_DEVICE *target, TOOLBOX_DIR **dir)
{
    return toolbox_cmd_list(target, TOOLBOX_SCSI_LIST_FILES, dir);
}

/* Toolbox function to list CD images for target */
int toolbox_cmd_list_cds(SCSI_DEVICE *target, TOOLBOX_DIR **dir)
{
    return toolbox_cmd_list(target, TOOLBOX_SCSI_LIST_CDS, dir);
}

/* Toolbox function to list files that can target either CD's or files */
int toolbox_cmd_list(SCSI_DEVICE *target, unsigned char toolbox_cmd,
                     TOOLBOX_DIR **dir)
{
    int file_count = 0;
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;
    unsigned char count_cmd = 0;

    memset(&cmd, 0, sizeof(SCSI_CMD));
    memset(&response, 0, sizeof(SCSI_CMD_RESPONSE));

    /* Check there is somewhere to put the response data */
    if (NULL != *dir)
    {
        LOG(VERBOSE, "toolbox_cmd_list() TOOLBOX_DIR is NULL\n");
        return INVALID_ARGS;
    }

    /* Are we getting CD or file count? */
    if (TOOLBOX_SCSI_LIST_FILES == toolbox_cmd)
    {
        count_cmd = TOOLBOX_SCSI_COUNT_FILES;
    }
    else
    {
        count_cmd = TOOLBOX_SCSI_COUNT_CDS;
    }

    /* try to get the file count from the target */
    file_count = toolbox_cmd_count(target, count_cmd);
    if (0 > file_count)
    {
        /* file_count carries the error */
        return file_count;
    }

    /* Allocate the toolbox command response buffer */
    TOOLBOX_FILE *file_buffer = malloc(sizeof(TOOLBOX_FILE) * file_count);
    if (NULL == file_buffer)
    {
        LOG(NORMAL,
            "toolbox_cmd_list() Failed to allocate TOOLBOX_FILE buffer\n");
        return ALLOC_ERROR;
    }

    /* setup the SCSI cmd */
    cmd.cdb[0] = toolbox_cmd;
    cmd.cdb_len = 10;
    cmd.recv_buffer = (unsigned char *)file_buffer;
    cmd.recv_buffer_len = sizeof(TOOLBOX_FILE) * file_count;

    /* perform BLUESCSI_TOOLBOX_LIST_FILES */
    if (scsi_cmd(target, &cmd, &response))
    {
        free(file_buffer);
        return -1;
    }

    /* Allocate the function response struct */
    *dir = malloc(sizeof(TOOLBOX_DIR));
    if (NULL == *dir)
    {
        LOG(VERBOSE, "toolbox_cmd_list() Failed to allocate TOOLBOX_DIR *\n");
        free(file_buffer);
        return ALLOC_ERROR;
    }

    /* Fill it out and return */
    (*dir)->count = file_count;
    (*dir)->entries = file_buffer;
    return NO_ERROR;
}

int toolbox_cmd_get_metadata(SCSI_DEVICE *target, unsigned char data_type,
                             unsigned char *metadata,
                             unsigned int *metadata_len)
{
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;

    if (NULL == target)
    {
        LOG(VERBOSE, "toolbox_cmd_get_metadata() Invalid SCSI target\n");
        return INVALID_ARGS;
    }

    if (data_type > TOOLBOX_SCSI_META_GET_WORKING_DIR)
    {
        LOGF(VERBOSE,
             "toolbox_cmd_get_metadata() Invalid Toolbox Metadata Command %d\n",
             data_type);
        return INVALID_ARGS;
    }

    if (NULL == metadata)
    {
        LOG(VERBOSE, "toolbox_cmd_get_metadata() metadata was NULL\n");
        return INVALID_ARGS;
    }

    if (NULL == metadata_len || 0 == *metadata_len)
    {
        LOG(VERBOSE, "toolbox_cmd_get_metadata() Invalid metadata length "
                     "pointer or size\n");
        return INVALID_ARGS;
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
        return CMD_FAILED;
    }

    return NO_ERROR;
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
        LOG(VERBOSE, "toolbox_cmd_get_file() Invalid SCSI target\n");
        return INVALID_ARGS;
    }
    if (NULL == src)
    {
        LOG(VERBOSE, "toolbox_cmd_get_file() Source is NULL\n");
        return INVALID_ARGS;
    }

    memset(&cmd, 0, sizeof(SCSI_CMD));
    memset(&response, 0, sizeof(SCSI_CMD_RESPONSE));

    if (toolbox_cmd_list_files(target, &dir))
    {
        LOG(VERBOSE, "toolbox_cmd_get_file() toolbox_cmd_list_files failed\n");
        return CMD_FAILED;
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
        LOGF(VERBOSE, "toolbox_cmd_get_file() Couldn't find %s on target\n",
             src);
        toolbox_dir_free(dir);
        return NOT_FOUND;
    }

    file_size = toolbox_get_file_size(f);
    blocks = file_size / 4096;
    last_block_size = file_size % 4096;

    cmd.cdb[0] = TOOLBOX_SCSI_GET_FILE;
    cmd.cdb[1] = f->index;
    cmd.cdb_len = 10;
    cmd.recv_buffer = buffer;
    cmd.recv_buffer_len = sizeof(buffer);

    FILE *local = NULL;
    if (NULL == dst)
    {
        /* use the source filename */
        local = fopen(src, "wb");
    }
    else
    {
        /* use the provided filename */
        local = fopen(dst, "wb");
    }
    /* tranfer 4k blocks */
    long blocks_transferred = 0;
    while (blocks_transferred != blocks)
    {
        memset(buffer, 0, sizeof(buffer));
        if (scsi_cmd(target, &cmd, &response))
        {
            LOGF(NORMAL,
                 "toolbox_cmd_get_file() Failed at block %ld with SCSI status "
                 "0x%X\n",
                 blocks_transferred, response.status);
            fclose(local);
            toolbox_dir_free(dir);
            return CMD_FAILED;
        }
        fwrite(buffer, sizeof(unsigned char), sizeof(buffer), local);
        blocks_transferred++;
        cmd.cdb[2] = (unsigned char)((blocks_transferred) >> 24) & 0xff;
        cmd.cdb[3] = (unsigned char)((blocks_transferred) >> 16) & 0xff;
        cmd.cdb[4] = (unsigned char)((blocks_transferred) >> 8) & 0xff;
        cmd.cdb[5] = (unsigned char)(blocks_transferred) & 0xff;
    }
    /* if there is a last block of irregular size, handle it */
    if (0 != last_block_size)
    {
        memset(buffer, 0, sizeof(buffer));
        cmd.recv_buffer_len = last_block_size;
        /* transfer the last block */
        if (scsi_cmd(target, &cmd, &response))
        {
            LOGF(NORMAL,
                 "toolbox_cmd_get_file() Last block transfer failed with SCSI "
                 "status 0x%X\n",
                 response.status);
            fclose(local);
            toolbox_dir_free(dir);
            return CMD_FAILED;
        }

        fwrite(buffer, sizeof(unsigned char), last_block_size, local);
    }

    fclose(local);
    return NO_ERROR;
}

int toolbox_cmd_get_debug(SCSI_DEVICE *target, int *debug)
{
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;
    unsigned char buffer[1] = {0};

    if (NULL == target || NULL == debug)
    {
        LOG(VERBOSE, "toolbox_cmd_get_debug() Invalid args\n");
        return INVALID_ARGS;
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
        LOGF(VERBOSE, "toolbox_cmd_get_debug() Failed with SCSI status 0x%X\n",
             response.status);
        return CMD_FAILED;
    }

    *debug = buffer[0];

    return NO_ERROR;
}

int toolbox_cmd_set_debug(SCSI_DEVICE *target, int debug)
{
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;

    if (NULL == target)
    {
        LOG(VERBOSE, "toolbox_cmd_set_debug() Invalid args\n");
        return INVALID_ARGS;
    }

    memset(&cmd, 0, sizeof(SCSI_CMD));
    memset(&response, 0, sizeof(SCSI_CMD_RESPONSE));

    cmd.cdb[0] = TOOLBOX_SCSI_TOGGLE_DEBUG;
    cmd.cdb[1] = TOOLBOX_SCSI_DEBUG_SET;
    cmd.cdb[2] = debug;
    cmd.cdb_len = 10;

    if (scsi_cmd(target, &cmd, &response))
    {
        LOGF(NORMAL, "toolbox_cmd_set_debug() Failed with SCSI status 0x%X\n",
             response.status);
        return CMD_FAILED;
    }

    return NO_ERROR;
}
int toolbox_cmd_send_file(SCSI_DEVICE *target, char *src, char *dst)
{
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;
    unsigned char buffer[4096] = {0};
    FILE *handle;
    struct stat fstat;

    if (NULL == target || NULL == src)
    {
        LOG(VERBOSE, "toolbox_cmd_send_file() Invalid args\n");
        return INVALID_ARGS;
    }

    if (stat(src, &fstat) < 0)
    {
        LOGF(NORMAL, "toolbox_cmd_send_file() stat() failed for %s: %s\n", src,
             strerror(errno));
        return CMD_FAILED;
    }

    handle = fopen(src, "rb");
    if (NULL == handle)
    {
        LOGF(NORMAL,
             "toolbox_cmd_send_file() Couldn't open %s for reading: %s\n", src,
             strerror(errno));
        return CMD_FAILED;
    }

    memset(&cmd, 0, sizeof(SCSI_CMD));
    memset(&response, 0, sizeof(SCSI_CMD_RESPONSE));

    /* send a prep/open */
    cmd.cdb[0] = TOOLBOX_SCSI_SEND_FILE_PREP;
    cmd.cdb_len = 10;
    cmd.send_buffer = buffer;
    cmd.send_buffer_len = 33;

    if (NULL == dst)
    {
        strncpy((char *)buffer, src, strlen(src));
    }
    else
    {
        strncpy((char *)buffer, dst, strlen(dst));
    }

    if (scsi_cmd(target, &cmd, &response))
    {
        LOGF(NORMAL,
             "toolbox_cmd_send_file() SEND_FILE_PREP failed with SCSI status "
             "0x%X\n",
             response.status);
        fclose(handle);
        return CMD_FAILED;
    }

    unsigned int blocks = fstat.st_size / sizeof(buffer);
    int last_block_size = fstat.st_size % sizeof(buffer);

    // printf("cdb[1] %d cdb[2] %d\n", cmd.cdb[1], cmd.cdb[2]);
    cmd.cdb[0] = TOOLBOX_SCSI_SEND_FILE_10;
    cmd.cdb_len = 10;
    /* set to 512 bytes */
    cmd.cdb[1] = sizeof(buffer) >> 8;
    cmd.cdb[2] = (unsigned char)sizeof(buffer);
    cmd.send_buffer_len = sizeof(buffer);
    cmd.send_buffer = buffer;

    for (unsigned int block_index = 0; block_index < blocks; block_index++)
    {
        fread(buffer, sizeof(unsigned char), sizeof(buffer), handle);
        if (scsi_cmd(target, &cmd, &response))
        {
            LOGF(NORMAL,
                 "toolbox_cmd_send_file() Error transferring block %d with "
                 "SCSI status 0x%X\n",
                 block_index, response.status);
            /* set this to skip that process since it is a lost cause */
            last_block_size = 0;
            break;
        }
    }

    if (last_block_size)
    {
        memset(buffer, 0, sizeof(buffer));
        cmd.send_buffer_len =
            fread(buffer, sizeof(unsigned char), sizeof(buffer), handle);
        if ((unsigned int)last_block_size != cmd.send_buffer_len)
        {
            LOGF(NORMAL,
                 "toolbox_cmd_send_file() Error last block size %d != bytes "
                 "read %d\n",
                 last_block_size, cmd.send_buffer_len);
            /* marker that it failed */
            last_block_size = -1;
        }
        else
        {
            cmd.cdb[1] = last_block_size >> 8;
            cmd.cdb[2] = last_block_size;
            if (scsi_cmd(target, &cmd, &response))
            {
                LOGF(NORMAL,
                     "toolbox_cmd_send_file() Error transferring last block "
                     "with SCSI status 0x%X\n",
                     response.status);
                /* marker that it failed */
                last_block_size = -1;
            }
        }
    }

    /* send an end/close */
    memset(&cmd, 0, sizeof(SCSI_CMD));
    cmd.cdb[0] = TOOLBOX_SCSI_SEND_FILE_END;
    cmd.cdb_len = 10;

    if (scsi_cmd(target, &cmd, &response))
    {
        LOGF(NORMAL,
             "toolbox_cmd_send_file() SEND_FILE_END failed with SCSI status "
             "0x%X\n",
             response.status);
        fclose(handle);
        return CMD_FAILED;
    }

    fclose(handle);

    /* check the marker */
    if (last_block_size < 0)
    {
        return CMD_FAILED;
    }

    return NO_ERROR;
}
