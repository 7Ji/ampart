#define _GNU_SOURCE
#include "priv/handle.h"

ampart_status ampart_handle_create(ampart_handle *handle, ampart_iop snapshot_out, ampart_iop log_out) {
    ampart_handle result;
    if (!(result = malloc(sizeof *result))) {
        if (log_out) {
            flog_with_level(log_out, error, "failed to allocate memory for handle, errno: %d, error: %s", errno, strerror(errno));
            return AMPART_BAD;
        }
    }
    result->snapshot_out = snapshot_out;
    result->log_out = log_out;
    *handle = result;
    return AMPART_OK;
}

ampart_status ampart_handle_free(ampart_handle handle) {
    log_info(handle, "freeing handle");
    free(handle);
    return AMPART_OK;
}
