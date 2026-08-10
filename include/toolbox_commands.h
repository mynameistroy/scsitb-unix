#ifndef __TOOLBOX_COMMANDS_H__
#define __TOOLBOX_COMMANDS_H__

#include "scsi_device.h"

#define TOOLBOX_MAX_FILE_LEN 33

#define TOOLBOX_LEGACY_XFER_MODE 0
#define TOOLBOX_DEFAULT_XFER_MODE 1

/* From the BlueSCSI toolbox wiki */
typedef struct {
    unsigned char index; /* byte 00: file index in directory */
    unsigned char type;  /* byte 01: type 0 = file, 1 = directory */
    char name[33];       /* byte 02-34: filename (32 byte max) + space for NUL
                            terminator */
    unsigned char
        size[5]; /* byte 35-39: file size (40 bit big endian unsigned) */
} TOOLBOX_FILE;

typedef struct {
    unsigned char count;   /* count of files in directory */
    TOOLBOX_FILE *entries; /* directory entries */
} TOOLBOX_DIR;

/* toolbox SCSI cmd byte */
#define TOOLBOX_SCSI_LIST_FILES 0xD0
#define TOOLBOX_SCSI_GET_FILE 0xD1
#define TOOLBOX_SCSI_COUNT_FILES 0xD2
#define TOOLBOX_SCSI_SEND_FILE_PREP 0xD3
#define TOOLBOX_SCSI_SEND_FILE_10 0xD4
#define TOOLBOX_SCSI_SEND_FILE_END 0xD5
#define TOOLBOX_SCSI_TOGGLE_DEBUG 0xD6
#define TOOLBOX_SCSI_LIST_CDS 0xD7
#define TOOLBOX_SCSI_SET_NEXT_CD 0xD8
#define TOOLBOX_SCSI_METADATA 0xD9
#define TOOLBOX_SCSI_COUNT_CDS 0xDA

/* TOOLBOX TOOGLE_DEBUG subcommands */
#define TOOLBOX_SCSI_DEBUG_SET 0
#define TOOLBOX_SCSI_DEBUG_GET 1

/* TOOLBOX_METADATA subcommands */
#define TOOLBOX_SCSI_META_LIST_DEVICES 0
#define TOOLBOX_SCSI_META_GET_CAPABILITIES 0x1
#define TOOLBOX_SCSI_META_SET_WORKING_DIR 0x2
#define TOOLBOX_SCSI_META_GET_WORKING_DIR 0x3

/* TOOLBOX device type */
#define TOOLBOX_DEVICE_TYPE_FIXED 0
#define TOOLBOX_DEVICE_TYPE_REMOVABLE 1
#define TOOLBOX_DEVICE_TYPE_OPTICAL 2
#define TOOLBOX_DEVICE_TYPE_FLOPPY 3
#define TOOLBOX_DEVICE_TYPE_MAGNETO 4
#define TOOLBOX_DEVICE_TYPE_TAPE 5
#define TOOLBOX_DEVICE_TYPE_NETWORK 6
#define TOOLBOX_DEVICE_TYPE_ZIP100 7
#define TOOLBOX_DEVICE_TYPE_NONE 0xff

/* TOOLBOX_METADATA capability flags */
#define TOOLBOX_CAP_LARGE_TRANSFERS 0
#define TOOLBOX_CAP_LARGE_SEND 1 << 1
#define TOOLBOX_CAP_SET_WORKING_DIR 1 << 2

extern unsigned char toolbox_count_files_cdb[12];
extern unsigned char toolbox_list_files_cdb[12];
extern unsigned char toolbox_count_cds_cdb[12];
extern unsigned char toolbox_send_file_end_cdb[12];

/* free a toolbox_dir struct */
void toolbox_dir_free(TOOLBOX_DIR *dir);

/* get a count of files in a directory */
int toolbox_cmd_count_files(SCSI_DEVICE *target);

/* get the list of file/dir entries in a directory */
int toolbox_cmd_list_files(SCSI_DEVICE *target, TOOLBOX_DIR **dir);

/* get a file from a directory */
int toolbox_cmd_get_file(SCSI_DEVICE *target, char *src, char *dst);

/* send file from host */
int toolbox_cmd_send_file(SCSI_DEVICE *target, char *src, char *dst);

/* get a count of CD's */
int toolbox_cmd_count_cds(SCSI_DEVICE *target);

/* get the list of CD's */
int toolbox_cmd_list_cds(SCSI_DEVICE *target, TOOLBOX_DIR **dir);

/* set next CD image */
int toolbox_cmd_set_next_cd(SCSI_DEVICE *target, int index);

/* set/get debug */
int toolbox_cmd_set_debug(SCSI_DEVICE *target, int debug);
int toolbox_cmd_get_debug(SCSI_DEVICE *target, int *debug);

/* get toolbox metadata */
int toolbox_cmd_get_metadata(SCSI_DEVICE *target, unsigned char data_type,
                             unsigned char *metadata,
                             unsigned int *metadata_len);
int toolbox_cmd_list_devices(SCSI_DEVICE *target, unsigned char *device_list);
int toolbox_cmd_get_capabilities(SCSI_DEVICE *target,
                                 unsigned char capabilities[]);
int toolbox_cmd_set_working_dir(SCSI_DEVICE *target, char *directory);
int toolbox_cmd_get_working_dir(SCSI_DEVICE *target, char *directory,
                                unsigned int *dir_len);

char *toolbox_s2s_to_str(unsigned char s2s_type);
long toolbox_get_file_size(TOOLBOX_FILE *file);
#endif
