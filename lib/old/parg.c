/* Self */

#include "parg.h"

/* System */

#include <string.h>

/* Local */

#include "common.h"
#include "ept.h"
#include "util.h"

/* Macro */

#define PARG_PARSE_RAW_ALLOWANCE_CHECK(name) \
    if (have_##name && require_##name & PARG_DISALLOW) { \
        prln_error("argument contains "#name" but it's not allowed: %s", arg); \
        return -2; \
    } \
    if (!have_##name && require_##name & PARG_REQUIRED) { \
        prln_error("argument does not contain "#name" but it must be set: %s", arg); \
        return -2; \
    }

#define PARG_PARSE_DEFINER_ECREATE_MODE(definer, arg) \
    parg_parse_definer( \
        definer, \
        arg, \
        PARG_REQUIRED, \
        PARG_ALLOW_ABSOLUTE | PARG_ALLOW_RELATIVE, \
        PARG_ALLOW_ABSOLUTE, \
        PARG_ANY \
    )

#define PARG_PARSE_DEFINER_ECLONE_MODE(definer, arg) \
    parg_parse_definer( \
        definer, \
        arg, \
        PARG_REQUIRED, \
        PARG_REQUIRED | PARG_ALLOW_ABSOLUTE, \
        PARG_REQUIRED | PARG_ALLOW_ABSOLUTE, \
        PARG_REQUIRED \
    )

#define PARG_PARSE_DEFINER_DCLONE_MODE(definer, arg) \
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
            prln_error("relative value set when not allowed");
            return 1;
        }
    } else {
        if (requirement & PARG_ALLOW_ABSOLUTE) {
            *relative = false;
        } else {
            prln_error("absolute value set when not allowed");
            return 2;
        }
    }
    if (end < start) {
        prln_error("end before start");
        return 3;
    }
    const size_t len = end - start;
    if (len) {
        char *const str = malloc((len + 1) * sizeof *str);
        if (!str) {
            prln_error("failed to create temp buffer to parse number");
            return 4;
        }
        strncpy(str, start, len);
        str[len] = '\0';
        *value = util_human_readable_to_size(str);
        free(str);
    } else {
        if (requirement & PARG_REQUIRED) {
            prln_error("required field not set");
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
        prln_error("end before start");
        return 1;
    }
    const size_t len = end - start;
    if (len) {
        char *str = malloc((len + 1) * sizeof *str);
        if (!str) {
            prln_error("failed to create temp buffer to parse number");
            return 2;
        }
        strncpy(str, start, len);
        str[len] = '\0';
        *value = util_human_readable_to_size(str);
        free(str);
    } else {
        prln_error("offset/size modifier must be set for relative mode");
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
        prln_error("illegal arguments");
        return -1;
    }
    const char *seperators[3];
    if (parg_parse_get_seperators(seperators, arg)) {
        prln_error("argument too short: %s", arg);
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
            prln_error("partition name too long in arg: %s", arg);
            return 2;
        }
        strncpy(definer->name, arg, len_name);
        if (ept_valid_partition_name(definer->name)) {
            prln_error("partition name %s is illegal", definer->name);
            memset(definer->name, 0, MAX_PARTITION_NAME_LENGTH);
            return 3;
        }
        definer->set_name = true;
    }
    if (have_offset) {
        if (parg_parse_u64_definer(&definer->offset, &definer->relative_offset, require_offset, seperators[0] + 1, seperators[1])) {
            prln_error("failed to parse offset in arg: %s", arg);
            return 4;
        }
        definer->set_offset = true;
    }
    if (have_size) {
        if (parg_parse_u64_definer(&definer->size, &definer->relative_size, require_size, seperators[1] + 1, seperators[2])) {
            prln_error("failed to parse size in arg: %s", arg);
            return 5;
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
    for (char *c = (char *)selecor_start; c; ++c) {
        switch(*c) {
            case '\0':
                prln_error("only selector is set, no action available");
                return NULL;
            case '%':
                if (!*(c + 1)) {
                    prln_error("no new name after clone operator is set");
                    return NULL;
                }
                modifier->modify_part = PARG_MODIFY_PART_CLONE;
                return c;
            case '?':
                modifier->modify_part = PARG_MODIFY_PART_DELETE;
                return c;
            case '@': // ^-1@:-7  ^bootloader@+7 ^bootloader@-1
                if (!*(c + 1)) {
                    prln_error("no placer after selector is set");
                    return NULL;
                }
                modifier->modify_part = PARG_MODIFY_PART_PLACE;
                return c;
            case ':':
                if (!*(c + 1)) {
                    prln_error("no modifier after selector is set");
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
            prln_error("name selector too long");
            return 1;
        }
        strncpy(modifier->select_name, selector, len);
        if (ept_valid_partition_name(modifier->select_name)) {
            prln_warn("name selector %s is illegal as a partition name, but I'll let you use that to select", modifier->select_name);
        }
    } else {
        char *str_buffer = malloc((len + 1) * sizeof *str_buffer);
        if (!str_buffer) {
            prln_error_with_errno("failed to allocate memory to parse relative selector");
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
    char const * const              adjustor,
    bool const                      allow_adjustor_offset
){
    const char *seperators[3];
    if (parg_parse_get_seperators(seperators, adjustor)) {
        prln_error("adjustor too short: %s", adjustor);
        return 1;
    }
    if (seperators[0] != adjustor) {
        size_t len_name = seperators[0] - adjustor;
        if (len_name > MAX_PARTITION_NAME_LENGTH - 1) {
            return 2;
        }
        strncpy(modifier->name, adjustor, len_name);
        if (ept_valid_partition_name(modifier->name)) {
            prln_error("adjustor name %s illegal", modifier->name);
            memset(modifier->name, 0, MAX_PARTITION_NAME_LENGTH);
            return 3;
        }
        modifier->modify_name = PARG_MODIFY_DETAIL_SET;
    } else {
        modifier->modify_name = PARG_MODIFY_DETAIL_PRESERVE;
    }
    if (seperators[1] - seperators[0] > 1) {
        if (!allow_adjustor_offset) {
            prln_error("offset adjusted when not allowed: %s", adjustor);
            return 3;
        }
        if (parg_parse_u64_adjustor(&modifier->offset, &modifier->modify_offset, seperators[0] + 1, seperators[1])) {
            prln_error("failed to parse offset modifier in adjustor: %s", adjustor);
            return 4;
        }
    } else {
        modifier->modify_offset = PARG_MODIFY_DETAIL_PRESERVE;
    }
    if (seperators[2] - seperators[1] > 1) {
        if (parg_parse_u64_adjustor(&modifier->size, &modifier->modify_size, seperators[1] + 1, seperators[2])) {
            prln_error("failed to parse size modifier in adjustor: %s", adjustor);
            free(modifier);
            return 5;
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
        prln_error("new name to clone to is too long: %s", cloner);
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
    char const * const              arg,
    bool const                      allow_adjustor_offset
) {
    switch (modifier->modify_part) {
        case PARG_MODIFY_PART_ADJUST:
            if (parg_parse_modifier_get_adjustor(modifier, arg, allow_adjustor_offset)) {
                prln_error("adjustor invalid: %s", arg);
                return 1;
            }
            break;
        case PARG_MODIFY_PART_DELETE:
            break;
        case PARG_MODIFY_PART_CLONE:
            if (parg_parse_modifier_get_cloner(modifier, arg)) {
                prln_error("cloner invalid: %s", arg);
                return 1;
            }
            break;
        case PARG_MODIFY_PART_PLACE:
            if (parg_parse_modifier_get_placer(modifier, arg)) {
                prln_error("placer invalid: %s", arg);
                return 1;
            }
            break;
        default:
            prln_error("illegal part modification method");
            return 1;
    }
    return 0;
}


int
parg_parse_modifier(
    struct parg_modifier * const    modifier,
    char const * const              arg,
    bool const                      allow_adjustor_offset
) {
    if (!modifier || util_string_is_empty(arg)) {
        prln_error("illegal arguments");
        return -1;
    }
    if (parg_parse_modifier_get_select_method(modifier, arg[1])) {
        prln_error("mo selector given in arg: %s", arg);
        return 1;
    }
    char const *const select_end = parg_parse_modifier_get_select_end(modifier, arg + 2);
    if (!select_end) {
        prln_error("selector/operator invalid/incomplete: %s", arg);
        return 2;
    }
    if (parg_parse_modifier_get_selector(modifier, arg + 1, select_end - arg - 1)) {
        prln_error("selector invalid: %s", arg);
        return 4;
    }
    if (parg_parse_modifier_dispatcher(modifier, select_end + 1, allow_adjustor_offset)) {
        return 5;
    }
    return 0;
}

int
parg_parse_editor(
    struct parg_editor *    editor,
    char const * const      arg,
    bool const              eedit
){
    if (!editor || util_string_is_empty(arg)) {
        prln_error("illegal arguments");
        return -1;
    }
    if (arg[0] == '^') {
        if (parg_parse_modifier(&editor->modifier, arg, eedit)) {
            return 1;
        }
        editor->modify = true;
    } else {
        if (eedit) {
            if (PARG_PARSE_DEFINER_ECREATE_MODE(&editor->definer, arg)) {
                return 2;
            }
        } else { // dedit
            if (PARG_PARSE_DEFINER_DCLONE_MODE(&editor->definer, arg)) {
                return 3;
            }
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
parg_parse_dedit_mode(
    struct parg_editor_helper * const   ehelper,
    int const                           argc,
    char const * const * const          argv
){
    if (!ehelper ||  argc <= 0 || !argv) {
        prln_error("illegal arguments");
        return -1;
    }
    if (!(ehelper->editors = malloc(argc * sizeof *ehelper->editors))) {
        prln_error("failed to allocate memory for editors");
        return 1;
    }
    ehelper->count = argc;
    for (int i = 0; i < argc; ++i) {
        if (parg_parse_editor(ehelper->editors + i, argv[i], false)) {
            free(ehelper->editors);
            return 2;
        }
    }
    return 0;
}

int
parg_parse_eedit_mode(
    struct parg_editor_helper * const   ehelper,
    int const                           argc,
    char const * const * const          argv
){
    if (!ehelper ||  argc <= 0 || !argv) {
        prln_error("illegal arguments");
        return -1;
    }
    if (!(ehelper->editors = malloc(argc * sizeof *ehelper->editors))) {
        prln_error("failed to allocate memory for editors");
        return 1;
    }
    ehelper->count = argc;
    for (int i = 0; i < argc; ++i) {
        if (parg_parse_editor(ehelper->editors + i, argv[i], true)) {
            free(ehelper->editors);
            return 2;
        }
    }
    return 0;
}

int
parg_parse_dclone_mode(
    struct parg_definer_helper_static * const   dhelper,
    int const                                   argc,
    char const * const * const                  argv
){
    if (!dhelper || argc <= 0 || argc > MAX_PARTITIONS_COUNT || !argv) {
        prln_error("illegal argument counts");
        return -1;
    }
    dhelper->count = argc;
    for (int i = 0; i < argc; ++i) {
        if (PARG_PARSE_DEFINER_DCLONE_MODE(dhelper->definers + i, argv[i])) {
            prln_error("failed to parse argument: %s", argv[i]);
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
        prln_error("illegal argument counts");
        return -1;
    }
    dhelper->count = argc;
    for (int i = 0; i < argc; ++i) {
        if (PARG_PARSE_DEFINER_ECLONE_MODE(dhelper->definers + i, argv[i])) {
            prln_error("failed to parse argument: %s", argv[i]);
            return 1;
        }
    }
    return 0;
}

int
parg_parse_ecreate_mode(
    struct parg_definer_helper_static * const   dhelper,
    int const                                   argc,
    char const * const * const                  argv
){
    if (!dhelper ||  argc <= 0 || argc > MAX_PARTITIONS_COUNT || !argv) {
        prln_error("illegal arguments");
        return -1;
    }
    dhelper->count = argc;
    for (int i = 0; i < argc; ++i) {
        if (PARG_PARSE_DEFINER_ECREATE_MODE(dhelper->definers + i, argv[i])) {
            prln_error("failed to parse argument: %s", argv[i]);
            return 1;
        }
    }
    return 0;
}

void
parg_report_failed_select(
    struct parg_modifier const * const modifier
){
    switch (modifier->select) {
        case PARG_SELECT_NAME:
            prln_error("no partition selected by Name selector %s", modifier->select_name);
            break;
        case PARG_SELECT_RELATIVE:
            prln_error("no partition selected by Relative selector %d", modifier->select_relative);
            break;
    }
}

int
parg_adjustor_adjust_u64(
    uint64_t * const    target,
    enum parg_modify_detail_method const method,
    uint64_t const value
){
    switch(method) {
        case PARG_MODIFY_DETAIL_ADD:
            *target += value;
            break;
        case PARG_MODIFY_DETAIL_SUBSTRACT:
            if (*target < value) {
                prln_error("target value 0x%lx smaller than substract source 0x%lx, refuse to continue", *target, value);
                return 1;
            }
            *target -= value;
            break;
        case PARG_MODIFY_DETAIL_SET:
            *target = value;
            return 0;
        default:
            return 0;
    }
    return 0;
}

int
parg_get_place_target(
    struct parg_modifier const * const  modifier,
    unsigned int const                  current,
    unsigned int const                  count
){
    switch (modifier->modify_place) {
        case PARG_MODIFY_PLACE_ABSOLUTE:
            if (modifier->place >= 0) {
                return modifier->place;
            } else {
                return count + modifier->place;
            }
        case PARG_MODIFY_PLACE_RELATIVE:
            return current + modifier->place;
        default:
            prln_error("illegal place method, return -1 as failure");
            return -1;
    }
}

/* parg.c: Processing partition arguments */