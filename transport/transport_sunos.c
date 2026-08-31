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
#include <sys/scsi/impl/uscsi.h>
#include <sys/types.h>
#include <unistd.h>

extern int errno;

#define RDSK_DIR "/dev/rdsk"

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

        /* only want to try certain devices /dev/rdsk/cXtXdXs2 */
        if ('2' != f->d_name[7])
        {
            continue;
        }

        snprintf(full_dev_path, sizeof(full_dev_path), "%s/%s", RDSK_DIR,
                 f->d_name);

        LOGF(VERBOSE, "Trying %s\n", full_dev_path);

        /* /dev/rdsk/cXtXdXsX name constructed */
        SCSI_DEVICE *dev = scsi_open(full_dev_path);
        if (NULL == dev)
        {
            LOGF(VERBOSE,
                 "get_scsi_device_list(): Couldn't open scsi device "
                 "%s\n",
                 full_dev_path);
            continue;
        }

        if (toolbox_cmd_get_capabilities(dev, buffer))
        {
            /* not a compatible device */
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
    char volpath[64] = {0};
    int is_cdrom = 0;
    char buf[2] = {0};

    if (NULL == device_name)
    {
        LOG(VERBOSE, "scsi_open() Invalid device_name\n");
        return NULL;
    }

    /* open a handle to the device */
    handle = open(device_name, O_RDONLY);
    if (handle < 0)
    {
        /* Could be a CDROM so try seeing if we can get the volmgr alias */
        if (EBUSY == errno)
        {
            DIR *vol_alias_dir = NULL;
            struct dirent *voldev = NULL;

            /* try to open the volmgt alias dev directory */
            vol_alias_dir = opendir("/vol/dev/aliases");
            if (NULL == vol_alias_dir)
            {
                LOG(VERBOSE, "scsi_open() Unable to open /vol/dev/aliases\n");
                return NULL;
            }

            /* strip the slice */
            char *device_path = strdup(device_name);
            char *vol_device_name = basename(device_path);
            vol_device_name[7] = '\0';
            vol_device_name[6] = '\0';

            for (voldev = readdir(vol_alias_dir); NULL != voldev;
                 voldev = readdir(vol_alias_dir))
            {
                /* it is a symlink so get the real path */
                memset(volpath, 0, sizeof(volpath));
                char real_path[64] = {0};
                snprintf(volpath, sizeof(volpath), "/vol/dev/aliases/%s",
                         voldev->d_name);
                realpath(volpath, real_path);

                if (NULL != strstr(real_path, vol_device_name))
                {
                    handle = open(real_path, O_RDONLY | O_NDELAY);
                    if (handle < 0)
                    {
                        LOGF(VERBOSE, "scsi_open() Failed for %s:%s\n",
                             device_name, strerror(errno));
                        return NULL;
                    }
                    /* found a device that matches */
                    is_cdrom++;
                    break;
                }
            }
            free(device_path);
        }
        else
        {
            LOGF(VERBOSE, "scsi_open() Failed for %s:%s\n", device_name,
                 strerror(errno));
            return NULL;
        }
    }

    /* allocate a SCSI_DEVICE */
    device = malloc(sizeof(SCSI_DEVICE));
    if (NULL == device)
    {
        LOG(VERBOSE, "scsi_open() SCSI_DEVICE allocation error\n");
        return NULL;
    }

    device->handle = handle;
    if (!is_cdrom)
    {
        strcpy(device->device_path, device_name);
        strcpy(device->name, basename(device_name));
        /* get rid of the slice in the display name */
        device->name[7] = '\0';
        device->name[6] = '\0';
    }
    else
    {
        strcpy(device->device_path, volpath);
        strcpy(device->name, basename(volpath));
    }

    // get the bus, id, lun
    buf[0] = basename(device_name)[1];
    device->host_id = atoi(buf);
    buf[0] = basename(device_name)[3];
    device->id = atoi(buf);
    buf[0] = basename(device_name)[5];
    device->lun = atoi(buf);

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
    struct uscsi_cmd ucmd;
    unsigned char sense_info[32] = {0};

    memset(&ucmd, 0, sizeof(struct uscsi_cmd));

    if (NULL == target || NULL == cmd || NULL == response)
    {
        LOG(VERBOSE, "scsi_cmd() invalid arg\n");
        return INVALID_ARGS;
    }

    if (NULL != cmd->recv_buffer && NULL != cmd->send_buffer)
    {
        ucmd.uscsi_flags = USCSI_READ | USCSI_WRITE;
        ucmd.uscsi_buflen = cmd->send_buffer_len;
        ucmd.uscsi_bufaddr = (caddr_t)cmd->send_buffer;
    }
    else if (NULL == cmd->recv_buffer && NULL == cmd->send_buffer)
    {
        ucmd.uscsi_flags = 0;
        ucmd.uscsi_buflen = 0;
        ucmd.uscsi_bufaddr = NULL;
    }
    else if (NULL == cmd->send_buffer)
    {
        ucmd.uscsi_flags = USCSI_READ;
        ucmd.uscsi_buflen = cmd->recv_buffer_len;
        ucmd.uscsi_bufaddr = (caddr_t)cmd->recv_buffer;
    }
    else if (NULL == cmd->recv_buffer)
    {
        ucmd.uscsi_flags = USCSI_WRITE;
        ucmd.uscsi_buflen = cmd->send_buffer_len;
        ucmd.uscsi_bufaddr = (caddr_t)cmd->send_buffer;
    }

    /* setup the rest of the USCSI command struct */
    ucmd.uscsi_flags |= USCSI_RQENABLE;
    ucmd.uscsi_cdb = (caddr_t)cmd->cdb;
    ucmd.uscsi_cdblen = cmd->cdb_len;
    ucmd.uscsi_timeout = 60;
    ucmd.uscsi_rqbuf = (caddr_t)sense_info;
    ucmd.uscsi_rqlen = sizeof(sense_info);

    /* perform scsi cmd */
    if (ioctl(target->handle, USCSICMD, &ucmd))
    {
        LOG(VERBOSE, "scsi_cmd() ioctl failed\n");
        response->error = errno;
        return CMD_FAILED;
    }

    /* check status */
    if (0 != ucmd.uscsi_status)
    {
        LOGF(VERBOSE, "scsi_cmd() failed (cmd: 0x%X, status: 0x%X\n",
             cmd->cdb[0], ucmd.uscsi_status);
        if (0 == ucmd.uscsi_rqstatus)
        {
            /* add additional sense data to response */
            response->asc = sense_info[12];
            response->ascq = sense_info[13];
        }
        else
        {
            LOGF(VERBOSE, "scsi_cmd() request sense failed status: 0x%X\n",
                 ucmd.uscsi_rqstatus);
        }
        return CMD_FAILED;
    }

    return NO_ERROR;
}
