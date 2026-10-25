#include "../include/scsi_device.h"
#include "../include/scsitb.h"
#include "../include/toolbox_commands.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/dsreq.h>

extern int errno;

#define RDSK_DIR "/dev/scsi"

int get_scsi_device_list(SCSI_DEVICE ***device_list, unsigned int *count)
{
    if (NULL != *device_list)
    {
        LOG(VERBOSE, "get_scsi_device_list() invalid args\n");
        return INVALID_ARGS;
    }

    /* check all targets available in RDSK_DIR */
    /* There is probably a better way, some day I might figure it out */
    DIR *dev_dir = opendir(RDSK_DIR);
    if (NULL == dev_dir)
    {
        LOGF(VERBOSE, "get_scsi_device_list() Couldn't open device dir %s\n",
             strerror(errno));
        return CMD_FAILED;
    }

    for (struct dirent *f = readdir(dev_dir); f != NULL; f = readdir(dev_dir))
    {
        char full_dev_path[64] = {0};
        unsigned char buffer[8] = {0};

        if (f->d_name[0] == '.')
            continue;

        snprintf(full_dev_path, sizeof(full_dev_path), "%s/%s", RDSK_DIR,
                 f->d_name);

        LOGF(VERBOSE, "Trying %s\n", full_dev_path);

        /* /dev/scsi/scXdxlx name constructed */
        SCSI_DEVICE *dev = scsi_open(full_dev_path);
        if (NULL == dev)
        {
            LOGF(VERBOSE,
                 "get_scsi_device_list(): Couldn't open scsi device "
                 "%s\n",
                 full_dev_path);
            continue;
        }

        LOG(VERBOSE, "get_scsi_device_list(): Toolbox check\n");
        if (toolbox_cmd_get_capabilities(dev, buffer))
        {
            /* not a compatible device */
            LOG(VERBOSE, "get_scsi_device_list(): toolbox check failed, regular SCSI device\n");
            scsi_close(dev);
        }
        else
        {
            /* supported so add to the list */
            unsigned char s2s_list[8] = {0};

            scsi_inquiry(dev);
            toolbox_cmd_list_devices(dev, s2s_list);

            dev->api = buffer[0];
            dev->capabilities = buffer[1];
            dev->s2s_type = s2s_list[dev->id];
            (*count)++;
            SCSI_DEVICE **new_device_list =
                realloc(*device_list, sizeof(SCSI_DEVICE *) * (*count));
            if (NULL == device_list)
            {
                printf("get_scsi_device_list() Couldn't alloc new device "
                       "list\n");
                return ALLOC_ERROR;
            }
            *device_list = new_device_list;
            (*device_list)[*count - 1] = dev;
        }
    }

    closedir(dev_dir);
    return NO_ERROR;
}

SCSI_DEVICE *scsi_open(char *device_name)
{
    SCSI_DEVICE *device = NULL;
    int handle = 0;
    char buf[2] = {0};

    if (NULL == device_name)
    {
        LOG(VERBOSE, "scsi_open() Invalid device_name\n");
        return NULL;
    }

    /* open a handle to the device */
    handle = open(device_name, O_RDWR);
    if (handle < 0)
    {
        LOGF(VERBOSE, "scsi_open() Failed for %s:%s\n", device_name,
             strerror(errno));
        return NULL;
    }

    /* allocate a SCSI_DEVICE */
    device = malloc(sizeof(SCSI_DEVICE));
    if (NULL == device)
    {
        LOG(VERBOSE, "scsi_open() SCSI_DEVICE allocation error\n");
        return NULL;
    }

    device->handle = handle;
    strcpy(device->device_path, device_name);
    strcpy(device->name, basename(device_name));

    // get the bus, id, lun
    buf[0] = basename(device_name)[2];
    device->host_id = atoi(buf);
    buf[0] = basename(device_name)[4];
    device->id = atoi(buf);
    buf[0] = basename(device_name)[6];
    device->lun = atoi(buf);

    LOGF(VERBOSE, "scsi_open() %s %s b:%d d:%d l:%d\n", device->device_path, device->name, device->host_id, device->id, device->lun);

    return device;
}

void scsi_close(SCSI_DEVICE *target)
{
    /* no-op */
    if (NULL == target)
    {
        return;
    }

    /* close the handle to the scsi device */
    close(target->handle);

    /* dealloc struct */
    free(target);
    target = NULL;
}

int scsi_inquiry(SCSI_DEVICE *target)
{
    SCSI_CMD cmd;
    SCSI_CMD_RESPONSE response;
    unsigned char buffer[96] = {0};

    if (NULL == target)
    {
        LOG(VERBOSE, "scsi_inquiry() Invalid SCSI target\n");
        return INVALID_ARGS;
    }

    /* perform INQUIRY */
    memset(&cmd, 0, sizeof(SCSI_CMD));
    memcpy(cmd.cdb, inquiry_cdb, sizeof(inquiry_cdb));
    cmd.cdb_len = sizeof(inquiry_cdb);
    cmd.recv_buffer = buffer;
    cmd.recv_buffer_len = sizeof(buffer);

    if (scsi_cmd(target, &cmd, &response))
    {
        LOGF(VERBOSE,
             "scsi_inquiry() scsi_cmd() failed errno:%d SCSI Status:0x%X\n",
             response.error, response.status);
        return CMD_FAILED;
    }

    /* populate SCSI_DEVICE with information */
    if (extract_inquiry_data(cmd.recv_buffer, target))
    {
        LOG(VERBOSE, "scsi_inquiry() Error extracting inquire data\n");
        return CMD_FAILED;
    }

    /* get the string for device_type */
    char *dtype_str = device_type_to_str(target->inquiry_data.device_type);
    strncpy(target->device_type_name, dtype_str,
            sizeof(target->device_type_name));

    return NO_ERROR;
}

int scsi_cmd(SCSI_DEVICE *target, SCSI_CMD *cmd, SCSI_CMD_RESPONSE *response)
{
    dsreq_t dsr;
    dsreq_t *dsp = &dsr;
    unsigned char sense_info[32] = {0};

    memset(&dsr, 0, sizeof(dsreq_t));

    if (NULL == target || NULL == cmd || NULL == response)
    {
        LOG(VERBOSE, "scsi_cmd() invalid arg\n");
        return INVALID_ARGS;
    }

    if (cmd->recv_buffer_len && cmd->send_buffer_len)
    {
        dsp->ds_flags |= DSRQ_READ | DSRQ_WRITE;    
        DATABUF(dsp) = cmd->recv_buffer;
        DATALEN(dsp) = cmd->recv_buffer_len;
    }
    else if (cmd->recv_buffer_len)
    {
        dsp->ds_flags |= DSRQ_READ;
        DATABUF(dsp) = cmd->recv_buffer;
        DATALEN(dsp) = cmd->recv_buffer_len;
    }
    else
    {
        dsp->ds_flags |= DSRQ_WRITE;
        DATABUF(dsp) = cmd->send_buffer;
        DATALEN(dsp) = cmd->send_buffer_len;
    }
    
    /* setup the rest of the dsreq_t struct */
    CMDBUF(dsp) = cmd->cdb;
    CMDLEN(dsp) = cmd->cdb_len;
    SENSEBUF(dsp) = sense_info;
    SENSELEN(dsp) = sizeof(sense_info);
    TIME(dsp) = 5000; 

    /* perform scsi cmd */
    if (ioctl(target->handle, DS_ENTER, &dsr))
    {
        LOG(VERBOSE, "scsi_cmd() ioctl failed\n");
        response->error = errno;
        return CMD_FAILED;
    }

    /* check status */
    if (0 != dsr.ds_status) 
    {
        LOGF(VERBOSE, "scsi_cmd() failed (cmd: 0x%X, status: 0x%X\n",
             cmd->cdb[0], dsr.ds_status);
        if (0 != dsr.ds_sensesent)
        {
            /* add additional sense data to response */
            response->asc = sense_info[12];
            response->ascq = sense_info[13];
        }
        else
        {
            LOG(VERBOSE, "scsi_cmd() request sense failed status\n")
        }
        return CMD_FAILED;
    }

    return NO_ERROR;
}
