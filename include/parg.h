#ifndef HAVE_PARG_H
#define HAVE_PARG_H
#include "common.h"

/* Definition */

#define PARG_ANY            0b00000000
#define PARG_DISALLOW       0b10000000
#define PARG_REQUIRED       0b00000001
#define PARG_ALLOW_ABSOLUTE 0b00000010
#define PARG_ALLOW_RELATIVE 0b00000100

/* Enumerable */

enum 
    parg_select_method {
        PARG_SELECT_NAME,
        PARG_SELECT_RELATIVE
    };

enum 
    parg_modify_detail_method {
        PARG_MODIFY_DETAIL_PRESERVE,
        PARG_MODIFY_DETAIL_SET,
        PARG_MODIFY_DETAIL_ADD,
        PARG_MODIFY_DETAIL_SUBSTRACT,
    };

enum 
    parg_modify_part_method {
        PARG_MODIFY_PART_ADJUST,
        PARG_MODIFY_PART_DELETE,
        PARG_MODIFY_PART_CLONE,
        PARG_MODIFY_PART_PLACE
    };

enum 
    parg_modify_place_method {
        PARG_MODIFY_PLACE_PRESERVE,
        PARG_MODIFY_PLACE_ABSOLUTE,
        PARG_MODIFY_PLACE_RELATIVE
    };

/* Structure */
struct 
    parg_definer {
        char        name[MAX_PARTITION_NAME_LENGTH];
        uint64_t    offset;
        uint64_t    size;
        uint32_t    masks;
        bool        relative_offset;
        bool        relative_size;
        bool        set_name;
        bool        set_offset;
        bool        set_size;
        bool        set_masks;
    };

struct 
    parg_modifier {
        enum parg_select_method         select;
        enum parg_modify_part_method    modify_part;
        enum parg_modify_place_method   modify_place;
        enum parg_modify_detail_method  modify_name;
        enum parg_modify_detail_method  modify_offset;
        enum parg_modify_detail_method  modify_size;
        enum parg_modify_detail_method  modify_masks;
        char                            select_name[16];
        int                             select_relative;
        int                             place;
        char                            name[16];
        uint64_t                        offset;
        uint64_t                        size;
        uint32_t                        masks;
    };

struct 
    parg_editor {
        bool                       modify;
        union {
            struct parg_definer    definer;
            struct parg_modifier   modifier;
        };
    };

#endif