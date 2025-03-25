#pragma once
#include <stdio.h>

#define AMPART_LIB_MAJOR 0
#define AMPART_LIB_MINOR 0
#define AMPART_LIB_PATCH 1

#define AMPART_OK       0
#define AMPART_BAD      1
typedef int ampart_status;
typedef struct ampart_handle *ampart_handle;
typedef FILE *ampart_iop;
ampart_status ampart_handle_create(ampart_handle *handle, ampart_iop snapshot_out, ampart_iop log_out);
ampart_status ampart_handle_free(ampart_handle handle);

typedef unsigned char ampart_parg_type;
#define AMPART_PARG_CREATE  1

typedef struct ampart_parg {
    unsigned char ampart_parg_type;
} ampart_parg;

ampart_status ampart_parg_parse(ampart_handle, ampart_parg *parg, char const *str);

#define AMPART_ACTION_NONE          0b00000000
#define AMPART_ACTION_READ          0b00000001
#define AMPART_ACTION_MOD           0b00000010
#define AMPART_ACTION_

typedef unsigned char ampart_actions;

typedef struct ampart_plan *ampart_plan;
ampart_status ampart_plan_create(ampart_handle handle, ampart_plan *plan, ampart_actions actions, ampart_parg *pargs, size_t len_pargs);
ampart_status ampart_plan_create_raw(ampart_handle handle, ampart_plan *plan, ampart_actions actions, char const *restrict *pargs, size_t len_pargs);
ampart_status ampart_plan_free(ampart_handle handle, ampart_plan plan);
ampart_status ampart_plan_work(ampart_handle handle, ampart_plan plan);

ampart_status ampart_oneshot(ampart_iop snapshot_out, ampart_iop log_out, ampart_actions actions, char const *restrict *parg, size_t len_pargs);
