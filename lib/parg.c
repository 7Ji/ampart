#define _GNU_SOURCE
#include "priv/log.h"
#include "priv/parg.h"
#define PARG_MAXLEN 64

static ampart_status parg_modifier_from_str(ampart_parg const parg, char const *const restrict str, size_t const len_str) {

}

static ampart_status parg_definer_from_str(ampart_parg const parg, char const *const restrict str, size_t const len_str) {
    return AMPART_OK;
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
