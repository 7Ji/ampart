#define _GNU_SOURCE
#include "priv/log.h"
#include "priv/plan.h"

// ampart_status ampart_plan_create(ampart_plan *plan, ampart_actions actions, ampart_parg *pargs, size_t len_pargs) {
//     return AMPART_OK;
// }

// ampart_status ampart_plan_create_raw(ampart_handle handle, ampart_plan *plan, ampart_actions actions, char const *restrict *pargs, size_t len_pargs) {
//     ampart_plan result;
//     if (!(result = malloc(sizeof *result))) {
//         log_error_with_errno(handle, "failed to allocate memory for plan, errno: %d, error: %s", errno, strerror(errno));
//         return AMPART_BAD;
//     }
//     *plan = result;
//     return AMPART_OK;
// }

// ampart_status ampart_plan_free(ampart_handle handle, ampart_plan plan) {
//     log_info(handle, "freeing handle");
//     free(plan);
//     return AMPART_OK;
// }

// ampart_status ampart_plan_work(ampart_handle handle, ampart_plan plan) {
//     return AMPART_OK;
// }
