#define _GNU_SOURCE
#include "priv/log.h"
#include "priv/parg.h"
#define PARG_MAXLEN 64

static ampart_status parg_modifier_from_str(ampart_parg const parg, char const *const restrict str, size_t const len_str) {
    struct parg_modifier *modifier = &parg->modifier;


    parg->type = PARG_TYPE_MODIFIER;
    return AMPART_OK;
}

static ampart_status parg_definer_from_str(ampart_parg const parg, char const *const restrict str, size_t const len_str) {
    struct parg_definer *definer = &parg->definer;


    parg->type = PARG_TYPE_DEFINER;
    return AMPART_OK;
}

// static char const *str_from_condition(uint8_t condition) {
//     if (condition) {
//         return "true";
//     } else {
//         return "false";
//     }
// }

static ampart_status parg_show_definer(struct parg_definer const *const definer) {
    log_info("definer: name: %s (set: %hhu), offset: 0x%"PRI_TABLE_SIZE_X" (set: %hhu, relative: %hhu), size: 0x%"PRI_TABLE_SIZE_X" (set: %hhu), masks: 0x%"PRI_TABLE_MASKS_X" (set: %hhu)",
        definer->name, (definer->defined & PARG_DEFINE_NAME) > 0,
        definer->offset, (definer->defined & PARG_DEFINE_OFFSET) > 0, (definer->defined & PARG_DEFINE_OFFSET_RELATIVE) > 0,
        definer->size, (definer->defined & PARG_DEFINE_SIZE) > 0,
        definer->masks, (definer->defined & PARG_DEFINE_MASKS) > 0
    );
    return AMPART_OK;
}

static ampart_status check_report_name(char const *const restrict scope, char const *const restrict name, parg_len_t const len_name) {
    if (name[0] && len_name) {
        log_info("%s: name: %s", scope, name);
        return AMPART_OK;
    } else {
        log_error("%s: invalid name or empty name", scope);
        return AMPART_BAD;
    }
}

static ampart_status parg_show_name_selector(struct parg_name_selector const *const name_selector) {
    return check_report_name("modifier/name-selector", name_selector->name, name_selector->len_name);
};

static ampart_status parg_show_position_selector(struct parg_position_selector const *const position_selector) {
    log_info("modifier/position-selector: position %"PRI_PARG_POSITION, position_selector->position);
    return AMPART_OK;
}

static ampart_status parg_show_selector(struct parg_selector const *const selector) {
    switch (selector->type) {
    case PARG_SELECT_BY_NAME:
        return parg_show_name_selector(&selector->by_name);
    case PARG_SELECT_BY_POSITION:
        return parg_show_position_selector(&selector->by_position);
    default:
        log_error("modifier/selector: illegal selector type %"PRI_PARG_TYPE" instead of either %"PRI_PARG_TYPE" (by-name) or %"PRI_PARG_TYPE" (by-position)", selector->type, PARG_SELECT_BY_NAME, PARG_SELECT_BY_POSITION);
        return AMPART_BAD;
    }
    return AMPART_OK;
}

static ampart_status parg_show_cloner(struct parg_cloner const *const cloner) {
    return check_report_name("modifier/cloner", cloner->name, cloner->len_name);
}

static ampart_status parg_show_placer(struct parg_placer const *const restrict placer) {
    char const *restrict method;
    switch (placer->method) {
    case PARG_PLACE_ABSOLUTE:
        method = "absolute";
        break;
    case PARG_PLACE_RELATIVE:
        method = "relative";
        break;
    default:
        log_error("modifier/placer: %"PRI_PARG_POSITION" with invalid placing method %"PRI_PARG_TYPE" instead of either %"PRI_PARG_TYPE" (absolute) or %"PRI_PARG_TYPE" (relative)", placer->position, placer->method, PARG_PLACE_ABSOLUTE, PARG_PLACE_RELATIVE);
        return AMPART_BAD;
    }
    log_info("modifier/placer: %"PRI_PARG_POSITION" (%s)", placer->position, method);
    return AMPART_OK;
}

static ampart_status parg_show_modifier(struct parg_modifier const *const modifier) {
    ampart_status r = parg_show_selector(&modifier->selector);
    switch (modifier->type) {
    case PARG_MODIFY_DELETE:
        log_info("modifier/deleter");
        break;
    case PARG_MODIFY_CLONE:
        r = parg_show_cloner(&modifier->cloner);
        break;
    case PARG_MODIFY_PLACE:
        r = parg_show_placer(&modifier->placer);
        break;
    case PARG_MODIFY_ADJUST:
        break;
    default:
        log_error("modifier: illegal modifier type %"PRI_PARG_TYPE "instead of either %"PRI_PARG_TYPE" (deletor), %"PRI_PARG_TYPE" (cloner), %"PRI_PARG_TYPE" (placer) or %"PRI_PARG_TYPE" (adjustor)");
        r = AMPART_BAD;
    }
    return r;
}

static ampart_status parg_show_inner(struct ampart_parg const *const parg) {
    switch (parg->type) {
    case PARG_TYPE_DEFINER:
        return parg_show_definer(&parg->definer);
    case PARG_TYPE_MODIFIER:
        return parg_show_modifier(&parg->modifier);
    default:
        log_error("illegal parg type %"PRI_PARG_TYPE" instead of either %"PRI_PARG_TYPE" (definer) or %"PRI_PARG_TYPE" (modifier)", parg->type, PARG_TYPE_DEFINER, PARG_TYPE_MODIFIER);
        return AMPART_BAD;
    }
}

ampart_status parg_show(ampart_parg const parg)  {
    parg_show_inner(parg);
}

ampart_status parg_from_str_inner(ampart_parg const parg, char const *const restrict str, size_t const len_str) {
    switch (str[0]) {
    case '^':
        switch (str[1]) {
        case '-':
            switch (str[2]) {
            case '0'...'9':
                break;
            default:
                log_error("cannot parse parg '%s' which appears to be a modifier but does not have a seemingly valid negative absolute position selector", str);
                return AMPART_BAD;
            }
        case '0'...'9':
        case 'a'...'z':
        case 'A'...'Z':
            break;
        default:
            log_error("cannot parse parg '%s' which appears to be a modifier but does not have a seemingly valid selector", str);
            return AMPART_BAD;
        }
        return parg_modifier_from_str(parg, str + 1, len_str - 1);
    case 'a'...'z':
    case 'A'...'Z':
        return parg_definer_from_str(parg, str, len_str);
    default:
        log_error("cannot parse parg '%s' that does not begin with either ^ or alphabet character", str);
        return AMPART_BAD;
    }
    return AMPART_OK;
}

ampart_status ampart_parg_free(ampart_parg parg) {
    if (parg) {
        free(parg);
        return AMPART_OK;
    } else {
        log_error("cannot free a NULL parg");
        return AMPART_BAD;
    }
}

ampart_status ampart_parg_from_str(ampart_parg *const parg, char const *str, size_t len_str) {
    if (!len_str) {
        len_str = strnlen(str, PARG_MAXLEN);
        if (!len_str) {
            log_error("refuse to parse an empty parg at %p", str);
            return AMPART_BAD;
        }
    }
    ampart_parg result;
    if (!(result = malloc(sizeof *result))) {
        log_error_with_errno("failed to allocate memory for parg");
        return AMPART_BAD;
    }
    if (parg_from_str_inner(result, str, len_str)) {
        ampart_parg_free(result);
    }

    *parg = result;
    return AMPART_OK;
}
ampart_status ampart_parg_to_str(ampart_parg const parg, char *str, size_t const maxlen_str, size_t *const len_str) {
    return AMPART_OK;
}
