#ifndef __SCSI_DEVICE_H__
#define __SCSI_DEVICE_H__

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

extern unsigned char inquiry_cdb[6];

/* SCSI device types */
#define SCSI_DISK 0
#define SCSI_TAPE 1
#define SCSI_PRINTER 2
#define SCSI_PROCESSOR 3
#define SCSI_WORM 4
#define SCSI_CDROM 5
#define SCSI_SCANNER 6
#define SCSI_OPTICAL_MEMORY 7
#define SCSI_MEDIUM_CHANGER 8
#define SCSI_COMM 9
#define SCSI_UNKNOWN 0x1f

/* SCSI protocol structs */
typedef struct {
    unsigned char device_type;
    unsigned char peripheral_qualifier;
    unsigned char device_type_qualifier;
    unsigned char removeable;
    unsigned char ansi_version;
    unsigned char ecma_version;
    unsigned char iso_version;
    unsigned char response_data_format;
    unsigned char trmiop;
    unsigned char aenc;
    unsigned char additional_length;
    unsigned char sftre;
    unsigned char cmdque;
    unsigned char linked;
    unsigned char sync;
    unsigned char wbus16;
    unsigned char wbus32;
    unsigned char reladr;
    unsigned char vendor[16];
    unsigned char product[24];
    unsigned char revision[8];
} SCSI_INQUIRY;

typedef struct {
    int handle;
    char name[32];
    char addr[16];
    int is_toolbox_compatible;
    SCSI_INQUIRY inquiry_data;
    char sense_data[32];
} SCSI_DEVICE;

/* SCSI transport functions */
SCSI_DEVICE *scsi_open(char *device_name);
void scsi_close(SCSI_DEVICE *target);
int scsi_inquiry(SCSI_DEVICE *target);

int extract_inquiry_data(unsigned char *raw, SCSI_DEVICE *target);
#endif
