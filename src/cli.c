/* Self */

#include "cli.h"

/* System */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

/* Local */

#include "ept.h"
#include "gzip.h"
#include "io.h"
#include "util.h"

/* Definition */

#define CLI_WRITE_NOTHING   0b00000000
#define CLI_WRITE_DTB       0b00000001
#define CLI_WRITE_TABLE     0b00000010
#define CLI_WRITE_MIGRATES  0b00000100

/* Variable */

char const  cli_mode_strings[][10] = {
    "",
    "dtoe",
    "etod",
    "epedantic",
    "dedit",
    "eedit",
    "dsnapshot",
    "esnapshot",
    "dclone",
    "eclone",
    "ecreate"
};

char const  cli_content_type_strings[][9] = {
    "auto",
    "dtb",
    "reserved",
    "disk"
};

struct cli_options cli_options = {
    .mode = CLI_MODE_INVALID,
    .content = CLI_CONTENT_TYPE_AUTO,
    .dry_run = false,
    .strict_device = false,
    .write = CLI_WRITE_DTB | CLI_WRITE_TABLE | CLI_WRITE_MIGRATES,
    .offset_reserved = EPT_PARTITION_GAP_RESERVED + EPT_PARTITION_BOOTLOADER_SIZE,
    .offset_dtb = DTB_PARTITION_OFFSET,
    .gap_partition = EPT_PARTITION_GAP_GENERIC,
    .gap_reserved = EPT_PARTITION_GAP_RESERVED,
    .size = 0,
    .target = NULL
};

/* Function */

void 
cli_version(){
    fputs("ampart-ng (Amlogic eMMC partition tool) by 7Ji, development version, debug usage only\n", stderr);
}

size_t
cli_human_readable_to_size_and_report(
    char const * const  literal, 
    char const * const  name
){
    const size_t size = util_human_readable_to_size(literal);
    char suffix;
    const double size_d = util_size_to_human_readable(size, &suffix);
    fprintf(stderr, "CLI interface: Setting %s to %zu / 0x%lx (%lf%c)\n", name, size, size, size_d, suffix);
    return size;
}

static inline 
void 
cli_describe_options() {
    char suffix_gap_reserved, suffix_gap_partition, suffix_offset_reserved, suffix_offset_dtb;
    double gap_reserved = util_size_to_human_readable(cli_options.gap_reserved, &suffix_gap_reserved);
    double gap_partition = util_size_to_human_readable(cli_options.gap_partition, &suffix_gap_partition);
    double offset_reserved = util_size_to_human_readable(cli_options.offset_reserved, &suffix_offset_reserved);
    double offset_dtb = util_size_to_human_readable(cli_options.offset_dtb, &suffix_offset_dtb);
    fprintf(stderr, "CLI describe options: mode %s, operating on %s, content type %s, dry run: %s, reserved gap: %lu (%lf%c), generic gap: %lu (%lf%c), reserved offset: %lu (%lf%c), dtb offset: %lu (%lf%c)\n", cli_mode_strings[cli_options.mode], cli_options.target, cli_content_type_strings[cli_options.content], cli_options.dry_run ? "yes" : "no", cli_options.gap_reserved, gap_reserved, suffix_gap_reserved, cli_options.gap_partition, gap_partition, suffix_gap_partition, cli_options.offset_reserved, offset_reserved, suffix_offset_reserved, cli_options.offset_dtb, offset_dtb, suffix_offset_dtb);
}

static inline
int
cli_read(
    struct dtb_buffer_helper * * const  bhelper,
    struct ept_table * * const          table
){
    int fd = open(cli_options.target, O_RDONLY);
    if (fd < 0) {
        fputs("CLI read: Failed to open target\n", stderr);
        return 1;
    }
    size_t dtb_offset = 0, ept_offset = 0;
    switch (cli_options.content) {
        case CLI_CONTENT_TYPE_DTB:
            break;
        case CLI_CONTENT_TYPE_RESERVED:
            dtb_offset = cli_options.offset_dtb;
            break;
        case CLI_CONTENT_TYPE_DISK:
            dtb_offset = cli_options.offset_reserved + cli_options.offset_dtb;
            ept_offset = cli_options.offset_reserved;
            break;
        default:
            fputs("CLI read: Ilegal target content type (auto), this should not happen\n", stderr);
            close(fd);
            return 2;
    }
    fprintf(stderr, "CLI read: Seeking to %zu to read DTB and report\n", dtb_offset);
    if (lseek(fd, dtb_offset, SEEK_SET) < 0) {
        fprintf(stderr, "CLI read: Failed to seek for DTB, errno: %d, error: %s\n", errno, strerror(errno));
        close(fd);
        return 3;
    }
    *bhelper = dtb_read_into_buffer_helper_and_report(fd, cli_options.size - dtb_offset, cli_options.content != CLI_CONTENT_TYPE_DTB);
    if (cli_options.content != CLI_CONTENT_TYPE_DTB) {
        fprintf(stderr, "CLI read: Seeking to %zu to read EPT and report\n", ept_offset);
        if (lseek(fd, ept_offset, SEEK_SET) < 0) {
            fprintf(stderr, "CLI read: Failed to seek for EPT, errno: %d, error: %s\n", errno, strerror(errno));
            if (*bhelper) {
                free(*bhelper);
                *bhelper = NULL;
            }
            close(fd);
            return 4;
        }
        *table = ept_read_and_report(fd, cli_options.size - ept_offset);
        // if (!table) {
        //     switch (cli_options.mode) {
        //         case CLI_MODE_ETOD:
        //         case CLI_MODE_PEDANTIC:
        //         case CLI_MODE_EEDIT:
        //         case CLI_MODE_ESNAPSHOT:
        //             fprintf(stderr, "CLI early stage: Mode %s requires EPT to exist and is valid, these requirement are however not met, giving up\n", cli_mode_strings[cli_options.mode]);
        //             close(fd);
        //             return 7;
        //         default:
        //             break;
        //     }
        // }
        // free(table);
    }
    close(fd);
    return 0;
}

// static inline
// int 
// cli_early_stage(
//     int     argc,
//     char *  argv[]
// ){
//     int const r = cli_read();
//     for (int i =0; i<argc; ++i) {
//         printf("%d: %s\n", i, argv[i]);
//     }
//     return r;
// }

static inline
int
cli_parse_mode(){
    for (enum cli_modes mode = CLI_MODE_INVALID; mode <= CLI_MODE_ECREATE; ++mode) {
        if (!strcmp(cli_mode_strings[mode], optarg)) {
            fprintf(stderr, "CLI interface: Mode is set to %s\n", optarg);
            cli_options.mode = mode;
            return 0;
        }
    }
    fprintf(stderr, "CLI interface: Invalid mode %s\n", optarg);
    return 1;
}

static inline
int
cli_parse_content(){
    for (enum cli_content_types content = CLI_CONTENT_TYPE_AUTO; content <= CLI_CONTENT_TYPE_DISK; ++content) {
        if (!strcmp(cli_content_type_strings[content], optarg)) {
            fprintf(stderr, "CLI interface: Content type is set to %s\n", optarg);
            cli_options.content = content;
            return 0;
        }
    }
    fprintf(stderr, "CLI interface: Invalid type %s\n", optarg);
    return 1;
}

static inline
int
cli_parse_options(
    int const * const   argc,
    char *              argv[]
){
    int c, option_index = 0;
    struct option const long_options[] = {
        {"version",         no_argument,        NULL,   'v'},
        {"help",            no_argument,        NULL,   'h'},
        {"mode",            required_argument,  NULL,   'm'},
        {"content",         required_argument,  NULL,   'c'},
        {"strict-device",   no_argument,        NULL,   's'},
        {"dry-run",         no_argument,        NULL,   'd'},
        {"offset-reserved", required_argument,  NULL,   'R'},
        {"offset-dtb",      required_argument,  NULL,   'D'},
        {"gap-partition",   required_argument,  NULL,   'p'},
        {"gap-reserved",    required_argument,  NULL,   'r'},
        {NULL,              0,                  NULL,  '\0'}
    };
    while ((c = getopt_long(*argc, argv, "vhm:c:dR:D:p:r:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'v':   // version
                cli_version();
                return -1;
            case 'h':   // help
                cli_version();
                fputs("\nampart does not provide command-line help message due to its complex syntax, please refer to the project Wiki for documentation:\nhttps://github.com/7Ji/ampart/wiki\n\n", stderr);
                return -1;
            case 'm':  // mode:
                if (cli_parse_mode()) {
                    return 1;
                }
                break;
            case 'c':   // type:
                if (cli_parse_content()) {
                    return 2;
                }
                break;
            case 's':   // strict-device
                fputs("CLI interface: Enabled strict-device, ampart won't try to find the corresponding disk when target is a block device and is not a full eMMC drive\n", stderr);
                cli_options.strict_device = true;
                break;
            case 'd':   // dry-run
                fputs("CLI interface: Enabled dry-run, no write will be made to the underlying files/devices\n", stderr);
                cli_options.dry_run = true;
                break;
            case 'R':   // offset-reserved:
                cli_options.offset_reserved = cli_human_readable_to_size_and_report(optarg, "offset of reserved partition relative to the whole eMMC drive");
                break;
            case 'D':   // offset-dtb:
                cli_options.offset_dtb = cli_human_readable_to_size_and_report(optarg, "offset of dtb partition relative to the reserved partition");
                break;
            case 'p':   // gap-partition:
                cli_options.gap_partition = cli_human_readable_to_size_and_report(optarg, "gap between partitions (except between bootloader and reserved)");
                break;
            case 'r':   // gap-reserved:
                cli_options.gap_reserved = cli_human_readable_to_size_and_report(optarg, "gap between bootloader and reserved partitions");
                break;
            default:
                fprintf(stderr, "CLI interface: Unrecognizable option %s\n", argv[optind-1]);
                return 3;
        }
    }
    return 0;
}

static inline
int
cli_find_disk(){
    char *path_disk = io_find_disk(cli_options.target);
    if (!path_disk) {
        fprintf(stderr, "CLI interface: Failed to get the corresponding disk of %s, try to force the target type or enable strict-device mode to disable auto-identification\n", cli_options.target);
        return 1;
    }
    fprintf(stderr, "CLI interface: Operating on '%s' instead, content type is now disk\n", path_disk);
    free(cli_options.target);
    cli_options.target = path_disk;
    cli_options.content = CLI_CONTENT_TYPE_DISK;
    return 0;
}

static inline
int
cli_options_complete_target_info(){
    struct io_target_type *target_type = io_identify_target_type(cli_options.target);
    if (!target_type) {
        fprintf(stderr, "CLI interface: failed to identify the type of target '%s'\n", cli_options.target);
        return 1;
    }
    cli_options.size = target_type->size;
    io_describe_target_type(target_type, NULL);
    bool find_disk = false;
    if (cli_options.content == CLI_CONTENT_TYPE_AUTO) {
        fputs("CLI interface: Start auto content type identification, specify target type if you don't want this\n", stderr);
        switch (target_type->content) {
            case IO_TARGET_TYPE_CONTENT_DISK:
                fputs("CLI interface: Content auto identified as whole disk\n", stderr);
                cli_options.content = CLI_CONTENT_TYPE_DISK;
                break;
            case IO_TARGET_TYPE_CONTENT_RESERVED:
                fputs("CLI interface: Content auto identified as reserved partition\n", stderr);
                cli_options.content = CLI_CONTENT_TYPE_RESERVED;
                break;
            case IO_TARGET_TYPE_CONTENT_DTB:
                fputs("CLI interface: Content auto identified as DTB partition\n", stderr);
                cli_options.content = CLI_CONTENT_TYPE_DTB;
                break;
            case IO_TARGET_TYPE_CONTENT_UNSUPPORTED:
                fprintf(stderr, "CLI interface: failed to identify the content type of target '%s', please set its type manually\n", cli_options.target);
                return 2;
        }
        if (target_type->file == IO_TARGET_TYPE_FILE_BLOCKDEVICE && target_type->content != IO_TARGET_TYPE_CONTENT_DISK) {
            find_disk = true;
        }
    } else if (target_type->file == IO_TARGET_TYPE_FILE_BLOCKDEVICE && cli_options.content != CLI_CONTENT_TYPE_DISK) {
        find_disk = true;
    }
    free(target_type);
    if (!cli_options.strict_device && find_disk) {
        int r = cli_find_disk();
        if (r) {
            return 3;
        }
    }
    return 0;
}

static inline
int
cli_complete_options(
    int const   argc,
    char *      argv[]
){
    if (cli_options.mode == CLI_MODE_INVALID) {
        fputs("CLI interface: Mode not set or invalid, you must specify the mode with --mode [mode] argument\n", stderr);
        return 1;
    }
    if (cli_options.dry_run) {
        cli_options.write = CLI_WRITE_NOTHING;
    }
    if (optind < argc) {
        cli_options.target = strdup(argv[optind++]);
        if (!cli_options.target) {
            fprintf(stderr, "CLI interface: Failed to duplicate target string '%s'\n", argv[optind-1]);
            return 2;
        }
        fprintf(stderr, "CLI interface: Operating on target file/block device '%s'\n", cli_options.target);
        int const r = cli_options_complete_target_info();
        if (r) {
            return 3;
        }
    } else {
        fputs("CLI interface: Too few arguments, target file/block device must be set as the first non-positional argument\n", stderr);
        return 4;
    }
    if (cli_options.content == CLI_CONTENT_TYPE_AUTO) {
        fputs("CLI interface: Content type not identified, give up, try setting it manually\n", stderr);
        return 5;
    }
    return 0;
}

static inline
int
cli_mode_dtoe(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table
){
    fputs("CLI mode dtoe: Create EPT from DTB\n", stderr);
    if (!bhelper || !bhelper->dtb_count) {
        fputs("CLI mode dtoe: DTB not correct or invalid\n", stderr);
        return 1;
    }
    if (dtb_check_buffers_partitions(bhelper)) {
        fputs("CLI mode dtoe: Not all DTB entries have partitions node and identical, refuse to work\n", stderr);
        return 2;
    }
    uint64_t capacity;
    if (cli_options.content == CLI_CONTENT_TYPE_DISK) {
        capacity = cli_options.size;
    } else {
        if (!table) {
            fputs("CLI mode dtoe: Can't get eMMC size, since target is not a full disk (image), and EPT is not valid, refuse to work\n", stderr);
            return 3;
        }
        capacity = ept_get_capacity(table);
    }
    struct ept_table *table_new = ept_complete_dtb(bhelper->dtbs->partitions, capacity);
    if (!table_new) {
        fputs("CLI mode dtoe: Failed to create new partition table\n", stderr);
        return 4;
    }
    ept_report(table_new);
    if (table && !ept_compare(table, table_new)) {
        fputs("CLI mode dtoe: New table is the same as the old table, no need to update\n", stderr);
        free(table_new);
        return 0;
    }
    if (cli_options.content != CLI_CONTENT_TYPE_DTB) {
        // Write
    }
    free(table_new);
    return 0;
}

static inline
int
cli_mode_etod(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table
){
    fputs("CLI mode etod: Recreate partitions node in DTB from EPT\n", stderr);
    if (!bhelper) {
        return 1;
    }
    if (!table) {
        return 2;
    }
    return 0;
}

static inline
int
cli_mode_epedantic(
    struct ept_table const * const  table
){
    fputs("CLI mode epedantic: Check if EPT is pedantic\n", stderr);
    if (!table) {
        return 1;
    }
    return 0;
}

static inline
int
cli_mode_dedit(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table,
    int const                               argc,
    char *                                  argv[]
){
    fputs("CLI mode dedit: Edit partitions node in DTB, and potentially create EPT from it\n", stderr);
    if (argc <= 0) {
        fputs("CLI mode dedit: No PARG, early quite\n", stderr);
        return 0;
    }
    for (int i =0; i<argc; ++i) {
        printf("%d: %s\n", i, argv[i]);
    }
    if (!bhelper) {
        return 1;
    }
    if (!table) {
        return 2;
    }
    
    return 0;
}

static inline
int
cli_mode_eedit(
    struct ept_table const * const  table,
    int const                       argc,
    char *                          argv[]
){
    fputs("CLI mode eedit: Edit EPT\n", stderr);
    if (argc <= 0) {
        fputs("CLI mode eedit: No PARG, early quite\n", stderr);
        return 0;
    }
    for (int i =0; i<argc; ++i) {
        printf("%d: %s\n", i, argv[i]);
    }
    if (!table) {
        return 1;
    }
    return 0;
}

static inline
int
cli_mode_dsnapshot(
    struct dtb_buffer_helper const * const  bhelper
){
    fputs("CLI mode dsnapshot: Take snapshot of partitions node in DTB\n", stderr);
    if (!bhelper) {
        return 1;
    }
    return 0;
}

static inline
int
cli_mode_esnapshot(
    struct ept_table const * const  table
){
    fputs("CLI mode esnapshot: Take snapshot of EPT\n", stderr);
    if (!table) {
        return 1;
    }
    return 0;
}

static inline
int
cli_mode_dclone(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table,
    int const                               argc,
    char *                                  argv[]
){
    fputs("CLI mode dclone: Apply a snapshot taken in dsnapshot mode\n", stderr);
    if (argc <= 0) {
        fputs("CLI mode dclone: No PARG, early quite\n", stderr);
        return 0;
    }
    for (int i =0; i<argc; ++i) {
        printf("%d: %s\n", i, argv[i]);
    }
    if (!bhelper) {
        return 1;
    }
    if (!table) {
        return 2;
    }
    return 0;
}

static inline
int
cli_mode_eclone(
    struct ept_table const * const  table,
    int const                       argc,
    char *                          argv[]
){
    fputs("CLI mode eclone: Apply a snapshot taken in esnapshot mode\n", stderr);
    if (argc <= 0) {
        fputs("CLI mode eclone: No PARG, early quit\n", stderr);
        return 0;
    }
    for (int i =0; i<argc; ++i) {
        printf("%d: %s\n", i, argv[i]);
    }
    if (!table) {
        return 1;
    }
    return 0;
}

static inline
int
cli_mode_ecreate(
    struct ept_table const * const  table,
    int const                       argc,
    char *                          argv[]
){
    fputs("CLI mode ecreate: Create EPT in a YOLO way\n", stderr);
    if (argc <= 0) {
        fputs("CLI mode ecreate: No PARG, early quit\n", stderr);
        return 0;
    }
    for (int i =0; i<argc; ++i) {
        printf("%d: %s\n", i, argv[i]);
    }
    if (!table) {
        return 1;
    }
    return 0;
}

static inline
int 
cli_dispatcher(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table,
    int const                               argc,
    char *                                  argv[]
){
    // for (int i =0; i<argc; ++i) {
    //     printf("%d: %s\n", i, argv[i]);
    // }
    if (cli_options.mode == CLI_MODE_INVALID) {
        fputs("CLI dispatcher: invalid mode\n", stderr);
        return -1;
    } else {
        fprintf(stderr, "CLI dispatcher: Dispatch to mode %s\n", cli_mode_strings[cli_options.mode]);
    }
    switch (cli_options.mode) {
        case CLI_MODE_INVALID:
            return -1;
        case CLI_MODE_DTOE:
            return cli_mode_dtoe(bhelper, table);
        case CLI_MODE_ETOD:
            return cli_mode_etod(bhelper, table);
        case CLI_MODE_EPEDANTIC:
            return cli_mode_epedantic(table);
        case CLI_MODE_DEDIT:
            return cli_mode_dedit(bhelper, table, argc, argv);
        case CLI_MODE_EEDIT:
            return cli_mode_eedit(table, argc, argv);
        case CLI_MODE_DSNAPSHOT:
            return cli_mode_dsnapshot(bhelper);
        case CLI_MODE_ESNAPSHOT:
            return cli_mode_esnapshot(table);
        case CLI_MODE_DCLONE:
            return cli_mode_dclone(bhelper, table, argc, argv);
        case CLI_MODE_ECLONE:
            return cli_mode_eclone(table, argc, argv);
        case CLI_MODE_ECREATE:
            return cli_mode_ecreate(table, argc, argv);
    }
    return 0;
}

int 
cli_interface(
    int const   argc,
    char *      argv[]
){
    int r = cli_parse_options(&argc, argv);
    if (r < 0) {
        return 0;
    } else if (r > 0) {
        return r;
    }
    if ((r = cli_complete_options(argc, argv))) {
        return 10 + r;
    }
    cli_describe_options();
    struct dtb_buffer_helper *bhelper = NULL;
    struct ept_table *table = NULL;
    if ((r = cli_read(&bhelper, &table))) {
        return 20 + r;
    }
    cli_dispatcher(bhelper, table, argc - optind, argv + optind);
    dtb_free_buffer_helper(&bhelper);
    if (table) {
        free(table);
    }
    return 0;
}