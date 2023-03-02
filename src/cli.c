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
#include "parg.h"
#include "util.h"

/* Definition */

#define CLI_WRITE_NOTHING   0b00000000
#define CLI_WRITE_DTB       0b00000001
#define CLI_WRITE_TABLE     0b00000010
#define CLI_WRITE_MIGRATES  0b00000100

#define CLI_LAZY_WRITE

#define CLI_WEBREPORT_URL           "https://7ji.github.io/ampart-web-reporter/?%s&%s"
#define CLI_WEBREPORT_URL_MAXLEN    0x800

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
    "webreport",
    "dclone",
    "eclone",
    "ecreate"
};

char const  cli_migrate_strings[][20] = {
    "essential",
    "none",
    "all"
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
    .migrate = CLI_MIGRATE_ESSENTIAL,
    .dry_run = false,
    .strict_device = false,
    .rereadpart = true,
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
#ifdef CLI_VERSION
    fputs("ampart-ng (Amlogic eMMC partition tool) by 7Ji, version "CLI_VERSION"\n", stderr);
#else
    fputs("ampart-ng (Amlogic eMMC partition tool) by 7Ji, development version, debug usage only\n", stderr);
#endif
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
    fprintf(stderr, "CLI describe options: mode %s, operating on %s, content type %s, migration strategy: %s, dry run: %s, reserved gap: %lu (%lf%c), generic gap: %lu (%lf%c), reserved offset: %lu (%lf%c), dtb offset: %lu (%lf%c)\n", cli_mode_strings[cli_options.mode], cli_options.target, cli_content_type_strings[cli_options.content], cli_migrate_strings[cli_options.migrate], cli_options.dry_run ? "yes" : "no", cli_options.gap_reserved, gap_reserved, suffix_gap_reserved, cli_options.gap_partition, gap_partition, suffix_gap_partition, cli_options.offset_reserved, offset_reserved, suffix_offset_reserved, cli_options.offset_dtb, offset_dtb, suffix_offset_dtb);
}

static inline
int
cli_read(
    struct dtb_buffer_helper * const    bhelper,
    struct ept_table * const            table
){
    int fd = open(cli_options.target, O_RDONLY);
    if (fd < 0) {
        fputs("CLI read: Failed to open target\n", stderr);
        return 1;
    }
    off_t const offset_dtb = io_seek_dtb(fd);
    if (offset_dtb < 0) {
        close(fd);
        return 2;
    }
    dtb_read_into_buffer_helper_and_report(bhelper, fd, cli_options.size - offset_dtb, cli_options.content != CLI_CONTENT_TYPE_DTB);
    if (cli_options.content != CLI_CONTENT_TYPE_DTB) {
        off_t const offset_ept = io_seek_ept(fd);
        if (offset_ept < 0) {
            close(fd);
            return 3;
        }
        ept_read_and_report(table, fd, cli_options.size - offset_ept);
    }
    close(fd);
    return 0;
}

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
cli_parse_migrate(){
    for (enum cli_migrate migrate = CLI_MIGRATE_ESSENTIAL; migrate <= CLI_MIGRATE_ALL; ++migrate) {
        if (!strcmp(cli_migrate_strings[migrate], optarg)) {
            fprintf(stderr, "CLI interface: Migration strategy is set to %s\n", optarg);
            cli_options.migrate = migrate;
            return 0;
        }
    }
    fprintf(stderr, "CLI interface: Invalid migration strategy %s\n", optarg);
    return 1;
}

static inline
void
cli_help() {
    fputs(
        "\n"
        "ampart ([nop 1] ([nop 1 arg]) [nop 2] ([nop 2 arg])...) [target] [parg 1] [parg 2]...\n\n"
        " => [nop]: non-positional argument, can be written anywhere\n"
        "  -> if a nop has its required argument, that required argument must be right after that very nop.\n"
        "   --version/-v\t\tprint the version and early quit\n"
        "   --help/-h\t\tprint this help message and early quit\n"
        "   --mode/-m [mode]\tset ampart to run in one of the following modes:\n"
        "\t\t\t -> dtoe: generate EPT from DTB\n"
        "\t\t\t -> etod: generate DTB partitions from EPT\n"
        "\t\t\t -> epedantic: check if EPT is pedantic\n"
        "\t\t\t -> dedit: edit partitions in DTB\n"
        "\t\t\t -> eedit: edit EPT\n"
        "\t\t\t -> dsnapshot: take a snapshot of current partitions in DTB\n"
        "\t\t\t -> esnapshot: take a snapshot of current EPT\n"
        "\t\t\t -> webreport: report partitions on web with ampart-web-report\n"
        "\t\t\t -> dclone: clone-in a previously taken dsnapshot\n"
        "\t\t\t -> eclone: clone-in a previously taken esnapshot\n"
        "\t\t\t -> ecreate: create partitions in a YOLO way\n"
        "   --content/-c [type]\tset the content type of [target] to one of the following:\n"
        "\t\t\t -> auto: auto-identifying (default)\n"
        "\t\t\t -> dtb: content is DTB, either plain, multi or gzipped\n"
        "\t\t\t -> reserved: content is Amlogic reserved partition\n"
        "\t\t\t -> disk: content is a whole eMMC (dump)\n"
        "   --migrate/-M [style]\thow to migrate partitions:\n"
        "\t\t\t -> none: don't migrate any partitions\n"
        "\t\t\t -> essential: only migrate essential partitions (reserved, env, misc, logo, etc) (default)\n"
        "\t\t\t -> all: migrate all partitions\n"
        "   --strict-device/-s\tif target is a block device and --content is set, stick with that, don't try to find corresponding block device for whole eMMC\n"
        "   --dry-run/-d\t\tdon't write anything\n"
        "   --offset-reserved/-R [value]\toffset of reserved partition in disk\n"
        "   --offset-dtb/-D [value]\toffset of dtb in reserved partition\n"
        "   --gap-partition/-p [value]\tgap between partitions\n"
        "   --gap-reserved/-r [value]\tgap before reserved partition\n"
        "\n"
        " => [target]: target file or block device to operate on\n"
        "  -> could be or contain content of either DTB, reserved partition, or the whole disk\n"
        "\n"
        " => [parg]: partition argument\n"
        "\n"
        "since writing too much help message will make the result binary too large, the full documentation can't be provided in CLI help message, please refer to the project repo for documentation:\nhttps://github.com/7Ji/ampart\n\n", 
        stderr
    );
}

static inline
int
cli_parse_options(
    int const * const       argc,
    char * const * const    argv
){
    int c, option_index = 0;
    struct option const long_options[] = {
        {"version",         no_argument,        NULL,   'v'},
        {"help",            no_argument,        NULL,   'h'},
        {"mode",            required_argument,  NULL,   'm'},
        {"content",         required_argument,  NULL,   'c'},
        {"migrate",         required_argument,  NULL,   'M'},
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
                cli_help();
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
            case 'M':
                if (cli_parse_migrate()) {
                    return 3;
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
    struct io_target_type target_type;
    if (io_identify_target_type(&target_type, cli_options.target)) {
        fprintf(stderr, "CLI interface: failed to identify the type of target '%s'\n", cli_options.target);
        return 1;
    }
    cli_options.size = target_type.size;
    io_describe_target_type(&target_type, NULL);
    bool find_disk = false;
    if (cli_options.content == CLI_CONTENT_TYPE_AUTO) {
        fputs("CLI interface: Content type set as auto, will use the type identified earlier as type\n", stderr);
        switch (target_type.content) {
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
        if (target_type.file == IO_TARGET_TYPE_FILE_BLOCKDEVICE && target_type.content != IO_TARGET_TYPE_CONTENT_DISK) {
            find_disk = true;
        }
    } else if (target_type.file == IO_TARGET_TYPE_FILE_BLOCKDEVICE && cli_options.content != CLI_CONTENT_TYPE_DISK) {
        find_disk = true;
    }
    if (!cli_options.strict_device && find_disk) {
        int r = cli_find_disk();
        if (r) {
            return 3;
        }
    }
    if ((cli_options.rereadpart =
            cli_options.rereadpart &&
            target_type.file == IO_TARGET_TYPE_FILE_BLOCKDEVICE &&
            cli_options.content == CLI_CONTENT_TYPE_DISK)) {
        fputs("CLI interface: Target is block device for a whole drive and we will try to re-read partitions after adjusting EPT\n", stderr);
    } else {
        fputs("CLI interface: Will not attempt to re-read partitions after adjusting EPT\n", stderr);
    }
    return 0;
}

static inline
int
cli_complete_options(
    int const               argc,
    char * const * const    argv
){
    if (cli_options.mode == CLI_MODE_INVALID) {
        fputs("CLI interface: Mode not set or invalid, you must specify the mode with --mode [mode] argument\n", stderr);
        // return 1;
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
cli_write_dtb(
    struct dtb_buffer_helper const * const              bhelper,
    struct dts_partitions_helper_simple const * const   dparts
){
    if (!bhelper || !bhelper->dtb_count || (dparts && !dparts->partitions_count)) {
        fputs("CLI write DTB: Buffer and Dparts not both valid and contain partitions, refuse to continue\n", stderr);
        return -1;
    }
    if (cli_options.content == CLI_CONTENT_TYPE_AUTO) {
        fputs("CLI write DTB: Target content type not recognized, this should not happen, refuse to continue\n", stderr);
        return -2;
    }
    if (dparts) {
        fputs("CLI write DTB: Trying to write DTB with the following partitions:\n", stderr);
        dts_report_partitions_simple(dparts);
        if (dts_valid_partitions_simple(dparts)) {
            fputs("CLI write DTB: Partitions illegal, refuse to continue\n", stderr);
            return 1;
        }
    } else {
        fputs("CLI write DTB: Trying to write DTB with no partitions\n", stderr);
    }
    uint8_t *dtb_new;
    size_t dtb_new_size;
    if (dtb_compose(&dtb_new, &dtb_new_size, bhelper, dparts)) {
        fputs("CLI write DTB: Failed to generate new DTBs\n", stderr);
        return 1;
    }
    fprintf(stderr, "CLI write DTB: size of new DTB (as a whole) is 0x%lx\n", dtb_new_size);
    if (cli_options.content != CLI_CONTENT_TYPE_DTB && dtb_as_partition(&dtb_new, &dtb_new_size)) {
        fputs("CLI write DTB: Failed to package DTB in partition\n", stderr);
        return 2;
    }
    if (cli_options.dry_run) {
        fputs("CLI write DTB: In dry-run mode, assuming success\n", stderr);
        free(dtb_new);
        return 0;
    }
    int fd = open(cli_options.target, cli_options.content == CLI_CONTENT_TYPE_DTB ? O_WRONLY | O_TRUNC : O_WRONLY);
    if (fd < 0) {
        fputs("CLI write DTB: Failed to open target\n", stderr);
        free(dtb_new);
        return 1;
    }
    off_t const dtb_offset = io_seek_dtb(fd);
    if (dtb_offset < 0) {
        fputs("CLI write DTB: Failed to seek\n", stderr);
        close(fd);
        free(dtb_new);
        return 2;
    }
    if (io_write_till_finish(fd, dtb_new, dtb_new_size)) {
        fputs("CLI write DTB: Failed to write\n", stderr);
        close(fd);
        free(dtb_new);
        return 3;
    }
    if (fsync(fd)) {
        fputs("CLI write DTB: Failed to sync\n", stderr);
        close(fd);
        free(dtb_new);
        return 4;
    }
    close(fd);
    free(dtb_new);
    fputs("CLI write DTB: Write successful\n", stderr);
    return 0;
}

static inline
int
cli_write_ept(
    struct ept_table const * const  old,
    struct ept_table const * const  new
) {
    if (!new) {
        fputs("CLI write EPT: Table invalid, refuse to continue\n", stderr);
        return 1;
    }
    fputs("CLI write EPT: Trying to write the following EPT:\n", stderr);
    ept_report(new);
    struct io_migrate_helper mhelper;
    bool const can_migrate = old && cli_options.migrate != CLI_MIGRATE_NONE && !ept_migrate_plan(&mhelper, old, new, cli_options.migrate == CLI_MIGRATE_ALL ? true : false) && cli_options.content == CLI_CONTENT_TYPE_DISK;
    switch (cli_options.content) {
        case CLI_CONTENT_TYPE_DTB:
            fputs("CLI write EPT: Target is DTB, no need to write\n", stderr);
            if (can_migrate) {
                free(mhelper.entries);
            }
            return 0;
        case CLI_CONTENT_TYPE_AUTO:
            fputs("CLI write EPT: Target content type not recognized, this should not happen, refuse to continue\n", stderr);
            if (can_migrate) {
                free(mhelper.entries);
            }
            return 2;
        default:
            break;
    }
    if (ept_valid_table(new)) {
        fputs("CLI write EPT: Table illegal, refuse to continue\n", stderr);
        return 3;
    }
    if (cli_options.dry_run) {
        fputs("CLI write EPT: In dry-run mode, assuming success\n", stderr);
        if (can_migrate) {
            free(mhelper.entries);
        }
        return 0;
    }
    int const fd = open(cli_options.target, (can_migrate ? O_RDWR : O_WRONLY) | O_DSYNC);
    if (fd < 0) {
        fputs("CLI write EPT: Failed to open target\n", stderr);
        if (can_migrate) {
            free(mhelper.entries);
        }
        return 1;
    }
    if (can_migrate) {
        mhelper.fd = fd;
        if (io_migrate(&mhelper)) {
            fputs("CLI write EPT: Failed to migrate\n", stderr);
            close(fd);
            free(mhelper.entries);
            return 2;
        }
        free(mhelper.entries);
    }
    off_t const ept_offset = io_seek_ept(fd);
    if (ept_offset < 0) {
        fputs("CLI write EPT: Failed to seek\n", stderr);
        close(fd);
        return 3;
    }
    if (io_write_till_finish(fd, (struct ept_table *)new, sizeof *new)){
        fputs("CLI write EPT: Failed to write\n", stderr);
        close(fd);
        return 4;
    }
    if (fsync(fd)) {
        fputs("CLI write EPT: Failed to sync\n", stderr);
        close(fd);
        return 5;
    }
    fputs("CLI write EPT: Write successful\n", stderr);
    if (cli_options.rereadpart) {
        fputs("CLI write EPT: Trying to tell kernel to re-read partitions\n", stderr);
        /* Just don't care about return value */
        io_rereadpart(fd);
    }
    close(fd);
    return 0;
}

static inline
size_t
cli_get_capacity(
    struct ept_table const * const  table
){
    if (cli_options.content == CLI_CONTENT_TYPE_DISK) {
        fprintf(stderr, "CLI get capacity: Using target file/block device size %zu as the capacity, since it's full disk\n", cli_options.size);
        return cli_options.size;
    } else {
        size_t const capacity = ept_get_capacity(table);
        fprintf(stderr, "CLI get capacity: Using max partition end %zu as the capacity, since target is %s and is not full disk\n", capacity, cli_content_type_strings[cli_options.content]);
        return capacity;
    }
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
    uint64_t const capacity = cli_get_capacity(table);
    if (!capacity) {
        fputs("CLI mode dtoe: Cannot get valid capacity, give up\n", stderr);
        return 3;
    }
    struct ept_table table_new;
    if (ept_table_from_dts_partitions_helper(&table_new, &bhelper->dtbs->phelper, capacity)) {
        fputs("CLI mode dtoe: Failed to create new partition table\n", stderr);
        return 4;
    }
    ept_report(&table_new);
#ifdef CLI_LAZY_WRITE
    if (table && !ept_compare_table(table, &table_new)) {
        fputs("CLI mode dtoe: New table is the same as the old table, no need to update\n", stderr);
        return 0;
    }
#endif
    if (cli_write_ept(table, &table_new)) {
        fputs("CLI mode dtoe: Failed to write new EPT\n", stderr);
        return 5;
    }
    return 0;
}

static inline
int
cli_mode_epedantic(
    struct ept_table const * const  table
){
    fputs("CLI mode epedantic: Check if EPT is pedantic\n", stderr);
    if (!table || !table->partitions_count || ept_valid_table(table)) {
        fputs("CLI mode epedantic: EPT does not exist or is invalid, refuse to work\n", stderr);
        return -1;
    }
    if (EPT_IS_PEDANTIC(table)) {
        fputs("CLI mode epedantic: EPT is pedantic\n", stderr);
        return 0;
    } else {
        fputs("CLI mode epedantic: EPT is not pedantic\n", stderr);
        return 1;
    }
}

static inline
int
cli_mode_etod(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table
){
    fputs("CLI mode etod: Recreate partitions node in DTB from EPT\n", stderr);
    if (!bhelper) {
        fputs("CLI mode etod: DTB does not exist, refuse to continue\n", stderr);
        return -1;
    }
    if (!table || !table->partitions_count || ept_valid_table(table)) {
        fputs("CLI mode etod: EPT does not exist or is invalid, refuse to work\n", stderr);
        return -2;
    }
    if (ept_is_not_pedantic(table)) {
        fputs("CLI mode etod: Refuse to convert a non-pedantic EPT to DTB\n", stderr);
        return 1;
    }
    size_t const capacity = cli_get_capacity(table);
    if (!capacity) {
        fputs("CLI mode etod: Failed to get valid capacity\n", stderr);
        return 2;
    }
    struct dts_partitions_helper_simple dparts;
    if (ept_table_to_dts_partitions_helper(table, &dparts, capacity)) {
        fputs("CLI mode etod: Failed to convert to DTB partitions\n", stderr);
        return 3;
    }
    dts_report_partitions_simple(&dparts);
#ifdef CLI_LAZY_WRITE
    if (bhelper && !dts_compare_partitions_mixed(&bhelper->dtbs->phelper, &dparts)) {
        fputs("CLI mode etod: Result partitions are the same as old DTB, no need to write\n", stderr);
        return 0;
    }
#endif
    if (cli_write_dtb(bhelper, &dparts)) {
        fputs("CLI mode dtoe: Failed to write DTB\n", stderr);
        return 4;
    }
    return 0;
}

static inline
int
cli_check_parg_count(
    int const   argc,
    int const   max
){
    if (argc <= 0) {
        fputs("CLI check PARG count: No PARG, early quit\n", stderr);
        return -1;
    }
    if (max > 0 && argc > max) {
        fprintf(stderr, "CLI check PARG count: Too many PARGS, only %d is allowed yet you've defined %d\n", max, argc);
        return 1;
    }
    return 0;
}

static inline
int
cli_write_ept_from_dtb(
    struct ept_table const * const                    table,
    struct dts_partitions_helper_simple const * const dparts
){
    if (!table || !dparts) {
        fputs("CLI write EPT from DTB: Illegal arguments\n", stderr);
        return -1;
    }
    fputs("CLI write EPT from DTB: Checking if EPT also needs update\n", stderr);
    size_t const capacity = cli_get_capacity(table);
    if (!capacity) {
        fputs("CLI write EPT from DTB: Failed to check capacity\n", stderr);
        return 1;
    }
    struct ept_table table_new;
    if (ept_table_from_dts_partitions_helper_simple(&table_new, dparts, capacity)) {
        fputs("CLI write EPT from DTB: Failed create EPT from DTB\n", stderr);
        return 2;
    }
#ifdef CLI_LAZY_WRITE
    if (table && !ept_compare_table(table, &table_new)) {
        fputs("CLI write EPT from DTB: Corresponding table same, no need to write it\n", stderr);
    } else {
#endif
        if (cli_write_ept(table, &table_new)) {
            fputs("CLI write EPT from DTB: Failed to write EPT\n", stderr);
            return 3;
        }
#ifdef CLI_LAZY_WRITE
    }
#endif
    return 0;
}

static inline
int
cli_mode_dedit(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table,
    int const                               argc,
    char const * const * const              argv
){
    fputs("CLI mode dedit: Edit partitions node in DTB, and potentially create EPT from it\n", stderr);
    if (cli_check_parg_count(argc, 0) || !bhelper) {
        fputs("CLI mode dedit: Illegal arguments\n", stderr);
        return -1;
    }
    struct dts_partitions_helper_simple dparts;
    if (dts_partitions_helper_to_simple(&dparts, &bhelper->dtbs->phelper)) {
        fputs("CLI mode dedit: Failed to convert to simple helper\n", stderr);
        return 2;
    }
    if (dts_dedit_parse(&dparts, argc, argv)) {
        fputs("CLI mode dedit: Failed to parse arguments\n", stderr);
        return 3;
    }
#ifdef CLI_LAZY_WRITE
    if (bhelper && !dts_compare_partitions_mixed(&bhelper->dtbs->phelper, &dparts)) {
        fputs("CLI mode dedit: Result DTB same as old, no need to write\n", stderr);
    } else {
#endif
        if (cli_write_dtb(bhelper, &dparts)) {
            fputs("CLI mode dedit: Failed to write DTB\n", stderr);
            return 4;
        }
#ifdef CLI_LAZY_WRITE
    }
#endif
    if (table && cli_options.content != CLI_CONTENT_TYPE_DTB && cli_write_ept_from_dtb(table, &dparts)) {
        fputs("CLI mode dedit: Failed to also write EPT\n", stderr);
        return 5;
    }
    return 0;
}

static inline
int
cli_write_dtb_from_ept(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table,
    size_t const                            capacity
){
    if (!bhelper || !table || !capacity) {
        fputs("CLI write DTB from EPT: Illegal arguments\n", stderr);
        return -1;
    }
    if (EPT_IS_PEDANTIC(table)) {
        fputs("CLI write DTB from EPT: Result EPT is pedantic, also updating DTB\n", stderr);
        struct dts_partitions_helper_simple dparts;
        if (ept_table_to_dts_partitions_helper(table, &dparts, capacity)) {
            fputs("CLI write DTB from EPT: Failed to create corresponding DTB partitions list\n", stderr);
            return 4;
        }
#ifdef CLI_LAZY_WRITE
        if (bhelper && bhelper->dtb_count && !dts_compare_partitions_mixed(&bhelper->dtbs->phelper, &dparts)) {
            fputs("CLI write DTB from EPT: Corresponding DTB partitions not updated, no need to write\n", stderr);
        } else {
#endif
            if (cli_write_dtb(bhelper, &dparts)) {
                fputs("CLI write DTB from EPT: Failed to write corresponding DTB partitions list\n", stderr);
                return 5;
            }
#ifdef CLI_LAZY_WRITE
        }
#endif
    } else {
        fputs("CLI write DTB from EPT: Result EPT is not pedantic, removing partitions node in DTB\n", stderr);
#ifdef CLI_LAZY_WRITE
        if (bhelper && bhelper->dtb_count && !bhelper->dtbs->has_partitions) {
            fputs("CLI write DTB from EPT: DTB does not have partitions node, no need to remove it\n", stderr);
        } else {
#endif
            if (cli_write_dtb(bhelper, NULL)) {
                fputs("CLI write DTB from EPT: Failed to remove partitions node in DTB\n", stderr);
                return 3;
            }
#ifdef CLI_LAZY_WRITE
        }
#endif
    }
    return 0;
}

static inline
int
cli_mode_eedit(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table,
    int const                               argc,
    char const * const * const              argv
){
    fputs("CLI mode eedit: Edit EPT\n", stderr);
    if (!bhelper || !table || cli_check_parg_count(argc, 0)) {
        fputs("CLI mode eedit: Illegal arguments\n", stderr);
        return -1;
    }
    size_t const capacity = cli_get_capacity(table);
    if (!capacity) {
        return 1;
    }
    struct ept_table table_new = *table;
    if (ept_eedit_parse(&table_new, capacity, argc, argv)) {
        return 2;
    }
#ifdef CLI_LAZY_WRITE
    if (ept_compare_table(table, &table_new)) {
#endif
        if (cli_write_ept(table, &table_new)) {
            fputs("CLI mode eedit: Failed to write new EPT\n", stderr);
            return 3;
        }
#ifdef CLI_LAZY_WRITE
    } else {
        fputs("CLI mode eedit: Old and new table same, no need to write\n", stderr);
    }
#endif
    if (bhelper && cli_write_dtb_from_ept(bhelper, &table_new, capacity)) {
        fputs("CLI mode eedit: Failed to update DTB\n", stderr);
        return 4;
    }
    return 0;
}

static inline
int
cli_mode_dsnapshot(
    struct dtb_buffer_helper const * const  bhelper
){
    fputs("CLI mode dsnapshot: Take snapshot of partitions node in DTB\n", stderr);
    if (!bhelper || !bhelper->dtb_count) {
        fputs("CLI mode dsnapshot: DTB not correct or invalid\n", stderr);
        return 1;
    }
    if (dtb_check_buffers_partitions(bhelper)) {
        fputs("CLI mode dtoe: Not all DTB entries have partitions node and identical, refuse to work\n", stderr);
        return 2;
    }
    if (dtb_snapshot(bhelper)) {
        return 3;
    }
    return 0;
}

static inline
int
cli_mode_esnapshot(
    struct ept_table const * const  table
){
    fputs("CLI mode esnapshot: Take snapshot of EPT\n", stderr);
    if (!table || !table->partitions_count || ept_valid_table(table)) {
        fputs("CLI mode esnapshot: EPT does not exist or is invalid, refuse to work\n", stderr);
        return 1;
    }
    if (ept_snapshot(table)) {
        return 2;
    }
    return 0;
}

static inline
int
cli_mode_webreport(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table
) {
    fputs("CLI mode webreport: Print a URL that can be opened in browser to get well-formatted partitio info\n", stderr);
    if (!bhelper || !bhelper->dtb_count) {
        fputs("CLI mode webreport: DTB not correct or invalid\n", stderr);
        return 1;
    }
    if (!table || !table->partitions_count || ept_valid_table(table)) {
        fputs("CLI mode webreport: EPT does not exist or is invalid, refuse to work\n", stderr);
        return 2;
    }
    // The reported URL should look this:
    // https://7ji.github.io/ampart-web-reporter/?esnapshot=bootloader:0:4194304:0%20reserved:37748736:67108864:0%20cache:113246208:754974720:2%20env:876609536:8388608:0%20logo:893386752:33554432:1%20recovery:935329792:33554432:1%20rsv:977272832:8388608:1%20tee:994050048:8388608:1%20crypt:1010827264:33554432:1%20misc:1052770304:33554432:1%20instaboot:1094713344:536870912:1%20boot:1639972864:33554432:1%20system:1681915904:1073741824:1%20params:2764046336:67108864:2%20bootfiles:2839543808:754974720:2%20data:3602907136:4131389440:4&dsnapshot=logo::33554432:1%20recovery::33554432:1%20rsv::8388608:1%20tee::8388608:1%20crypt::33554432:1%20misc::33554432:1%20instaboot::536870912:1%20boot::33554432:1%20system::1073741824:1%20cache::536870912:2%20params::67108864:2%20data::-1:4    
    char arg_dsnapshot[CLI_WEBREPORT_URL_MAXLEN] = "";
    char arg_esnapshot[CLI_WEBREPORT_URL_MAXLEN] = "";
    unsigned len_dsnapshot = 0;
    unsigned len_esnapshot = 0;
    int r = dtb_webreport(bhelper, arg_dsnapshot, &len_dsnapshot);
    if (r) {
        fputs("CLI mode webreport: Failed to prepare dsnapshot argument\n", stderr);
        return 2 + r;
    }
    r = ept_webreport(table, arg_esnapshot, &len_esnapshot);
    if (r) {
        fputs("CLI mode webreport: Failed to prepare esnapshot argument\n", stderr);
        return 6 + r;
    }
    char url[2048];
    r = snprintf(url, CLI_WEBREPORT_URL_MAXLEN, CLI_WEBREPORT_URL, arg_dsnapshot, arg_esnapshot);
    if (r >= CLI_WEBREPORT_URL_MAXLEN) {
        fputs("CLI mode webreport: URL truncated\n", stderr);
        return 10;
    } else if (r < 0) {
        fputs("CLI mode webreport: Error occured during output\n", stderr);
        return 11;
    }
    fputs("CLI mode webreport: Please copy the following URL to your browser to check the well-formatted partition info:\n", stderr);
    puts(url);
    return 0;
}

static inline
int
cli_mode_dclone(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table,
    int const                               argc,
    char const * const * const              argv
){
    fputs("CLI mode dclone: Apply a snapshot taken in dsnapshot mode\n", stderr);
    int const r = cli_check_parg_count(argc, MAX_PARTITIONS_COUNT);
    if (r) {
        if (r < 0) return 0; else return 1;
    }
    if (!bhelper || !bhelper->dtb_count || !bhelper->dtbs->buffer) {
        fputs("CLI mode dclone: No valid DTB, refuse to work\n", stderr);
        return 2;
    }
    struct dts_partitions_helper_simple dparts;
    if (dts_dclone_parse(&dparts, argc, argv)) {
        fputs("CLI mode dclone: Failed to parse PARGs\n", stderr);
        return 3;
    }
#ifdef CLI_LAZY_WRITE
    if (!dts_compare_partitions_mixed(&bhelper->dtbs->phelper, &dparts)) {
        fputs("CLI mode dclone: New partitions same as old, no need to write\n", stderr);
    } else {
#endif
        if (cli_write_dtb(bhelper, &dparts)) {
            fputs("CLI mode dclone: Failed to write\n", stderr);
            return 4;
        }
#ifdef CLI_LAZY_WRITE
    }
#endif
    if (table && cli_options.content != CLI_CONTENT_TYPE_DTB && cli_write_ept_from_dtb(table, &dparts)) {
        fputs("CLI mode dclone: Failed to also write EPT\n", stderr);
        return 5;
    }
    return 0;
}

static inline
int
cli_mode_eclone(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table,
    int const                               argc,
    char const * const * const              argv
){
    fputs("CLI mode eclone: Apply a snapshot taken in esnapshot mode\n", stderr);
    int const r = cli_check_parg_count(argc, MAX_PARTITIONS_COUNT);
    if (r) {
        if (r < 0) return 0; else return 1;
    }
    if (!table || !ept_valid_table(table)) {
        fputs("CLI mode eclone: Warning, old table corrupted or not valid, continue anyway\n", stderr);
    }
    size_t const capacity = cli_get_capacity(table);
    if (!capacity) {
        fputs("CLI mode eclone: Cannot get valid capacity, give up\n", stderr);
        return 2;
    }
    struct ept_table table_new;
    if (ept_eclone_parse(&table_new, argc, argv, capacity)) {
        fputs("CLI mode eclone: Failed to get new EPT\n", stderr);
        return 3;
    }
#ifdef CLI_LAZY_WRITE
    if (ept_compare_table(table, &table_new)) {
        fputs("CLI mode eclone: New table is different, need to write\n", stderr);
#endif
        if (cli_write_ept(table, &table_new)) {
            fputs("CLI mode eclone: Failed to write EPT\n", stderr);
            return 4;
        }
#ifdef CLI_LAZY_WRITE
    } else {
        fputs("CLI mode eclone: New table is the same as old table, no need to write\n", stderr);
    }
#endif
    if (cli_write_dtb_from_ept(bhelper, &table_new, capacity)) {
        fputs("CLI mode eclone: Failed to also update DTB\n", stderr);
        return 5;
    }
    return 0;
}

static inline
int
cli_mode_ecreate(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table,
    int const                               argc,
    char const * const * const              argv
){
    fputs("CLI mode ecreate: Create EPT in a YOLO way\n", stderr);
    if (argc > MAX_PARTITIONS_COUNT) { // 0 is allowed, where a default empty table will be created
        fputs("CLI mode ecreate: You've defined too many partitions\n", stderr);
        return -1;
    }
    if (!table || !ept_valid_table(table)) {
        fputs("CLI mode ecreate: Warning, old table corrupted or not valid, continue anyway\n", stderr);
    }
    size_t const capacity = cli_get_capacity(table);
    if (!capacity) {
        fputs("CLI mode ecreate: Cannot get valid capacity, give up\n", stderr);
        return 2;
    }
    struct ept_table table_new;
    if (ept_ecreate_parse(&table_new, table, capacity, argc, argv)) {
        fputs("CLI mode ecreate: Failed to get new EPT\n", stderr);
        return 3;
    }
#ifdef CLI_LAZY_WRITE
    if (ept_compare_table(table, &table_new)) {
#endif
        if (cli_write_ept(table, &table_new)) {
            fputs("CLI mode ecreate: Failed to write new EPT\n", stderr);
            return 4;
        }
#ifdef CLI_LAZY_WRITE
    } else {
        fputs("CLI mode ecreate: Old and new table same, no need to write\n", stderr);
    }
#endif
    if (bhelper && cli_write_dtb_from_ept(bhelper, &table_new, capacity)) {
        fputs("CLI mode ecreate: Failed to update DTB\n", stderr);
        return 5;
    }
    return 0;
}

static inline
int 
cli_dispatcher(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table,
    int const                               argc,
    char const * const * const              argv
){
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
            return cli_mode_eedit(bhelper, table, argc, argv);
        case CLI_MODE_DSNAPSHOT:
            return cli_mode_dsnapshot(bhelper);
        case CLI_MODE_ESNAPSHOT:
            return cli_mode_esnapshot(table);
        case CLI_MODE_WEBREPORT:
            return cli_mode_webreport(bhelper, table);
        case CLI_MODE_DCLONE:
            return cli_mode_dclone(bhelper, table, argc, argv);
        case CLI_MODE_ECLONE:
            return cli_mode_eclone(bhelper, table, argc, argv);
        case CLI_MODE_ECREATE:
            return cli_mode_ecreate(bhelper, table, argc, argv);
    }
    return 0;
}

int 
cli_interface(
    int const               argc,
    char * const * const    argv
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
    struct dtb_buffer_helper bhelper;
    struct ept_table table;
    if ((r = cli_read(&bhelper, &table))) {
        return 20 + r;
    }
    r = cli_dispatcher(&bhelper, &table, argc - optind, (char const * const *)(argv + optind));
    dtb_free_buffer_helper(&bhelper);
    if (r) {
        return 30 + r;
    }
    return 0;
}

/* cli.c: Actual CLI API implementation of ampart */