
#include "../include/scsi_device.h"

#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <scsi/sg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

extern int errno;

SCSI_DEVICE *scsi_open(char *device_name)
{
    SCSI_DEVICE *device = NULL;

    if (NULL == device_name)
    {
        printf("Invalid device_name");
        return NULL;
    }

    /* open up a handle to the block device */

    /* allocate a SCSI_DEVICE and fill it out */
    device = malloc(sizeof(SCSI_DEVICE));
    if (NULL == device)
    {
        printf("Allocation Error\n");
        return NULL;
    }

    /* open the handle */
    device->handle = open(device_name, O_RDWR);
    if (device->handle < 0)
    {
        printf("scsi_open failed for %s\n", device_name);
        free(device);
        return NULL;
    }
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
    unsigned char response[36] = {0};
    unsigned char sense_info[32] = {0};

    sg_io_hdr_t io_hdr;
    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));

    /* setup the io struct */
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(inquiry_cdb);
    io_hdr.cmdp = inquiry_cdb;
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = sizeof(response);
    io_hdr.dxferp = response;
    io_hdr.sbp = sense_info;
    io_hdr.mx_sb_len = sizeof(sense_info);
    io_hdr.timeout = 20000;

    if (NULL == target)
    {
        printf("Invalid SCSI target\n");
        return -1;
    }
    printf("SCSI_INQUIRY: %s\n", target->name);

    /* perform INQUIRY */
    if (ioctl(target->handle, SG_IO, &io_hdr))
    {
        printf("ioctl SG_IO failed (%d)\n", errno);
        return -1;
    }

    if (SG_INFO_OK != (io_hdr.info & SG_INFO_OK_MASK))
    {
        printf("SCSI INQUIRY failed (status: 0x%X, host_status: 0x%X, "
               "driver_status: 0x%X",
               io_hdr.status, io_hdr.host_status, io_hdr.driver_status);
        return -1;
    }

    /* get the Channel:ID:LUN */
    struct sg_scsi_id id;
    if (ioctl(target->handle, SG_GET_SCSI_ID, &id))
    {
        printf("ioctl SG_GET_SCSI_ID failed (%d)\n", errno);
        return -1;
    }
    snprintf(target->addr, sizeof(target->addr), "%d:%d:%d", id.channel,
             id.scsi_id, id.lun);

    /* populate SCSI_DEVICE with information */
    if (extract_inquiry_data(response, target))
    {
        printf("Error extracting inquiry data\n");
        return -1;
    }

    /* get the string for device_type */
    char *dtype_str = device_type_to_str(target->inquiry_data.device_type);
    strncpy(target->device_type_name, dtype_str,
            sizeof(target->device_type_name) - 1);
    free(dtype_str);

    return 0;
}
