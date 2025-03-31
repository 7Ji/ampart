#pragma once
#include "ampart.h"
#include "table.h"

#define PARG_DEFINE_NAME            0b00000001
#define PARG_DEFINE_OFFSET          0b00000010
#define PARG_DEFINE_OFFSET_RELATIVE 0b00000100
#define PARG_DEFINE_SIZE            0b00001000
#define PARG_DEFINE_MASKS           0b00010000


typedef uint8_t parg_flags_t, parg_len_t;
#define PRI_PARG_LEN PRIu8

struct parg_definer {
    table_name_t name;
    table_size_t offset, size;
    table_masks_t masks;
    parg_len_t len_name;
    parg_flags_t defined;
};

struct parg_name_selector {
    table_name_t name;
    parg_len_t len_name;
};

typedef int8_t parg_position_t;
#define PRI_PARG_POSITION PRIi8
#define PARG_POSITION_MAX   INT8_MAX
#define PARG_POSITION_MIN   INT8_MIN

struct parg_position_selector {
    parg_position_t position;
};

#define PARG_SELECT_BY_NONE     0
#define PARG_SELECT_BY_NAME     1
#define PARG_SELECT_BY_POSITION 2

typedef uint8_t parg_type_t;
#define PRI_PARG_TYPE PRIu8

struct parg_selector {
    parg_type_t type;
    union {
        struct parg_name_selector by_name;
        struct parg_position_selector by_position;
    };
};

struct parg_cloner {
    table_name_t name;
    parg_len_t len_name;
};

#define PARG_PLACE_NONE     0
#define PARG_PLACE_RELATIVE 1
#define PARG_PLACE_ABSOLUTE 2

struct parg_placer {
    parg_position_t position;
    parg_flags_t method;
};

#define PARG_ADJUST_NAME             0b00000001
#define PARG_ADJUST_OFFSET_SET       0b00000010
#define PARG_ADJUST_OFFSET_ADD       0b00000100
#define PARG_ADJUST_OFFSET_SUBSTRACT 0b00001000
#define PARG_ADJUST_OFFSET           0b00001110
#define PARG_ADJUST_SIZE_SET         0b00010000
#define PARG_ADJUST_SIZE_ADD         0b00100000
#define PARG_ADJUST_SIZE_SUBSTRACT   0b01000000
#define PARG_ADJUST_SIZE             0b01110000
#define PARG_ADJUST_MASKS            0b10000000

struct parg_adjustor {
    table_name_t name;
    table_size_t offset, size;
    table_masks_t masks;
    parg_len_t len_name;
    parg_flags_t adjusted;
};

#define PARG_MODIFY_NONE    0
#define PARG_MODIFY_DELETE  1
#define PARG_MODIFY_CLONE   2
#define PARG_MODIFY_PLACE   3
#define PARG_MODIFY_ADJUST  4

struct parg_modifier {
    parg_type_t type;
    struct parg_selector selector;
    union {
        struct parg_cloner cloner;
        struct parg_placer placer;
        struct parg_adjustor adjustor;
        // no need for deletor
    };
};

#define PARG_TYPE_DEFINER   0
#define PARG_TYPE_MODIFIER  1

struct ampart_parg {
    parg_type_t type;
    union {
        struct parg_definer definer;
        struct parg_modifier modifier;
    };
};

ampart_status parg_from_str_inner(ampart_parg restrict parg, char const *restrict str, size_t len_str);
