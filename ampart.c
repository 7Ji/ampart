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
#define SIZE_PART   40      // This should ALWAYS be 40, regardless of platform
#define SIZE_TABLE  1304    // This should ALWAYS be 1304, regardless of platform
#define SIZE_HEADER 0x200   // Count of bytes that should be \0 for whole emmc disk, things may become tricky if users have created a mbr partition table, as 0x200 is the size of mbr and it then becomes occupied
#define SIZE_ENV    0x800000
#define VERSION     "v0.1"

struct partition {
	char name[16];
	uint64_t size;
	uint64_t offset;
	unsigned mask_flags;
    // padding 4byte
};

struct mmc_partitions_fmt {
	char magic[4];
	unsigned char version[12];
	int part_num;
	int checksum;
	struct partition partitions[32];
};

struct table_helper {
    struct mmc_partitions_fmt *table;
    struct partition *bootloader;
    struct partition *reserved;
    struct partition *env;
};


struct disk_helper {
    uint64_t start;
    uint64_t free;
    uint64_t size;
};

struct options {
    bool input_reserved;
    bool input_device;
    bool snapshot;
    bool clone;
    bool dryrun;
    char *path_input;
    char *dir_input;
    char *name_input;
    char *output;
    uint64_t offset;
    uint64_t size;
} options = {0};

uint32_t mmc_partition_tbl_checksum_calc(struct partition *part, int part_num) {
	int i, j;
	uint32_t checksum = 0, *p;
	for (i = 0; i < part_num; i++) {
		p = (uint32_t *)part;
		for (j = sizeof(struct partition)/sizeof(checksum);
				j > 0; j--) {
			checksum += *p;
			p++;
		}
	}
	return checksum;
}

char suffixes[]="BKMGTPEZY";  // static is not needed, when out of any function's scope
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
        if (size <= 1024 || size % 1024) {
            break;
        }
        size /= 1024;
    }
    sprintf(buffer, "%llu%c", size, suffixes[i]);
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
    unsigned remainder = size % 4096; // Would this even be negative? Guess not
    if ( remainder ) {
        char buffer[9];
        size_byte_to_human_readable(buffer, size);
        printf("Warning: size/offset %llu (%s) is rounded up to ", size, buffer);
        size += (4096-remainder);
        size_byte_to_human_readable(buffer, size);
        printf("%llu (%s) for 4K alignment\n", size, buffer);
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
            *has_prefix = true;
            strcpy(size_h_new, ++ptr);
            start = 1;
        }
        else {
            die("Illegal prefix found! You should only use + as prefix!");
        }
    }
    else {
        *has_prefix = false;
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
    // puts(name);
    // if (!strcmp(name, "bootloader")) {
    //     puts("oops");

    // }
    if (!allow_reserved) {
        if (!strcmp(name, "bootloader") || !strcmp(name, "reserved") || !strcmp(name, "env")) {
            die("Illegal partition name '%s'! You can not use bootloader, reserved or env as partition name! These will be automatically generated by ampart from the old partition table!", name);
        }
    }
    return;
}

int get_part_argument_length (char *argument, bool allow_end) {
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
        die("Partation argument incomplete! msut be in format of name:offset:size:mask");
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
    strcpy(partition->name, arg);
    valid_partition_name(partition->name, false);
    printf(" - Name: %s\n", partition->name);
    arg += length + 1;
    // Offset
    char buffer1[9], buffer2[9];
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
            size_byte_to_human_readable(buffer1, partition->offset);
            size_byte_to_human_readable(buffer2, disk->start);
            die("Partition offset smaller than end of last partition: %llu(%s) < %llu(%s)", partition->offset, buffer1, disk->start, buffer2);
        }
        if (partition->offset > disk->size) {
            size_byte_to_human_readable(buffer1, partition->offset);
            size_byte_to_human_readable(buffer2, disk->start);
            die("Partition offset greater than disk size:  %llu(%s) > %llu(%s)", partition->offset, buffer1,  disk->start, buffer2);
        }
    }
    size_byte_to_human_readable(buffer1, partition->offset);
    printf(" - Offset: %llu (%s)\n", partition->offset, buffer1);
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
            size_byte_to_human_readable(buffer1, partition_end);
            size_byte_to_human_readable(buffer2, disk->free);
            die("Partition end point overflows! End point greater than disk size: %llu (%s) > %llu (%s)", partition_end, buffer1, disk->free, buffer2);
        }
    }
    disk->start = partition_end;
    disk->free = disk->size - partition_end;
    size_byte_to_human_readable(buffer1, partition->size);
    printf(" - Size: %llu (%s)\n", partition->size, buffer1);
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
    strcpy(partition->name, arg);
    valid_partition_name(partition->name, true);
    printf(" - Name: %s\n", partition->name);
    arg += length + 1;
    // Offset
    char buffer1[9], buffer2[9];
    length = get_part_argument_length(arg, false);
    if ( !length ) {
        die("In clone mode part offset MUST be set");
    }
    partition->offset = size_human_readable_to_byte_no_prefix(arg, false);
    size_byte_to_human_readable(buffer1, partition->offset);
    printf(" - Offset: %llu (%s)\n", partition->offset, buffer1);
    arg += length + 1;
    // Size
    length = get_part_argument_length(arg, false);
    uint64_t partition_end;
    if ( !length ) {
        die("In clone mode part size MUST be set");
    }
    partition->size=size_human_readable_to_byte_no_prefix(arg, false);
    size_byte_to_human_readable(buffer1, partition->size);
    printf(" - Size: %llu (%s)\n", partition->size, buffer1);
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

void valid_partition_table(struct table_helper *table_h) {
    char good[] = ", GOOD √";
    puts("Validating partition table...");
    struct mmc_partitions_fmt *table = table_h->table;
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
    int checksum = mmc_partition_tbl_checksum_calc(table->partitions, table->part_num);
    printf("Checksum: calculated %x, recorded %x", checksum, table->checksum);
    if ( checksum != table->checksum ) {
        die("\nERROR: Checksum mismatch!");
    }
    else {
        puts(good);
    }
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
            table_h->reserved=part;
            has_reserved = true;
        }
        else if (!strcmp(part->name, "env")) {
            table_h->env=part;
            has_env = true;
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

uint64_t summary_partition_table(struct mmc_partitions_fmt *table) {
    char line[]="==============================================================";
    bool overlap=false;
    uint64_t offset=0, gap;
    //char part_size[8], part_offset[8], offset_h[8];
    puts(line);
    puts("NAME                          OFFSET                SIZE  MARK");
    puts(line);
    struct partition *part;
    char buffer_offset[9], buffer_size[9];
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
            size_byte_to_human_readable(buffer_offset, gap);
            printf("%+9llx(%+8s)\n", gap, buffer_offset);
        };
        offset = part->offset + part->size;
        size_byte_to_human_readable(buffer_offset, part->offset);
        size_byte_to_human_readable(buffer_size, part->size);
        printf("%-16s %+9llx(%+8s) %+9llx(%+8s)     %u\t\n", part->name, part->offset, buffer_offset, part->size, buffer_size, part->mask_flags);
    }
    puts(line);
    if (overlap) {
        puts("Warning: Overlap found in partition table, this is extremely dangerous as your Android installation might already be broken.");
        puts(" - Please confirm if you've failed with ceemmc as it may overlay data with CE_STORAGE");
        puts(" - If your device bricks, do not blame on ampart as it's just the one helping you discovering it");
    }
    size_byte_to_human_readable(buffer_offset, offset);
    printf("Disk size totalling %llu (%s) according to partition table\n", offset, buffer_offset);
    return offset;
}

void is_reserved(struct options *options) {
    if (!strcmp(options->dir_input, "/dev")) {
        printf("Path '%s' seems a device file\n", options->path_input);
        if (!strncmp(options->name_input, "mmcblk", 6) && ( options->name_input[6] >= '0' && options->name_input[6] <= '9')) {
            bool has_p = false;
            bool has_num = false;
            for (int i=6; i<100; ++i) {
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
        char buffer[SIZE_HEADER];
        FILE *fp = fopen(options->path_input, "r");
        if (fp==NULL) {
            die("Can not open path '%s' as read-only, check your permission!", options->path_input);
        }
        fread(buffer, SIZE_HEADER, 1, fp);
        options->input_reserved = false;
        for (int i=0; i<SIZE_HEADER; ++i) {
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
    puts("\nampart (Amlogic emmc partition tool) "VERSION"\nCopyright (C) 2022 7Ji (pugokushin@gmail.com)\nLicense GPLv3: GNU GPL version 3 <https://gnu.org/licenses/gpl.html>.\nThis is free software: you are free to change and redistribute it.\nThere is NO WARRANTY, to the extent permitted by law.\n");
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
        "\t\t\toverwrite the offset of the reserved partition, in case OEM has modified it, only valid when input is a whole disk, default:36M\n"
        "\t--snapshot/-s\toutputs partition arguments that can be used to restore the partition table to what it looks like now, early quit\n"
        "\t--clone/-c\trun in clone mode, parse partition arguments outputed by --snapshot/-s, no filter\n"
        "\t--dry-run/-D\tdo not actually update the part table\n"
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
        {"dry-run", no_argument,        NULL,   'D'},
        {"output",  required_argument,  NULL,   'o'},   // Optionally write output to a given path, instead of write it back
        {NULL,      0,                  NULL,    0},    // This is needed for when user just mess up with their input
    };
    char buffer[9];
    bool input_disk = false;
    while ((c = getopt_long(argc, argv, "vhdrO:scDo:", long_options, &option_index)) != -1) {
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
                printf("Using offset %llu (%s) for reserved partition\n", options.offset, buffer);
                puts("Warning: unless your reserved partition offset has been modified by the OEM, you should not overwrite the offset");
                break;
            case 's':
                puts("Notice: running in snapshot mode, new partition table won't be parsed");
                options.snapshot = true;
                break;
            case 'c':
                puts("Notice: running in clone mode, partition arguments won't be filtered, reserved partitions can be set");
                options.clone = true;
                break;
            case 'D':
                puts("Notice: running in dry-run mode, changes won't be written to disk");
                options.dryrun = true;
                break;
            case 'o':
                options.output = optarg;
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
    if ( optind == argc ) {
        help(argv[0]);
        die("You must specify a path to reserved partition (e.g./dev/reserved) or whole emmc disk (e.g./dev/mmcblk0) or image files dumped from them!");
    }
    // First argument must be path
    options.path_input = argv[optind++];
    // Copy path to a new string so basename and dirname won't screw it up
    char path_new[strlen(options.path_input)+1];
    strcpy(path_new, options.path_input);
    options.name_input=basename(path_new);
    options.dir_input=dirname(path_new);
    options.input_device=!strcmp(options.dir_input, "/dev");
    if ( !input_disk && !options.input_reserved ) {
        is_reserved(&options);
    } // From here onward we only need input_reserved
    return;
};

uint64_t get_disk_size() {
    struct stat stat_input;
    if (stat(options.path_input, &stat_input)) {
        die("Path '%s' does not exist!", options.path_input);
    }
    uint64_t size_disk = 0;
    if (options.input_device) {
        printf("Path '%s' is a device, getting its size via ioctl\n", options.path_input);
        char *path_device = NULL;
        if (options.input_reserved) {
            unsigned major_reserved = major(stat_input.st_rdev);
            DIR *d;
            struct dirent * dir;
            struct stat stat_device;
            char path_device_full[14];  // /dev/mmcblk10 is just 13, +\0 for 14 max
            d = opendir("/dev");
            if ( d == NULL ) {
                die("Failed to open /dev for scanning, check your permission");
            }
            while((dir = readdir(d)) != NULL) {
                if (!strncmp(dir->d_name, "mmcblk", 6) && strlen(dir->d_name) == 7) {
                    strcpy(path_device_full, "/dev/");
                    strcat(path_device_full, dir->d_name);
                    if (stat(path_device_full, &stat_device)) {
                        closedir(d);
                        die("Can not check stat for device file '%s', check your permission!", path_device_full);
                    }
                    if (major(stat_device.st_rdev) == major_reserved) {
                        closedir(d);
                        path_device = path_device_full;
                        break;
                    }
                }
            }
            if (path_device == NULL) {
                closedir(d);
                die("Failed to find corresponding mmcblk device");
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
    char buffer[9];
    if (size_disk) {
        size_byte_to_human_readable(buffer, size_disk);
        printf("Disk size is %llu (%s)\n", size_disk, buffer);
    }
    return size_disk;
}

uint64_t read_partition_table(struct table_helper *table_h) {
    puts("Reading old partition table...");
    struct mmc_partitions_fmt *table = table_h->table;
    int fd=open(options.path_input, O_RDONLY);
    FILE *fp = fopen(options.path_input, "r");
    if ( fp == NULL ) {
        die("ERROR: Can not open input path as read mode, check your permission!\n");
    }
    char buffer[9];
    if (!options.input_reserved) {
        size_byte_to_human_readable(buffer, options.offset);
        printf("Notice: Seeking %llu (%s) (offset of reserved partition) into disk\n", options.offset, buffer);
        fseek(fp, options.offset, SEEK_SET);
    }
    fread(table, SIZE_TABLE, 1, fp);
    fclose(fp);
    valid_partition_table(table_h);
    printf("Partition table read from '%s':\n", options.path_input);
    return summary_partition_table(table);
};

void reload_emmc() {
    // Notifying kernel about emmc partition table change
    if ( options.input_device ) {
        const char device[] = "emmc:0001";
        const char path_unbind[] = "/sys/bus/mmc/drivers/mmcblk/unbind";
        const char path_bind[] = "/sys/bus/mmc/drivers/mmcblk/bind";
        puts("Notifying kernel about partition table change...");
        puts("We need to reload the driver for emmc as the meson-mmc driver does not like partition table being hot-updated");
        printf("Opening '%s' so we can unbind driver for '%s'\n", path_unbind, device);
        FILE *fp = fopen(path_unbind, "w");
        if ( fp == NULL ) {
            die("ERROR: can not open '%s' for unbinding driver for '%s', \n - You will need to reboot manually for the new partition table to be picked up by kernel \n - DO NOT access the old parititions under /dev for now as they are not updated!", path_unbind, device);
        }
        fputs(device, fp);
        fclose(fp);
        puts("Successfully unbinded the driver, all partitions and the disk itself are not present under /dev as a result of this");
        printf("Opening '%s' so we can bind driver for '%s'\n", path_bind, device);
        fp = fopen(path_bind, "w");
        if ( fp == NULL ) {
            die("ERROR: can not open '%s' for binding driver for '%s', \n - You will need to reboot manually for the new partition table to be picked up \n - You can not access the old partitions and the disk for now.", path_unbind, device);
        }
        fputs(device, fp);
        fclose(fp);
        puts("Successfully binded the driver, you can use the new partition table now!");
    };
}

void snapshot(struct mmc_partitions_fmt * table) {
    puts("Give one of the following partition arguments with options --clone/-c to ampart to reset the partition table to what it looks like now:");
    struct partition *part;
    for (int i=0; i<table->part_num; ++i) {
        part=&(table->partitions[i]);
        printf("%s:%llu:%llu:%u ", part->name, part->offset, part->size, part->mask_flags);
    }
    putc('\n', stdout);
    char buffer_offset[9];
    char buffer_size[9];
    for (int i=0; i<table->part_num; ++i) {
        part=&(table->partitions[i]);
        size_byte_to_human_readable_int(buffer_offset, part->offset);
        size_byte_to_human_readable_int(buffer_size, part->size);
        printf("%s:%s:%s:%u ", part->name, buffer_offset, buffer_size, part->mask_flags);
    }
    putc('\n', stdout);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    get_options(argc, argv);
    struct disk_helper disk = {0};
    disk.size = get_disk_size();
    struct mmc_partitions_fmt *table = calloc(1, SIZE_TABLE);
    struct table_helper table_h = { table, 0 };
    uint64_t size_disk_table = read_partition_table(&table_h);
    if (!disk.size) { // Only when disk.size is 0, will we use the size got from partition table
        disk.size = size_disk_table;
    }
    char buffer[9];
    size_byte_to_human_readable(buffer, disk.size);
    printf("Using %llu (%s) as the disk size\n", disk.size, buffer);
    if ( options.snapshot ) {
        snapshot(table);
    }
    if ( optind == argc ) {
        exit(EXIT_SUCCESS);
    }
    struct mmc_partitions_fmt *table_new = calloc(1, SIZE_TABLE);
    struct partition *partition_new;
    char *partition_arg;
    int partitions_count = argc - optind;
    int i=0;
    if ( options.clone ) {
        if (partitions_count > 32) {
            die("You've defined too many partitions (%d>32), this is forbidden in clone mode", partitions_count);
        }
        while ( optind < argc && i < 32 ) {
            partition_new = &(table_new->partitions[i++]);
            partition_arg = argv[optind++];
            printf("Parsing user input for partition (clone mode): %s\n", partition_arg);
            partition_from_argument_clone(partition_new, partition_arg);
        }
        table_new->part_num = partitions_count;
    }
    else {
        // Only when partition is defined, will we try to parse partition table defined by user
        uint64_t reserved_end = table_h.reserved->offset + table_h.reserved->size;
        disk.start = reserved_end + table_h.env->size;
        disk.free = disk.size - disk.start;
        size_byte_to_human_readable(buffer, disk.free);
        printf("Usable space of the disk is %llu (%s)\n", disk.free, buffer);
        size_byte_to_human_readable(buffer, reserved_end);
        printf("- This is due to reserved partition ends at %llu (%s)\n", reserved_end, buffer);
        size_byte_to_human_readable(buffer, table_h.env->size);
        printf("- And env partition takes %llu (%s) \n", table_h.env->size, buffer);
        puts("Make sure you've backed up the env partition as its offset will mostly change\n - This is because all user-defined partitions will be created after the env partition\n - Yet most likely the old env partition was created after a cache partition\n - Which wastes a ton of space if we start at there");
        if (partitions_count > 29) {
            partitions_count = 29;
            puts("Warning: You've defined too many partitions, only 29 of them will be accepted");
        }
        struct partition partitions_new[29] = {0};
        while ( optind < argc && i < 29 ) { // 32 total - 3 reserved (bootloader + reserved + env)
            partition_new = &partitions_new[i++];
            partition_arg = argv[optind++];
            printf("Parsing user input for partition: %s\n", partition_arg);
            partition_from_argument(partition_new, partition_arg, &disk);
        }
        //memcpy(table_new, table, 16); // 4byte for magic, 16byte for version
        table_new->part_num = partitions_count + 3;
        memcpy(&(table_new->partitions[0]), table_h.bootloader, SIZE_PART);
        memcpy(&(table_new->partitions[1]), table_h.reserved, SIZE_PART);
        memcpy(&(table_new->partitions[2]), table_h.env, SIZE_PART);
        table_new->partitions[2].offset=reserved_end;
        for (i=0; i<partitions_count; ++i) {
            memcpy(&(table_new->partitions[i+3]), &partitions_new[i], SIZE_PART);
        }
    }
    memcpy(table_new, table, 16);// 4byte for magic, 12byte for version
    table_new->checksum = mmc_partition_tbl_checksum_calc(table_new->partitions, table_new->part_num);
    struct table_helper table_h_new = {table_new, 0};
    puts("New partition table is generated successfully in memory");
    valid_partition_table(&table_h_new);
    summary_partition_table(table_new);
    if ( options.dryrun ) {
        puts("Running in dry-run mode, no actual writting, exiting...");
        exit(EXIT_SUCCESS);
    }
    printf("Re-opening input path '%s' to write new patition table...\n", options.path_input);
    FILE *fp = fopen(options.path_input, "r+");
    if (fp==NULL) {
        die("Can not open path '%s' as read/write/append, new partition table not written, check your permission!");
    }
    if (!options.input_reserved) {
        size_byte_to_human_readable(buffer, options.offset);
        printf("Notice: Seeking %llu (%s) (offset of reserved partition) into disk\n", options.offset, buffer);
        fseek(fp, options.offset, SEEK_SET);
    }
    fwrite(table_new, SIZE_TABLE, 1, fp);
    fclose(fp);
    if (options.input_device) {
        reload_emmc();
    }
    puts("Everything done! Enjoy your fresh-new partition table!");
    exit(EXIT_SUCCESS);
}