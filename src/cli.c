#include "cli_p.h"

static inline int cli_parse_u64(uint64_t *value, bool *relative, uint8_t requirement, const char *start, const char *end) {
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
        char *str = malloc(sizeof(char) * (len + 1));
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
    if (have_name && require_name & CLI_ARGUMENT_DISALLOW) {
        fprintf(stderr, "CLI parse partition: Argument contains name but it's not allowed: %s\n", arg);
        return NULL;
    }
    if (!have_name && require_name & CLI_ARGUMENT_REQUIRED) {
        fprintf(stderr, "CLI parse partition: Argument does not contains name but it must be set: %s\n", arg);
        return NULL;
    }
    bool have_offset = seperators[1] - seperators[0] > 1;
    if (have_offset && require_offset & CLI_ARGUMENT_DISALLOW) {
        fprintf(stderr, "CLI parse partition: Argument contains offset but it's not allowed: %s\n", arg);
        return NULL;
    }
    if (!have_offset && require_offset & CLI_ARGUMENT_REQUIRED) {
        fprintf(stderr, "CLI parse partition: Argument does not contain offset but it must be set: %s\n", arg);
        return NULL;
    }
    bool have_size = seperators[2] - seperators[1] > 1;
    if (have_size && require_size & CLI_ARGUMENT_DISALLOW) {
        fprintf(stderr, "CLI parse partition: Argument contains size but it's not allowed: %s\n", arg);
        return NULL;
    }
    if (!have_size && require_size & CLI_ARGUMENT_REQUIRED) {
        fprintf(stderr, "CLI parse partition: Argument does not contain size but it must be set: %s\n", arg);
        return NULL;
    }
    bool have_masks = *(seperators[2] + 1);
    if (have_masks && require_masks & CLI_ARGUMENT_DISALLOW) {
        fprintf(stderr, "CLI parse partition: Argument contains masks but it's not allowed: %s\n", arg);
        return NULL;
    }
    if (!have_masks && require_masks & CLI_ARGUMENT_REQUIRED) {
        fprintf(stderr, "CLI parse partition: Argument does not contains mask but it must be set: %s\n", arg);
        return NULL;
    }
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

struct cli_partition_definer *cli_parse_partition_yolo_mode(const char *arg) {
    return cli_parse_partition_raw(
        arg,
        CLI_ARGUMENT_REQUIRED,
        CLI_ARGUMENT_ALLOW_ABSOLUTE | CLI_ARGUMENT_ALLOW_RELATIVE,
        CLI_ARGUMENT_ALLOW_ABSOLUTE,
        CLI_ARGUMENT_ANY,
        4
    );
}

struct cli_partition_definer *cli_parse_partition_clone_mode(const char *arg) {
    return cli_parse_partition_raw(
        arg,
        CLI_ARGUMENT_REQUIRED,
        CLI_ARGUMENT_REQUIRED | CLI_ARGUMENT_ALLOW_ABSOLUTE,
        CLI_ARGUMENT_REQUIRED | CLI_ARGUMENT_ALLOW_ABSOLUTE,
        CLI_ARGUMENT_REQUIRED,
        0
    );
}

struct cli_partition_definer *cli_parse_partition_safe_mode(const char *arg) {
    return cli_parse_partition_raw(
        arg,
        CLI_ARGUMENT_REQUIRED,
        CLI_ARGUMENT_DISALLOW,
        CLI_ARGUMENT_REQUIRED | CLI_ARGUMENT_ALLOW_ABSOLUTE,
        CLI_ARGUMENT_REQUIRED,
        4
    );
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
        struct cli_partition_definer *definer = cli_parse_partition_yolo_mode(arg);
        if (!definer) {
            fprintf(stderr, "CLI parse partition update mode: failed to parse defining mode arg: %s\n", arg);
            free(updater);
            return NULL;
        }
        updater->definer = *definer;
    }
    return updater;
}

void cli_initialize() {
    cli_options.gap_partition = TABLE_PARTITION_GAP_GENERIC;
    cli_options.gap_reserved = TABLE_PARTITION_GAP_RESERVED;
    cli_options.offset_reserved = TABLE_PARTITION_GAP_RESERVED + TABLE_PARTITION_BOOTLOADER_SIZE;
    cli_options.offset_dtb = DTB_PARTITION_OFFSET;
    cli_options.write = CLI_WRITE_DTB | CLI_WRITE_TABLE | CLI_WRITE_MIGRATES;
}

int cli_interface(const int argc, char * const argv[]) {
    cli_initialize();
    int c, option_index = 0;
    static struct option long_options[] = {
        {"version",         no_argument,        NULL,   'v'},
        {"help",            no_argument,        NULL,   'h'},
        {"mode",            required_argument,  NULL,   'm'},   // The mode: yolo, clone, safe, update. Default: yolo
        {"type",            required_argument,  NULL,   't'},   // The type of input file: auto, dtb, reserved, disk. Default: auto
        {"dry-run",         no_argument,        NULL,   'd'},
        {"offset-reserved", required_argument,  NULL,   'R'},
        {"offset-dtb",      required_argument,  NULL,   'D'},
        {"gap-partition",   required_argument,  NULL,   'p'},
        {"gap-reserved",    required_argument,  NULL,   'r'},
    };
    while ((c = getopt_long(argc, argv, "vhm:t:dR:D:p:r:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'v':   // version
                puts("version");
                return 0;
            case 'h':   // help
                puts("help");
                return 0;
            case 'm':   // mode:
                for (enum cli_modes mode = CLI_MODE_YOLO; mode < CLI_MODE_SNAPSHOT; ++mode) {
                    if (!strcmp(cli_mode_strings[mode], optarg)) {
                        cli_options.mode = mode;
                        break;
                    }
                }
                break;
            case 't':   // type:
                for (enum cli_types type = CLI_TYPE_AUTO; type < CLI_TYPE_DISK; ++type) {
                    if (!strcmp(cli_type_strings[type], optarg)) {
                        cli_options.type = type;
                        break;
                    }
                }
                break;
                break;
            case 'd':   // dry-run
                puts("Dry-run");
                cli_options.write = CLI_WRITE_NOTHING;
                break;
            case 'R':   // offset-reserved:
                cli_options.offset_reserved = util_human_readable_to_size(optarg);
                break;
            case 'D':   // offset-dtb:
                cli_options.offset_dtb = util_human_readable_to_size(optarg);
                break;
            case 'p':   // gap-partition:
                cli_options.gap_partition = util_human_readable_to_size(optarg);
                break;
            case 'r':   // gap-reserved:
                cli_options.gap_reserved = util_human_readable_to_size(optarg);
                break;
        }
    }
    if (optind < argc) {
        printf("non-option ARGV-elements: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
    }
    return 0;
}

// }
// struct cli_partition_updater *cli_parse_partition_yolo_update_mode(const char *arg) {
//     return cli_parse_partition_update_raw(arg, false);
// }

// struct cli_partition_updater *cli_parse_partition_safe_update_mode(const char *arg) {
//     return cli_parse_partition_update_raw(arg, true);
// }