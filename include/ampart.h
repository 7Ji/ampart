#pragma once
#include <stdio.h>

#define AMPART_OK       0
#define AMPART_BAD      1
typedef int ampart_status;
typedef struct ampart_handle ampart_handle;
typedef FILE *ampart_iop;
ampart_status ampart_init(ampart_handle *handle, ampart_iop snapshot_out, ampart_iop log_out);

#define AMPART_ACTION_NONE          0b00000000
#define AMPART_ACTION_READ          0b00000001
#define AMPART_ACTION_MOD           0b00000010
#define AMPART_ACTION_

ampart_status ampart_do();
