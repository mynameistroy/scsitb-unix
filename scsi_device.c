#include "include/scsi_device.h"

/* SCSI command cdbs */
unsigned char inquiry_cdb[6] = {0x12, 0, 0, 0, 36, 0};
