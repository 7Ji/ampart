/*
 * A simple, fast, yet powerful partition tool for Amlogic's proprietary emmc partition format
 *
 * Copyright (C) 2022 7Ji (pugokushin@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */


// Ampart is never intened to be included in other C/C++ programs, and that's the reason why I didn't write a header. It is an independent partition tool that should be used directly by users/scripts, and the interface to that use case is guaranteed to be the same, while due to the heavy development, the functions' definition and relationship are not guaranteed to not change. So, inclusion of ampart with your custom header will most likely ends up with broken calls when you pull a newer version of ampart. I would, in the future, abstract the core concepts into a header, but that's not something I would do before perfection of ampart the tool itself, let alone I need time to prepare for a big exam at the end of this year.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <libgen.h>
#include <linux/fs.h>
#include <unistd.h>
#include <zlib.h>
#include <time.h>

#define VERSION     "v0.1-development" // This should ONLY be uncommented for actual releases, for snapshot builds it will be generated using git commit hash instead
#define PART_NUM                  0x20 // This should ALWAYS be 0x20=32, regardless of platform. Defining it here saves some precious time used on sizeof()
#define SIZE_PART                 0x28 // This should ALWAYS be 0x28=40, regardless of platform. Defining it here saves some precious time used on sizeof()
#define SIZE_TABLE               0x518 // This should ALWAYS be 0x518=1304, regardless of platform. Defining it here saves some precious time used on sizeof()
#define SIZE_DTB_HEADER           0x28 // This should ALWAYS be 0x28=40, regardless of platform. Defining it here saves some precious time used on sizeof()
#define SIZE_MBR                 0x200 // Count of bytes that should be \0 for whole emmc disk, as stock Amlogic emmc should have no MBR. Things may become tricky if users have created a mbr partition table. In that case auto-recognization may not work for dumped image for whole emmc (for whole emmc disk DEVICE though, auto-recognization works without checking MBR)
#define SIZE_DTB               0x40000  //256K      
#define SIZE_DTB_UNCOMPRESS   0xA00000  //10M, yeah I know it's a lot, but hey, just in case there is a 'Super Compressor' that can compress this much data into a tiny 256K gzip (wow, a 40:1 ratio)
#define DTB_OFFSET            0x400000  //4M, relative to reserved part
#define PAGE_SIZE                0x800  //Minimum emmc I/O size

#define MAGIC_MULTI_DTB     0x5F4C4D41
#define MAGIC_SINGLE_DTB    0xEDFE0DD0
#define MAGIC_GZIPPED_DTB   0x00008B1F
#define MAGIC_XIAOMI_DTB    0x000089EF  // The BS Xiaomi proprietary DTB format, found on a user's Xiaomi mibox3s (Chinese firmware, the global firmware does not have such a format)
#define MAGIC_PHICOMM_DTB   0x000004DA  // The BS Phicomm proprietary DTB format, found on a user's Phicomm N1

#define PART_DTB_VERSION             1  // In fact, all existing devices use this version
#define PART_DTB_MAGIC      0x00447E41 
// Timestamp should not be updated, and checksum should be calculated live

// These are helper value used to parse around, to save extra process time after the first comparision using those magics listed above
#define DTB_TYPE_ILLEGAL             0  // We consider the initialized value 0 as illegal
#define DTB_TYPE_SINGLE              1
#define DTB_TYPE_MULTI               2
#define DTB_TYPE_GZIP                3

#define SYS_EMMC_DEVICE     "emmc:0001"
#define SYS_MMCBLK          "/sys/bus/mmc/drivers/mmcblk"
#define SYS_MMCBLK_UNBIND   SYS_MMCBLK"/unbind"
#define SYS_MMCBLK_BIND     SYS_MMCBLK"/bind"

#define RESERVED_DEV_NAMES  "amaudio amaudio_ctl amaudio_utils amstream_abuf amstream_hevc amstream_mpps amstream_mpts amstream_rm amstream_sub amstream_vbuf amstream_vframe amsubtitle amvdac amvecm amvideo amvideo_poll aocec autofs block bus char console cpu_dma_latency cvbs ddr_parameter disk display dtb efuse esm fd firmware_vdec framerate_dev full fuse hwrng initctl input ion kmem kmsg log mali mapper media mem mqueue net network_latency null port ppmgr ppp psaux ptmx pts random rfkill rtc secmem shm snd stderr stdin stdout tsync tty ubi_ctrl uhid uinput unifykeys urandom vad vcs vcsa vfm vhci watchdog wifi_power zero"

#define TABLE_UPDATE_ACTION_NORMAL    0
#define TABLE_UPDATE_ACTION_DELETE    1
#define TABLE_UPDATE_ACTION_CLONE     2

const unsigned char dtb_partitions_start[]={0, 0, 0, 1, 0x70, 0x61, 0x72, 0x74, 0x69, 0x74, 0x69, 0x6F, 0x6E, 0x73};
const unsigned char dtb_partitions_end[]= {0,0,0,2,0,0,0,2};
const unsigned char dtb_magic[] = {0xD0, 0x0D, 0xFE, 0xED} ; 

#define LEN_DTB_PARTITIONS_START 14
#define LEN_DTB_PARTITIONS_END 8


struct partition {
	char name[16];
	uint64_t size;
	uint64_t offset;
	unsigned mask_flags;
    // padding 4byte
};

struct insertion_helper {
    struct partition *part;
    bool insert;
};

struct partition_table {
	char magic[4];
	unsigned char version[12];
	int part_num;
	int checksum;
	struct partition partitions[32];
};

struct table_helper {
    struct partition_table *table;
    struct partition *bootloader;
    struct partition *reserved;
    struct partition *env;
    struct partition *logo;
    struct partition *misc;
    struct insertion_helper insertion[3];
    short insertables;
};

struct disk_helper {
    uint64_t start;
    uint64_t free;
    uint64_t size;
};

struct dtb_header {
    uint32_t magic;
    uint32_t totalsize;  // Change if partitions removed
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;  // Change if partitions removed
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;  // Change if partitions removed
    // The three bad-boys will be decreased on the same level
};

struct dtb_partition {
    unsigned char data[SIZE_DTB-16];
    unsigned int magic;
    unsigned int version;
	unsigned int timestamp;
	unsigned int checksum;
};

struct options {
    bool input_reserved;
    bool input_device;
    bool snapshot;
    bool mode_clone;
    bool mode_update;
    bool dryrun;
    bool no_reload;
    bool check_compatibility;
    bool no_node;
    int dtb_type;
    int dtb_subtype; // when type is gzipped, this stores the subtype (multi/single), otherwise always 0
    char path_input[128]; // For fuck's sake, will there be a maniac hide their image this deeply?
    char path_disk[128];
    char dir_input[64];
    char name_input[64];
    char output[64];
    uint64_t offset;
    uint64_t size;
} options = {0};

// Dedicated string buffer, so we don't need to allocate over and over again
char s_buffer_1[9]; 
char s_buffer_2[9];
char s_buffer_3[9];

uint32_t table_checksum(struct partition *part, int part_num) {
	int i, j;
	uint32_t checksum = 0, *p;
	for (i = 0; i < part_num; i++) {
		p = (uint32_t *)part;
		for (j = SIZE_PART/4;
				j > 0; --j) {
			checksum += *p;
			p++;
		}
	}
	return checksum;
}

unsigned int dtb_checksum(struct dtb_partition *dtb) {
    // All content of the dtb partition (256K) except the last 4 byte (checksum) is sumed for checksum (thus summing 256K-4)
    int i = 0;
	int size = SIZE_DTB - 4;
	unsigned int *buffer;
	unsigned int checksum = 0;

	size = size >> 2; // Integer divide by 4, shifting is faster
	buffer = (unsigned int *) dtb;
	while (i < size)
		checksum += buffer[i++];
	return checksum;
}

const char suffixes[]="BKMGTPEZY";  // static is not needed, when out of any function's scope
void size_byte_to_human_readable(char* buffer, uint64_t size) { 
    double num = size;
    int i;
    for (i=0; i<9; ++i) {
        if (num <= 1024.0) {
            break;
        }
        num /= 1024.0;
    }
    sprintf(buffer, "%4.2f%c", num, suffixes[i]);
    return;
}

// Sometimes if you don't want create a buffer and don't mind split printf
void size_byte_to_human_readable_print(uint64_t size) {
    double num = size;
    int i;
    for (i=0; i<9; ++i) {
        if (num <= 1024.0) {
            break;
        }
        num /= 1024.0;
    }
    printf("%4.2f%c", num, suffixes[i]);
    return;
}

// Only shrink to the lowest point where size is still an integer
void size_byte_to_human_readable_int(char* buffer, uint64_t size) {
     int i;
    for (i=0; i<9; ++i) {
        if (size <= 0x400 || size % 0x400) {
            break;
        }
        size /= 0x400;
    }
    sprintf(buffer, "%"PRIu64"%c", size, suffixes[i]);
    return;
}

// Fancier die function that supports formatting
void die (const char * format, ...) { 
    va_list vargs;
    va_start (vargs, format);
    fprintf (stderr, "ERROR: ");
    vfprintf (stderr, format, vargs);
    fprintf (stderr, "\n");
    va_end (vargs);
    exit (EXIT_FAILURE);
}

uint64_t four_kb_alignment(uint64_t size) {
    unsigned remainder = size % 0x1000; // Would this even be negative? Guess not
    if ( remainder ) {
        size_byte_to_human_readable(s_buffer_1, size);
        printf("Warning: size/offset %"PRIu64" (%s) is rounded up to ", size, s_buffer_1);
        size += (0x1000-remainder);
        size_byte_to_human_readable(s_buffer_1, size);
        printf("%"PRIu64" (%s) for 4K alignment\n", size, s_buffer_1);
    }
    return size;
}

uint64_t size_human_readable_to_byte(char * size_h, bool * has_prefix) {
    int length; // We manually obtain length here, because the pointer sent by getopt can not be used directly for obtaining the length
    if ( !(length=strlen(size_h)) ) {  // Die early for 0 length size_h
        return 0;
    }
    int start;
    uint64_t multiply;
    char *prefix, *ptr = size_h;
    char size_h_new[length+1];
    prefix = &size_h[0];
    if ( *prefix < '0' || *prefix > '9' ) {
        if (size_h[0] == '+') {
            if (has_prefix) {
                *has_prefix = true;
            }
            strcpy(size_h_new, ++ptr);
            start = 1;
        }
        else {
            die("Illegal prefix found! You should only use + as prefix!");
        }
    }
    else {
        if (has_prefix) {
            *has_prefix = false;
        }
        strcpy(size_h_new, ptr);
        start = 0;
    }
    char *suffix = &(size_h_new[length-1-start]);  // As the last should always be \0(NULL)
    if ( *suffix < '0' || *suffix > '9' ) {
        if ( *suffix == 'B' ) {
            multiply = 1;
        }
        else if ( *suffix == 'K' ) {
            multiply = 0x400;
        }
        else if ( *suffix == 'M' ) {
            multiply = 0x100000;
        }
        else if ( *suffix == 'G' ) {
            multiply = 0x40000000;
        }
        else {
            die("Illegal suffix found! You should only use B, K, M or G as suffix!");
        }
        *suffix='\0';  // Replace suffix with null, the size string should be shorter now
    }
    else {
        multiply = 1;
    }
    return strtoull(size_h_new, NULL, 10) * multiply;
}

uint64_t size_human_readable_to_byte_no_prefix(char * size_h, bool allow_empty) {
    int length; // We manually obtain length here, because the pointer sent by getopt can not be used directly for obtaining the length
    if ( !(length=strlen(size_h)) ) {  // Die early for 0 length size_h
        if (allow_empty) {
            return 0;
        }
        else {
            die("Offset/Size can NOT be empty");
        }
    }
    uint64_t multiply;
    char size_h_new[length+1];
    strcpy(size_h_new, size_h);
    char *suffix = &(size_h_new[length-1]);  // As the last should always be \0(NULL)
    if ( *suffix < '0' || *suffix > '9' ) {
        if ( *suffix == 'B' ) {
            multiply = 1;
        }
        else if ( *suffix == 'K' ) {
            multiply = 0x400;
        }
        else if ( *suffix == 'M' ) {
            multiply = 0x100000;
        }
        else if ( *suffix == 'G' ) {
            multiply = 0x40000000;
        }
        else {
            die("Illegal suffix found! You should only use B, K, M or G as suffix for offset/size!");
        }
        *suffix='\0';  // Replace suffix with null, the size string should be shorter now
    }
    else {
        multiply = 1;
    }
    return strtoull(size_h_new, NULL, 10) * multiply;
}

void no_device_names(char *name) {
    char device_names[] = RESERVED_DEV_NAMES;
    char *token = strtok(device_names, " ");
    while( token != NULL ) {
        if (!strcmp(token, name)) {
            die("Illegal partition name: '%s' will be used by other devices and should be AVOIDED", token);
        }
        token = strtok(NULL, " ");
    }
    return;
}

void valid_partition_name(char *name, bool allow_reserved) {
    int i, length;
    bool warned = false, warned_uppercase = false, warned_underscore = false;
    char *c;
    for(i=0; i<16; ++i) { // i can be 0-16, when it's 16, oops
        c = &name[i];
        if ( 'a' <= *c && *c <= 'z' ) {
            continue;
        }
        else if ( 'A' <= *c && *c <= 'Z' ) {
            if (!warned_uppercase) {
                if (!warned) {
                    printf("Warning: Misbehaviour found in partition name '%s':\n", name);
                    warned = true;
                }
                puts(" - Uppercase letter (A-Z) is used, this is not recommended as it may confuse some kernel and bootloader");
                warned_uppercase = true;
            }
        }
        else if ( *c == '_' ) {
            if (!warned_underscore) {
                if (!warned) {
                    printf("Warning: Misbehaviour found in partition name '%s':\n", name);
                    warned = true;
                }
                puts(" - Underscore (_) is used, this is not recommended as it may confuse some kernel and bootloader");
                warned_underscore = true;
            }
        }
        else if ( *c == '\0' ) {
            break;
        }
        else {
            die("You can only use a-z, A-Z and _ for the partition name!");
        }
    }
    if (i==0) {
        die("Empty partition name is not allowed!");
    };
    if (*c != '\0') {
        die("Partition name is too long and not terminated by NULL!");
    }
    if (!allow_reserved) {
        if (!strcmp(name, "bootloader") || !strcmp(name, "reserved") || !strcmp(name, "env")) {
            die("Illegal partition name '%s'! You can not use bootloader, reserved or env as partition name! These will be automatically generated by ampart from the old partition table!", name);
        }
    }
    no_device_names(name);
    return;
}

int get_part_argument_length (char *argument, bool allow_end) {
    // We don't use strtok, as it will ignore an empty arg between : and :
    int i;
    char *c=argument;
    bool valid=false;
    for (i=0; i<100; ++i) {  // I don't really think you can write an arg that long, but hey, just in case
        if (*c == ':') {
            *c = '\0'; // rewrite it to NULL so strcpy can play it safe
            valid=true;
            break;
        }
        else if (*c == '\0') {
            if (allow_end) {
                valid=true;
            }
            break;
        }
        ++c;
    }
    if (!valid) {
        die("Partation argument incomplete!");
    }
    return i; // the length here does not contain : or \0
}

void partition_from_argument (struct partition *partition, char *argument, struct disk_helper *disk) {
    char argument_new[strlen(argument)+1], *arg=argument_new;  // A seperate argument is used here so we won't mess up with the original one
    strcpy(argument_new, argument); 
    // Name
    int length = get_part_argument_length(argument_new, false);
    if (!length) {
        die("Partition name must be set!");
    }
    if ( length > 15 ) {
        die("Partition name '%s' too long! It can only be 15 characters long!", arg);
    }
    valid_partition_name(arg, false);
    strcpy(partition->name, arg);
    printf(" - Name: %s\n", partition->name);
    arg += length + 1;
    // Offset
    length = get_part_argument_length(arg, false);
    if ( !length ) {
        partition->offset=disk->start; // Auto increment offset
    }
    else {
        bool relative;
        partition->offset = four_kb_alignment(size_human_readable_to_byte(arg, &relative));
        if (relative) {
            partition->offset += disk->start;
        }
        if (partition->offset < disk->start) {
            size_byte_to_human_readable(s_buffer_1, partition->offset);
            size_byte_to_human_readable(s_buffer_2, disk->start);
            die("Partition offset smaller than end of last partition: %"PRIu64"(%s) < %"PRIu64"(%s)", partition->offset, s_buffer_1, disk->start, s_buffer_2);
        }
        if (partition->offset > disk->size) {
            size_byte_to_human_readable(s_buffer_1, partition->offset);
            size_byte_to_human_readable(s_buffer_2, disk->start);
            die("Partition offset greater than disk size:  %"PRIu64"(%s) > %"PRIu64"(%s)", partition->offset, s_buffer_1,  disk->start, s_buffer_2);
        }
    }
    size_byte_to_human_readable(s_buffer_1, partition->offset);
    printf(" - Offset: %"PRIu64" (%s)\n", partition->offset, s_buffer_1);
    arg += length + 1;
    // Size
    length = get_part_argument_length(arg, false);
    uint64_t partition_end;
    if ( !length ) {
        // Auto fill the rest
        partition->size = disk->size - partition->offset;
        partition_end = disk->size;
    }
    else {
        partition->size=four_kb_alignment(size_human_readable_to_byte_no_prefix(arg, true));
        partition_end = partition->offset + partition->size;
        if ( partition_end > disk->size) {
            size_byte_to_human_readable(s_buffer_1, partition_end);
            size_byte_to_human_readable(s_buffer_2, disk->free);
            die("Partition end point overflows! End point greater than disk size: %"PRIu64" (%s) > %"PRIu64" (%s)", partition_end, s_buffer_1, disk->free, s_buffer_2);
        }
    }
    disk->start = partition_end;
    disk->free = disk->size - partition_end;
    size_byte_to_human_readable(s_buffer_1, partition->size);
    printf(" - Size: %"PRIu64" (%s)\n", partition->size, s_buffer_1);
    arg += length + 1;
    // Mask
    length = get_part_argument_length(arg, true);
    if ( !length ) {
        partition->mask_flags=4;
    }
    else {
        partition->mask_flags=atoi(arg);
        if ((partition->mask_flags != 1) && (partition->mask_flags != 2) && (partition->mask_flags != 4)) {
            //printf("%d\n", partition->mask_flags);
            die("Invalid mask %u, must be either 1, 2 or 4, or leave it alone to use default value 4", partition->mask_flags);
        }
    }
    printf(" - Mask: %u\n", partition->mask_flags);
    return;
}

// Just basic filtering
void partition_from_argument_clone(struct partition *partition, char *argument) {
    char argument_new[strlen(argument)+1], *arg=argument_new;  // A seperate argument is used here so we won't mess up with the original one
    strcpy(argument_new, argument);
    // Name
    int length = get_part_argument_length(argument_new, false);
    if (!length) {
        die("Partition name must be set!");
    }
    if ( length > 15 ) {
        die("Partition name '%s' too long! It can only be 15 characters long!", arg);
    }
    valid_partition_name(arg, true);
    strcpy(partition->name, arg);
    printf(" - Name: %s\n", partition->name);
    arg += length + 1;
    // Offset
    length = get_part_argument_length(arg, false);
    if ( !length ) {
        die("In clone mode part offset MUST be set");
    }
    partition->offset = size_human_readable_to_byte_no_prefix(arg, false);
    size_byte_to_human_readable(s_buffer_1, partition->offset);
    printf(" - Offset: %"PRIu64" (%s)\n", partition->offset, s_buffer_1);
    arg += length + 1;
    // Size
    length = get_part_argument_length(arg, false);
    uint64_t partition_end;
    if ( !length ) {
        die("In clone mode part size MUST be set");
    }
    partition->size=size_human_readable_to_byte_no_prefix(arg, false);
    size_byte_to_human_readable(s_buffer_1, partition->size);
    printf(" - Size: %"PRIu64" (%s)\n", partition->size, s_buffer_1);
    arg += length + 1;
    // Mask
    length = get_part_argument_length(arg, true);
    if ( !length ) {
        die("In clone mode part mask MUST be set");
    }
    partition->mask_flags=atoi(arg);
    if ((partition->mask_flags != 0) && (partition->mask_flags != 1) && (partition->mask_flags != 2) && (partition->mask_flags != 4)) {
        //printf("%d\n", partition->mask_flags);
        die("Invalid mask %u, must be either 0, 1, 2 or 4", partition->mask_flags);
    }
    printf(" - Mask: %u\n", partition->mask_flags);
    return;
}

uint64_t get_max_part_end(struct partition_table *table) {
    int i;
    uint64_t end=0, part_end=0;
    struct partition * part=NULL;
    for (i=0; i<table->part_num; ++i) {
        part = &(table->partitions[i]);
        part_end = part->offset + part->size;
        if (part_end>end) {
            end=part_end;
        }
    }
    return end;
}


void table_update(struct partition_table *table, char * argument, struct disk_helper *disk) {
    disk->start = get_max_part_end(table);
    disk->free=disk->size-disk->start;
    char argument_new[strlen(argument)+1], *arg=argument_new; 
    strcpy(argument_new, argument);
    int length;
    if (argument[0] != '^') { // No selector, creation mode
        puts("No selector found, normal creation mode");
        if (table->part_num == PART_NUM) {
            die("Attempting to add partition when partition count is already at its maximum (32)");
        }
        partition_from_argument(&(table->partitions[(table->part_num)++]), arg, disk);
        return;
    }
    ++arg; // From the selector themselvies
    short action=TABLE_UPDATE_ACTION_NORMAL; // 0 for normal, 1 for delete, 2 for clone
    short relative=0;
    int id;
    if (arg[0] == '-') { // Tail selection
        puts("Notice: negative selector encountered");
        relative=-1;
    }
    else if (arg[0] >= '0' && arg[0] <= '9') { // Head selection
        puts("Notice: positive selector encountered");
        relative=1;
    }
    length=get_part_argument_length(arg, true);
    if (!length) {
        die("Partition selector must be EXPLICTLY set");
    }
    if (arg[length-1] == '?') {
        action=TABLE_UPDATE_ACTION_DELETE;
        arg[length-1] = '\0';
    }
    else if (arg[length-1] == '%') {
        action=TABLE_UPDATE_ACTION_CLONE;
        if (table->part_num == PART_NUM) {
            die("Trying to clone clone a partition when partition count is already at its maximum (32)");
        }
        arg[length-1] = '\0';
    }
    if (!length) {
        die("Partition selector must be EXPLICTLY set");
    }
    int i;
    struct partition *part=NULL;
    if (relative) {
        id = atoi(arg);
        printf("Relative selector before parsing: %i\n", id);
        if (relative == -1) {
            id = table->part_num + id;
        }
        if (id < 0 || id > table->part_num) {
            die("Illegal part id: %d. Max: %i", id, table->part_num);
        }
        part = &(table->partitions[id]);
    }
    else {
        struct partition *part_cmp=NULL;
        for (i=0; i<table->part_num; ++i) {
            part_cmp = &(table->partitions[i]);
            if (!strcmp(part_cmp->name, arg)) {
                id = i;
                part=part_cmp;
                break;
            }
        }
        if (part == NULL) {
            die("Failed to find a partition with name '%s' in the old partition table", arg);
        }
    }
    if (part == NULL) {
        // It's really unneccessary to check it again here, but just for safety
        die("No partition selected, refuse to continue");
    }
    printf("Partition selected: %i:%s\n", id, part->name);
    if (action==TABLE_UPDATE_ACTION_DELETE) {
        for (i=id; i<table->part_num-1; ++i) {  // Move all parts one backwards
            memcpy(&(table->partitions[i]), &(table->partitions[i+1]), SIZE_PART);
        }
        memset(&(table->partitions[i]), 0, SIZE_PART);
        --(table->part_num);
        printf("Deleted partition %i (oh, I forgot what it actually was)\n", id);
        return;
    }
    if (action==TABLE_UPDATE_ACTION_CLONE) {
        arg += length + 1;
        length = get_part_argument_length(arg, true);
        if (!length) {
            die("Partition name must be set for the new cloned partition");
        }
        if (length > 15) {
            die("Partition name '%s' for the new cloned partition too long! It can only be 15 characters long!", arg);
        }
        valid_partition_name(arg, false);
        memcpy(&(table->partitions[(table->part_num++)]), part, SIZE_PART);
        strcpy(table->partitions[table->part_num-1].name, arg);
        return;
    }
    // Modification mode
    // name
    arg += length + 1;
    length = get_part_argument_length(arg, false);
    if (length) {
        if (length > 15) {
            die("New partition name '%s' too long! It can only be 15 characters long!", arg);
        }
        valid_partition_name(arg, false);
        strcpy(part->name, arg);
        printf(" - Name -> %s\n", part->name);
    }
    // offset
    arg += length + 1;
    length = get_part_argument_length(arg, false);
    relative = 0;
    uint64_t temp;
    if (length) {
        if ( arg[0] == '-' ) {
            relative = -1;
        }
        else if (arg[0] == '+') {
            relative = 1;
        }
        if (relative) {
            temp = size_human_readable_to_byte_no_prefix(arg+1, false);
        }
        if (relative) {
            if (relative == 1) {
                part->offset += temp;
            }
            else {
                part->offset -= temp;
            }
        }
        else {
            part->offset = temp;
        }
        part->offset = four_kb_alignment(part->offset);
        size_byte_to_human_readable(s_buffer_1, part->offset);
        printf(" - Offset -> %"PRIu64"(%s)\n", part->offset, s_buffer_1);
    }
    // Size
    arg += length + 1;
    length = get_part_argument_length(arg, false);
    relative = 0;
    if (length) {
        if ( arg[0] == '-' ) {
            relative = -1;
        }
        else if (arg[0] == '+') {
            relative = 1;
        }
        if (relative) {
            temp = size_human_readable_to_byte_no_prefix(arg+1, false);
        }
        if (relative) {
            if (relative == 1) {
                part->size += temp;
            }
            else {
                part->size -= temp;
            }
        }
        else {
            part->size = temp;
        }
        part->size = four_kb_alignment(part->size);
        size_byte_to_human_readable(s_buffer_1, part->size);
        printf(" - Size -> %"PRIu64"(%s)\n", part->size, s_buffer_1);
    }
    // Mask
    arg += length + 1;
    length = get_part_argument_length(arg, true);
    if (length) {
        printf("%d\n", length);
        part->mask_flags=atoi(arg);
        printf("%d\n", part->mask_flags);
        puts(arg);
        if ((part->mask_flags != 1) && (part->mask_flags != 2) && (part->mask_flags != 4)) {
            die("Invalid mask %u, must be either 0, 1, 2 or 4, or leave it empty to not touch it", part->mask_flags);
        }
        printf(" - Mask -> %d", part->mask_flags);
    }
    return;
}

void valid_table_header(struct partition_table *table) {
    char good[] = ", GOOD √";
    printf("Partitions count: %d", table->part_num);
    if ( table->part_num > 32 || table->part_num < 3 ) {
        die("\nPartitions count illegal (%d), it must be between 3 and 32!", table->part_num);
    }
    else {
        puts(good);
    }
    printf("Magic: %s", table->magic);
    if (strcmp(table->magic, "MPT")) {
        die("\nMagic not supported (%s), Must be MPT!", table->magic);
    }
    else {
        puts(good);
    }
    printf("Version: %s", table->version);
    if (strcmp(table->version, "01.00.00")) {
        die("\nERROR: Version not supported (%s), Must be 01.00.00!", table->version);
    }
    else {
        puts(good);
    }
    int checksum = table_checksum(table->partitions, table->part_num);
    printf("Checksum: calculated %x, recorded %x", checksum, table->checksum);
    if ( checksum != table->checksum ) {
        die("\nERROR: Checksum mismatch!");
    }
    else {
        puts(good);
    }
}

void valid_partition_table(struct table_helper *table_h) {
    puts("Validating partition table...");
    struct partition_table *table = table_h->table;
    valid_table_header(table);
    short has_bootloader=false, has_reserved=false, has_env=false;
    struct partition *part;
    char names[table->part_num][16];
    char *name;
    int i, j;
    for (i=0; i<table->part_num; i++) {
        part = &(table->partitions[i]);
        name = part->name;
        for (j=0; j<i; j++) {
            if (!strcmp(name, names[j])) {
                die("Duplicated name found in partation table: %s", name);
            }
        }
        if (!strcmp(part->name, "bootloader")) {
            table_h->bootloader=part;
            has_bootloader = true;
        }
        else if (!strcmp(part->name, "reserved")) {
            if (options.input_reserved) {  // We would like to write to corresponding disk, so offset should be updated
                options.offset = part->offset;
                printf("Notice: offset of reserved partition found in table, using %"PRIu64" instead for the offset of reserved partition\n", options.offset);
            }
            table_h->reserved=part;
            has_reserved = true;
        }
        else if (!strcmp(part->name, "env")) {
            table_h->env=part;
            table_h->insertion[table_h->insertables++].part=part;
            has_env = true;
        }
        else if (!strcmp(part->name, "logo")) {
            table_h->logo=part;
            table_h->insertion[table_h->insertables++].part=part;
        }
        else if (!strcmp(part->name, "misc")) {
            table_h->misc=part;
            table_h->insertion[table_h->insertables++].part=part;
        }
        else {
            valid_partition_name(part->name, false);
        }
        strcpy(names[i], part->name);
    }
    if (!has_bootloader) {
        puts("Partition table does not have bootloader partition!");
    }
    if (!has_reserved) {
        puts("Partition table does not have reserved partition!");
    }
    if (!has_env) {
        puts("Partition table does not have env partition!");
    }
    if ((has_bootloader + has_reserved + has_env) < 3) {
        exit(EXIT_FAILURE);
    }
}

uint64_t summary_partition_table(struct partition_table *table) {
    char line[]="==============================================================";
    bool overlap=false;
    uint64_t offset=0, gap;
    puts(line);
    puts("NAME                          OFFSET                SIZE  MARK");
    puts(line);
    struct partition *part;
    for (int i = 0; i < table->part_num; ++i) {
        part = &(table->partitions[i]);
        if (offset != part->offset) {
            if (part->offset > offset) {
                gap = part->offset - offset;
                printf("  (GAP)                              ");
            }
            else {
                gap = offset - part->offset;
                overlap=true;
                printf("  (OVERLAP)                          ");
            }
            size_byte_to_human_readable(s_buffer_1, gap);
            printf("%+9llx(%+8s)\n", gap, s_buffer_1);
        };
        offset = part->offset + part->size;
        size_byte_to_human_readable(s_buffer_1, part->offset);
        size_byte_to_human_readable(s_buffer_2, part->size);
        printf("%-16s %+9llx(%+8s) %+9llx(%+8s)     %u\t\n", part->name, part->offset, s_buffer_1, part->size, s_buffer_2, part->mask_flags);
    }
    puts(line);
    if (overlap) {
        puts("Warning: Overlap found in partition table, this is extremely dangerous as your Android installation might already be broken.");
        puts(" - Please confirm if you've failed with other software");
        puts(" - Or have run some installtion scripts/tools that may duplicate data partition");
        puts(" - If your device bricks, do not blame on ampart as it's just the one helping you discovering it");
    }
    uint64_t size=get_max_part_end(table);
    size_byte_to_human_readable(s_buffer_1, size);
    printf("Disk size totalling %"PRIu64" (%s) according to partition table\n", size, s_buffer_1);
    return size;
}

void is_reserved(struct options *options) {
    if (!strcmp(options->dir_input, "/dev")) {
        printf("Path '%s' seems a device file\n", options->path_input);
        if (!strncmp(options->name_input, "mmcblk", 6)) {
            if (strlen(options->name_input) == 7) {
                options->input_reserved = false;
            }
            else {
                bool has_p = false;
                bool has_num = false;
                for (int i=6; i<strlen(options->name_input); ++i) {
                    if (options->name_input[i] == '\0') {
                        break;
                    }
                    if (!has_num && options->name_input[i] >= '0' && options->name_input[i] <='9') {
                        has_num = true;
                    }
                    if (!has_p && options->name_input[i] == 'p') {
                        has_p = true;
                    }
                }
                if (!has_num) {
                    printf("Warning: number not found in name (%s) which seems a device file, maybe your input path is incorret?\n", options->name_input);
                }
                if (has_p) {
                    options->input_reserved = true;
                }
                else {
                    options->input_reserved = false;
                }
            }

        }
        else if (!strcmp(options->name_input, "reserved")) {
            options->input_reserved = true;
        }
        else {
            printf("Warning: can not figure out whether '%s' is a reserved partition or a whole emmc disk, defaulting as reserved part", options->path_input);
            options->input_reserved = true;
        }
    }
    else { // A normal file
        printf("Path '%s' seems a dumped image, checking the head bytes to distinguish it...\n", options->path_input);
        char buffer[SIZE_MBR];
        FILE *fp = fopen(options->path_input, "r");
        if (fp==NULL) {
            die("Can not open path '%s' as read-only, check your permission!", options->path_input);
        }
        fread(buffer, SIZE_MBR, 1, fp);
        options->input_reserved = false;
        for (int i=0; i<SIZE_MBR; ++i) {
            if (buffer[i]) {
                options->input_reserved = true;
                break;
            }
        }
        fclose(fp);
    }
    if (options->input_reserved) {
        printf("Path '%s' detected as reserved partation\n", options->path_input);
    }
    else {
        printf("Path '%s' detected as whole emmc disk\n", options->path_input);
    }
}

void version() {
    puts("\nampart (AMLogic emmc partition tool) "VERSION"\nCopyright (C) 2022 7Ji (pugokushin@gmail.com)\nLicense GPLv3: GNU GPL version 3 <https://gnu.org/licenses/gpl.html>.\nThis is free software: you are free to change and redistribute it.\nThere is NO WARRANTY, to the extent permitted by law.\n");
}

void help(char *path) {
    printf("Usage: %s [reserved/emmc] [partition] ... [option [optarg]]... \n\n", path);
    char help_message[] =
        "positional arguments:\n"
        "\treserved/emmc\tpath to reserved partition (e.g. /dev/reserved) or a whole emmc (e.g. /dev/mmcblk0), can also be dumped image, default:/dev/reserved\n"
        "\t\t\tampart auto-recognises its type by name, if failed, assume it a reserved partition\n"
        "\tpartition\tpartition you want to add, format: name:offset:size:mask\n"
        "\t\t\tname: composed of a-z, A-Z, _(underscore, must not be the 1st character), 15 characters max\n"
        "\t\t\toffset: integer offset of the part in the whole disk, or starts with + for after the last part, multiplies of 4KB, can end with B,K,M,G\n"
        "\t\t\tsize: integer size of the part, or empty for filling the disk, multiplies of 4KB, can end with B,K,M,G\n"
        "\t\t\tmask: mask of the part, either 1, 2 or 4, 0 is reserved for clone mode only, 1 for u-boot parts like logo,2 for system parts, 4 for data parts, default: 4\n"
        "\t\t\te.g.\tdata::: a data partition, auto offset, fills all the remaining space, mask 4\n"
        "\t\t\t\tsystem::2G:2 a system parrtition, auto offset, 2G, mask 2\n"
        "\t\t\t\tcache:+8M:512M:2 a cache partition, offset 8M after the last part, 512M, mask 2\n\n"
        "options:\n"
        "\t--version-v\tprint the version message, early quit\n"
        "\t--help/-h\tdisplay this message, early quit\n"
        "\t--disk/-d\tno auto-recognization, treat input as a whole disk\n"
        "\t--reserved/-r\tno auto-recognization, treat input as a reserved partition\n"
        "\t--offset/-O [offset]\n"
        "\t\t\toverwrite the offset of the reserved partition, in case OEM has modified it\n\t\t\tonly valid when input is a whole disk, default:36M\n"
        "\t--snapshot/-s\toutputs partition arguments that can be used to restore the partition table to what it looks like now, early quit\n"
        "\t--clone/-c\trun in clone mode, parse partition arguments outputed by --snapshot/-s, no filter\n"
        "\t--update/-u\trun in update mode, exact partition can be selected and altered\n"
        "\t--no-node/-N\tdon't try to remove partitions node from dtb\n"
        "\t--dry-run/-D\tdo not actually update the part table\n"
        "\t--partprobe/-p\tforce a notification to the kernel about the partition layout change, early quit\n"
        "\t--no-reload/-n\tdo not notify kernel about the partition layout changes, remember to end your session with a --partprobe call\n\t\t\tif you are calling ampart for multiple times in a script\n"
        "\t--output/-o [path]\n"
        "\t\t\twrite the updated part table to somewhere else, instead of the input itself\n";
    puts(help_message);
}

void get_options(int argc, char **argv) {
    // options is a global variable
    int c, option_index = 0;
    options.offset = 0x2400000; // default offset, 32M
    static struct option long_options[] = {
        {"version", no_argument,        NULL,   'v'},
        {"help",    no_argument,        NULL,   'h'},
        {"disk",    no_argument,        NULL,   'd'},   // The input file is a whole disk (e.g. /dev/mmcblk0) or image file alike
        {"reserved",no_argument,        NULL,   'r'},   // The input file is a reserved partation (e.g. /dev/reserved) or image file alike
        {"offset",  required_argument,  NULL,   'O'},   // offset of the reserved partition in disk
        {"snapshot",no_argument,        NULL,   's'},
        {"clone",   no_argument,        NULL,   'c'},
        {"update",  no_argument,        NULL,   'u'},
        {"no-node", no_argument,        NULL,   'N'},
        {"dry-run", no_argument,        NULL,   'D'},
        {"partprobe",no_argument,       NULL,   'p'},
        {"no-reload",no_argument,       NULL,   'n'},
        {"output",  required_argument,  NULL,   'o'},   // Optionally write output to a given path, instead of write it back
        {NULL,      0,                  NULL,    0},    // This is needed for when user just mess up with their input
    };
    char buffer[9];
    bool input_disk = false;
    while ((c = getopt_long(argc, argv, "vhdrO:scuCNDpno:", long_options, &option_index)) != -1) {
        int this_option_optind = optind ? optind : 1;
        switch (c) {
            case 'v':
                version();
                exit(EXIT_SUCCESS);
            case 'd':
                puts("Notice: forcing input as a whole disk (e.g. /dev/mmcblk0)");
                input_disk = true;
                break;
            case 'r':
                puts("Notice: forcing input as a reserved part (e.g. /dev/reserved)");
                options.input_reserved = true;
                break;
            case 'O':
                options.offset = four_kb_alignment(size_human_readable_to_byte(optarg, NULL));
                size_byte_to_human_readable(buffer, options.offset);
                printf("Using offset %"PRIu64" (%s) for reserved partition\n", options.offset, buffer);
                puts("Warning: unless your reserved partition offset has been modified by the OEM, you should not overwrite the offset");
                break;
            case 's':
                puts("Notice: running in snapshot mode, new partition table won't be parsed");
                options.snapshot = true;
                break;
            case 'c':
                puts("Notice: running in clone mode, partition arguments won't be filtered, reserved partitions can be set");
                options.mode_clone = true;
                break;
            case 'u':
                puts("Notice: running in update mode");
                options.mode_update = true;
                break;
            case 'N':
                puts("Notice: running in no-partitions-node-removal mode\n - ampart will not check dtb or try to remove the partitions node from it\n - unless you are sure your u-boot won't check and revert the partition table according to partitions node in dtb, \n - you should NOT enable this.\n - as your new partitions may only be available during this boot.");
                options.no_node = true;
                break;
            case 'D':
                puts("Notice: running in dry-run mode, changes won't be written to disk");
                options.dryrun = true;
                break;
            case 'p':
                puts("Notice: forcing partprobe");
                reload_emmc();
                exit(EXIT_SUCCESS);
            case 'n':
                puts("Notice: no-reload is set, emmc driver will not be reloaded to recognize all the changes in part table\n - Unless you are using ampart in a script, you should avoid --no-reload as you may accidently write thing to wrong locations on emmc\n - If you are though, remember to call ampart with --partprobe at the end to reload properly");
                options.no_reload = true;
                break;
            case 'o':
                strcpy(options.output, optarg);
                printf("Notice: output to will be written to %s\n", options.output);
                break;
            default:
                version();
                help(argv[0]);
                exit(EXIT_SUCCESS);
        }
    }
    if ( input_disk && options.input_reserved ) {
        die("You CAN NOT force the input both as whole disk and as reserverd partition!");
    }
    if (options.mode_clone && options.mode_update) {
        die("You can only run one of the three modes: normal, clone or update\n - You've specicified to run in both clone and update mode which is not allowed");
    }
    if ( optind == argc ) {
        help(argv[0]);
        die("You must specify a path to reserved partition (e.g./dev/reserved) or whole emmc disk (e.g./dev/mmcblk0) or image files dumped from them!");
    }
    // First argument must be path
    strcpy(options.path_input, argv[optind++]);
    // Copy path to a new string so basename and dirname won't screw it up
    char path_new[strlen(options.path_input)+1];
    strcpy(path_new, options.path_input);
    strcpy(options.name_input, basename(path_new));
    strcpy(options.dir_input, dirname(path_new));
    options.input_device=!strcmp(options.dir_input, "/dev");
    if ( !input_disk && !options.input_reserved ) {
        is_reserved(&options);
    } // From here onward we only need input_reserved
    return;
}

uint64_t get_disk_size() {
    struct stat stat_input;
    if (stat(options.path_input, &stat_input)) {
        die("Path '%s' does not exist!", options.path_input);
    }
    uint64_t size_disk = 0;
    if (options.input_device) {
        printf("Path '%s' is a device, getting its size via ioctl\n", options.path_input);
        unsigned major_reserved = major(stat_input.st_rdev);
        unsigned minor_reserved = minor(stat_input.st_rdev);
        char *path_device = NULL;
        if (options.input_reserved) {
            puts("Finding corresponding disk for reserved partition...");
            DIR *d;
            struct dirent * dir;
            struct stat stat_device;
            char path_sys_block_reserved[20 + strlen(options.name_input)];  // /sys/block/mmcblkN/ is 19, 
            char path_device_full[13]; // 12 for /dev/mmcblkN
            d = opendir("/sys/block");
            if ( d == NULL ) {
                die("Failed to open /sys/block for scanning, check your permission");
            }
            while((dir = readdir(d)) != NULL) {
                if (!strncmp(dir->d_name, "mmcblk", 6) && strlen(dir->d_name) == 7) {
                    sprintf(path_sys_block_reserved, "/sys/block/%s/%s", dir->d_name, options.name_input);
                    if (stat(path_sys_block_reserved, &stat_device)) {
                        continue;
                    }
                    else {
                        sprintf(path_device_full, "/dev/%s", dir->d_name);
                        printf("Corresponding disk for '%s' is '%s'\n", options.path_input, path_device_full);
                        path_device = path_device_full;
                        closedir(d);
                        break;
                    }
                }
            }
            if (path_device == NULL) {
                closedir(d);
                die("Failed to find corresponding mmcblk device for '%s'", options.path_input);
            }
        }
        else {
            path_device = options.path_input;
        }
        int fd = open(path_device, O_RDONLY);
        if (fd == -1) {
            close(fd);
            die("Failed to open device file '%s' as read-only via ioctl! Check your permission.\n", options.path_input);
        }
        if (ioctl(fd, BLKGETSIZE64, &size_disk)==-1) {
            close(fd);
            die("Failed to get size info from '%s' via ioctl!\n", options.path_input);
        }
        strcpy(options.path_disk, path_device);
        close(fd);
    }
    else{
        if (options.input_reserved) {
            printf("Warning: Input '%s' is an image file for a reserved partition, we can only get disk size via the partition table latter\n", options.path_input);
            puts(" - The size may be incorrect due to disk may be not 100 percent parted");
            puts(" - But you should be fine it you've never touched the partition table");
        }
        else {
            printf("Path '%s' is an image file for a whole emmc disk, getting its size via stat\n",  options.path_input);
            size_disk = stat_input.st_size;
        }
    }
    if (size_disk) {
        size_byte_to_human_readable(s_buffer_1, size_disk);
        printf("Disk size is %"PRIu64" (%s)\n", size_disk, s_buffer_1);
    }
    return size_disk;
}

uint64_t read_partition_table(struct table_helper *table_h) {
    puts("Reading old partition table...");
    struct partition_table *table = table_h->table;
    // int fd=open(options.path_input, O_RDONLY);
    FILE *fp = fopen(options.path_input, "r");
    if ( fp == NULL ) {
        die("ERROR: Can not open input path as read mode, check your permission!\n");
    }
    char buffer[9];
    if (!options.input_reserved) {
        size_byte_to_human_readable(buffer, options.offset);
        printf("Notice: Seeking %"PRIu64" (%s) (offset of reserved partition) into disk\n", options.offset, buffer);
        fseek(fp, options.offset, SEEK_SET);
    }
    fread(table, SIZE_TABLE, 1, fp);
    valid_partition_table(table_h);
    printf("Partition table read from '%s':\n", options.path_input);
    uint64_t size=summary_partition_table(table);
    get_dtb_type(fp);
    fclose(fp); 
    return size;
};

void reload_emmc() {
    // Notifying kernel about emmc partition table change
    sync();
    puts("Notifying kernel about partition table change...");
    puts("We need to reload the driver for emmc as the meson-mmc driver does not like partition table being hot-updated");
    printf("Opening '%s' so we can unbind driver for '%s'\n", SYS_MMCBLK_UNBIND, SYS_EMMC_DEVICE);
    FILE *fp = fopen(SYS_MMCBLK_UNBIND, "w");
    if ( fp == NULL ) {
        die("ERROR: can not open '%s' for unbinding driver for '%s', \n - You will need to reboot manually for the new partition table to be picked up by kernel \n - DO NOT access the old parititions under /dev for now as they are not updated!", SYS_MMCBLK_UNBIND, SYS_EMMC_DEVICE);
    }
    fputs(SYS_EMMC_DEVICE, fp);
    fclose(fp);
    puts("Successfully unbinded the driver, all partitions and the disk itself are not present under /dev as a result of this");
    printf("Opening '%s' so we can bind driver for '%s'\n", SYS_MMCBLK_BIND, SYS_EMMC_DEVICE);
    fp = fopen(SYS_MMCBLK_BIND, "w");
    if ( fp == NULL ) {
        die("ERROR: can not open '%s' for binding driver for '%s', \n - You will need to reboot manually for the new partition table to be picked up \n - You can not access the old partitions and the disk for now.", SYS_MMCBLK_BIND, SYS_EMMC_DEVICE);
    }
    fputs(SYS_EMMC_DEVICE, fp);
    fclose(fp);
    puts("Successfully binded the driver, you can use the new partition table now!");
}

void snapshot(struct partition_table * table) {
    puts("Give one of the following partition arguments with options --clone/-c to ampart to reset the partition table to what it looks like now:");
    struct partition *part;
    for (int i=0; i<table->part_num; ++i) {
        part=&(table->partitions[i]);
        size_byte_to_human_readable_int(s_buffer_1, part->offset);
        size_byte_to_human_readable_int(s_buffer_2, part->size);
        printf("%s:%s:%s:%u ", part->name, s_buffer_1, s_buffer_2, part->mask_flags);
    }
    putc('\n', stdout);
    for (int i=0; i<table->part_num; ++i) {
        part=&(table->partitions[i]);
        printf("%s:%"PRIu64":%"PRIu64":%u ", part->name, part->offset, part->size, part->mask_flags);
    }
    putc('\n', stdout);
    exit(EXIT_SUCCESS);
}

void no_coreelec() {
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    fp = fopen("/etc/os-release", "r");
    if (fp == NULL) {
        puts("Warning: can not open /etc/os-release to check for system, if you are running CoreELEC you should not use ampart as ceemmc is suggested by CoreELEC officially");
        return;
    }
    while ((read = getline(&line, &len, fp)) != -1) {
        if (!strcmp(line, "NAME=\"CoreELEC\"\n")) {
            fclose(fp);
            if (line) {
                free(line);
            }
            die("Refused to run on CoreELEC, you should use ceemmc instead as it's the official installation tool approved by Team CoreELEC\n - Altering the source code to force ampart to run is strongly not recommended\n - Even ampart is a partition tool and ceemmc is an installtion tool, they are different\n - But its manipulating of the partition table is dangerous\n - Modifying the part table in an approved way by Team CoreELEC will be way much safer");
        }
    }
    fclose(fp);
    if (line) {
        free(line);
    }
}

struct table_helper * parse_table(struct disk_helper *disk, struct table_helper *table_h, int *argc, char **argv) {
    struct table_helper *table_h_new = calloc(1, sizeof(struct table_helper));
    struct partition_table *table_new = calloc(1, SIZE_TABLE);
    table_h_new->table=table_new;
    struct partition *partition_new;
    char *partition_arg;
    int partitions_count = *argc - optind;
    int i=0;
    if ( options.mode_clone ) {
        if (partitions_count > 32) {
            die("You've defined too many partitions (%d>32), this is forbidden in clone mode", partitions_count);
        }
        while ( optind < *argc && i < 32 ) {
            partition_new = &(table_new->partitions[i++]);
            partition_arg = argv[optind++];
            printf("Parsing user input for partition (clone mode): %s\n", partition_arg);
            partition_from_argument_clone(partition_new, partition_arg);
        }
        table_new->part_num = partitions_count;
    }
    else if ( options.mode_update ) {
        memcpy(table_new, table_h->table, SIZE_TABLE);
        // Counts of partition operations is not limited here
        while ( optind < *argc ) {
            partition_arg = argv[optind++];
            printf("Parsing user input for partition operation (update mode): %s\n", partition_arg);
            table_update(table_new, partition_arg, disk);
        }
    }
    else {  // Normal mode
        disk->start=table_h->bootloader->offset + table_h->bootloader->size; // Just in case one day bootloader does not start at 0
        insertion_optimizer(table_h->insertion, table_h->reserved->offset-disk->start, table_h->insertables);
        int j=0;
        memcpy(&(table_new->partitions[j++]), table_h->bootloader, SIZE_PART);
        for (i=0; i<table_h->insertables; ++i) {
            if (table_h->insertion[i].insert) {
                memcpy(&(table_new->partitions[j]), table_h->insertion[i].part, SIZE_PART);
                table_new->partitions[j].offset = disk->start;
                disk->start += table_new->partitions[j++].size;
            }
        }
        memcpy(&(table_new->partitions[j++]), table_h->reserved, SIZE_PART);
        disk->start = table_h->reserved->offset + table_h->reserved->size;
        for (i=0; i<table_h->insertables; ++i) {
            if (!table_h->insertion[i].insert) {
                memcpy(&(table_new->partitions[j]), table_h->insertion[i].part, SIZE_PART);
                table_new->partitions[j].offset = disk->start;
                disk->start += table_new->partitions[j++].size;
            }
        }
        disk->free = disk->size - disk->start;
        size_byte_to_human_readable(s_buffer_1, disk->free);
        printf("Usable space of the disk is %"PRIu64" (%s)\n", disk->free, s_buffer_1);
        puts("Make sure you've backed up the env, logo and misc partitions as their offset will mostly change,\n - even ampart will auto-migrate them for you\n - This is because all user-defined partitions will be created after the reserved partition\n - Yet most likely these old partitions have gap before them\n - Which wastes a ton of space if we start at there");
        if (partitions_count > 32 - j) {
            partitions_count = 32 - j;
            printf("Warning: You've defined too many partitions, only %d of them will be accepted\n - The theoretical maximum count of parts to have is 32\n - But 3 are reserved for bootloader, reserved and env\n And %d are reserved for logo and misc if they were present", partitions_count, j-3);
        }
        // struct partition partitions_new[partitions_count];
        // memset(partitions_new, 0, sizeof(partitions_new));
        i = 0;
        while ( optind < *argc && j < partitions_count + j ) { 
            partition_new = &(table_new->partitions[j++]);
            partition_arg = argv[optind++];
            printf("Parsing user input for partition: %s\n", partition_arg);
            partition_from_argument(partition_new, partition_arg, disk);
        }
        //memcpy(table_new, table, 16); // 4byte for magic, 16byte for version
        table_new->part_num = j;
    }
    memcpy(table_new, table_h->table, 16);// 4byte for magic, 12byte for version
    table_new->checksum = table_checksum(table_new->partitions, table_new->part_num);
    puts("New partition table is generated successfully in memory");
    valid_partition_table(table_h_new);
    summary_partition_table(table_new);
    return table_h_new;
}

void no_mounted(struct partition_table *table) {
    FILE *fp = fopen("/proc/mounts", "r");
    if (fp == NULL) {
        fclose(fp);
        die("Can not open /proc/mounts for checking if partation is mounted");
    }
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int i;
    char target[22]; // 5 for /dev/, 15 for name, 1 for space, 1 for null
    struct partition *parts = table->partitions;
    struct partition *part;
    while ((read = getline(&line, &len, fp)) != -1) {
        if (strncmp(line, "/dev/", 5)) {
            continue;
        }
        if (!strncmp(line, options.path_disk, 12)) {
            char *token = strtok(line, " ");
            die("Partition '%s' on the disk is mounted, refuse to continue", token);
        }
        for (i=0; i<table->part_num; ++i) {
            part = &parts[i];
            sprintf(target, "/dev/%s ", part->name);
            if (!strncmp(line, target, strlen(target))) {
                fclose(fp);
                if (line) {
                    free(line);
                }
                die("Partition '%s' on the disk is mounted, refuse to continue", part->name);
            }
        }
    }
    fclose(fp);
    if (line) {
        free(line);
    }
    return;
}


uint32_t swap_bytes_u32(uint32_t b)
{
    return ((b & 0xFF000000) >> 24) |
           ((b & 0x00FF0000) >> 8) |
           ((b & 0x0000FF00) << 8) |
           (b << 24);
}

unsigned char *pattern_finder(unsigned char * haystack, unsigned char * pattern, uint32_t size, unsigned length) {
    unsigned char *rtr, *ptr=haystack;
    unsigned matched=0;
    uint32_t i = 0;
    while (i<size) {  // Do not overflow
        if (haystack[i] == pattern[matched]) {
            ++matched;
            if (matched == length) {
                return &(haystack[i+1-matched]);
            }
        }
        else {
            i-=matched;
            matched=0;
        }
        ++i;
    }
    return NULL;
}


void get_dtb_type(FILE *fp) {
    // fp should be previous opened as read-only by the outer function read_partition_table
    uint64_t offset_dtb;
    if (!options.input_device && options.input_reserved) {
        offset_dtb = DTB_OFFSET;
    }
    else {
        offset_dtb = options.offset + DTB_OFFSET;
    }
    fseek(fp, offset_dtb, SEEK_SET);
    //unsigned char *dtb = malloc(SIZE_DTB);
    uint32_t magic;
    fread(&magic, 4, 1, fp);
    printf("DTB is stored in ");
    if (magic == MAGIC_MULTI_DTB) {
        puts("plain Amlogic multi-dtb format");
        options.dtb_type=DTB_TYPE_MULTI;
    }
    else if (magic == MAGIC_SINGLE_DTB) {
        puts("plain dtb format");
        options.dtb_type=DTB_TYPE_SINGLE;
    }
    else {
        uint32_t magic_half = magic & 0x0000FFFF;
        if (magic_half == MAGIC_GZIPPED_DTB) {
            puts("gzipped format");
            options.dtb_type=DTB_TYPE_GZIP;
            FILE* tmp = tmpfile();
            if (!tmp) {
                fclose(fp);
                die("Failed to create temporary file to analyze gzipped dtb");
            }
            unsigned char *buffer = malloc(SIZE_DTB);
            fseek(fp, offset_dtb, SEEK_SET);
            fread(buffer, SIZE_DTB, 1, fp);
            fwrite(buffer, SIZE_DTB, 1, tmp);
            rewind(tmp);
            gzFile gp = gzdopen(fileno(tmp), "r"); 
            uint32_t magic_sub;
            int rtr = gzread(gp, &magic_sub, 4);
            if (gzclose(gp) != Z_OK || rtr==-1) {
                fclose(fp);
                die("Failed to uncompress gzipped dtb");
            }
            printf(" - In which, DTB is stored in ");
            if (magic_sub == MAGIC_MULTI_DTB) {
                puts("Amlogic multi-dtb format");
                options.dtb_subtype=DTB_TYPE_MULTI;
            }
            else if (magic_sub == MAGIC_SINGLE_DTB) {
                puts("standard dtb format");
                options.dtb_subtype=DTB_TYPE_SINGLE;
            }
            else {
                puts("unknown format");
                if (!options.no_node) {
                    fclose(fp);
                    die("dtbs are neither stored in standard dtb format nor Amlogic multi-dtb format, this is not an expected behaviour\n - Maybe your dtb is corrupted?");
                }
                options.dtb_subtype=DTB_TYPE_ILLEGAL;
            }
        }
        else {
            if (magic_half == MAGIC_XIAOMI_DTB) {
                puts("Xiaomi's proprietary format");
            }
            else if (magic_half == MAGIC_PHICOMM_DTB) {
                puts("Phicomm's proprietary format");
            }
            else {
                puts("unknown format");
            }
            if (!options.no_node) {
                fclose(fp);
                die("Refuse to continue as we can not modify the on-emmc DTB to remove the partitions node\n - U-boot will compare the partitions node in on-emmc dtb and the actual partitions\n - And will 'friendly' 'fix' it if they are different\n - The new partition table will be reverted after a reboot\n - If you are sure your u-boot won't do this, or you've modified the dtb yourself\n - You can run ampart with --no-node/-N to force a modification");
            }
            options.dtb_type=DTB_TYPE_ILLEGAL; // We set it anyway, in case we will use it for one day in the future
        }
    }
}


bool remove_dtb_partitions(unsigned char *dtb) {
    struct dtb_header *header = malloc(SIZE_DTB_HEADER);
    memcpy(header, dtb, SIZE_DTB_HEADER);
    uint32_t dtb_size = swap_bytes_u32(header->totalsize);
    unsigned char * parts_start=pattern_finder(dtb+SIZE_DTB_HEADER, dtb_partitions_start, dtb_size-SIZE_DTB_HEADER, LEN_DTB_PARTITIONS_START);  // Where partitions node start
    if (!parts_start) {
        free(header);
        return false;
    }
    unsigned char * parts_end=pattern_finder(parts_start, dtb_partitions_end, dtb_size-(parts_start-dtb), LEN_DTB_PARTITIONS_END);
    uint32_t dtb_size_diff = parts_end - parts_start + LEN_DTB_PARTITIONS_END;
    if (dtb_size_diff > 4000) { // (0x58+12+16) * 32 + 16 + 8 = 3736, 4000 for double insurance
        free(header);
        return false;
    }
    puts("Notice: partitions node found in dtb, removing it");
    printf("Removing %"PRIu32" bytes from dtb\n", dtb_size_diff);
    unsigned char *ptr;
    int i=0;
    for (ptr=parts_start; ptr<(dtb+dtb_size)-dtb_size_diff; ++ptr) {
        *ptr = *(ptr+dtb_size_diff);
    }
    header->totalsize = swap_bytes_u32(dtb_size-dtb_size_diff);
    header->off_dt_strings = swap_bytes_u32(swap_bytes_u32(header->off_dt_strings)-dtb_size_diff);
    header->size_dt_struct = swap_bytes_u32(swap_bytes_u32(header->size_dt_struct)-dtb_size_diff);
    memcpy(dtb, header, SIZE_DTB_HEADER);
    free(header);
    return true;
}

void remove_dtbs_partitions(FILE *fp) {
    uint64_t offset_dtbs;
    if (!options.input_device && options.input_reserved) {
        offset_dtbs = DTB_OFFSET;
    }
    else {
        offset_dtbs = options.offset + DTB_OFFSET;
    }
    fseek(fp, offset_dtbs, SEEK_SET);
    struct dtb_partition *dtb_partition = malloc(SIZE_DTB);
    unsigned char *dtbs;
    // unsigned char *dtbs = calloc(1, SIZE_DTB), *dtbs_gzipped;
    fread(dtb_partition, SIZE_DTB, 1, fp);
    int rtr;
    off_t gz_raw_size;
    bool modified = false;
    if (options.dtb_type == DTB_TYPE_GZIP) {
        puts("Decompressing gzipped dtb...");
        FILE* tmp = tmpfile();
        if (!tmp) {
            fclose(fp);
            die("Failed to create temporary file to analyze gzipped dtb");
        }
        fwrite(dtb_partition->data, SIZE_DTB-16, 1, tmp);
        rewind(tmp);
        dtbs = calloc(1, SIZE_DTB_UNCOMPRESS);
        gzFile gp = gzdopen(fileno(tmp), "r"); 
        rtr = gzread(gp, dtbs, SIZE_DTB_UNCOMPRESS);
        gz_raw_size = gztell(gp);
        printf("Decompressed %li bytes from gzipped dtb\n", gz_raw_size);
        if (gzclose(gp) != Z_OK || rtr==-1) {
            die("Failed to decompress gzipped dtb");
        }
    }
    else {
        dtbs = dtb_partition->data;
    }
    // uint32_t dtb_magic_u32 = MAGIC_SINGLE_DTB;
    // unsigned char dtb_magic[4]; 
    // memcpy(dtb_magic, &dtb_magic_u32, 4);
    if (options.dtb_type == DTB_TYPE_MULTI || options.dtb_subtype == DTB_TYPE_MULTI ) {
        puts("Checking if Amlogic multi-dtb should be modified...");
        unsigned char *dtb = pattern_finder(dtbs, dtb_magic, SIZE_DTB, 4);
        while (dtb != NULL) {
            modified |= remove_dtb_partitions(dtb);
            dtb = pattern_finder(dtb+PAGE_SIZE, dtb_magic, SIZE_DTB, 4); // 0x800 is page size
        }
    }
    else if (options.dtb_type == DTB_TYPE_SINGLE || options.dtb_subtype == DTB_TYPE_SINGLE ) {
        puts("Check if single dtb should be modified...");
        modified |= remove_dtb_partitions(dtbs);
    }
    else {
        puts("What??? This should not happen. The dtb type is invalid when trying to modify dtb");
        exit(EXIT_FAILURE);
    }
    if (modified) {
        if (options.dtb_type == DTB_TYPE_GZIP) {
            puts("Compressing gzipped dtb...");
            FILE* tmp = tmpfile();
            if (!tmp) {
                fclose(fp);
                die("Failed to create temporary file to compress dtb to gzip");
            }
            gzFile gp = gzdopen(fileno(tmp), "w");
            if (!gzwrite(gp, dtbs, gz_raw_size)) {
                die("Failed to compress dtb to gzip");
            }
            gzflush(gp, Z_FINISH);
            long gz_zip_size = ftell(tmp);
            if (gz_zip_size > SIZE_DTB-16) {
                die("Compressed dtb too big! It can only be 256K at most");
            }
            printf("Gzipped dtb size is %li bytes\n", gz_zip_size);
            rewind(tmp);
            memset(dtb_partition->data, 0, SIZE_DTB-16);
            fread(dtb_partition->data, gz_zip_size, 1, tmp);
            fclose(tmp);
            time_t now = time(NULL);
            memcpy((dtb_partition->data)+4, &now, 4); // Actually dtb_partition+4 would also work, but for clarity I prefre dtb_partition->data
            free(dtbs);
        }
        dtb_partition->version = PART_DTB_VERSION;
        dtb_partition->magic = PART_DTB_MAGIC;
        dtb_partition->checksum = dtb_checksum(dtb_partition);
        fseek(fp, offset_dtbs, SEEK_SET);
        fwrite(dtb_partition, SIZE_DTB, 1, fp); // A duplicate copy should be written
        fwrite(dtb_partition, SIZE_DTB, 1, fp);
    }
    free(dtb_partition);
    return;
}

void insertion_optimizer (struct insertion_helper *insert_helpers, uint64_t space, unsigned len) {
    unsigned i, j;
    bool insert[len], insert_best[len];
    uint64_t free, min_free=space;
    memset(insert_best,0,sizeof(insert));
    for (i=0; i<len; ++i) {
        memset(insert,0,sizeof(insert));
        free=space;
        for (j=i; j<len; ++j) {
            if (insert_helpers[j].part->size <= free) {
                free -= insert_helpers[j].part->size;
                insert[j] = true;
            }
            if (!free) {
                break;
            }
        }
        if (free < min_free) {
            min_free = free;
            memcpy(insert_best, insert, sizeof(insert));
        }
        if (!free) {
            break;
        }
    }
    bool header=false;
    for (i=0; i<len; ++i) {
        if (insert_helpers[i].insert = insert_best[i]) {
            if (!header) {
                printf("[Insertion optimizer] the following partitions will be inserted into the gap between bootloader and reserved: ");
                header=true;
            }
            printf("%s ", insert_helpers[i].part->name);
        }
    }
    if (header) {
        putc('\n', stdout);
    }
    return;
}

bool should_migrate(struct partition *old, struct partition *new) {
    return old && new && old->offset != new->offset;
}

void migrate_seek(struct partition *part, FILE *fp) {
    if (fseek(fp, part->offset, SEEK_SET)) {
        die("Failed to seek in the disk for migration of reserved partition %s", part->name);
    }
}

unsigned char *migrate_read(struct partition *part, FILE *fp) {
    printf("Warning: offset of reserved partition %s changed, it should be migrated\n", part->name);
    unsigned char *buffer = malloc(part->size);
    if (!buffer) {
        die("Failed to allocate memory for migration of reserved partition %s", part->name);
    }
    migrate_seek(part, fp);
    if (fread(buffer, part->size, 1, fp) != 1) {
        die("Failed to read old reserved partiton %s into buffer for migration", part->name);
    }
    return buffer;
}

void migrate_write(unsigned char *buffer, struct partition *part, FILE *fp) {
    printf("Writing reserved partition %s to its new location...\n", part->name);
    migrate_seek(part, fp);
    if (fwrite(buffer, part->size, 1, fp) != 1) {
        die("Failed to write reserved partiton %s from buffer to its new location for migration", part->name);
    }
    free(buffer);
    return;
}

void migrate_parts(struct table_helper *table_h_new, struct table_helper *table_h_old, FILE *fp) {
    if (!options.input_device && options.input_reserved) {
        puts("Warning: unable to migrate partitions since input is a dumped image for reserved partition\n - You may need to restore env, logo and misc from a backup image if you want to write this image to the real reserved partition");
    }
    else {
        puts("Warning: input is a whole emmc disk, checking if we should migrate env, logo and misc partitions as their position may be moved");
        // Dude, I'm writing all of these at 23:45 PM and it just makes me DIZZY
        bool migrate_env = should_migrate(table_h_old->env, table_h_new->env), migrate_logo = should_migrate(table_h_old->logo, table_h_new->logo), migrate_misc = should_migrate(table_h_old->misc, table_h_new->misc);
        unsigned char *buffer_env=NULL, *buffer_logo=NULL, *buffer_misc=NULL;
        if (migrate_env) {
            buffer_env = migrate_read(table_h_old->env, fp);
        }
        if (migrate_logo) {
            buffer_logo = migrate_read(table_h_old->logo, fp);
        }
        if (migrate_misc) {
            buffer_misc = migrate_read(table_h_old->misc, fp);
        }
        // Read all of them, in case we overwrite them
        if (migrate_env || migrate_logo || migrate_misc) {
            puts("Old reserved partitions all read into memory");
        }
        if (migrate_env) {
            migrate_write(buffer_env, table_h_new->env, fp);
        }
        if (migrate_logo) {
            migrate_write(buffer_logo, table_h_new->logo, fp);
        }
        if (migrate_misc) {
            migrate_write(buffer_misc, table_h_new->misc, fp);
        }
    }
}

void write_table(struct table_helper *table_h_new, struct table_helper *table_h_old) {
    char *path_write;
    if (options.input_device && options.input_reserved) {
        puts("Notice: input is a reserved partition and is a device, we would write to corresponding disk instead");
        path_write = options.path_disk;
    }
    else {
        path_write = options.path_input;
    }  // 1.Device disk: to disk; 2.Device reserved: to disk; 3.Image disk: to disk; 4:Image reserved: to reserved
    printf("Oopening '%s' as read/append to write new patition table...\n", path_write);
    FILE *fp = fopen(path_write, "r+");
    if (fp==NULL) {
        die("Can not open path '%s' as read/write/append, new partition table not written, check your permission!");
    }
    if (!options.no_node) {
        remove_dtbs_partitions(fp);
    }
    migrate_parts(table_h_new, table_h_old, fp);
    if (!options.input_device && options.input_reserved) {
        puts("Notice: input is a dumped image for reserved partition, no seeking");
        fseek(fp, 0, SEEK_SET);
    }
    else {
        size_byte_to_human_readable(s_buffer_1, options.offset);
        printf("Notice: Seeking %"PRIu64" (%s) (offset of reserved partition) into disk\n", options.offset, s_buffer_1);
        fseek(fp, options.offset, SEEK_SET);
    }
    fwrite(table_h_new->table, SIZE_TABLE, 1, fp);
    fclose(fp);
    if (options.input_device) {
        if (options.no_reload) {
            puts("Warning: no-reload is set, skip notifying the kernel about part layout changes\n - Remember to call ampart with --partprobe to properly reload the partition layout for system");
        }
        else {
            reload_emmc();
        }
    }
    puts("Everything done! Enjoy your fresh-new partition table!");
}

int main(int argc, char **argv) {
    no_coreelec();
    get_options(argc, argv);
    struct disk_helper disk = { 0, 0, get_disk_size()};
    struct partition_table *table = calloc(1, SIZE_TABLE);
    struct table_helper table_h = { table, 0 };
    uint64_t size_disk_table = read_partition_table(&table_h);
    if (!disk.size) { // Only when disk.size is 0, will we use the size got from partition table
        disk.size = size_disk_table;
    }
    if ( options.snapshot ) {
        snapshot(table);
    }
    if ( optind == argc ) { // optind is static from getopt
        exit(EXIT_SUCCESS);
    }
    if (options.input_device) {
        printf("Warning: input is a device, check if any partitions under its corresponding emmc disk '%s' is mounted...\n", options.path_disk);
        no_mounted(table);
    }
    size_byte_to_human_readable(s_buffer_1, disk.size);
    printf("Using %"PRIu64" (%s) as the disk size\n", disk.size, s_buffer_1);
    struct table_helper * table_h_new = parse_table(&disk, &table_h, &argc, argv);
    if ( options.dryrun ) {
        puts("Running in dry-run mode, no actual writting, exiting...");
        exit(EXIT_SUCCESS);
    }
    write_table(table_h_new, &table_h);
    exit(EXIT_SUCCESS);
}