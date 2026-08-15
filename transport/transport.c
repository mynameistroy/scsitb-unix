#include "../include/scsi_device.h"

/* convert device_type to string */
#define DTYPE_MAX_LEN 16
char *device_type_to_str(int device_type)
{
    switch (device_type)
    {
        case 0:
            return "Disk";
        case 1:
            return "Tape";
        case 2:
            return "Printer";
        case 3:
            return "Processor";
        case 4:
            return "WORM";
        case 5:
            return "CD-ROM";
        case 6:
            return "Scanner";
        case 7:
            return "Optical";
        case 8:
            return "Changer";

        default:
            return "Unknown";
    }
}

/* get cdb size */
int get_cdb_len(unsigned char *cdb)
{
    int group_code = cdb[0] >> 5;
    switch (group_code)
    {
        case 0:
            return 6;
        case 1:
        case 2:
            return 10;
        case 5:
            return 12;
        default:
            /* return nonsensical to likely force an error */
            return 0xff;
    }
}

#ifdef LITTLE_ENDIAN

/* extract byte values to populate SCSI_DEVICE inquiry data */
int extract_inquiry_data(unsigned char *raw, SCSI_DEVICE *target)
{
    if (NULL == raw || NULL == target)
    {
        printf("Invalid parameters\n");
        return -1;
    }

    /* peripheral device type */
    target->inquiry_data.device_type = raw[0] & 0x1F;

    /* removeable */
    target->inquiry_data.removeable = raw[1] >> 7;

    /* ANSI version */
    target->inquiry_data.ansi_version = raw[2] >> 5;

    /* command queue */
    target->inquiry_data.cmdque = (raw[7] >> 5) & 0x2;

    /* linked */
    target->inquiry_data.linked = (raw[7] >> 4) & 0xE;

    /* sync */
    target->inquiry_data.sync = (raw[7] >> 3) & 0xF;

    /* wide 16 */
    target->inquiry_data.wbus16 = (raw[7] >> 2) & 0x10;

    /* wide 32 */
    target->inquiry_data.wbus32 = (raw[7] >> 1 & 0x11);

    /* vendor */
    memset(target->inquiry_data.vendor, 0, sizeof(target->inquiry_data.vendor));
    memcpy(target->inquiry_data.vendor, &raw[8], 8);
    /* product */
    memset(target->inquiry_data.product, 0,
           sizeof(target->inquiry_data.product));
    memcpy(target->inquiry_data.product, &raw[16], 16);
    /* revision */
    memset(target->inquiry_data.revision, 0,
           sizeof(target->inquiry_data.revision));
    memcpy(target->inquiry_data.revision, &raw[32], 4);

    return 0;
}

#else
/* big endian */

#endif
