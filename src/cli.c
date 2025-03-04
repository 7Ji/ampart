/* Self */

#include "cli.h"

/* System */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

/* Local */

#include "common.h"
#include "ept.h"
#include "gzip.h"
#include "io.h"
#include "parg.h"
#include "util.h"
#include "version.h"

/* Definition */

#define CLI_WRITE_NOTHING   0b00000000
#define CLI_WRITE_DTB       0b00000001
#define CLI_WRITE_TABLE     0b00000010
#define CLI_WRITE_MIGRATES  0b00000100

#define CLI_LAZY_WRITE

#define CLI_WEBREPORT_URL_PARENT    "https://7ji.github.io/ampart-web-reporter/?"
#define CLI_WEBREPORT_ARG_ESNAPSHOT "esnapshot="
#define CLI_WEBREPORT_ARG_DSNAPSHOT "dsnapshot="
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
    .target = ""
};

/* Function */

void
cli_version(){
    prln_info("ampart-ng (Amlogic eMMC partition tool) by 7Ji, version %s", version);
}

size_t
cli_human_readable_to_size_and_report(
    char const * const  literal,
    char const * const  name
){
    const size_t size = util_human_readable_to_size(literal);
    char suffix;
    const double size_d = util_size_to_human_readable(size, &suffix);
    prln_info("setting %s to %zu / 0x%lx (%lf%c)", name, size, size, size_d, suffix);
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
    prln_info("mode %s, operating on %s, content type %s, migration strategy: %s, dry run: %s, reserved gap: %lu (%lf%c), generic gap: %lu (%lf%c), reserved offset: %lu (%lf%c), dtb offset: %lu (%lf%c)", cli_mode_strings[cli_options.mode], cli_options.target, cli_content_type_strings[cli_options.content], cli_migrate_strings[cli_options.migrate], cli_options.dry_run ? "yes" : "no", cli_options.gap_reserved, gap_reserved, suffix_gap_reserved, cli_options.gap_partition, gap_partition, suffix_gap_partition, cli_options.offset_reserved, offset_reserved, suffix_offset_reserved, cli_options.offset_dtb, offset_dtb, suffix_offset_dtb);
}

static inline
int
cli_read(
    struct dtb_buffer_helper * const    bhelper,
    struct ept_table * const            table
){
    int fd = open(cli_options.target, O_RDONLY);
    if (fd < 0) {
        prln_error_with_errno("failed to open target");
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
            prln_info("mode is set to %s", optarg);
            cli_options.mode = mode;
            return 0;
        }
    }
    prln_fatal("invalid mode %s", optarg);
    return 1;
}

static inline
int
cli_parse_content(){
    for (enum cli_content_types content = CLI_CONTENT_TYPE_AUTO; content <= CLI_CONTENT_TYPE_DISK; ++content) {
        if (!strcmp(cli_content_type_strings[content], optarg)) {
            prln_info("content type is set to %s", optarg);
            cli_options.content = content;
            return 0;
        }
    }
    prln_fatal("invalid type %s", optarg);
    return 1;
}

static inline
int
cli_parse_migrate(){
    for (enum cli_migrate migrate = CLI_MIGRATE_ESSENTIAL; migrate <= CLI_MIGRATE_ALL; ++migrate) {
        if (!strcmp(cli_migrate_strings[migrate], optarg)) {
            prln_info("migration strategy is set to %s", optarg);
            cli_options.migrate = migrate;
            return 0;
        }
    }
    prln_fatal("invalid migration strategy %s", optarg);
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
                prln_warn("enabled strict-device, ampart won't try to find the corresponding disk when target is a block device and is not a full eMMC drive");
                cli_options.strict_device = true;
                break;
            case 'd':   // dry-run
                prln_warn("enabled dry-run, no write will be made to the underlying files/devices");
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
                prln_fatal("unrecognizable option %s", argv[optind-1]);
                return 3;
        }
    }
    return 0;
}

static inline
void
cli_option_replace_target(
    // struct cli_options *const restrict cli_options,
    char const *const restrict target
){
    size_t len_target = strnlen(target, (sizeof cli_options.target) - 1);
    memcpy(cli_options.target, target, len_target);
    cli_options.target[len_target] = '\0';
}

static inline
int
cli_find_disk(){
    char *path_disk = io_find_disk(cli_options.target);
    if (!path_disk) {
        prln_error("failed to get the corresponding disk of %s, try to force the target type or enable strict-device mode to disable auto-identification", cli_options.target);
        return 1;
    }
    prln_warn("operating on '%s' instead, content type is now disk", path_disk);
    cli_option_replace_target(path_disk);
    free(path_disk);
    cli_options.content = CLI_CONTENT_TYPE_DISK;
    return 0;
}

static inline
int
cli_options_complete_target_info(){
    struct io_target_type target_type;
    if (io_identify_target_type(&target_type, cli_options.target)) {
        prln_error("failed to identify the type of target '%s'", cli_options.target);
        return 1;
    }
    cli_options.size = target_type.size;
    io_describe_target_type(&target_type, NULL);
    bool find_disk = false;
    if (cli_options.content == CLI_CONTENT_TYPE_AUTO) {
        prln_info("content type set as auto, will use the type identified earlier as type");
        switch (target_type.content) {
            case IO_TARGET_TYPE_CONTENT_DISK:
                prln_info("content auto identified as whole disk");
                cli_options.content = CLI_CONTENT_TYPE_DISK;
                break;
            case IO_TARGET_TYPE_CONTENT_RESERVED:
                prln_info("content auto identified as reserved partition");
                cli_options.content = CLI_CONTENT_TYPE_RESERVED;
                break;
            case IO_TARGET_TYPE_CONTENT_DTB:
                prln_info("content auto identified as DTB partition");
                cli_options.content = CLI_CONTENT_TYPE_DTB;
                break;
            case IO_TARGET_TYPE_CONTENT_UNSUPPORTED:
                prln_error("failed to identify the content type of target '%s', please set its type manually", cli_options.target);
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
        prln_info("target is block device for a whole drive and we will try to re-read partitions after adjusting EPT");
    } else {
        prln_info("will not attempt to re-read partitions after adjusting EPT");
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
        prln_info("mode not set or invalid, you must specify the mode with --mode [mode] argument");
        // return 1;
    }
    if (cli_options.dry_run) {
        cli_options.write = CLI_WRITE_NOTHING;
    }
    if (optind < argc) {
        cli_option_replace_target(argv[optind++]);
        prln_warn("operating on target file/block device '%s'", cli_options.target);
        int const r = cli_options_complete_target_info();
        if (r) {
            return 3;
        }
    } else {
        prln_fatal("too few arguments, target file/block device must be set as the first non-positional argument");
        return 4;
    }
    if (cli_options.content == CLI_CONTENT_TYPE_AUTO) {
        prln_fatal("content type not identified, give up, try setting it manually");
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
        prln_error("buffer and Dparts not both valid and contain partitions, refuse to continue");
        return -1;
    }
    if (cli_options.content == CLI_CONTENT_TYPE_AUTO) {
        prln_fatal("target content type not recognized, this should not happen, refuse to continue");
        return -2;
    }
    if (dparts) {
        prln_info("trying to write DTB with the following partitions:");
        dts_report_partitions_simple(dparts);
        if (dts_valid_partitions_simple(dparts)) {
            prln_error("partitions illegal, refuse to continue");
            return 1;
        }
    } else {
        prln_info("trying to write DTB with no partitions");
    }
    uint8_t *dtb_new;
    size_t dtb_new_size;
    if (dtb_compose(&dtb_new, &dtb_new_size, bhelper, dparts)) {
        prln_error("failed to generate new DTBs");
        return 1;
    }
    prln_error("size of new DTB (as a whole) is 0x%lx", dtb_new_size);
    if (cli_options.content != CLI_CONTENT_TYPE_DTB && dtb_as_partition(&dtb_new, &dtb_new_size)) {
        prln_error("failed to package DTB in partition");
        return 2;
    }
    if (cli_options.dry_run) {
        prln_info("in dry-run mode, assuming success");
        free(dtb_new);
        return 0;
    }
    int fd = open(cli_options.target, cli_options.content == CLI_CONTENT_TYPE_DTB ? O_WRONLY | O_TRUNC : O_WRONLY);
    if (fd < 0) {
        prln_error("failed to open target");
        free(dtb_new);
        return 1;
    }
    off_t const dtb_offset = io_seek_dtb(fd);
    if (dtb_offset < 0) {
        prln_error("failed to seek");
        close(fd);
        free(dtb_new);
        return 2;
    }
    if (io_write_till_finish(fd, dtb_new, dtb_new_size)) {
        prln_error("failed to write");
        close(fd);
        free(dtb_new);
        return 3;
    }
    if (fsync(fd)) {
        prln_error("failed to sync");
        close(fd);
        free(dtb_new);
        return 4;
    }
    close(fd);
    free(dtb_new);
    prln_info("write successful");
    return 0;
}

static inline
int
cli_write_ept(
    struct ept_table const * const  old,
    struct ept_table const * const  new
) {
    if (!new) {
        prln_error("table invalid, refuse to continue");
        return 1;
    }
    prln_info("trying to write the following EPT:");
    ept_report(new);
    struct io_migrate_helper mhelper;
    bool const can_migrate = old && cli_options.migrate != CLI_MIGRATE_NONE && !ept_migrate_plan(&mhelper, old, new, cli_options.migrate == CLI_MIGRATE_ALL ? true : false) && cli_options.content == CLI_CONTENT_TYPE_DISK;
    switch (cli_options.content) {
        case CLI_CONTENT_TYPE_DTB:
            prln_info("target is DTB, no need to write");
            if (can_migrate) {
                free(mhelper.entries);
            }
            return 0;
        case CLI_CONTENT_TYPE_AUTO:
            prln_error("target content type not recognized, this should not happen, refuse to continue");
            if (can_migrate) {
                free(mhelper.entries);
            }
            return 2;
        default:
            break;
    }
    if (ept_valid_table(new)) {
        prln_error("table illegal, refuse to continue");
        return 3;
    }
    if (cli_options.dry_run) {
        prln_info("in dry-run mode, assuming success");
        if (can_migrate) {
            free(mhelper.entries);
        }
        return 0;
    }
    int const fd = open(cli_options.target, (can_migrate ? O_RDWR : O_WRONLY) | O_DSYNC);
    if (fd < 0) {
        prln_error("failed to open target");
        if (can_migrate) {
            free(mhelper.entries);
        }
        return 1;
    }
    if (can_migrate) {
        mhelper.fd = fd;
        if (io_migrate(&mhelper)) {
            prln_error("failed to migrate");
            close(fd);
            free(mhelper.entries);
            return 2;
        }
        free(mhelper.entries);
    }
    off_t const ept_offset = io_seek_ept(fd);
    if (ept_offset < 0) {
        prln_error("failed to seek");
        close(fd);
        return 3;
    }
    if (io_write_till_finish(fd, (struct ept_table *)new, sizeof *new)){
        prln_error("failed to write");
        close(fd);
        return 4;
    }
    if (fsync(fd)) {
        prln_error("failed to sync");
        close(fd);
        return 5;
    }
    prln_info("write successful");
    if (cli_options.rereadpart) {
        prln_error("trying to tell kernel to re-read partitions");
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
        prln_info("using target file/block device size %zu as the capacity, since it's full disk", cli_options.size);
        return cli_options.size;
    } else {
        size_t const capacity = ept_get_capacity(table);
        prln_info("using max partition end %zu as the capacity, since target is %s and is not full disk", capacity, cli_content_type_strings[cli_options.content]);
        return capacity;
    }
}

static inline
int
cli_mode_dtoe(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table
){
    prln_info("create EPT from DTB");
    if (!bhelper || !bhelper->dtb_count) {
        prln_error("DTB not correct or invalid");
        return 1;
    }
    if (dtb_check_buffers_partitions(bhelper)) {
        prln_error("not all DTB entries have partitions node and identical, refuse to work");
        return 2;
    }
    uint64_t const capacity = cli_get_capacity(table);
    if (!capacity) {
        prln_error("cannot get valid capacity, give up");
        return 3;
    }
    struct ept_table table_new;
    if (ept_table_from_dts_partitions_helper(&table_new, &bhelper->dtbs->phelper, capacity)) {
        prln_error("failed to create new partition table");
        return 4;
    }
    ept_report(&table_new);
#ifdef CLI_LAZY_WRITE
    if (table && !ept_compare_table(table, &table_new)) {
        prln_info("new table is the same as the old table, no need to update");
        return 0;
    }
#endif
    if (cli_write_ept(table, &table_new)) {
        prln_error("failed to write new EPT");
        return 5;
    }
    return 0;
}

static inline
int
cli_mode_epedantic(
    struct ept_table const * const  table
){
    prln_info("check if EPT is pedantic");
    if (!table || !table->partitions_count || ept_valid_table(table)) {
        prln_error("EPT does not exist or is invalid, refuse to work");
        return -1;
    }
    if (EPT_IS_PEDANTIC(table)) {
        prln_info("EPT is pedantic");
        return 0;
    } else {
        prln_info("EPT is not pedantic");
        return 1;
    }
}

static inline
int
cli_mode_etod(
    struct dtb_buffer_helper const * const  bhelper,
    struct ept_table const * const          table
){
    prln_info("recreate partitions node in DTB from EPT");
    if (!bhelper) {
        prln_error("DTB does not exist, refuse to continue");
        return -1;
    }
    if (!table || !table->partitions_count || ept_valid_table(table)) {
        prln_error("EPT does not exist or is invalid, refuse to work");
        return -2;
    }
    if (ept_is_not_pedantic(table)) {
        prln_error("refuse to convert a non-pedantic EPT to DTB");
        return 1;
    }
    size_t const capacity = cli_get_capacity(table);
    if (!capacity) {
        prln_error("failed to get valid capacity");
        return 2;
    }
    struct dts_partitions_helper_simple dparts;
    if (ept_table_to_dts_partitions_helper(table, &dparts, capacity)) {
        prln_error("failed to convert to DTB partitions");
        return 3;
    }
    dts_report_partitions_simple(&dparts);
#ifdef CLI_LAZY_WRITE
    if (bhelper && !dts_compare_partitions_mixed(&bhelper->dtbs->phelper, &dparts)) {
        prln_info("result partitions are the same as old DTB, no need to write");
        return 0;
    }
#endif
    if (cli_write_dtb(bhelper, &dparts)) {
        prln_error("failed to write DTB");
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
        prln_warn("no PARG, early quit");
        return -1;
    }
    if (max > 0 && argc > max) {
        prln_error("too many PARGS, only %d is allowed yet you've defined %d", max, argc);
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
        prln_error("illegal arguments");
        return -1;
    }
    prln_info("checking if EPT also needs update");
    size_t const capacity = cli_get_capacity(table);
    if (!capacity) {
        prln_error("failed to check capacity");
        return 1;
    }
    struct ept_table table_new;
    if (ept_table_from_dts_partitions_helper_simple(&table_new, dparts, capacity)) {
        prln_error("failed create EPT from DTB");
        return 2;
    }
#ifdef CLI_LAZY_WRITE
    if (table && !ept_compare_table(table, &table_new)) {
        prln_info("corresponding table same, no need to write it");
    } else {
#endif
        if (cli_write_ept(table, &table_new)) {
            prln_error("failed to write EPT");
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
    prln_info("edit partitions node in DTB, and potentially create EPT from it");
    if (cli_check_parg_count(argc, 0) || !bhelper) {
        prln_error("illegal arguments");
        return -1;
    }
    struct dts_partitions_helper_simple dparts;
    if (dts_partitions_helper_to_simple(&dparts, &bhelper->dtbs->phelper)) {
        prln_error("failed to convert to simple helper");
        return 2;
    }
    if (dts_dedit_parse(&dparts, argc, argv)) {
        prln_error("failed to parse arguments");
        return 3;
    }
#ifdef CLI_LAZY_WRITE
    if (bhelper && !dts_compare_partitions_mixed(&bhelper->dtbs->phelper, &dparts)) {
        prln_error("result DTB same as old, no need to write");
    } else {
#endif
        if (cli_write_dtb(bhelper, &dparts)) {
            prln_error("failed to write DTB");
            return 4;
        }
#ifdef CLI_LAZY_WRITE
    }
#endif
    if (table && cli_options.content != CLI_CONTENT_TYPE_DTB && cli_write_ept_from_dtb(table, &dparts)) {
        prln_error("failed to also write EPT");
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
        prln_error("illegal arguments");
        return -1;
    }
    if (EPT_IS_PEDANTIC(table)) {
        prln_warn("result EPT is pedantic, also updating DTB");
        struct dts_partitions_helper_simple dparts;
        if (ept_table_to_dts_partitions_helper(table, &dparts, capacity)) {
            prln_error("failed to create corresponding DTB partitions list");
            return 4;
        }
#ifdef CLI_LAZY_WRITE
        if (bhelper && bhelper->dtb_count && !dts_compare_partitions_mixed(&bhelper->dtbs->phelper, &dparts)) {
            prln_info("corresponding DTB partitions not updated, no need to write");
        } else {
#endif
            if (cli_write_dtb(bhelper, &dparts)) {
                prln_error("failed to write corresponding DTB partitions list");
                return 5;
            }
#ifdef CLI_LAZY_WRITE
        }
#endif
    } else {
        prln_warn("result EPT is not pedantic, removing partitions node in DTB");
#ifdef CLI_LAZY_WRITE
        if (bhelper && bhelper->dtb_count && !bhelper->dtbs->has_partitions) {
            prln_info("DTB does not have partitions node, no need to remove it");
        } else {
#endif
            if (cli_write_dtb(bhelper, NULL)) {
                prln_error("failed to remove partitions node in DTB");
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
    prln_info("edit EPT");
    if (!bhelper || !table || cli_check_parg_count(argc, 0)) {
        prln_error("illegal arguments");
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
            prln_error("failed to write new EPT");
            return 3;
        }
#ifdef CLI_LAZY_WRITE
    } else {
        prln_info("old and new table same, no need to write");
    }
#endif
    if (bhelper && cli_write_dtb_from_ept(bhelper, &table_new, capacity)) {
        prln_error("failed to update DTB");
        return 4;
    }
    return 0;
}

static inline
int
cli_mode_dsnapshot(
    struct dtb_buffer_helper const * const  bhelper
){
    prln_info("take snapshot of partitions node in DTB");
    if (!bhelper || !bhelper->dtb_count) {
        prln_error("DTB not correct or invalid");
        return 1;
    }
    if (dtb_check_buffers_partitions(bhelper)) {
        prln_error("not all DTB entries have partitions node and identical, refuse to work");
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
    prln_info("take snapshot of EPT");
    if (!table || !table->partitions_count || ept_valid_table(table)) {
        prln_error("EPT does not exist or is invalid, refuse to work");
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
    // The reported URL should look this:
    // https://7ji.github.io/ampart-web-reporter/?esnapshot=bootloader:0:4194304:0%20reserved:37748736:67108864:0%20cache:113246208:754974720:2%20env:876609536:8388608:0%20logo:893386752:33554432:1%20recovery:935329792:33554432:1%20rsv:977272832:8388608:1%20tee:994050048:8388608:1%20crypt:1010827264:33554432:1%20misc:1052770304:33554432:1%20instaboot:1094713344:536870912:1%20boot:1639972864:33554432:1%20system:1681915904:1073741824:1%20params:2764046336:67108864:2%20bootfiles:2839543808:754974720:2%20data:3602907136:4131389440:4&dsnapshot=logo::33554432:1%20recovery::33554432:1%20rsv::8388608:1%20tee::8388608:1%20crypt::33554432:1%20misc::33554432:1%20instaboot::536870912:1%20boot::33554432:1%20system::1073741824:1%20cache::536870912:2%20params::67108864:2%20data::-1:4    
    char arg_dsnapshot[CLI_WEBREPORT_URL_MAXLEN];
    char arg_esnapshot[CLI_WEBREPORT_URL_MAXLEN];
    char url[CLI_WEBREPORT_URL_MAXLEN];
    unsigned len_dsnapshot;
    unsigned len_esnapshot;
    unsigned len_used;
    unsigned len_current;
    bool has_esnapshot;

    prln_info("print a URL that can be opened in browser to get well-formatted partitio info");
    if (!bhelper || !bhelper->dtb_count) {
        prln_error("DTB not correct or invalid");
        return 1;
    }
    if (table && table->partitions_count && !ept_valid_table(table)) {
        has_esnapshot = true;
    } else {
        prln_error("EPT does not exist or is invalid, web report would not contain EPT");
        has_esnapshot = false;
    }

    len_dsnapshot = 0;
    int r = dtb_webreport(bhelper, arg_dsnapshot, &len_dsnapshot);
    if (r) {
        prln_error("failed to prepare dsnapshot argument");
        return 2 + r;
    }
    len_esnapshot = 0;
    if (has_esnapshot) {
        r = ept_webreport(table, arg_esnapshot, &len_esnapshot);
        if (r) {
            prln_error("failed to prepare esnapshot argument");
            return 6 + r;
        }
    }
    if ((sizeof CLI_WEBREPORT_URL_PARENT) - 1 + (has_esnapshot ? ((sizeof CLI_WEBREPORT_ARG_ESNAPSHOT) - 1 + len_esnapshot + 1): 0) + (sizeof CLI_WEBREPORT_ARG_DSNAPSHOT) + - 1 + len_dsnapshot + 1 > CLI_WEBREPORT_URL_MAXLEN) {
        prln_error("result URL length would be too long, aborting");
        return 9;
    }
    len_current = (sizeof CLI_WEBREPORT_URL_PARENT) - 1;
    memcpy(url, CLI_WEBREPORT_URL_PARENT, len_current);
    len_used = len_current;
    if (has_esnapshot) {
        len_current = sizeof(CLI_WEBREPORT_ARG_ESNAPSHOT) - 1;
        memcpy(url + len_used, CLI_WEBREPORT_ARG_ESNAPSHOT, len_current);
        len_used += len_current;
        memcpy(url + len_used, arg_esnapshot, len_esnapshot);
        len_used += len_esnapshot;
        url[len_used++] = '&';
    }
    len_current = sizeof(CLI_WEBREPORT_ARG_DSNAPSHOT) - 1;
    memcpy(url + len_used, CLI_WEBREPORT_ARG_DSNAPSHOT, len_current);
    len_used += len_current;
    memcpy(url + len_used, arg_dsnapshot, len_dsnapshot);
    len_used += len_dsnapshot;
    url[len_used] = '\0';
    prln_info("please copy the following URL to your browser to check the well-formatted partition info:");
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
    prln_info("apply a snapshot taken in dsnapshot mode");
    int const r = cli_check_parg_count(argc, MAX_PARTITIONS_COUNT);
    if (r) {
        if (r < 0) return 0; else return 1;
    }
    if (!bhelper || !bhelper->dtb_count || !bhelper->dtbs->buffer) {
        prln_error("no valid DTB, refuse to work");
        return 2;
    }
    struct dts_partitions_helper_simple dparts;
    if (dts_dclone_parse(&dparts, argc, argv)) {
        prln_error("failed to parse PARGs");
        return 3;
    }
#ifdef CLI_LAZY_WRITE
    if (!dts_compare_partitions_mixed(&bhelper->dtbs->phelper, &dparts)) {
        prln_info("new partitions same as old, no need to write");
    } else {
#endif
        if (cli_write_dtb(bhelper, &dparts)) {
            prln_error("failed to write");
            return 4;
        }
#ifdef CLI_LAZY_WRITE
    }
#endif
    if (table && cli_options.content != CLI_CONTENT_TYPE_DTB && cli_write_ept_from_dtb(table, &dparts)) {
        prln_error("failed to also write EPT");
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
    prln_info("apply a snapshot taken in esnapshot mode");
    int const r = cli_check_parg_count(argc, MAX_PARTITIONS_COUNT);
    if (r) {
        if (r < 0) return 0; else return 1;
    }
    if (!table || !ept_valid_table(table)) {
        prln_warn("old table corrupted or not valid, continue anyway");
    }
    size_t const capacity = cli_get_capacity(table);
    if (!capacity) {
        prln_error("cannot get valid capacity, give up");
        return 2;
    }
    struct ept_table table_new;
    if (ept_eclone_parse(&table_new, argc, argv, capacity)) {
        prln_error("failed to get new EPT");
        return 3;
    }
#ifdef CLI_LAZY_WRITE
    if (ept_compare_table(table, &table_new)) {
        prln_warn("new table is different, need to write");
#endif
        if (cli_write_ept(table, &table_new)) {
            prln_error("failed to write EPT");
            return 4;
        }
#ifdef CLI_LAZY_WRITE
    } else {
        prln_warn("new table is the same as old table, no need to write");
    }
#endif
    if (cli_write_dtb_from_ept(bhelper, &table_new, capacity)) {
        prln_error("failed to also update DTB");
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
    prln_info("create EPT in a YOLO way");
    if (argc > MAX_PARTITIONS_COUNT) { // 0 is allowed, where a default empty table will be created
        prln_error("you've defined too many partitions");
        return -1;
    }
    if (!table || !ept_valid_table(table)) {
        prln_warn("old table corrupted or not valid, continue anyway");
    }
    size_t const capacity = cli_get_capacity(table);
    if (!capacity) {
        prln_error("cannot get valid capacity, give up");
        return 2;
    }
    struct ept_table table_new;
    if (ept_ecreate_parse(&table_new, table, capacity, argc, argv)) {
        prln_error("failed to get new EPT");
        return 3;
    }
#ifdef CLI_LAZY_WRITE
    if (ept_compare_table(table, &table_new)) {
#endif
        if (cli_write_ept(table, &table_new)) {
            prln_error("failed to write new EPT");
            return 4;
        }
#ifdef CLI_LAZY_WRITE
    } else {
        prln_info("old and new table same, no need to write");
    }
#endif
    if (bhelper && cli_write_dtb_from_ept(bhelper, &table_new, capacity)) {
        prln_error("failed to update DTB");
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
        prln_fatal("invalid mode");
        return -1;
    } else {
        prln_info("dispatch to mode %s", cli_mode_strings[cli_options.mode]);
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