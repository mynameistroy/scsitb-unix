#include <unistd.h>
#include <sys/dir.h>
#include <dirent.h>
#include <fcntl.h>
#include <libc.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dev/scsireg.h>

#include "../include/scsi_device.h"
#include "../include/scsitb.h"
#include "../include/toolbox_commands.h"

extern int errno;

/* NeXT is weird in that all devices are handled via a single device */
#define NEXT_SCSI_DEVICE "/dev/sg0"

int get_scsi_device_list(SCSI_DEVICE ***device_list, unsigned int *count)
{
    if (NULL != *device_list)
    {
        LOG(VERBOSE, "get_scsi_device_list() invalid args\n");
        return INVALID_ARGS;
    }

    // Iterate over all SCSI ID's at LUN 0
    for (int id = 0; id < 8; id++)
    {
        unsigned char buffer[8];

        SCSI_DEVICE *dev = scsi_open(NEXT_SCSI_DEVICE);
        if (NULL == dev)
        {
            LOGF(VERBOSE, "get_scsi_device_list() Couldn't open scsi device %d;%d\n", id, dev->lun);
            continue;
        }

        /* set the SCSI ID */
        dev->id = id;
        sprintf(dev->name, "%d", id);

        /* send a request sense to determine if it is a valid device */
        SCSI_CMD cmd;

        memset(&cmd, 0, sizeof(SCSI_CMD));
        cmd.cdb[0] = 0x4; // REQUEST SENSE
        cmd.cdb_len = 6;
        cmd.recv_buffer = buffer;
        cmd.recv_buffer_len = sizeof(buffer);

        SCSI_CMD_RESPONSE response;

        if (scsi_cmd(dev, &cmd, &response))
        {
            scsi_close(dev);
            continue;
        } 

        /* send a toolbox command to validate if it is a compatible device */

        if (toolbox_cmd_get_capabilities(dev, buffer))
        {
            /* Not a compatible device */
            scsi_close(dev);
            continue;
        }
        else
        {
            /* supported so add to the list */
            int ret = 0;
            ret = scsi_inquiry(dev);
            if (0 != ret)
            {
                scsi_close(dev);
                continue;
            }
            unsigned char s2s_list[8] = {0};
            toolbox_cmd_list_devices(dev, s2s_list);
            dev->api = buffer[0];
            dev->capabilities = buffer[1];
            dev->s2s_type = s2s_list[dev->id];
            (*count)++;
            SCSI_DEVICE **new_device_list = realloc(*device_list, sizeof(SCSI_DEVICE *) * (*count));
            if (NULL == device_list)
            {
                LOG(VERBOSE,
                    "get_scsi_device_list() Couldn't alloc new device_list\n");
                return ALLOC_ERROR;
            }
            *device_list = new_device_list;
            (*device_list)[*count - 1] = dev;
        }
    }

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

    /* allocate a SCSI_DEVICE and fill it out */
    device = malloc(sizeof(SCSI_DEVICE));
    if (NULL == device)
    {
        LOG(VERBOSE, "scsi_open() SCSI_DEVICE allocation error\n");
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
    
    free(cmd.recv_buffer);
    return 0;
}

int scsi_cmd(SCSI_DEVICE *target, SCSI_CMD *cmd, SCSI_CMD_RESPONSE *response)
{
    int handle = 0;
    struct scsi_adr sa;
    struct scsi_req sr;

    memset(&sr, 0, sizeof(struct scsi_req));
    memset(response, 0, sizeof(SCSI_CMD_RESPONSE));

    if (NULL == target || NULL == cmd || NULL == response)
    {
        LOG(VERBOSE, "scsi_cmd() invalid arg\n");
        return INVALID_ARGS;
    }

    /* open the handle */
    handle = open(NEXT_SCSI_DEVICE, O_RDWR);
    if (handle < 0)
    {
        LOGF(VERBOSE, "scsi_cmd() open() failed for %s:%d:%s\n", NEXT_SCSI_DEVICE, target->id, 
             strerror(errno));
        return CMD_FAILED;
    }

    sa.sa_target = target->id;
    sa.sa_lun = 0; /* Anything beyond LUN 0 isn't supported yet */
    
    if (ioctl(handle, SGIOCSTL, &sa) < 0)
    {
        LOGF(VERBOSE, "scsi_cmd() SGIOCSTL failed for %s: %s\n", target->name, strerror(errno));
        close(handle);
        return CMD_FAILED;
    }

    /* NeXT doesn't seem to support read/write in the same SCSI command */
    if (cmd->recv_buffer_len && cmd->send_buffer_len)
    {
        LOG(VERBOSE, "scsi_cmd() Can't have bi-directional SCSI commands\n");
        close(handle);
        return CMD_FAILED;
    }

    switch(cmd->cdb_len)
    {
        case 6:
        {
            struct cdb_6 *cdb = &sr.sr_cdb.cdb_c6;
            cdb->c6_opcode = cmd->cdb[0];
            cdb->c6_lun = 0; 
            if (cmd->recv_buffer_len)
            {
                cdb->c6_len = cmd->recv_buffer_len;
                sr.sr_dma_dir = SR_DMA_RD;
                sr.sr_addr = (caddr_t)cmd->recv_buffer;
                sr.sr_dma_max = cmd->recv_buffer_len;
            }
            else
            {
                cdb->c6_len = cmd->send_buffer_len;
                sr.sr_dma_dir = SR_DMA_WR;
                sr.sr_addr = (caddr_t)cmd->send_buffer;
                sr.sr_dma_max = cmd->send_buffer_len;
            }
            
        }
        break;

        case 10:
        {
            struct cdb_10 *cdb = &sr.sr_cdb.cdb_c10;
            cdb->c10_opcode = cmd->cdb[0];
            cdb->c10_lun = 0;
            if (cmd->recv_buffer_len)
            {
                cdb->c10_len = cmd->recv_buffer_len;
                sr.sr_dma_dir = SR_DMA_RD;
                sr.sr_addr = (caddr_t)cmd->recv_buffer;
                sr.sr_dma_max = cmd->recv_buffer_len;
            }
            else
            {
                cdb->c10_len = cmd->send_buffer_len;
                sr.sr_dma_dir = SR_DMA_WR;
                sr.sr_addr = (caddr_t)cmd->send_buffer;
                sr.sr_dma_max = cmd->send_buffer_len;
            }
        }
        break;
    
        case 12:
        break;
        
        default:
            LOGF(VERBOSE, "scsi_cmd() Invalid or unsupported CDB size %d\n", cmd->cdb_len);
            close(handle);
            return CMD_FAILED;
    } 

    /* Set timeout */
    sr.sr_ioto = 10;
    
    // LOGF(VERBOSE, "scsi_cmd %d:%d %s\n", target->id, target->lun, target->name);

    if(ioctl(handle, SGIOCREQ, &sr) < 0)
    {
        LOGF(VERBOSE, "scsi_cmd() SGIOCREQ failed : %s\n", strerror(errno));
        response->error = errno;
        close(handle);
        return CMD_FAILED;
    }

    if (sr.sr_io_status)
    {
        LOGF(VERBOSE, "sr_io_status %d\n", sr.sr_io_status);
        response->status = sr.sr_io_status;
        if(sr.sr_io_status == SR_IOST_CHKSV)
        {
            LOGF(VERBOSE, "scsi_cmd() sense key 0x%02X, sense code 0x%02X\n", sr.sr_esense.er_sensekey, sr.sr_esense.er_addsensecode);
            response->asc = sr.sr_esense.er_addsensecode;
            response->ascq = sr.sr_esense.er_qualifier;
        }
        close(handle);
        return CMD_FAILED;
    }
    close(handle);
    
    return 0;
}
