#include <stdio.h>
#include <strings.h>

#include "include/scsi_device.h"
#include "include/toolbox_commands.h"

unsigned char toolbox_count_files_cdb[12] = {0xD2, 0, 0, 0, 0, 0,
                                             0,    0, 0, 0, 0, 0};

unsigned char toolbox_list_files_cdb[12] = {0xD0, 0, 0, 0, 0, 0,
                                            0,    0, 0, 0, 0, 0};

/* toolbox info */
int toolbox_cmd_info(int argc, char **argv)
{
    /* use INQUIRY to get information about valid toolbox supporting SCSI
     * devices */

    return 0;
}
