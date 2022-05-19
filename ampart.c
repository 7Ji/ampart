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
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <libgen.h>
#include <linux/fs.h>

bool is_operator(char* arg, char* shortopt, char* longopt) {
    if ( !strcmp(arg, shortopt) || !strcmp(arg, longopt)) {
        return true;
    }
    return false;
}

// long str_to_long(char* str) {
//     if ( str[0] == '0' && sizeof(str) > 1 && (str[1]=='x' || str[1]=='X')) {
//         return strtol(str, NULL, 16);
//     }
//     else {
//         return atoll(str);
//     }
// }

struct partition {
	char name[16];
	uint64_t size;
	uint64_t offset;
	unsigned mask_flags;
};

struct mmc_partitions_fmt {
	char magic[4];
	unsigned char version[12];
	int part_num;
	int checksum;
	struct partition partitions[32];
};

int mmc_partition_tbl_checksum_calc(struct partition *part, int part_num) {
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

void size_byte_to_human_readable(char* buffer, uint64_t size) { // For god's sake, why the hell can't aarch64 devices use a returned character directly?
    static char suffixes[]="BKMG";
    double num = size;
    int i;
    for (i=0; i<4; ++i) {
        if (num <= 1024) {
            break;
        }
        num /= 1024;
    }
    sprintf(buffer, "%4.2f%c", num, suffixes[i]);
    return;
}

static void die (const char * format, ...)
{
    va_list vargs;
    va_start (vargs, format);
    fprintf (stderr, "ERROR: ");
    vfprintf (stderr, format, vargs);
    fprintf (stderr, "\n");
    va_end (vargs);
    exit (EXIT_FAILURE);
}

// void die(char * message) {
//     fprintf(stderr, message);
//     exit(EXIT_FAILURE);
// }

uint64_t four_kb_alignment(uint64_t size) {
    int remainder = size % 4096;
    if ( remainder ) {
        char buffer[9];
        size_byte_to_human_readable(buffer, size);
        printf("Warning: size/offset %d (%s) is rounded up to ", size, buffer);
        size += (4096-remainder);
        printf("%d (%s) for 4K alignment\n", size, buffer);
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
    long size;
    char *prefix, *suffix, *ptr = size_h;
    char size_h_new[length];
    prefix = &size_h[0];
    if ( *prefix < '0' || *prefix > '9' ) {
        if (size_h[0] == '+') {
            if ( has_prefix ) {
                *has_prefix = true;
            }
            strcpy(size_h_new, ++ptr);
            start = 1;
        }
        else {
            die("Illegal prefix found! You should only use + as prefix for offset/size!");
        }
    }
    else {
        if ( has_prefix ) {
            *has_prefix = false;
        }
        strcpy(size_h_new, ptr);
        start = 0;
    }
    suffix = &(size_h_new[length-1-start]);  // As the last should always be \0(NULL)
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
    return atoll(size_h_new) * multiply;
}

void valid_partition_name(char *name) {
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
            die("You can only use a-z, A-Z and _ as characters of the partition name!");
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
    // if (!strcmp(name, "bootloader") || !strcmp(name, "reserved") || !strcmp(name, "env")) {
    //     die("ERROR: You can not use bootloader, reserved or env as partition name! These will be automatically generated by ampart from the old partition table!\n");
    // }
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

void partition_from_argument (struct partition *partition, char *argument) {
    char argument_new[strlen(argument)+1];  // A seperate argument is used here so we won't mess up with the original one
    strcpy(argument_new, argument);
    char *arg = argument_new;
    // Name
    int length = get_part_argument_length(arg, false);
    if ( length > 15 ) {
        die("Partition name can only be 15 characters long!");
    }
    if ( !length ) {
        die("Partition name must be set!");
    }
    strcpy(partition->name, arg);
    valid_partition_name(partition->name);
    printf("Name: %s\n", partition->name);
    arg += length + 1; //move the cursor, plus one for :
    //char buff[8];
    // Offset
    length = get_part_argument_length(arg, false);
    if ( !length ) {
        // Auto increment offset

    }
    arg += length + 1;
    // Size
    length = get_part_argument_length(arg, false);
    if ( !length ) {
        // Auto fill the rest
    }
    else {
        partition->size=four_kb_alignment(size_human_readable_to_byte(arg, NULL));
    }
    char buffer[9];
    size_byte_to_human_readable(buffer, partition->size);
    printf("Size: %llu (%s)\n", partition->size, buffer);

    arg += length + 1;
    // Mask
    length = get_part_argument_length(arg, true);
    if ( !length ) {
        partition->mask_flags=4;
    }
    else {
        partition->mask_flags=atol(arg);
        if ((partition->mask_flags != 2) && (partition->mask_flags != 4)) {
            //printf("%d\n", partition->mask_flags);
            die("Invalid mask %u, must be either 2 or 4, or leave it alone to use default value 4", partition->mask_flags);
        }
    }
    printf("Mask: %u\n", partition->mask_flags);
    return;
}

void valid_partition_table(struct mmc_partitions_fmt *table) {
    char good[] = ", GOOD √";
    puts("Validating partition table...");
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
    int checksum = mmc_partition_tbl_checksum_calc(&(table->partitions), table->part_num);
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
            has_bootloader = true;
        }
        else if (!strcmp(part->name, "reserved")) {
            has_reserved = true;
        }
        else if (!strcmp(part->name, "env")) {
            has_env = true;
        }
        else {
            valid_partition_name(part->name);
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

struct options {
    bool input_disk;
    bool input_reserved;
    bool input_device;
    bool dryrun;
    char *path_input;
    char *dir_input;
    char *name_input;
    char *output;
    uint64_t offset;
    uint64_t size;
};

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
    }
    else { // A normal file
        printf("Path '%s' seems a dumped image, checking the head bytes to distinguish it...\n", options->path_input);
        char buffer[0x200];
        FILE *fp = fopen(options->path_input, "r");
        if (fp==NULL) {
            die("Can not open path '%s' as read-only, check your permission!", options->path_input);
        }
        fread(buffer, 0x200, 1, fp);
        options->input_reserved = false;
        for (int i=0; i<0x200; ++i) {
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
        "\t\t\tmask: mask of the part, either 2 or 4, 2 for system parts, 4 for data parts, default: 4\n"
        "\t\t\te.g.\tdata::: a data partition, auto offset, fills all the remaining space, mask 4\n"
        "\t\t\t\tsystem::2G:2 a system parrtition, auto offset, 2G, mask 2\n"
        "\t\t\t\tcache:+8M:512M:2 a cache partition, offset 8M after the last part, 512M, mask 2\n\n"
        "options:\n"
        "\t--help/-h\tdisplay this message\n"
        "\t--disk/-d\tno auto-recognization, treat input as a whole disk\n"
        "\t--reserved/-r\tno auto-recognization, treat input as a reserved partition\n"
        "\t--offset/-O [offset]\n"
        "\t\t\toverwrite the offset of the reserved partition, in case OEM has modified it, only valid when input is a whole disk, default:36M\n"
        "\t--dry-run/-D\tdo not actually update the part table\n"
        "\t--output/-o [path]\n"
        "\t\t\twrite the updated part table to somewhere else, instead of the input itself\n";
    puts(help_message);
    exit(EXIT_SUCCESS);
}

struct options *get_options(int argc, char **argv) {
    int c, option_index = 0;  
    static struct options options = {0}, *ptr=&options;
    options.offset = 0x2400000; // default offset, 32M
    static struct option long_options[] = {
        {"help",    no_argument,        NULL,   'h'},
        {"disk",    no_argument,        NULL,   'd'},   // The input file is a whole disk (e.g. /dev/mmcblk0) or image file alike
        {"reserved",no_argument,        NULL,   'r'},   // The input file is a reserved partation (e.g. /dev/reserved) or image file alike
        {"offset",  required_argument,  NULL,   'O'},   // offset of the reserved partition in disk
        {"dry-run", no_argument,        NULL,   'D'},
        {"output",  required_argument,  NULL,   'o'},   // Optionally write output to a given path, instead of write it back
        {NULL,      0,                  NULL,    0},    // This is needed for when user just mess up with their input
    };
    while ((c = getopt_long(argc, argv, "hdrO:Do:", long_options, &option_index)) != -1) {
        int this_option_optind = optind ? optind : 1;
        switch (c) {
            case 'd':
                puts("Notice: forcing input as a whole disk (e.g. /dev/mmcblk0)");
                options.input_disk = true;
                break;
            case 'r':
                puts("Notice: forcing input as a reserved part (e.g. /dev/reserved)");
                options.input_reserved = true;
                break;
            case 'O':
                options.offset = four_kb_alignment(size_human_readable_to_byte(optarg, NULL));
                printf("Using offset %li for reserved partition\n", options.offset);
                puts("Warning: unless your reserved partition offset has been modified by the OEM, you should not overwrite the offset");
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
                help(argv[0]);
        }
    }
    if ( options.input_disk && options.input_reserved ) {
        die("You CAN NOT force the input both as whole disk and as reserverd partition!");
    }
    if ( optind == argc ) {
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
    if ( !options.input_disk && !options.input_reserved ) {
        is_reserved(&options);
    } // From here onward we only need input_reserved
    return ptr;
};

int main(int argc, char **argv) {
    struct options *options = get_options(argc, argv);
    struct stat stat_input;
    if (stat(options->path_input, &stat_input)) {
        die("Input file '%s' does not exist!", options->path_input);
    }
    uint64_t size_disk = 0;
    
    if (options->input_device) {
        printf("Input '%s' is a device, getting its size from /proc/partitions\n", options->path_input);
        uint64_t size_disk_ioctl;
        int major = major(stat_input.st_rdev);
        // if (options->input_reserved) {
        //     unsigned long disk = makedev(major(stat_input.st_rdev), 0);
            
        // }
        // else {
        //     int fd=open(options->path_input, O_RDONLY);
        //     if (fd == -1) {
        //         close(fd);
        //         die("Failed to open device file '%s' as read-only via ioctl! Check your permission.\n", options->path_input);
        //     }
        //     if (ioctl(fd, BLKGETSIZE64, &size_disk_ioctl)==-1) {
        //         close(fd);
        //         die("Failed to get size info from '%s' via ioctl!\n", options->path_input);
        //     }
        //     close(fd);
        // }
    }
    else{
        printf("Warning: Input '%s' is an image file for", options->path_input);
        if (options->input_reserved) {
            puts(" a reserved partition, we can only get its size via the partition table latter");
            puts(" - The size may not be correct due to disk may be not 100 percent parted");
        }
        else {
            puts(" a whole emmc disk, getting its size via stat");
            size_disk = stat_input.st_size;
        }
    }
    char buffer[9];
    if (size_disk) {
        size_byte_to_human_readable(buffer, size_disk);
        printf("Disk size is %llu (%s)\n", size_disk, buffer);
    }

    int fd=open(options->path_input, O_RDONLY);
    FILE *fp = fopen(options->path_input, "r");
    if ( fp == NULL ) {
        die("ERROR: Can not open input path as read mode, check your permission!\n");
    }
    if (!options->input_reserved) {
        size_byte_to_human_readable(buffer, options->offset);
        printf("Notice: Seeking %llu (%s) bytes into disk\n", options->offset, buffer);
        fseek(fp, options->offset, SEEK_SET);
    }
    int size_table = sizeof(struct mmc_partitions_fmt);
    struct mmc_partitions_fmt *table = malloc(size_table);
    memset(table, 0, size_table);  // Just for safety
    fread(table, size_table, 1, fp);
    valid_partition_table(table);
    printf("Partition table read from '%s':\n", options->path_input);
    uint64_t size_disk_table = summary_partition_table(table);
    size_disk = (size_disk_table > size_disk) ? size_disk_table : size_disk;
    size_byte_to_human_readable(buffer, size_disk);
    printf("Using %llu (%s) as the disk size\n", size_disk, buffer);
    if ( optind < argc ) {  // Only when partition is defined, will we try to parse partition table defined by user
        int partitions_count;
        partitions_count = argc - optind;
        partitions_count = (partitions_count > 29) ? 29 : partitions_count; //Max is 32, but bootloader, reserved, env can not be used
        struct partition partitions_new[partitions_count];
        memset(partitions_new, 0, sizeof(partitions_new));  // Fill the partition structs with 0
        struct partition *partition_new;
        char *partition_arg;
        int i = 0;
        while ( optind < argc && i < 29 ) {
            partition_new = &partitions_new[i++];
            partition_arg = argv[optind++];
            printf("Parsing user input for partition: %s\n", partition_arg);
            partition_from_argument(partition_new, partition_arg);
        }
    }
    if ( options->dryrun ) {
        return 0;
    }
    exit(EXIT_SUCCESS);

    

    // printf("Offset: %lu / 0x%x Bytes\nSize: %lu / 0x%x Bytes\n", offset, offset, size, size);
    FILE *fp_w = fopen("imgs/gxl_new.img", "r+");
    // //strcpy(table->version, "7ISHERE");

    fwrite(table, size_table, 1, fp_w);
    exit(EXIT_SUCCESS);
}