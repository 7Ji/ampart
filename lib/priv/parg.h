#pragma once
#include <stdbool.h>
#include "ampart.h"
#include "table.h"

#define PARG_DEFINE_NAME            0b00000001
#define PARG_DEFINE_OFFSET          0b00000010
#define PARG_DEFINE_OFFSET_RELATIVE 0b00000100
#define PARG_DEFINE_SIZE            0b00001000
#define PARG_DEFINE_SIZE_RELATIVE   0b00010000
#define PARG_DEFINE_MASKS           0b00100000

typedef uint8_t parg_flags_t;

struct parg_definer {
    table_name_t name;
    table_size_t offset, size;
    table_masks_t masks;
    parg_flags_t defined;
};

struct parg_name_selector {
    table_name_t name;
    uint8_t len_name;
};

struct parg_position_selector {
    int8_t position;
};

#define PARG_SELECT_BY_NAME     0
#define PARG_SELECT_BY_POSITION 1

typedef uint8_t parg_type_t;

struct parg_selector {
    parg_type_t type;
    union {
        struct parg_name_selector by_name;
        struct parg_position_selector by_position;
    };
};

struct parg_cloner {
    table_name_t name;
    uint8_t len_name;
};

#define PARG_PLACE_RELATIVE  0b00000001
#define PARG_PLACE_ABSOLUTE  0b00000010

struct parg_placer {
    int8_t position;
    parg_flags_t method;
};

#define PARG_MODIFY_DELETE  0
#define PARG_MODIFY_CLONE   1
#define PARG_MODIFY_PLACE   2
#define PARG_MODIFY_ADJUST  3

struct parg_modifier {
    parg_type_t type;
    struct parg_selector selector;
    union {
        struct parg_cloner cloner;
        struct parg_placer placer;
        struct parg_definer adjustor;
        // no need for deletor
    };
};

struct ampart_parg {
    parg_type_t type;
    union {
        struct parg_definer definer;
        struct parg_modifier modifier;
    };
};

ampart_status ampart_parg_from_str_inner(ampart_parg const parg, char const *str, size_t const len_str);
