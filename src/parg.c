/* Self */

#include "parg.h"

/* System */

#include <string.h>

/* Local */

#include "util.h"

/* Macro */

#define PARG_PARSE_RAW_ALLOWANCE_CHECK(name) \
    if (have_##name && require_##name & PARG_DISALLOW) { \
        fprintf(stderr, "PARG parse: Argument contains "#name" but it's not allowed: %s\n", arg); \
        return -2; \
    } \
    if (!have_##name && require_##name & PARG_REQUIRED) { \
        fprintf(stderr, "PARG parse: Argument does not contain "#name" but it must be set: %s\n", arg); \
        return -2; \
    }

#define PARG_PARSE_YOLO_MODE(definer, arg) \
    parg_parse_definer( \
        definer, \
        arg, \
        PARG_REQUIRED, \
        PARG_ALLOW_ABSOLUTE | PARG_ALLOW_RELATIVE, \
        PARG_ALLOW_ABSOLUTE, \
        PARG_ANY \
    )

#define PARG_PARSE_ECLONE_MODE(definer, arg) \
    parg_parse_definer( \
        definer, \
        arg, \
        PARG_REQUIRED, \
        PARG_REQUIRED | PARG_ALLOW_ABSOLUTE, \
        PARG_REQUIRED | PARG_ALLOW_ABSOLUTE, \
        PARG_REQUIRED \
    )

#define PARG_PARSE_DCLONE_MODE(definer, arg) \
    parg_parse_definer( \
        definer, \
        arg, \
        PARG_REQUIRED, \
        PARG_DISALLOW, \
        PARG_REQUIRED | PARG_ALLOW_ABSOLUTE, \
        PARG_REQUIRED \
    )

/* Function */

static inline 
int 
parg_parse_u64_definer(
    uint64_t * const    value,
    bool * const        relative,
    uint8_t const       requirement,
    char const *        start,
    char const * const  end
){
    if (*start == '+') {
        if (requirement & PARG_ALLOW_RELATIVE) {
            *relative = true;
            ++start;
        } else {
            fputs("PARG parse u64 definer: Relative value set when not allowed\n", stderr);
            return 1;
        }
    } else {
        if (requirement & PARG_ALLOW_ABSOLUTE) {
            *relative = false;
        } else {
            fputs("PARG parse u64 definer: Absolute value set when not allowed\n", stderr);
            return 2;
        }
    }
    if (end < start) {
        fputs("PARG parse u64 definer: End before start\n", stderr);
        return 3;
    }
    const size_t len = end - start;
    if (len) {
        char *const str = malloc((len + 1) * sizeof *str);
        if (!str) {
            fputs("PARG parse u64 definer: Failed to create temp buffer to parse number\n", stderr);
            return 4;
        }
        strncpy(str, start, len);
        str[len] = '\0';
        *value = util_human_readable_to_size(str);
        free(str);
    } else {
        if (requirement & PARG_REQUIRED) {
            fputs("PARG parse u64 definer: Required field not set\n", stderr);
            return 5;
        } else {
            *value = 0;
        }
    }
    return 0;
}

static inline 
int 
parg_parse_u64_adjustor(
    uint64_t *                          value,
    enum parg_modify_detail_method *    method,
    char const *                        start,
    char const *                        end
){
    switch (*start) {
        case '+':
            *method = PARG_MODIFY_DETAIL_ADD;
            ++start;
            break;
        case '-':
            *method = PARG_MODIFY_DETAIL_SUBSTRACT;
            ++start;
            break;
        default:
            *method = PARG_MODIFY_DETAIL_SET;
            break;
    }
    if (end < start) {
        fputs("PARG parse u64 adjustor: End before start\n", stderr);
        return 1;
    }
    const size_t len = end - start;
    if (len) {
        char *str = malloc((len + 1) * sizeof *str);
        if (!str) {
            fputs("PARG parse u64 adjustor: Failed to create temp buffer to parse number\n", stderr);
            return 2;
        }
        strncpy(str, start, len);
        str[len] = '\0';
        *value = util_human_readable_to_size(str);
        free(str);
    } else {
        fputs("PARG parse u64 adjustor: Offset/Size modifier must be set for relative mode\n", stderr);
        return 3;
    }
    return 0;
}

static inline
int
parg_parse_get_seperators(
    char const * * const    seperators,
    char const * const      arg
) {
    unsigned int seperator_id = 0;
    for (const char *c = arg; *c; ++c) {
        if (*c == ':') {
            seperators[seperator_id++] = c;
        }
        if (seperator_id == 3) {
            break;
        }
    }
    if (seperator_id < 3) {
        return 1;
    }
    return 0;
}

int
parg_parse_definer(
    struct parg_definer * const definer,
    char const * const          arg,
    uint8_t                     require_name,
    uint8_t                     require_offset,
    uint8_t                     require_size,
    uint8_t                     require_masks
){
    if (!definer || util_string_is_empty(arg)) {
        fputs("PARG parse definer: Illegal arguments\n", stderr);
        return -1;
    }
    const char *seperators[3];
    if (parg_parse_get_seperators(seperators, arg)) {
        fprintf(stderr, "PARG parse definer: Argument too short: %s\n", arg);
        return 1;
    }
    bool have_name = seperators[0] != arg;
    PARG_PARSE_RAW_ALLOWANCE_CHECK(name)
    bool have_offset = seperators[1] - seperators[0] > 1;
    PARG_PARSE_RAW_ALLOWANCE_CHECK(offset)
    bool have_size = seperators[2] - seperators[1] > 1;
    PARG_PARSE_RAW_ALLOWANCE_CHECK(size)
    bool have_masks = *(seperators[2] + 1);
    PARG_PARSE_RAW_ALLOWANCE_CHECK(masks)
    memset(definer, 0, sizeof *definer);
    if (have_name) {
        size_t len_name = seperators[0] - arg;
        if (len_name > MAX_PARTITION_NAME_LENGTH - 1) {
            fprintf(stderr, "PARG parse definer: Partition name too long in arg: %s\n", arg);
            return 2;
        }
        strncpy(definer->name, arg, len_name);
        definer->set_name = true;
    }
    if (have_offset) {
        if (parg_parse_u64_definer(&definer->offset, &definer->relative_offset, require_offset, seperators[0] + 1, seperators[1])) {
            fprintf(stderr, "PARG parse definer: Failed to parse offset in arg: %s\n", arg);
            return 3;
        }
        definer->set_offset = true;
    }
    if (have_size) {
        if (parg_parse_u64_definer(&definer->size, &definer->relative_size, require_size, seperators[1] + 1, seperators[2])) {
            fprintf(stderr, "PARG parse definer: Failed to parse size in arg: %s\n", arg);
            return 4;
        }
        definer->set_size = true;
    }
    if (have_masks) {
        definer->masks = strtoul(seperators[2] + 1, NULL, 0);
        definer->set_masks = true;
    }
    return 0;
}

static inline
int
parg_parse_modifier_get_select_method (
    struct parg_modifier * const    modifier,
    char const                      c
){
    switch(c) {
        case '\0':
        case ':':
        case '%':
        case '?':
        case '@':
            return 1;
        case '-':
        case '0'...'9':
            modifier->select = PARG_SELECT_RELATIVE;
            return 0;
        default:
            modifier->select = PARG_SELECT_NAME;
            return 0;
    }

}

static inline
char *
parg_parse_modifier_get_select_end (
    struct parg_modifier * const    modifier,
    char const * const              selecor_start
){
    for (char *c = (char *)selecor_start; !c; ++c) {
        switch(*c) {
            case '\0':
                fputs("PARG parse modifier: only selector is set, no action available\n", stderr);
                return NULL;
            case '%':
                if (!*(c + 1)) {
                    fputs("PARG parse modifier: no new name after clone operator is set\n", stderr);
                    return NULL;
                }
                modifier->modify_part = PARG_MODIFY_PART_CLONE;
                return c;
            case '?':
                modifier->modify_part = PARG_MODIFY_PART_DELETE;
                return c;
            case '@': // ^-1@:-7  ^bootloader@+7 ^bootloader@-1
                if (!*(c + 1)) {
                    fputs("PARG parse modifier: no placer after selector is set\n", stderr);
                    return NULL;
                }
                modifier->modify_part = PARG_MODIFY_PART_PLACE;
                return c;
            case ':':
                if (!*(c + 1)) {
                    fputs("PARG parse modifier: no modifier after selector is set\n", stderr);
                    return NULL;
                }
                modifier->modify_part = PARG_MODIFY_PART_ADJUST; // It's not needed, but anyway
                return c;
            default:
                break;
        }
    }
    return NULL;
}

static inline
int
parg_parse_modifier_get_selector(
    struct parg_modifier * const    modifier,
    char const * const              selector,
    size_t const                    len
){
    if (modifier->select == PARG_SELECT_NAME) {
        if (len > MAX_PARTITION_NAME_LENGTH - 1) {
            fputs("PARG parse modifier: Name selector too long\n", stderr);
            return 1;
        }
        strncpy(modifier->select_name, selector, len);
    } else {
        char *str_buffer = malloc((len + 1) * sizeof *str_buffer);
        if (!str_buffer) {
            fputs("PARG parse modifier: Failed to allocate memory to parse relative selector\n", stderr);
            return 2;
        }
        strncpy(str_buffer, selector, len);
        modifier->select_relative = strtol(str_buffer, NULL, 0);
        free(str_buffer);
    }
    return 0;
}

static inline
int
parg_parse_modifier_get_adjustor(
    struct parg_modifier * const    modifier,
    char const * const              adjustor
){
    const char *seperators[3];
    if (parg_parse_get_seperators(seperators, adjustor)) {
        fprintf(stderr, "PARG parse modifier: Adjustor too short: %s\n", adjustor);
        return 1;
    }
    if (seperators[0] != adjustor) {
        size_t len_name = seperators[0] - adjustor;
        if (len_name > MAX_PARTITION_NAME_LENGTH - 1) {
            return 1;
        }
        strncpy(modifier->name, adjustor, len_name);
        modifier->modify_name = PARG_MODIFY_DETAIL_SET;
    } else {
        modifier->modify_name = PARG_MODIFY_DETAIL_PRESERVE;
    }
    if (seperators[1] - seperators[0] > 1) {
        if (parg_parse_u64_adjustor(&modifier->offset, &modifier->modify_offset, seperators[0] + 1, seperators[1])) {
            fprintf(stderr, "PARG parse modifier: Failed to parse offset modifier in adjustor: %s\n", adjustor);
            return 1;
        }
    } else {
        modifier->modify_offset = PARG_MODIFY_DETAIL_PRESERVE;
    }
    if (seperators[2] - seperators[1] > 1) {
        if (parg_parse_u64_adjustor(&modifier->size, &modifier->modify_size, seperators[1] + 1, seperators[2])) {
            fprintf(stderr, "PARG parse partition: Failed to parse size modifier in adjustor: %s\n", adjustor);
            free(modifier);
            return 1;
        }
    } else {
        modifier->modify_size = PARG_MODIFY_DETAIL_PRESERVE;
    }
    if (*(seperators[2] + 1)) {
        modifier->masks = strtoul(seperators[2] + 1, NULL, 0);
        modifier->modify_masks = PARG_MODIFY_DETAIL_SET;
    } else {
        modifier->modify_masks = PARG_MODIFY_DETAIL_PRESERVE;
    }
    return 0;
}

static inline
int
parg_parse_modifier_get_cloner(
    struct parg_modifier * const    modifier,
    char const * const              cloner
){
    size_t len_new_name = strlen(cloner);
    if (len_new_name > MAX_PARTITION_NAME_LENGTH - 1) {
        fprintf(stderr, "PARG parse modifier: New name to clone to is too long: %s\n", cloner);
        return 1;
    }
    strncpy(modifier->name, cloner, len_new_name);
    modifier->name[len_new_name] = '\0'; // No need, but write it anyway
    return 0;
}

static inline
int
parg_parse_modifier_get_placer(
    struct parg_modifier * const    modifier,
    char const *                    placer
){
    switch(*placer) {
        case '\0':
            return 1;
        case '=': // :-1 (last), :5 (5th), :+3 (3rd)
            ++placer;
            modifier->modify_place = PARG_MODIFY_PLACE_ABSOLUTE;
            break;
        case '+': // -1 (minus 1), +3 (add 3)
        case '-':
            modifier->modify_place = PARG_MODIFY_PLACE_RELATIVE;
            break;
        default:  // 5 (5th)
            modifier->modify_place = PARG_MODIFY_PLACE_ABSOLUTE;
            break;
    }
    modifier->place = strtol(placer, NULL, 0);
    if (modifier->modify_place == PARG_MODIFY_PLACE_RELATIVE && !modifier->place) {
        modifier->modify_place = PARG_MODIFY_PLACE_PRESERVE;
    }
    return 0;
}

static inline
int
parg_parse_modifier_dispatcher(
    struct parg_modifier * const    modifier,
    char const * const              arg
) {
    switch (modifier->modify_part) {
        case PARG_MODIFY_PART_ADJUST:
            if (parg_parse_modifier_get_adjustor(modifier, arg)) {
                fprintf(stderr, "PARG parse modifier: adjustor invalid: %s\n", arg);
                return 1;
            }
            break;
        case PARG_MODIFY_PART_DELETE:
            break;
        case PARG_MODIFY_PART_CLONE:
            if (parg_parse_modifier_get_cloner(modifier, arg)) {
                fprintf(stderr, "PARG parse modifier: cloner invalid: %s\n", arg);
                return 1;
            }
            break;
        case PARG_MODIFY_PART_PLACE:
            if (parg_parse_modifier_get_placer(modifier, arg)) {
                fprintf(stderr, "PARG parse modifier: placer invalid: %s\n", arg);
                return 1;
            }
            break;
        default:
            fputs("PARG parse modifier: illegal part modification method\n", stderr);
            return 1;
    }
    return 0;
}


int
parg_parse_modifier(
    struct parg_modifier * const    modifier,
    char const * const              arg
) {
    if (!modifier || util_string_is_empty(arg)) {
        fputs("PARG parse modifier: Illegal arguments\n", stderr);
        return -1;
    }
    if (parg_parse_modifier_get_select_method(modifier, arg[1])) {
        fprintf(stderr, "PARG parse modifier: no selector given in arg: %s\n", arg);
        return 1;
    }
    char const *const select_end = parg_parse_modifier_get_select_end(modifier, arg + 2);
    if (!select_end) {
        fprintf(stderr, "PARG parse modifier: selector/operator invalid/incomplete: %s\n", arg);
        return 2;
    }
    if (parg_parse_modifier_get_selector(modifier, arg + 1, select_end - arg - 1)) {
        fprintf(stderr, "PARG parse modifier: selector invalid: %s\n", arg);
        return 3;
    }
    if (parg_parse_modifier_dispatcher(modifier, select_end + 1)) {
        return 4;
    }
    return 0;
}

int
parg_parse_editor(
    struct parg_editor *    editor,
    char const * const      arg
){
    if (!editor || util_string_is_empty(arg)) {
        fputs("PARG parse editor: Illegal arguments\n", stderr);
        return -1;
    }
    if (arg[0] == '^') {
        if (parg_parse_modifier(&editor->modifier, arg)) {
            return 1;
        }
        editor->modify = true;
    } else {
        if (PARG_PARSE_YOLO_MODE(&editor->definer, arg)) {
            return 2;
        }
        editor->modify = false;
    }
    return 0;
}

void
parg_free_definer_helper_dynamic(
    struct parg_definer_helper_dynamic * * const    dhelper
){
    if (*dhelper) {
        free((*dhelper)->definers);
        free(*dhelper);
        *dhelper = NULL;
    }
}

int
parg_parse_dclone_mode(
    struct parg_definer_helper_static * const   dhelper,
    int const                                   argc,
    char const * const * const                  argv
){
    if (!dhelper || argc <= 0 || argc > MAX_PARTITIONS_COUNT || !argv) {
        fputs("PARG parse dclone mode: Illegal argument counts\n", stderr);
        return -1;
    }
    dhelper->count = argc;
    for (int i = 0; i < argc; ++i) {
        if (PARG_PARSE_DCLONE_MODE(dhelper->definers + i, argv[i])) {
            fprintf(stderr, "PARG parse dclone mode: Failed to parse argument: %s\n", argv[i]);
            return 1;
        }
    }
    return 0;
}

int
parg_parse_eclone_mode(
    struct parg_definer_helper_static * const   dhelper,
    int const                                   argc,
    char const * const * const                  argv
){
    if (!dhelper || argc <= 0 || argc > MAX_PARTITIONS_COUNT || !argv) {
        fputs("PARG parse eclone mode: Illegal argument counts\n", stderr);
        return -1;
    }
    dhelper->count = argc;
    for (int i = 0; i < argc; ++i) {
        if (PARG_PARSE_ECLONE_MODE(dhelper->definers + i, argv[i])) {
            fprintf(stderr, "PARG parse eclone mode: Failed to parse argument: %s\n", argv[i]);
            return 1;
        }
    }
    return 0;
}

int
parg_parse_dedit_mode(
    struct parg_editor_helper * const   ehelper,
    int const                           argc,
    char const * const * const          argv
){
    if (!ehelper ||  argc <= 0 || argc > MAX_PARTITIONS_COUNT || !argv) {
        fputs("PARG parse dclone mode: Illegal arguments\n", stderr);
        return -1;
    }
    
    // dhelper->definers = malloc(argc * sizeof *dhelper->definers);
    // if (!dhelper->definers) {
    //     fputs("PARG parse dclone mode: Failed to allocate memroy for definers\n", stderr);
    //     return NULL;
    // }
    // dhelper->count = argc;
    // struct parg_definer *definer;
    // for (int i = 0; i < argc; ++i) {
    //     definer = PARG_PARSE_DCLONE_MODE(argv[i]);
    //     if (!definer) {
    //         fprintf(stderr, "PARG parse dclone mode: Failed to parse argument: %s\n", argv[i]);
    //         free(dhelper->definers);
    //         free(dhelper);
    //         return NULL;
    //     }
    //     dhelper->definers[i] = *definer;
    //     free(definer);
    // }
    return 0;
}

/* parg.c: Processing partition arguments */