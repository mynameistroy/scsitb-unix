#include "include/scsi_device.h"

/* SCSI command cdbs */
unsigned char inquiry_cdb[6] = {0x12, 0, 0, 0, 36, 0};

/* get all scsi devices */
SCSI_DEVICE *get_scsi_device_list(int only_toolbox_devices)
{
    int valid_targets = 0;
    /* find all valid scsi targets */

    /* create list of SCSI_DEVICE structs */
    SCSI_DEVICE *dlist = malloc(sizeof(SCSI_DEVICE) * valid_targets);
    if (NULL == dlist)
    {
        printf("Error allocating memory for dlist\n");
        return NULL;
    }

    return dlist;
}
