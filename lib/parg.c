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

static void log_illegal_selector_type(parg_type_t const type) {
    log_error("modifier/selector: illegal selector type %"PRI_PARG_TYPE" instead of either %"PRI_PARG_TYPE" (by-name) or %"PRI_PARG_TYPE" (by-position)", type, PARG_SELECT_BY_NAME, PARG_SELECT_BY_POSITION);
}

static ampart_status parg_show_selector(struct parg_selector const *const selector) {
    switch (selector->type) {
    case PARG_SELECT_BY_NAME:
        return parg_show_name_selector(&selector->by_name);
    case PARG_SELECT_BY_POSITION:
        return parg_show_position_selector(&selector->by_position);
    default:
        log_illegal_selector_type(selector->type);
        return AMPART_BAD;
    }
    return AMPART_OK;
}

static ampart_status parg_show_cloner(struct parg_cloner const *const cloner) {
    return check_report_name("modifier/cloner", cloner->name, cloner->len_name);
}

static void log_illegal_placer_method(parg_type_t const method) {
    log_error("modifier/placer: invalid placing method %"PRI_PARG_TYPE" instead of either %"PRI_PARG_TYPE" (absolute) or %"PRI_PARG_TYPE" (relative)", method, PARG_PLACE_ABSOLUTE, PARG_PLACE_RELATIVE);
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
        log_illegal_placer_method(placer->method);
        return AMPART_BAD;
    }
    log_info("modifier/placer: %"PRI_PARG_POSITION" (%s)", placer->position, method);
    return AMPART_OK;
}

static ampart_status parg_show_adjustor(struct parg_adjustor const *const restrict adjustor) {
    log_info("adjustor: name: %s (set: %hhu), offset: 0x%"PRI_PARG_SSIZE_X" (%"PRI_PARG_SSIZE_I", set: %hhu, relative: %hhu), size: 0x%"PRI_PARG_SSIZE_X" (%"PRI_PARG_SSIZE_I", set: %hhu, relative: %hhu), masks: 0x%"PRI_TABLE_MASKS_X" (set: %hhu)",
        adjustor->name, (adjustor->adjusted & PARG_ADJUST_NAME) > 0,
        adjustor->offset, adjustor->offset, (adjustor->adjusted & PARG_ADJUST_OFFSET) > 0, (adjustor->adjusted & PARG_ADJUST_OFFSET_RELATIVE) > 0,
        adjustor->size, adjustor->size, (adjustor->adjusted & PARG_ADJUST_SIZE) > 0, (adjustor->adjusted & PARG_ADJUST_SIZE_RELATIVE) > 0,
        adjustor->masks, (adjustor->adjusted & PARG_ADJUST_MASKS) > 0
    );
    return AMPART_OK;
}

static void log_illegal_modifier_type(parg_type_t const type) {
    log_error("modifier: illegal modifier type %"PRI_PARG_TYPE "instead of either %"PRI_PARG_TYPE" (deletor), %"PRI_PARG_TYPE" (cloner), %"PRI_PARG_TYPE" (placer) or %"PRI_PARG_TYPE" (adjustor)", type, PARG_MODIFY_DELETE, PARG_MODIFY_CLONE, PARG_MODIFY_PLACE, PARG_MODIFY_ADJUST);
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
        r = parg_show_adjustor(&modifier->adjustor);
        break;
    default:
        log_illegal_modifier_type(modifier->type);
        r = AMPART_BAD;
    }
    return r;
}

static void log_illegal_parg_type(parg_type_t const type) {
    log_error("illegal parg type %"PRI_PARG_TYPE" instead of either %"PRI_PARG_TYPE" (definer) or %"PRI_PARG_TYPE" (modifier)", type, PARG_TYPE_DEFINER, PARG_TYPE_MODIFIER);
}

static ampart_status parg_show_inner(struct ampart_parg const *const parg) {
    switch (parg->type) {
    case PARG_TYPE_DEFINER:
        return parg_show_definer(&parg->definer);
    case PARG_TYPE_MODIFIER:
        return parg_show_modifier(&parg->modifier);
    default:
        log_illegal_parg_type(parg->type);
        return AMPART_BAD;
    }
}

ampart_status ampart_parg_show(ampart_parg const parg) {
    return parg_show_inner(parg);
}

ampart_status parg_from_str_inner(ampart_parg const restrict parg, char const *const restrict str, size_t const len_str) {
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

ampart_status ampart_parg_from_str(ampart_parg *const restrict parg, char const *restrict str, size_t len_str) {
    if (!len_str) {
        len_str = strnlen(str, PARG_MAXLEN);
        if (!len_str) {
            log_error("refuse to parse an empty parg at %p", str);
            return AMPART_BAD;
        }
    }
    ampart_parg restrict result;
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

static void log_name_too_long(char const *const restrict type, char const *const name) {
    log_error("%s name '%s' too long and it would be truncated when printing", type, name);
}

ampart_status ampart_parg_to_str(ampart_parg const restrict parg, char *restrict str, size_t const maxlen_str, size_t *const len_str) {
    char buffer[AMPART_PARG_STR_MAXLEN + 1];
    size_t len_used = 0;
    int len_printf;
    switch (parg->type) {
    case PARG_TYPE_DEFINER:
        if (parg->definer.len_name > TABLE_NAME_MAXLEN) {
            log_name_too_long("definer", parg->definer.name);
            return AMPART_BAD;
        } else if (parg->definer.len_name) {
            if (len_used + parg->definer.len_name >= AMPART_PARG_STR_MAXLEN) {
                log_error("definer already too long before even name");
                return AMPART_BAD;
            }
            memcpy(buffer, parg->definer.name, parg->definer.len_name);
            len_used += parg->definer.len_name;
        }
        buffer[len_used++] = ':';
        if (parg->definer.defined & PARG_DEFINE_OFFSET) {
            if (len_used + 4 >= AMPART_PARG_STR_MAXLEN) {
                log_error("definer already too long before even offset");
                return AMPART_BAD;
            }
            if (parg->definer.defined & PARG_DEFINE_OFFSET_RELATIVE) {
                buffer[len_used++] = '+';
            }
            buffer[len_used++] = '0';
            buffer[len_used++] = 'x';
            len_printf = snprintf(buffer + len_used, AMPART_PARG_STR_MAXLEN + 1 - len_used, "%"PRI_TABLE_SIZE_X, parg->definer.offset);
            if (len_printf < 0) {
                log_error_with_errno("failed to printf definer offset");
                return AMPART_BAD;
            }
            len_used += len_printf;
        }
        if (len_used + 1 >= AMPART_PARG_STR_MAXLEN) {
            log_error("definer already too long till only offset");
            return AMPART_BAD;
        }
        buffer[len_used++] = ':';
        if (parg->definer.defined & PARG_DEFINE_SIZE) {
            if (len_used + 3 >= AMPART_PARG_STR_MAXLEN) {
                log_error("definer already too long before even size");
                return AMPART_BAD;
            }
            buffer[len_used++] = '0';
            buffer[len_used++] = 'x';
            len_printf = snprintf(buffer + len_used, AMPART_PARG_STR_MAXLEN + 1 - len_used, "%"PRI_TABLE_SIZE_X, parg->definer.size);
            if (len_printf < 0) {
                log_error_with_errno("failed to printf definer size");
                return AMPART_BAD;
            }
            len_used += len_printf;
        }
        if (len_used + 1 >= AMPART_PARG_STR_MAXLEN) {
            log_error("definer already too long till only size");
            return AMPART_BAD;
        }
        buffer[len_used++] = ':';
        if (parg->definer.defined & PARG_DEFINE_MASKS) {
            if (len_used + 3 >= AMPART_PARG_STR_MAXLEN) {
                log_error("definer already too long before even masks");
                return AMPART_BAD;
            }
            buffer[len_used++] = '0';
            buffer[len_used++] = 'x';
            len_printf = snprintf(buffer + len_used, AMPART_PARG_STR_MAXLEN + 1 - len_used, "%"PRI_TABLE_MASKS_X, parg->definer.masks);
            if (len_printf < 0) {
                log_error_with_errno("failed to printf definer size");
                return AMPART_BAD;
            }
            len_used += len_printf;
        }
        break;
    case PARG_TYPE_MODIFIER:
        buffer[len_used++] = '^';
        switch (parg->modifier.selector.type) {
        case PARG_SELECT_BY_NAME:
            if (parg->modifier.selector.by_name.len_name > TABLE_NAME_MAXLEN) {
                log_name_too_long("selector", parg->modifier.selector.by_name.name);
                return AMPART_BAD;
            } else if (parg->modifier.selector.by_name.len_name) {
                if (len_used + parg->modifier.selector.by_name.len_name >= AMPART_PARG_STR_MAXLEN) {
                    log_error("modifier already too long before even selector name");
                    return AMPART_BAD;
                }
                memcpy(buffer, parg->modifier.selector.by_name.name, parg->modifier.selector.by_name.len_name);
                len_used += parg->modifier.selector.by_name.len_name;
            }
            break;
        case PARG_SELECT_BY_POSITION:
            if (len_used + 2 >= AMPART_PARG_STR_MAXLEN) {
                log_error("modifier already too long before even selector position");
                return AMPART_BAD;
            }
            len_printf = snprintf(buffer + len_used, AMPART_PARG_STR_MAXLEN + 1 - len_used, "%"PRI_PARG_POSITION, parg->modifier.selector.by_position.position);
            if (len_printf < 0) {
                log_error_with_errno("failed to printf position-selector");
                return AMPART_BAD;
            }
            len_used += len_printf;
            break;
        default:
            log_illegal_selector_type(parg->modifier.selector.type);
            return AMPART_BAD;
        }
        if (len_used + 1 >= AMPART_PARG_STR_MAXLEN) {
            log_error("modifier already too long before even modifier operator");
            return AMPART_BAD;
        }
        switch (parg->modifier.type) {
        case PARG_MODIFY_DELETE:
            buffer[len_used++] = '?';
            break;
        case PARG_MODIFY_CLONE:
            buffer[len_used++] = '%';
            if (parg->modifier.cloner.len_name > TABLE_NAME_MAXLEN) {
                log_name_too_long("cloner", parg->modifier.cloner.name);
                return AMPART_BAD;
            } else if (parg->modifier.cloner.len_name) {
                if (len_used + parg->modifier.cloner.len_name >= AMPART_PARG_STR_MAXLEN) {
                    log_error("modifier already too long before even cloner name");
                    return AMPART_BAD;
                }
                memcpy(buffer, parg->modifier.cloner.name, parg->modifier.cloner.len_name);
                len_used += parg->modifier.cloner.len_name;
            } else {
                log_error("cloner has empty name");
                return AMPART_BAD;
            }
            break;
        case PARG_MODIFY_PLACE:
            buffer[len_used++] = '@';
            break;
        case PARG_MODIFY_ADJUST:
            buffer[len_used++] = ':';
            break;
        default:
            log_illegal_modifier_type(parg->modifier.type);
            return AMPART_BAD;
        }
        break;
    default:
        log_illegal_parg_type(parg->type);
        return AMPART_BAD;
    }
    if (len_used > AMPART_PARG_STR_MAXLEN) {
        log_error("parg cannot be terminated with NULL due to size limit");
        return AMPART_BAD;
    }
    buffer[len_used] = '\0';
    if (len_used > maxlen_str) {
        *len_str = maxlen_str;
    } else {
        *len_str = len_used;
    }
    memcpy(str, buffer, *len_str);
    buffer[*len_str] = '\0';
    log_info("converted parg '%s' to string '%s' (needed len %zu, max len %zu, actual len %zu)", buffer, str, len_used, maxlen_str, *len_str);
    return AMPART_OK;
}
