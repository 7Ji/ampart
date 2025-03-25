#define _GNU_SOURCE
#include "ampart.h"
// #include "priv/handle.h"

// #undef log_error
// #define log_error(format, arg...) \
//     flog_with_level(log_out, error, format, ##arg)

// ampart_status ampart_oneshot(ampart_iop snapshot_out, ampart_iop log_out, ampart_actions actions, char const *restrict *pargs, size_t len_pargs) {
//     ampart_handle handle = NULL;
//     if (ampart_handle_create(&handle, snapshot_out, log_out)) {
//         log_error("failed to initialize ampart handle");
//         return AMPART_BAD;
//     }
//     ampart_plan plan = NULL;
//     if (ampart_plan_create_raw(handle, &plan, actions, pargs, len_pargs)) {
//         log_error("failed to create plan");
//         return AMPART_BAD;
//     }
//     if (ampart_plan_work(handle, plan)) {
//         log_error("failed to work");
//         return AMPART_BAD;
//     }
//     ampart_plan_free(handle, plan);
//     ampart_handle_free(handle);
//     return AMPART_OK;
// }
