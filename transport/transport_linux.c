#define _GNU_SOURCE

#include "../include/scsi_device.h"
#include "../include/scsitb.h"
#include "../include/toolbox_commands.h"

#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <libudev.h>
#include <limits.h>
#include <scsi/sg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

extern int errno;

int get_scsi_device_list(SCSI_DEVICE ***device_list, unsigned int *count)
{
    if (NULL != *device_list)
    {
        LOG(VERBOSE, "get_scsi_device_list() invalid args\n");
        return INVALID_ARGS;
    }

    /* check all targets in the /sys/bus/scsi/devices */
    DIR *dev_dir = opendir("/sys/bus/scsi/devices/");
    if (NULL == dev_dir)
    {
        LOGF(VERBOSE, "get_scsi_device_list() Couldn't open device dir %s\n",
             strerror(errno));
        return CMD_FAILED;
    }

    for (struct dirent *f = readdir(dev_dir); f != NULL; f = readdir(dev_dir))
    {
        /* only look at X:X:X:X entries */
        int colons = 0;
        for (size_t i = 0; i < strlen(f->d_name); i++)
        {
            if (':' == f->d_name[i])
            {
                colons++;
            }
        }
        if (3 != colons)
        {
            continue;
        }

        char block_dev_dir[384] = {0};
        snprintf(block_dev_dir, sizeof(block_dev_dir),
                 "/sys/bus/scsi/devices/%s/scsi_generic/", f->d_name);

        DIR *block_dir = opendir(block_dev_dir);
        if (NULL == block_dir)
        {
            LOGF(VERBOSE, "get_scsi_device_list() Couldn't open %s (%s)\n",
                 block_dev_dir, strerror(errno));
            closedir(dev_dir);
            return CMD_FAILED;
        }

        struct dirent *block_dev = readdir(block_dir);
        block_dev = readdir(block_dir);
        block_dev = readdir(block_dir);
        char block_dev_str[16] = {0};
        snprintf(block_dev_str, strlen(block_dev->d_name) + 6, "/dev/%s",
                 block_dev->d_name);

        /* /dev/sd* device acquired, now file out the SCSI_DEVICE struct */
        SCSI_DEVICE *dev = scsi_open(block_dev_str);
        if (NULL == dev)
        {
            printf("get_scsi_device_list() Couldn't open scsi device %s\n",
                   block_dev_str);
            closedir(block_dir);
            continue;
        }

        /* send a toolbox command to validate if it is a compatible device
         */
        unsigned char buffer[8];

        if (toolbox_cmd_get_capabilities(dev, buffer))
        {
            /* Not a compatible device */
            scsi_close(dev);
        }
        else
        {
            /* supported so add to the list */
            scsi_inquiry(dev);
            unsigned char s2s_list[8] = {0};
            toolbox_cmd_list_devices(dev, s2s_list);
            strcpy(dev->name, block_dev_str);
            dev->api = buffer[0];
            dev->capabilities = buffer[1];
            dev->s2s_type = s2s_list[dev->id];
            (*count)++;
            SCSI_DEVICE **new_device_list =
                realloc(*device_list, sizeof(SCSI_DEVICE *) * (*count));
            if (NULL == device_list)
            {
                printf(
                    "get_scsi_device_list() Couldn't alloc new device_list\n");
                return ALLOC_ERROR;
            }
            *device_list = new_device_list;
            (*device_list)[*count - 1] = dev;
        }
        closedir(block_dir);
    }

    closedir(dev_dir);

    return 0;
}

SCSI_DEVICE *scsi_open(char *device_name)
{
    SCSI_DEVICE *device = NULL;

    if (NULL == device_name)
    {
        LOG(VERBOSE, "scsi_open() Invalid device_name\n");
        return NULL;
    }

    /* open up a handle to the block device */

    /* allocate a SCSI_DEVICE and fill it out */
    device = malloc(sizeof(SCSI_DEVICE));
    if (NULL == device)
    {
        LOG(VERBOSE, "scsi_open() SCSI_DEVICE allocation error\n");
        return NULL;
    }

    /* open the handle */
    device->handle = open(device_name, O_RDWR);
    if (device->handle < 0)
    {
        LOGF(VERBOSE, "scsi_open() failed for %s:%s\n", device_name,
             strerror(errno));
        free(device);
        return NULL;
    }

    strcpy(device->name, basename(device_name));
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

    /* deallocate struct */
    free(target);
    target = NULL;
}

int scsi_inquiry(SCSI_DEVICE *target)
{
    if (NULL == target)
    {
        LOG(VERBOSE, "scsi_inquiry() Invalid SCSI target\n");
        return INVALID_ARGS;
    }

    /* perform INQUIRY */
    SCSI_CMD cmd;
    memset(&cmd, 0, sizeof(SCSI_CMD));
    memcpy(cmd.cdb, inquiry_cdb, sizeof(inquiry_cdb));
    cmd.cdb_len = sizeof(inquiry_cdb);
    cmd.recv_buffer = malloc(64);
    cmd.recv_buffer_len = 64;

    SCSI_CMD_RESPONSE response;

    if (scsi_cmd(target, &cmd, &response))
    {
        LOGF(VERBOSE,
             "scsi_inquiry() scsi_cmd() failed Errno:%d SCSI Status:%d\n",
             response.error, response.status);
        free(cmd.recv_buffer);
        return CMD_FAILED;
    }

    /* get the Channel:ID:LUN */
    struct sg_scsi_id id;
    if (ioctl(target->handle, SG_GET_SCSI_ID, &id))
    {
        LOGF(VERBOSE, "scsi_inquiry() SG_GET_SCSI_ID failed (%s)\n",
             strerror(errno));
        free(cmd.recv_buffer);
        return CMD_FAILED;
    }
    snprintf(target->addr, sizeof(target->addr), "%d:%d:%d", id.channel,
             id.scsi_id, id.lun);

    target->id = id.scsi_id;
    target->lun = id.lun;
    target->host_id = id.host_no;
    target->channel = id.channel;

    /* populate SCSI_DEVICE with information */
    if (extract_inquiry_data(cmd.recv_buffer, target))
    {
        LOG(VERBOSE, "scsi_inquiry() Error extracting inquiry data\n");
        free(cmd.recv_buffer);
        return CMD_FAILED;
    }

    /* get the string for device_type */
    char *dtype_str = device_type_to_str(target->inquiry_data.device_type);
    strncpy(target->device_type_name, dtype_str,
            sizeof(target->device_type_name));

    /* get host device information via udev */
    struct udev *udev = udev_new();
    if (NULL == udev)
    {
        LOG(VERBOSE, "scsi_inquiry() udev init failed\n");
        strncpy(target->host_device_name, "Unknown",
                sizeof(target->host_device_name));
    }
    else
    {
        char host_adapter[8];
        snprintf(host_adapter, sizeof(host_adapter) - 1, "host%d", id.host_no);
        struct udev_device *host = udev_device_new_from_subsystem_sysname(
            udev, "scsi_host", host_adapter);
        if (NULL == host)
        {
            LOGF(VERBOSE, "scsi_inquiry() Failed to find udev device at %s\n",
                 host_adapter);
        }
        else
        {
            /* get parent */
            struct udev_device *pci_dev =
                udev_device_get_parent_with_subsystem_devtype(host, "pci",
                                                              NULL);
            if (NULL == pci_dev)
            {
                LOGF(VERBOSE,
                     "scsi_inquiry() Failed to get pci device for %s\n",
                     host_adapter);
            }
            else
            {
                /* query database for vendor name */
                const char *vendor = udev_device_get_property_value(
                    pci_dev, "ID_VENDOR_FROM_DATABASE");

                if (NULL == vendor)
                {
                    vendor = udev_device_get_sysattr_value(pci_dev, "vendor");
                }

                if (NULL == vendor)
                {
                    strncpy(target->host_device_name, "Unknown",
                            sizeof(target->host_device_name) - 1);
                }
                else
                {
                    strncpy(target->host_device_name, vendor,
                            sizeof(target->host_device_name) - 1);
                }
            }
        }
        udev_device_unref(host);
    }
    udev_unref(udev);

    free(cmd.recv_buffer);
    return 0;
}

int scsi_cmd(SCSI_DEVICE *target, SCSI_CMD *cmd, SCSI_CMD_RESPONSE *response)
{
    unsigned char sense_info[32] = {0};

    sg_io_hdr_t io_hdr;
    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
    memset(response, 0, sizeof(SCSI_CMD_RESPONSE));

    if (NULL == target || NULL == cmd || NULL == response)
    {
        LOG(VERBOSE, "scsi_cmd() invalid arg\n");
        return INVALID_ARGS;
    }

    /* set scsi cmd direction */
    if (NULL != cmd->recv_buffer && NULL != cmd->send_buffer)
    {
        // printf("To & From Device, using send_buffer for both len:%d\n",
        //      cmd->send_buffer_len);
        io_hdr.dxfer_direction = SG_DXFER_TO_FROM_DEV;
        io_hdr.dxfer_len = cmd->send_buffer_len;
        io_hdr.dxferp = cmd->send_buffer;
    }
    else if (NULL == cmd->recv_buffer && NULL == cmd->send_buffer)
    {
        // printf("No data transfer to/from device\n");
        io_hdr.dxfer_direction = SG_FLAG_NO_DXFER;
        io_hdr.dxfer_len = 0;
        io_hdr.dxferp = NULL;
    }
    else if (NULL == cmd->send_buffer)
    {
        // printf("From Device, using recv_buffer len:%d\n",
        // cmd->recv_buffer_len);
        io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
        io_hdr.dxfer_len = cmd->recv_buffer_len;
        io_hdr.dxferp = cmd->recv_buffer;
    }
    else if (NULL == cmd->recv_buffer)
    {
        // printf("To Device, using send_buffer len:%d\n",
        // cmd->send_buffer_len);
        io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
        io_hdr.dxfer_len = cmd->send_buffer_len;
        io_hdr.dxferp = cmd->send_buffer;
    }

    /* setup the rest of the io struct */
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = cmd->cdb_len;
    io_hdr.cmdp = cmd->cdb;
    io_hdr.sbp = sense_info;
    io_hdr.mx_sb_len = sizeof(sense_info);
    io_hdr.timeout = 20000;
    io_hdr.flags = SG_FLAG_LUN_INHIBIT;

    LOGF(VERBOSE, "scsi_cmd() io_hdr cdb[1] 0x%X cdb[2] 0x%X\n", io_hdr.cmdp[1],
         io_hdr.cmdp[2]);

    /* perform scsi cmd */
    if (ioctl(target->handle, SG_IO, &io_hdr))
    {
        LOG(VERBOSE, "scsi_cmd() ioctl failed\n");
        response->error = errno;
        return CMD_FAILED;
    }

    /* check status */
    if (SG_INFO_OK != (io_hdr.info & SG_INFO_OK_MASK))
    {
        LOGF(VERBOSE,
             "scsi_cmd() failed (cmd: 0x%X status: 0x%X, host_status: 0x%X, "
             "driver_status: 0x%X\n",
             cmd->cdb[0], io_hdr.status, io_hdr.host_status,
             io_hdr.driver_status);
        response->status = io_hdr.masked_status;
        if (0 != io_hdr.sb_len_wr && 14 >= io_hdr.sb_len_wr)
        {
            /* there is some sense data to read
             * and it has enough data for Additional Sense Code (ADC)
             * Additional Sense Code Qualifier (ASCQ) */
            response->asc = sense_info[12];
            response->ascq = sense_info[13];
        }
        return CMD_FAILED;
    }

    return 0;
}
