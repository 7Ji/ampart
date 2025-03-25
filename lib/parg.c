#define _GNU_SOURCE
#include "priv/log.h"
#include "priv/parg.h"
#define PARG_MAXLEN 64

ampart_status ampart_parg_from_str_inner(ampart_parg const parg, char const *str, size_t const len_str) {



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
    }
    ampart_parg result;
    if (!(result = malloc(sizeof *result))) {
        log_error_with_errno("Failed to allocate memory for parg");
        return AMPART_BAD;
    }
    if (ampart_parg_from_str_inner(result, str, len_str)) {
        ampart_parg_free(result);
    }

    *parg = result;
    return AMPART_OK;
}
ampart_status ampart_parg_to_str(ampart_parg const parg, char *str, size_t const maxlen_str, size_t *const len_str) {
    return AMPART_OK;
}
