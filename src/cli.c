#include "cli_p.h"

static inline int cli_parse_u64(uint64_t *const value, bool *const relative, const uint8_t requirement, const char *start, const char *const end) {
    if (*start == '+') {
        if (requirement & CLI_ARGUMENT_ALLOW_RELATIVE) {
            *relative = true;
            ++start;
        } else {
            fputs("CLI parse u64: Relative value set when not allowed\n", stderr);
            return 1;
        }
    } else {
        if (requirement & CLI_ARGUMENT_ALLOW_ABSOLUTE) {
            *relative = false;
        } else {
            fputs("CLI parse u64: Absolute value set when not allowed\n", stderr);
            return 2;
        }
    }
    if (end < start) {
        fputs("CLI parse u64: End before start\n", stderr);
        return 3;
    }
    const size_t len = end - start;
    if (len) {
        char *const str = malloc(sizeof(char) * (len + 1));
        if (!str) {
            fputs("CLI parse u64: Failed to create temp buffer to parse number\n", stderr);
            return 4;
        }
        strncpy(str, start, len);
        str[len] = '\0';
        *value = util_human_readable_to_size(str);
        free(str);
    } else {
        if (requirement & CLI_ARGUMENT_REQUIRED) {
            fputs("CLI parse u64: Required field not set\n", stderr);
            return 5;
        } else {
            *value = 0;
        }
    }
    return 0;
}

static inline int cli_parse_u64_update_mode(uint64_t *value, enum cli_partition_modify_detail_method *method, const char *start, const char *end) {
    switch (*start) {
        case '+':
            *method = CLI_PARTITION_MODIFY_DETAIL_ADD;
            ++start;
            break;
        case '-':
            *method = CLI_PARTITION_MODIFY_DETAIL_SUBSTRACT;
            ++start;
            break;
        default:
            *method = CLI_PARTITION_MODIFY_DETAIL_SET;
            break;
    }
    if (end < start) {
        fputs("CLI parse u64 update mode: End before start\n", stderr);
        return 1;
    }
    const size_t len = end - start;
    if (len) {
        char *str = malloc(sizeof(char) * (len + 1));
        if (!str) {
            fputs("CLI parse u64 update mode: Failed to create temp buffer to parse number\n", stderr);
            return 2;
        }
        strncpy(str, start, len);
        str[len] = '\0';
        *value = util_human_readable_to_size(str);
        free(str);
    } else {
        fputs("CLI parse u64 update mode: Offset/Size modifier must be set for relative mode\n", stderr);
        return 3;
    }
    return 0;
}

struct cli_partition_definer *cli_parse_partition_raw(const char *arg, uint8_t require_name, uint8_t require_offset, uint8_t require_size, uint8_t require_masks, uint32_t default_masks) {
    if (!arg) {
        fputs("CLI parse partition: No argument given\n", stderr);
        return NULL;
    }
    if (!arg[0]) {
        fputs("CLI parse partition: Empty argument\n", stderr);
        return NULL;
    }
    const char *c;
    const char *seperators[3];
    unsigned int seperator_id = 0;
    for (c=arg; *c; ++c) {
        if (*c == ':') {
            seperators[seperator_id++] = c;
        }
        if (seperator_id == 3) {
            break;
        }
    }
    if (seperator_id < 3) {
        fprintf(stderr, "CLI parse partition: Argument too short: %s\n", arg);
        return NULL;
    }
    bool have_name = seperators[0] != arg;
    CLI_PARSE_PARTITION_RAW_ALLOWANCE_CHECK(name)
    bool have_offset = seperators[1] - seperators[0] > 1;
    CLI_PARSE_PARTITION_RAW_ALLOWANCE_CHECK(offset)
    bool have_size = seperators[2] - seperators[1] > 1;
    CLI_PARSE_PARTITION_RAW_ALLOWANCE_CHECK(size)
    bool have_masks = *(seperators[2] + 1);
    CLI_PARSE_PARTITION_RAW_ALLOWANCE_CHECK(masks)
    struct cli_partition_definer *part = malloc(sizeof(struct cli_partition_definer));
    if (!part) {
        fprintf(stderr, "CLI parse partition: Failed to allocate memory for arg: %s\n", arg);
        return NULL;
    }
    memset(part, 0, sizeof(struct cli_partition_definer));
    size_t len_name = seperators[0] - arg;
    if (len_name > MAX_PARTITION_NAME_LENGTH - 1) {
        fprintf(stderr, "CLI parse partition: Partition name too long in arg: %s\n", arg);
        free(part);
        return NULL;
    }
    if (have_name) {
        strncpy(part->name, arg, len_name);
    }
    if (have_offset) {
        if (cli_parse_u64(&part->offset, &part->relative_offset, require_offset, seperators[0] + 1, seperators[1])) {
            fprintf(stderr, "CLI parse partition: Failed to parse offset in arg: %s\n", arg);
            free(part);
            return NULL;
        }
    }
    if (have_size) {
        if (cli_parse_u64(&part->size, &part->relative_size, require_size, seperators[1] + 1, seperators[2])) {
            fprintf(stderr, "CLI parse partition: Failed to parse size in arg: %s\n", arg);
            free(part);
            return NULL;
        }
    }
    part->masks = have_masks ? strtoul(seperators[2] + 1, NULL, 0) : default_masks;
    return part;
}

struct cli_partition_updater *cli_parse_partition_update_mode(const char *arg) {
    if (!arg) {
        fputs("CLI parse partition update mode: no argument given\n", stderr);
        return NULL;
    }
    bool select_mode;
    switch (arg[0]) {
        case '\0':
            fputs("CLI parse partition update mode: empty argument\n", stderr);
            return NULL;
        case '^':
            select_mode = true;
            break;
        default:
            select_mode = false;
            break;
    }
    struct cli_partition_updater *updater = malloc(sizeof(struct cli_partition_updater));
    if (!updater) {
        fputs("CLI parse partition update mode: failed to allocate memory for partition updater\n", stderr);
        return NULL;
    }
    memset(updater, 0, sizeof(struct cli_partition_updater));
    updater->modify = select_mode;
    if (select_mode) {
        switch(arg[1]) {
            case '\0':
            case ':':
            case '%':
            case '?':
            case '@':
                fprintf(stderr, "CLI parse partition update mode: no selector given in arg: %s\n", arg);
                free(updater);
                return NULL;
            case '-':
            case '0'...'9':
                updater->modifier.select = CLI_PARTITION_SELECT_RELATIVE;
                break;
            default:
                updater->modifier.select = CLI_PARTITION_SELECT_NAME;
                break;
        }
        char *select_end = NULL;
        for (char *c = (char *)arg+2; !select_end; ++c) {
            switch(*c) {
                case '\0':
                    fprintf(stderr, "CLI parse partition update mode: only selector is set, no action available: %s\n", arg);
                    free(updater);
                    return NULL;
                case '%':
                    if (!*(c + 1)) {
                        fprintf(stderr, "CLI parse partition update mode: no new name after clone operator is set: %s\n", arg);
                        free(updater);
                        return NULL;
                    }
                    select_end = c;
                    updater->modifier.modify_part = CLI_PARTITION_MODIFY_PART_CLONE;
                    break;
                case '?':
                    select_end = c;
                    updater->modifier.modify_part = CLI_PARTITION_MODIFY_PART_DELETE;
                    break;
                case '@': // ^-1@:-7  ^bootloader@+7 ^bootloader@-1
                    if (!*(c + 1)) {
                        fprintf(stderr, "CLI parse partition update mode: no placer after selector is set: %s\n", arg);
                        free(updater);
                        return NULL;
                    }
                    select_end = c;
                    updater->modifier.modify_part = CLI_PARTITION_MODIFY_PART_PLACE;
                    break;
                case ':':
                    if (!*(c + 1)) {
                        fprintf(stderr, "CLI parse partition update mode: no modifier after selector is set: %s\n", arg);
                        free(updater);
                        return NULL;
                    }
                    select_end = c;
                    updater->modifier.modify_part = CLI_PARTITION_MODIFY_PART_ADJUST; // It's not needed, but anyway
                    break;
                default:
                    break;
            }
        }
        size_t len_selector = select_end - arg - 1; // At least 1
        if (updater->modifier.select == CLI_PARTITION_SELECT_NAME) {
            if (len_selector > MAX_PARTITION_NAME_LENGTH - 1) {
                fprintf(stderr, "CLI parse partition update mode: Name selector too long: %s\n", arg);
                free(updater);
                return NULL;
            }
            strncpy(updater->modifier.select_name, arg + 1, len_selector);
        } else {
            char *str_buffer = malloc(sizeof(char) * (len_selector + 1));
            if (!str_buffer) {
                fprintf(stderr, "CLI parse partition update mode: Failed to allocate memory to parse relative selector: %s\n", arg);
                free(updater);
                return NULL;
            }
            strncpy(str_buffer, arg + 1, len_selector);
            updater->modifier.select_relative = strtol(str_buffer, NULL, 0);
            free(str_buffer);
        }
        switch (updater->modifier.modify_part) {
            case CLI_PARTITION_MODIFY_PART_ADJUST: {
                unsigned int seperator_id = 0;
                const char *seperators[3];
                for (const char *c = select_end + 1; *c; ++c) {
                    if (*c == ':') {
                        seperators[seperator_id++] = c;
                    }
                    if (seperator_id == 3) {
                        break;
                    }
                }
                if (seperator_id < 3) {
                    fprintf(stderr, "CLI parse partition update mode: Argument too short: %s\n", arg);
                    return NULL;
                }
                if (seperators[0] != select_end + 1) {
                    size_t len_name = seperators[0] - select_end - 1;
                    if (len_name > MAX_PARTITION_NAME_LENGTH - 1) {
                        free(updater);
                        return NULL;
                    }
                    strncpy(updater->modifier.name, select_end + 1, len_name);
                    updater->modifier.modify_name = CLI_PARTITION_MODIFY_DETAIL_SET;
                } else {
                    updater->modifier.modify_name = CLI_PARTITION_MODIFY_DETAIL_PRESERVE;
                }
                if (seperators[1] - seperators[0] > 1) {
                    if (cli_parse_u64_update_mode(&updater->modifier.offset, &updater->modifier.modify_offset, seperators[0] + 1, seperators[1])) {
                        fprintf(stderr, "CLI parse partition update mode: Failed to parse offset modifier in arg: %s\n", arg);
                        free(updater);
                        return NULL;
                    }
                } else {
                    updater->modifier.modify_offset = CLI_PARTITION_MODIFY_DETAIL_PRESERVE;
                }
                if (seperators[2] - seperators[1] > 1) {
                    if (cli_parse_u64_update_mode(&updater->modifier.size, &updater->modifier.modify_size, seperators[1] + 1, seperators[2])) {
                        fprintf(stderr, "CLI parse partition: Failed to parse size modifier in arg: %s\n", arg);
                        free(updater);
                        return NULL;
                    }
                } else {
                    updater->modifier.modify_size = CLI_PARTITION_MODIFY_DETAIL_PRESERVE;
                }
                if (*(seperators[2] + 1)) {
                    updater->modifier.masks = strtoul(seperators[2] + 1, NULL, 0);
                    updater->modifier.modify_masks = CLI_PARTITION_MODIFY_DETAIL_SET;
                } else {
                    updater->modifier.modify_masks = CLI_PARTITION_MODIFY_DETAIL_PRESERVE;
                }
                break;
            } 
            case CLI_PARTITION_MODIFY_PART_DELETE:
                break;
            case CLI_PARTITION_MODIFY_PART_CLONE: {
                size_t len_new_name = strlen(select_end + 1);
                if (len_new_name > MAX_PARTITION_NAME_LENGTH - 1) {
                    fprintf(stderr, "CLI parse partition update mode: New name to clone to is too long: %s\n", arg);
                    free(updater);
                    return NULL;
                }
                strncpy(updater->modifier.name, select_end + 1, len_new_name);
                updater->modifier.name[len_new_name] = '\0'; // No need, but write it anyway
                break;
            }
            case CLI_PARTITION_MODIFY_PART_PLACE: {
                const char *placer_start;
                switch(*(select_end + 1)) {
                    case '\0':
                        free(updater);
                        return NULL;
                    case '=': // :-1 (last), :5 (5th), :+3 (3rd)
                        placer_start = select_end + 2;
                        updater->modifier.modify_place = CLI_PARTITION_MODIFY_PLACE_ABSOLUTE;
                        break;
                    case '+': // -1 (minus 1), +3 (add 3)
                    case '-':
                        placer_start = select_end + 1;
                        updater->modifier.modify_place = CLI_PARTITION_MODIFY_PLACE_RELATIVE;
                        break;
                    default:  // 5 (5th)
                        placer_start = select_end + 1;
                        updater->modifier.modify_place = CLI_PARTITION_MODIFY_PLACE_ABSOLUTE;
                        break;
                }
                updater->modifier.place = strtol(placer_start, NULL, 0);
                if (updater->modifier.modify_place == CLI_PARTITION_MODIFY_PLACE_RELATIVE && !updater->modifier.place) {
                    updater->modifier.modify_place = CLI_PARTITION_MODIFY_PLACE_PRESERVE;
                }
                break;
            }
            default:
                fputs("CLI parse partition update mode: illegal part modification method\n", stderr);
                free(updater);
                return NULL;
        }
    } else {
        struct cli_partition_definer *definer = CLI_PARSE_PARTITION_YOLO_MODE(arg);
        if (!definer) {
            fprintf(stderr, "CLI parse partition update mode: failed to parse defining mode arg: %s\n", arg);
            free(updater);
            return NULL;
        }
        updater->definer = *definer;
    }
    return updater;
}

void cli_version() {
    fputs("ampart-ng (Amlogic eMMC partition tool) by 7Ji, development version, debug usage only\n", stderr);
}

size_t cli_human_readable_to_size_and_report(const char *const literal, const char *const name) {
    const size_t size = util_human_readable_to_size(literal);
    char suffix;
    const double size_d = util_size_to_human_readable(size, &suffix);
    fprintf(stderr, "CLI interface: Setting %s to %zu / 0x%lx (%lf%c)\n", name, size, size, size_d, suffix);
    return size;
}

static inline void cli_describe_options() {
    char suffix_gap_reserved, suffix_gap_partition, suffix_offset_reserved, suffix_offset_dtb;
    double gap_reserved = util_size_to_human_readable(cli_options.gap_reserved, &suffix_gap_reserved);
    double gap_partition = util_size_to_human_readable(cli_options.gap_partition, &suffix_gap_partition);
    double offset_reserved = util_size_to_human_readable(cli_options.offset_reserved, &suffix_offset_reserved);
    double offset_dtb = util_size_to_human_readable(cli_options.offset_dtb, &suffix_offset_dtb);
    fprintf(stderr, "CLI describe options: mode %s, operating on %s, content type %s, dry run: %s, reserved gap: %lu (%lf%c), generic gap: %lu (%lf%c), reserved offset: %lu (%lf%c), dtb offset: %lu (%lf%c)\n", cli_mode_strings[cli_options.mode], cli_options.target, cli_content_type_strings[cli_options.content], cli_options.dry_run ? "yes" : "no", cli_options.gap_reserved, gap_reserved, suffix_gap_reserved, cli_options.gap_partition, gap_partition, suffix_gap_partition, cli_options.offset_reserved, offset_reserved, suffix_offset_reserved, cli_options.offset_dtb, offset_dtb, suffix_offset_dtb);
}


int cli_early_stage(int argc, char *argv[]) {
    int fd = open(cli_options.target, O_RDONLY);
    if (fd < 0) {
        fputs("CLI early stage: Failed to open target\n", stderr);
        return 1;
    }
    size_t dtb_offset;
    switch (cli_options.content) {
        case CLI_CONTENT_TYPE_DTB:
            dtb_offset = 0;
            break;
        case CLI_CONTENT_TYPE_RESERVED:
            dtb_offset = cli_options.offset_dtb;
            break;
        case CLI_CONTENT_TYPE_DISK:
            dtb_offset = cli_options.offset_reserved + cli_options.offset_dtb;
            break;
        default:
            fputs("CLI early stage: Ilegal target content type (auto), this should not happen\n", stderr);
            close(fd);
            return 2;
    }
    fprintf(stderr, "CLI early stage: Seeking to %zu to read DTB and report\n", dtb_offset);
    if (lseek(fd, dtb_offset, SEEK_SET) < 0) {
        close(fd);
        return 3;
    }
    int r = dtb_read_partitions_and_report(fd, cli_options.size - dtb_offset);
    close(fd);
    if (r > 0) {
        return 4;
    } else if (r < 0 ) {
        return 5;
    }
    for (int i =0; i<argc; ++i) {
        printf("%d: %s\n", i, argv[i]);
    }
    return 0;
}

int cli_interface(const int argc, char *argv[]) {
    int c, option_index = 0;
    static struct option long_options[] = {
        {"version",         no_argument,        NULL,   'v'},
        {"help",            no_argument,        NULL,   'h'},
        {"mode",            required_argument,  NULL,   'm'},   // The mode: yolo, clone, safe, update. Default: yolo
        {"content",         required_argument,  NULL,   'c'},   // The type of input file: auto, dtb, reserved, disk. Default: auto
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
                cli_version();
                return 0;
            case 'h':   // help
                cli_version();
                fputs("\nampart does not provide command-line help message due to its complex syntax, please refer to the project Wiki for documentation:\nhttps://github.com/7Ji/ampart/wiki\n\n", stderr);
                return 0;
            case 'm': {   // mode:
                bool mode_valid = false;
                for (enum cli_modes mode = CLI_MODE_YOLO; mode <= CLI_MODE_SNAPSHOT; ++mode) {
                    if (!strcmp(cli_mode_strings[mode], optarg)) {
                        fprintf(stderr, "CLI interface: Mode is set to %s\n", optarg);
                        mode_valid = true;
                        cli_options.mode = mode;
                        break;
                    }
                }
                if (mode_valid) {
                    break;
                } else {
                    fprintf(stderr, "CLI interface: Invalid mode %s\n", optarg);
                    return 1;
                }
            }
            case 'c': {   // type:
                bool type_valid = false;
                for (enum cli_content_types content = CLI_CONTENT_TYPE_AUTO; content <= CLI_CONTENT_TYPE_DISK; ++content) {
                    if (!strcmp(cli_content_type_strings[content], optarg)) {
                        fprintf(stderr, "CLI interface: Content type is set to %s\n", optarg);
                        type_valid = true;
                        cli_options.content = content;
                        break;
                    }
                }
                if (type_valid) {
                    break;
                } else {
                    fprintf(stderr, "CLI interface: Invalid type %s\n", optarg);
                    return 2;
                }
            }    
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
    if (cli_options.dry_run) {
        cli_options.write = CLI_WRITE_NOTHING;
    }
    if (optind < argc) {
        cli_options.target = strdup(argv[optind++]);
        if (!cli_options.target) {
            fprintf(stderr, "CLI interface: Failed to duplicate target string '%s'\n", argv[optind-1]);
            return 4;
        }
        fprintf(stderr, "CLI interface: Operating on target file/block device '%s'\n", cli_options.target);
        bool find_disk = false;
        struct io_target_type *target_type = io_identify_target_type(cli_options.target);
        if (!target_type) {
            fprintf(stderr, "CLI interface: failed to identify the type of target '%s'\n", cli_options.target);
            return 5;
        }
        cli_options.size = target_type->size;
        io_describe_target_type(target_type, NULL);
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
                    return 6;
            }
            if (target_type->file == IO_TARGET_TYPE_FILE_BLOCKDEVICE && target_type->content != IO_TARGET_TYPE_CONTENT_DISK) {
                find_disk = true;
            }
        } else if (target_type->file == IO_TARGET_TYPE_FILE_BLOCKDEVICE && cli_options.content != CLI_CONTENT_TYPE_DISK) {
            find_disk = true;
        }
        free(target_type);
        if (!cli_options.strict_device && find_disk) {
            char *path_disk = io_find_disk(cli_options.target);
            if (!path_disk) {
                fprintf(stderr, "CLI interface: Failed to get the corresponding disk of %s, try to force the target type or enable strict-device mode to disable auto-identification\n", cli_options.target);
                return 7;
            }
            fprintf(stderr, "CLI interface: Operating on '%s' instead, content type is now disk\n", path_disk);
            free(cli_options.target);
            cli_options.target = path_disk;
            cli_options.content = CLI_CONTENT_TYPE_DISK;
        }
    } else {
        fputs("CLI interface: Too few arguments, target file/block device must be set as the first non-positional argument\n", stderr);
        return 8;
    }
    if (cli_options.content == CLI_CONTENT_TYPE_AUTO) {
        fputs("CLI interface: Content type not identified, give up, try setting it manually\n", stderr);
        return 9;
    }
    cli_describe_options();
    int r = cli_early_stage(argc-optind, argv+optind);
    if (r)  {
        return 10 + r;
    }
    return 0;
}