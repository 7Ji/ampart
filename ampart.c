#define _GNU_SOURCE
#include "ampart.h"
#include <getopt.h>

#define prln_with_level(level, format, arg...) \
    fprintf(stderr, "[%s@"__FILE_NAME__":%u] " #level ": " format "\n", __func__, __LINE__, ##arg)

#define prln_fatal(format, arg...) \
    prln_with_level(fatal, format, ##arg)

#define prln_error(format, arg...) \
    prln_with_level(error, format, ##arg)

#define prln_warn(format, arg...) \
    prln_with_level(warning, format, ##arg)

#define prln_info(format, arg...) \
    prln_with_level(info, format, ##arg)

#ifdef DEBUGGING
#define prln_debug(format, arg...) \
    prln_with_level(debug, format, ##arg)
#else
#define prln_debug(format, arg...)
#endif

#define prln_error_with_errno(format, arg...) \
    prln_error(format", errno: %d, error: %s", ##arg, errno, strerror(errno))

void version() {
    prln_info("ampart-ng (Amlogic eMMC partition tool) by 7Ji, version %s, with libampart %u.%u.%u",
#ifdef AMPART_VERSION
        AMPART_VERSION
#else
        "unknown-development-only"
#endif
        , AMPART_LIB_MAJOR, AMPART_LIB_MINOR, AMPART_LIB_PATCH
    );
}

void help() {
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
        "Please refer to the project repo for full documentation:\nhttps://github.com/7Ji/ampart\n\n",
        stderr
    );
}

int main (int argc, char *argv[]) {
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
    while ((c = getopt_long(argc, argv, "vhm:c:dR:D:p:r:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'v':   // version
                version();
                return 0;
            case 'h':   // help
                version();
                help();
                return 0;
            // case 'm':  // mode:
            //     if (cli_parse_mode()) {
            //         return 1;
            //     }
            //     break;
            // case 'c':   // type:
            //     if (cli_parse_content()) {
            //         return 2;
            //     }
            //     break;
            // case 'M':
            //     if (cli_parse_migrate()) {
            //         return 3;
            //     }
            //     break;
            // case 's':   // strict-device
            //     prln_warn("enabled strict-device, ampart won't try to find the corresponding disk when target is a block device and is not a full eMMC drive");
            //     cli_options.strict_device = true;
            //     break;
            // case 'd':   // dry-run
            //     prln_warn("enabled dry-run, no write will be made to the underlying files/devices");
            //     cli_options.dry_run = true;
            //     break;
            // case 'R':   // offset-reserved:
            //     cli_options.offset_reserved = cli_human_readable_to_size_and_report(optarg, "offset of reserved partition relative to the whole eMMC drive");
            //     break;
            // case 'D':   // offset-dtb:
            //     cli_options.offset_dtb = cli_human_readable_to_size_and_report(optarg, "offset of dtb partition relative to the reserved partition");
            //     break;
            // case 'p':   // gap-partition:
            //     cli_options.gap_partition = cli_human_readable_to_size_and_report(optarg, "gap between partitions (except between bootloader and reserved)");
            //     break;
            // case 'r':   // gap-reserved:
            //     cli_options.gap_reserved = cli_human_readable_to_size_and_report(optarg, "gap between bootloader and reserved partitions");
            //     break;
            default:
                prln_fatal("unrecognizable option %s", argv[optind-1]);
                return 3;
        }
    }
    return 0;
    return AMPART_OK;
}
