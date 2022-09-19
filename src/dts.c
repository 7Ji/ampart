/* Self */

#include "dts.h"

/* System */

#include <byteswap.h>
#include <string.h>

/* Local */

#include "parg.h"
#include "util.h"

/* Definition */

#define DTS_BE_4                            0x04000000U
#define DTS_BE_8                            0x08000000U
#define DTS_PARTITIONS_NODE_START_LENGTH    12U

/* Enumerable */

#define DTS_ESSENTIAL_OFFSET_PARTS          0
#define DTS_ESSENTIAL_OFFSET_PNAME          1
#define DTS_ESSENTIAL_OFFSET_SIZE           2
#define DTS_ESSENTIAL_OFFSET_MASK           3
#define DTS_ESSENTIAL_OFFSET_PHANDLE        4
#define DTS_ESSENTIAL_OFFSET_LINUX_PHANDLE  5

/* Structure */

struct 
    dts_compose_partition_helper {
        size_t      len_name;
        size_t      len_pname;
        size_t      len_node_name;
        off_t       offset_partn;
        uint32_t    phandle;
        uint32_t    phandle_be32;
    };

struct 
    dts_property {
        uint32_t        len;
        uint8_t const * value;
    };

struct 
    dts_stringblock_essential_offsets {
        off_t parts;
        off_t pname;
        off_t size;
        off_t mask;
        off_t phandle;
        off_t linux_phandle;
    };

/* Variable */

struct dts_property dts_property = {0};
uint8_t const       dts_partitions_node_start[DTS_PARTITIONS_NODE_START_LENGTH] = "partitions";
struct dts_partitions_helper_simple const 
                    dts_partitions_helper_simple_empty = {.partitions_count = 0};

/* Function */

static inline
uint32_t
dts_report_offset(
    uint32_t const          i,
    uint32_t const * const  start,
    uint8_t const * const   node
){
    return (i + start - (const uint32_t *)node);
}

static inline
uint32_t
dts_get_prop_length(
    uint32_t const * const  current
){
    return bswap_32(*(current+1));
}

static inline
void
dts_skip_prop_with_length(
    uint32_t * const    i,
    uint32_t const      length
){
    *i += 2 + length / 4;
    if (length % 4) {
        *i += 1;
    }
}

static inline
void
dts_skip_prop_without_length(
    uint32_t * const        i,
    uint32_t const * const  current
){
    uint32_t len_prop = dts_get_prop_length(current);
    dts_skip_prop_with_length(i, len_prop);
}

static inline
size_t
dts_get_len_name(
    uint8_t const * const   node
){
    return strlen((char const*)node) + 1;
}

static inline
uint32_t const *
dts_get_start_with_len_name(
    uint8_t const * const   node,
    size_t const            len_name
) {
    return (uint32_t *)node + len_name / 4 + (bool)(len_name % 4);
}

static inline
uint32_t const *
dts_get_start_without_len_name(
    uint8_t const * const   node
){
    size_t const len_name = dts_get_len_name(node);
    return (uint32_t *)node + len_name / 4 + (bool)(len_name % 4);
}

static inline
uint32_t
dts_get_count(
    uint32_t const max_offset
){
    return max_offset / 4;
}

static inline
void
dts_report_invalid_token(
    const char * const      name,
    const uint32_t * const  current
){
    fprintf(stderr, "DTS %s: Invalid token %"PRIu32" (address %p)\n", name, bswap_32(*current), current);
}

static inline
void
dts_report_not_properly_end(
    const char * const  name
){
    fprintf(stderr, "DTS %s: Node not properly ended\n", name);
}

/**
 * @brief Skip the node and return the steps needed to skip the node
 * 
 * @param node 
 * @param max_offset 
 * @return uint32_t The length of the node, excluding the BEGIN_NODE and END_NODE token, measured in 4-byte steps
 */
static inline
uint32_t
dts_skip_node(
    uint8_t const * const   node,
    uint32_t const          max_offset
){
    uint32_t const count = dts_get_count(max_offset);
    if (!count) {
        return 0;
    }
    uint32_t const *const start = dts_get_start_without_len_name(node);
    const uint32_t *current;
    uint32_t offset_child;
    for (uint32_t i = 0; i < count; ++i) {
        current = start + i;
        // printf("0x%08x\n", bswap_32(*current));
        switch (*current) {
            case DTS_BEGIN_NODE_ACTUAL:
                offset_child = dts_skip_node((const uint8_t *)(current + 1), max_offset - 4*i);
                if (offset_child) {
                    i += offset_child + 1;
                } else {
                    fputs("DTS skip node: Failed to recursively skip child node\n", stderr);
                    return 0;
                }
                break;
            case DTS_END_NODE_ACTUAL:
                return dts_report_offset(i, start, node);
            case DTS_PROP_ACTUAL:
                dts_skip_prop_without_length(&i, current);
                break;
            case DTS_NOP_ACTUAL:
                break;
            default:
                dts_report_invalid_token("skip node", current);
                return 0;
        }
    }
    dts_report_not_properly_end("skip node");
    return 0;
}

uint32_t 
dts_get_node(
    uint8_t const * const   node, 
    uint32_t const          max_offset,
    char const * const      name,
    uint32_t const          layers,
    uint8_t const * * const target
){
    uint32_t const count = dts_get_count(max_offset);
    if (!count) {
        return 0;
    }
    if (strcmp((const char *)node, name)) {
        return dts_skip_node(node, max_offset);
    }
    if (!layers) {
        *target = node;
        return dts_skip_node(node, max_offset);
    }
    size_t const len_name = dts_get_len_name(node);
    uint32_t const *const start = dts_get_start_with_len_name(node, len_name);
    uint32_t const *current;
    uint32_t offset_child;
    for (uint32_t i = 0; i < count; ++i) {
        current = start + i;
        // printf("0x%08x\n", bswap_32(*current));
        switch (*current) {
            case DTS_BEGIN_NODE_ACTUAL:
                offset_child = dts_get_node((uint8_t const *)(current + 1), max_offset - 4*i, name + len_name, layers - 1, target);
                if (offset_child) {
                    i += offset_child + 1;
                } else {
                    fputs("DTS get node: Failed to travel through child node\n", stderr);
                    return 0;
                }
                break;
            case DTS_END_NODE_ACTUAL:
                return dts_report_offset(i, start, node);
            case DTS_PROP_ACTUAL:
                dts_skip_prop_without_length(&i, current);
                break;
            case DTS_NOP_ACTUAL:
                break;
            default:
                dts_report_invalid_token("get node", current);
                return 0;
        }
    }
    dts_report_not_properly_end("get node");
    return 0;
}

static inline
int 
dts_get_property_actual(
    uint8_t const * const   node,
    uint32_t const          property_offset
){
    uint32_t const *const start = dts_get_start_without_len_name(node);
    uint32_t const *current;
    uint32_t len_prop, name_off;
    for (uint32_t i = 0; ; ++i) {
        current = start + i;
        switch (*current) {
            case DTS_BEGIN_NODE_ACTUAL:
                fputs("DTS get property actual: child node starts, property not found, give up\n", stderr);
                return 1;
            case DTS_END_NODE_ACTUAL:
                fputs("DTS get property actual: node ends, give up\n", stderr);
                return 1;
            case DTS_PROP_ACTUAL:
                len_prop = bswap_32(*(current+1));
                name_off = bswap_32(*(current+2));
                if (name_off == property_offset) {
                    dts_property.len = len_prop;
                    dts_property.value = (const uint8_t *)(current + 3);
                    return 0;
                }
                dts_skip_prop_with_length(&i, len_prop);
                break;
            case DTS_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "DTS get property: Invalid token %"PRIu32"\n", bswap_32(*current));
                return 1;
        }
    }
}

int 
dts_get_property(
    uint8_t const * const                   node,
    struct stringblock_helper const * const shelper,
    char const * const                      property
){
    off_t const property_offset = stringblock_find_string(shelper, property);
    if (property_offset < 0) {
        return 1;
    }
    return dts_get_property_actual(node, property_offset);
}

static inline
int
dts_get_node_from_path_sanity_check(
    uint8_t const * const   dts,
    uint32_t const          max_offset,
    char const * const      path
){
    if (!dts) {
        fputs("DTS get node from path: No dtb to lookup\n", stderr);
        return 1;
    }
    if (!max_offset) {
        fputs("DTS get node from path: Max offset invalid\n", stderr);
        return 1;
    }
    if (max_offset % 4) {
        fputs("DTS get node from path: Offset is not multiply of 4\n", stderr);
        return 1;
    }
    if (!path) {
        fputs("DTS get node from path: No path to lookup\n", stderr);
        return 1;
    }
    if (!path[0]) {
        fputs("DTS get node from path: Empty path to lookup\n", stderr);
        return 1;
    }
    if (path[0] != '/') {
        fputs("DTS get node from path: Path does not start with /\n", stderr);
        return 1;
    }
    return 0;
}

static inline
int
dts_get_path_layers(
    char * const    path,
    size_t const    len
){
    int layers = 0;
    for (size_t i = 0; i<len; ++i) {
        switch (path[i]) {
            case '/':
                path[i] = '\0';
                ++layers;
                break;
            case '\0': // This should not happend
                fputs("DTS get path layers: Path ends prematurely\n", stderr);
                return -1;
        }
    }
    return layers;
}

uint8_t *
dts_skip_nop(
    uint8_t const * const   dts
){
    uint32_t const *start = (uint32_t const *)dts;
    while (*start == DTS_NOP_ACTUAL) {
        ++start;
    }
    if (*start != DTS_BEGIN_NODE_ACTUAL) {
        fputs("DTS skip NOP: Node does not start properly", stderr);
        return NULL;
    }
    return (uint8_t *)start;
}

uint8_t *
dts_get_node_from_path(
    uint8_t const * const   dts,
    uint32_t const          max_offset,
    char const * const      path,
    size_t                  len_path
){
    if (dts_get_node_from_path_sanity_check(dts, max_offset, path)) {
        return NULL;
    }
    if (!len_path) {
        if (!(len_path = strlen(path))) { // This should not happen
            fputs("DTS get node from path: Empty path to lookup\n", stderr);
            return NULL;
        }
    }
    uint8_t const *const start = dts_skip_nop(dts);
    if (!start) {
        fputs("DTS get node from path: Node does not start properly", stderr);
        return NULL;
    }
    // uint32_t const *current = (uint32_t const *)dts;
    // while (*current == DTS_NOP_ACTUAL) {
    //     ++current;
    // }
    // if (*current != DTS_BEGIN_NODE_ACTUAL) {
    //     fputs("DTS get node from path: Node does not start properly", stderr);
    //     return NULL;
    // }
    if (len_path == 1) {
        fputs("DTS get node from path: Early quit for root node\n", stderr);
        return (uint8_t *)start + 4;
    }
    char *const path_actual = strdup(path);
    if (!path_actual) {
        fputs("DTS get node from path: Failed to dup path\n", stderr);
        return NULL;
    }
    int const layers = dts_get_path_layers(path_actual, len_path);
    if (layers < 0) {
        free(path_actual);
        return NULL;
    }
    uint8_t *target = NULL;
    uint32_t r = dts_get_node(start + 4, max_offset - (start - dts) - 4, path_actual, layers, (uint8_t const **)&target);
    free(path_actual);
    if (r) {
        return target;
    } else {
        fputs("DTS get node from path: Error occured, can not find\n", stderr);
        return NULL;
    }
}


static inline
off_t 
dts_stringblock_essential_offset_get(
    struct stringblock_helper const * const shelper,
    char const * const                      name,
    uint32_t * const                        invalids
){
    const off_t offset = stringblock_find_string(shelper, name);
    if (offset < 0) {
        ++(*invalids);
        fprintf(stderr, "DTS stringblock essential offset: can not find %s in stringblock\n", name);
    }
    return offset;
}

static inline
int
dts_get_partitions_get_essential_offsets(
    struct dts_stringblock_essential_offsets * const    offsets,
    struct stringblock_helper const * const             shelper
){
    uint32_t offset_invalids = 0;
    uint32_t optional_invalids = 0;
    offsets->parts = dts_stringblock_essential_offset_get(shelper, "parts", &offset_invalids);
    offsets->pname = dts_stringblock_essential_offset_get(shelper, "pname", &offset_invalids);
    offsets->size = dts_stringblock_essential_offset_get(shelper, "size", &offset_invalids);
    offsets->mask = dts_stringblock_essential_offset_get(shelper, "mask", &offset_invalids);
    offsets->phandle = dts_stringblock_essential_offset_get(shelper, "phandle", &offset_invalids);
    offsets->linux_phandle = dts_stringblock_essential_offset_get(shelper, "linux,phandle", &optional_invalids);
    return offset_invalids;
}

static inline
int
dts_parse_partitions_node_begin(
    struct dts_partitions_helper * const        phelper,
    bool * const                                in_partition,
    struct dts_partition_entry * * const        partition,
    uint32_t const * const                      current,
    uint32_t * const                            i

){
    if (*in_partition) {
        fputs("DTS parse partitions node begin: encountered sub node inside partition, which is impossible\n", stderr);
        return 1;
    } else {
        if (phelper->partitions_count == MAX_PARTITIONS_COUNT) {
            fputs("DTS parse partitions node begin: partitions count exceeds maximum\n", stderr);
            return 2;
        }
        *partition = phelper->partitions + phelper->partitions_count++;
        size_t const len_node_name = strlen((const char *)(current + 1));
        if (len_node_name > 15) {
            fprintf(stderr, "DTS parse partitions node begin: partition name '%s' too long\n", (const char *)(current + 1));
            return 3;
        }
        *i += (len_node_name + 1) / 4;
        if ((len_node_name + 1) % 4) {
            ++*i;
        }
        strncpy((*partition)->name, (const char *)(current + 1), len_node_name);
        *in_partition = true;
    }
    return 0;
}

static inline
int
dts_parse_partitions_node_end(
    struct dts_partition_entry * const          partition
){
    if (partition && partition->phandle && partition->linux_phandle && partition->phandle != partition->linux_phandle) {
        fprintf(stderr, "DTS parse partitions end: partition '%s' has different phandle (%"PRIu32") and linux,phandle (%"PRIu32")\n", partition->name, partition->phandle, partition->linux_phandle);
        return 1;
    }
    return 0;
}

static inline
int
dts_parse_partitions_node_prop_root(
    uint32_t const * const                              current,
    uint32_t const                                      len_prop,
    uint32_t const                                      name_off,
    struct dts_stringblock_essential_offsets * const    offsets,
    struct dts_partitions_helper * const                phelper,
    struct stringblock_helper const * const             shelper

){
    if (len_prop == 4) {
        if (name_off == offsets->parts) {
            phelper->record_count = bswap_32(*(current+3));
        } else if (name_off == offsets->phandle) {
            phelper->phandle_root = bswap_32(*(current+3));
        } else if (name_off == offsets->linux_phandle) {
            phelper->linux_phandle_root = bswap_32(*(current+3));
        } else {
            if (strncmp(shelper->stringblock + name_off, "part-", 5)) {
                fprintf(stderr, "DTS parse partitions node prop root: invalid propertey '%s' in partitions node\n", shelper->stringblock + name_off);
                return 1;
            } else {
                unsigned long phandle_id = strtoul(shelper->stringblock + name_off + 5, NULL , 10);
                if (phandle_id > MAX_PARTITIONS_COUNT - 1) {
                    fprintf(stderr, "DTS parse partitions node prop root: invalid part id %lu in partitions node\n", phandle_id);
                    return 2;
                }
                phelper->phandles[phandle_id] = bswap_32(*(current+3));
            }
        }
    } else {
        fprintf(stderr, "DTS parse partitions node prop root: %s property of partitions node is not of length 4\n", shelper->stringblock + name_off);
        return 3;
    }
    return 0;
}

static inline
int
dts_parse_partitions_node_prop_child(
    uint32_t const * const                              current,
    uint32_t const                                      len_prop,
    uint32_t const                                      name_off,
    struct dts_stringblock_essential_offsets * const    offsets,
    struct dts_partition_entry * const                  partition,
    struct dts_partitions_helper * const                phelper,
    struct stringblock_helper const * const             shelper
){
    if (name_off == offsets->pname) {
        if (len_prop > 16) {
            fprintf(stderr, "DTS parse partitions node prop child: partition name '%s' too long\n", (const char *)(current + 3));
            return 1;
        }
        if (strcmp(partition->name, (const char *)(current + 3))) {
            fprintf(stderr, "DTS parse partitions node prop child: pname property %s different from partition node name %s\n", (const char *)(current + 3), partition->name);
            return 2;
        }
    } else if (name_off == offsets->size) {
        if (len_prop == 8) {
            partition->size = ((uint64_t)bswap_32(*(current+3)) << 32) | (uint64_t)bswap_32(*(current+4));
        } else {
            fputs("DTS parse partitions node prop child: partition size is not of length 8\n", stderr);
            return 3;
        }
    } else if (name_off == offsets->mask) {
        if (len_prop == 4) {
            partition->mask = bswap_32(*(current+3));
        } else {
            fputs("DTS parse partitions node prop child: partition mask is not of length 4\n", stderr);
            return 4;
        }
    } else if (name_off == offsets->phandle) {
        if (len_prop == 4) {
            partition->phandle = bswap_32(*(current+3));
        } else {
            fputs("DTS parse partitions node prop child: partition phandle is not of length 4\n", stderr);
            return 5;
        }
    } else if (name_off == offsets->linux_phandle) {
        if (len_prop == 4) {
            partition->linux_phandle = bswap_32(*(current+3));
        } else {
            fputs("DTS parse partitions node prop child: partition phandle is not of length 4\n", stderr);
            return 6;
        }
    } else {
        fprintf(stderr, "DTS parse partitions node prop child: invalid property for partition, ignored: %s\n", shelper->stringblock+name_off);
        free(phelper);
        return 7;
    }
    return 0;
}

static inline
int
dts_parse_partitions_node_prop(
    uint32_t const * const                              current,
    uint32_t * const                                    i,
    bool const                                          in_partition,
    struct dts_stringblock_essential_offsets * const    offsets,
    struct dts_partition_entry * const                  partition,
    struct dts_partitions_helper * const                phelper,
    struct stringblock_helper const * const             shelper
){
    uint32_t const len_prop = bswap_32(*(current+1));
    uint32_t const name_off = bswap_32(*(current+2));
    dts_skip_prop_with_length(i, len_prop);
    if (in_partition) {
        return dts_parse_partitions_node_prop_child(current, len_prop, name_off, offsets, partition, phelper, shelper);
    } else {
        return dts_parse_partitions_node_prop_root(current, len_prop, name_off, offsets, phelper, shelper);
    }
}

int
dts_get_partitions_from_node(
    struct dts_partitions_helper *          phelper,
    struct stringblock_helper const * const shelper
){
    if (memcmp(phelper->node, dts_partitions_node_start, DTS_PARTITIONS_NODE_START_LENGTH)) {
        fputs("DTS get partitions from node: node does not start properly\n", stderr);
        return 1;
    }
    struct dts_stringblock_essential_offsets offsets;
    if (dts_get_partitions_get_essential_offsets(&offsets, shelper)) {
        return 2;
    }
    uint8_t *node_temp = phelper->node;
    memset(phelper, 0, sizeof *phelper);
    phelper->node = node_temp;
    uint32_t const * const start = (const uint32_t *)(phelper->node + DTS_PARTITIONS_NODE_START_LENGTH);
    uint32_t const *current;
    bool in_partition = false;
    struct dts_partition_entry *partition = NULL;
    for (uint32_t i = 0; ; ++i) {
        current = start + i;
        switch (*current) {
            case DTS_BEGIN_NODE_ACTUAL:
                if (dts_parse_partitions_node_begin(phelper, &in_partition, &partition, current, &i)) {
                    return 3;
                }
                break;
            case DTS_END_NODE_ACTUAL:
                if (in_partition) {
                    if (dts_parse_partitions_node_end(partition)) {
                        return 4;
                    }
                    in_partition = false;
                } else {
                    // phelper->node = (uint8_t *)node;
                    return 0;
                }
                break;
            case DTS_PROP_ACTUAL:
                if (dts_parse_partitions_node_prop(current, &i, in_partition, &offsets, partition, phelper, shelper)) {
                    return 5;
                }
                break;
            case DTS_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "DTB get partitions: invalid token 0x%08x\n", bswap_32(*current));
                return 6;
        }
    }
}

int
dts_sort_partitions(
    struct dts_partitions_helper * const    phelper
){
    if (!phelper) {
        fputs("DTB sort partitions: no partitions helper to sort\n", stderr);
        return 1;
    }
    if (phelper->partitions_count != phelper->record_count) {
        fprintf(stderr, "DTB sort partitions: partitions node count (%"PRIu32") != record count (%"PRIu32")\n", phelper->partitions_count, phelper->record_count);
        return 2;
    }
    struct dts_partition_entry buffer;
    for (uint32_t i = 0; i<phelper->partitions_count; ++i) {
        if (phelper->phandles[i] != phelper->partitions[i].phandle) {
            for (uint32_t j = i + 1; j < phelper->partitions_count; ++j) {
                if (phelper->partitions[j].phandle == phelper->phandles[i]) {
                    buffer = phelper->partitions[i];
                    phelper->partitions[i] = phelper->partitions[j];
                    phelper->partitions[j] = buffer;
                    break;
                }
            }
        }
    }
    fputs("DTS sort partitions: partitions now in part-num order defined in partitions node's properties\n", stderr);
    return 0;
}

void
dts_report_partitions(
    struct dts_partitions_helper const * const  phelper
){
    fprintf(stderr, "DTS report partitions: %u partitions in the DTB:\n=======================================================\nID| name            |            size|(   human)| masks\n-------------------------------------------------------\n", phelper->partitions_count);
    const struct dts_partition_entry *part;
    double num_size;
    char suffix_size;
    for (uint32_t i=0; i<phelper->partitions_count; ++i) {
        part = phelper->partitions + i;
        if (part->size == (uint64_t)-1) {
            fprintf(stderr, "%2d: %-16s                  (AUTOFILL) %6"PRIu32"\n", i, part->name, part->mask);
        } else {
            num_size = util_size_to_human_readable(part->size, &suffix_size);
            fprintf(stderr, "%2d: %-16s %16"PRIx64" (%7.2lf%c) %6"PRIu32"\n", i, part->name, part->size, num_size, suffix_size, part->mask);
        }
    }
    fputs("=======================================================\n", stderr);
    return;
}


void
dts_report_partitions_simple(
    struct dts_partitions_helper_simple const * phelper
){
    fprintf(stderr, "DTS report partitions: %u partitions in the DTB:\n=======================================================\nID| name            |            size|(   human)| masks\n-------------------------------------------------------\n", phelper->partitions_count);
    const struct dts_partition_entry_simple *part;
    double num_size;
    char suffix_size;
    for (uint32_t i=0; i<phelper->partitions_count; ++i) {
        part = phelper->partitions + i;
        if (part->size == (uint64_t)-1) {
            fprintf(stderr, "%2d: %-16s                  (AUTOFILL) %6"PRIu32"\n", i, part->name, part->mask);
        } else {
            num_size = util_size_to_human_readable(part->size, &suffix_size);
            fprintf(stderr, "%2d: %-16s %16"PRIx64" (%7.2lf%c) %6"PRIu32"\n", i, part->name, part->size, num_size, suffix_size, part->mask);
        }
    }
    fputs("=======================================================\n", stderr);
    return;
}

int
dts_phandle_list_realloc(
    struct dts_phandle_list * const plist
){
    if (!plist || !plist->phandles || !plist->allocated_count) {
        return -1;
    }
    uint8_t *buffer = realloc(plist->phandles, plist->allocated_count * 2 * sizeof *buffer);
    if (buffer) {
        memset(buffer + plist->allocated_count, 0, plist->allocated_count * sizeof *buffer);
        plist->phandles = buffer;
    } else {
        fputs("DTS phandle list realloc: Failed to re-allocate memory\n", stderr);
        return 1;
    }
    plist->allocated_count *= 2;
    return 0;
}

static inline
int
dts_get_phandles_recursive_parse_prop(
    uint32_t const * const          current,
    uint32_t * const                i,
    uint32_t const                  offset_phandle,
    uint32_t const                  offset_linux_phandle,
    struct dts_phandle_list * const plist
){
    uint32_t const len_prop = bswap_32(*(current+1));
    uint32_t const name_off = bswap_32(*(current+2));
    dts_skip_prop_with_length(i, len_prop);
    if (name_off == offset_phandle || name_off == offset_linux_phandle) {
        if (len_prop == 4) {
            uint32_t const phandle = bswap_32(*(current+3));
            while (phandle >= plist->allocated_count) {
                if (dts_phandle_list_realloc(plist)) {
                    fputs("DTS get phandles recursive: Failed to re-allocate memory\n", stderr);
                    return 1;
                }
            }
            ++(plist->phandles[phandle]);
        } else {
            fputs("DTS get phandles recursive: phandle not of length 4\n", stderr);
            return 1;
        }
    }
    return 0;
}

uint32_t
dts_get_phandles_recursive(
    struct dts_phandle_list * const plist,
    uint8_t const * const           node,
    uint32_t const                  max_offset,
    uint32_t const                  offset_phandle,
    uint32_t const                  offset_linux_phandle
){
    uint32_t const count = dts_get_count(max_offset);
    if (!count) {
        return 0;
    }
    uint32_t const *const start = dts_get_start_without_len_name(node);
    uint32_t const *current;
    uint32_t offset_child;
    for (uint32_t i = 0; i < count; ++i) {
        current = start + i;
        switch (*current) {
            case DTS_BEGIN_NODE_ACTUAL:
                offset_child = dts_get_phandles_recursive(plist, (uint8_t const*)(current + 1), max_offset - 4*i, offset_phandle, offset_linux_phandle);
                if (offset_child) {
                    i += offset_child + 1;
                } else {
                    fputs("DTS get phandles recursive: Failed to recursively get phandles from child node\n", stderr);
                    return 0;
                }
                break;
            case DTS_END_NODE_ACTUAL:
                return i + start - (const uint32_t *)node;
            case DTS_PROP_ACTUAL:
                if (dts_get_phandles_recursive_parse_prop(current, &i, offset_phandle, offset_linux_phandle, plist)) {
                    return 0;
                }
                break;
            case DTS_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "DTS get phandles recursive: Invalid token %"PRIu32"\n", bswap_32(*current));
                return 0;
        }
    }
    fputs("DTS get phandles recursive: Node not properly ended\n", stderr);
    return 0;

}

static inline 
int
dts_phandle_list_finish_entry(
    struct dts_phandle_list * const plist,
    uint32_t const                  i,
    bool * const                    init,
    bool * const                    have_1,
    bool * const                    have_2
){
    if (plist->phandles[i]) {
        switch (plist->phandles[i]) {
            case 1:
                *have_1 = true;
                break;
            case 2:
                *have_2 = true;
                break;
            default:
                fprintf(stderr, "DTS phandle list finish: phandle %u/%x appears %u times, which is illegal\n", i, i, plist->phandles[i]);
                return 1;
        }
        if (*init)  {
            if (i<plist->min_phandle) {
                plist->min_phandle = i;
            }
            if (i>plist->max_phandle) {
                plist->max_phandle = i;
            }
        } else {
            *init = true;
            plist->min_phandle = i;
            plist->max_phandle = i;
        }
    }
    return 0;
}

int
dts_phandle_list_finish(
    struct dts_phandle_list * const plist
){
    bool init = false;
    bool have_1 = false;
    bool have_2 = false;
    for (uint32_t i = 0; i < plist->allocated_count; ++i) {
        if (dts_phandle_list_finish_entry(plist, i, &init, &have_1, &have_2)) {
            return 1;
        }
    }
    if (!init) {
        fputs("DTS phandle list finish: Not yet initialized\n", stderr);
        return 2;
    }
    if (have_1) {
        if (have_2) {
            fputs("DTS phandle list finish: Both 1 and 2 apperance counts for phandle, which is illegal\n", stderr);
            return 3;
        }
    } else if (have_2) {
        plist->have_linux_phandle = true;
    }
    return 0;
}

int
dts_get_property_string(
    uint8_t const * const   node,
    uint32_t const          property_offset,
    char *                  string,
    size_t const            max_len
){
    dts_property.len = 0;
    dts_property.value = NULL;
    if (dts_get_property_actual(node, property_offset)) {
        fputs("DTS get property string: Failed to get property\n", stderr);
        return 1;
    }
    if (max_len && dts_property.len > max_len) {
        fputs("DTS get property string: String too long\n", stderr);
        return 2;
    }
    strncpy(string, (const char *)dts_property.value, dts_property.len);
    return 0;
}


uint32_t 
dts_compare_partitions(
    struct dts_partitions_helper const * const  phelper_a, 
    struct dts_partitions_helper const * const  phelper_b
){
    uint32_t r = 0;
    uint32_t compare_partitions;
    uint32_t diff;
    if (phelper_a->partitions_count > phelper_b->partitions_count) {
        compare_partitions = phelper_b->partitions_count;
        diff = phelper_a->partitions_count - phelper_b->partitions_count;
    } else {
        compare_partitions = phelper_b->partitions_count;
        diff = phelper_b->partitions_count - phelper_a->partitions_count;
    }
    if (compare_partitions) {
        struct dts_partition_entry const *part_a, *part_b;
        for (uint32_t i = 0; i < compare_partitions; ++i) {
            part_a = phelper_a->partitions + i;
            part_b = phelper_b->partitions + i;
            if (strncmp(part_a->name, part_b->name, MAX_PARTITION_NAME_LENGTH)) {
                r += 1;
            }
            if (part_a->size != part_b->size) {
                r += 2;
            }
            if (part_a->mask != part_b->mask) {
                r += 4;
            }
        }
    } 
    return r + 8 * diff;
}

uint32_t 
dts_compare_partitions_simple(
    struct dts_partitions_helper_simple const * const  phelper_a, 
    struct dts_partitions_helper_simple const * const  phelper_b
){
    uint32_t r = 0;
    uint32_t compare_partitions;
    uint32_t diff;
    if (phelper_a->partitions_count > phelper_b->partitions_count) {
        compare_partitions = phelper_b->partitions_count;
        diff = phelper_a->partitions_count - phelper_b->partitions_count;
    } else {
        compare_partitions = phelper_b->partitions_count;
        diff = phelper_b->partitions_count - phelper_a->partitions_count;
    }
    if (compare_partitions) {
        struct dts_partition_entry_simple const *part_a, *part_b;
        for (uint32_t i = 0; i < compare_partitions; ++i) {
            part_a = phelper_a->partitions + i;
            part_b = phelper_b->partitions + i;
            if (strncmp(part_a->name, part_b->name, MAX_PARTITION_NAME_LENGTH)) {
                r += 1;
            }
            if (part_a->size != part_b->size) {
                r += 2;
            }
            if (part_a->mask != part_b->mask) {
                r += 4;
            }
        }
    } 
    return r + 8 * diff;
}

uint32_t 
dts_compare_partitions_mixed(
    struct dts_partitions_helper const * const  phelper_a, 
    struct dts_partitions_helper_simple const * const  phelper_b
){
    uint32_t r = 0;
    uint32_t compare_partitions;
    uint32_t diff;
    if (phelper_a->partitions_count > phelper_b->partitions_count) {
        compare_partitions = phelper_b->partitions_count;
        diff = phelper_a->partitions_count - phelper_b->partitions_count;
    } else {
        compare_partitions = phelper_b->partitions_count;
        diff = phelper_b->partitions_count - phelper_a->partitions_count;
    }
    if (compare_partitions) {
        struct dts_partition_entry const *part_a;
        struct dts_partition_entry_simple const *part_b;
        for (uint32_t i = 0; i < compare_partitions; ++i) {
            part_a = phelper_a->partitions + i;
            part_b = phelper_b->partitions + i;
            if (strncmp(part_a->name, part_b->name, MAX_PARTITION_NAME_LENGTH)) {
                r += 1;
            }
            if (part_a->size != part_b->size) {
                r += 2;
            }
            if (part_a->mask != part_b->mask) {
                r += 4;
            }
        }
    } 
    return r + 8 * diff;
}

int
dts_dclone_parse(
    struct dts_partitions_helper_simple * const dparts,
    int const                                   argc,
    char const * const * const                  argv
){
    if (argc < 0 || argc >= MAX_PARTITIONS_COUNT || !dparts || !argv) {
        fputs("DTS dclone parse: Illegal arguments\n", stderr);
        printf("%d, %u\n", argc, dparts->partitions_count);
        return -1;
    }
    struct parg_definer_helper_static dhelper;
    parg_parse_dclone_mode(&dhelper, argc, argv);
    if (parg_parse_dclone_mode(&dhelper, argc, argv) || !dhelper.count) {
        fputs("DTS dclone parse: Failed to parse new partitions\n", stderr);
        return 1;
    }
    *dparts = dts_partitions_helper_simple_empty;
    dparts->partitions_count = dhelper.count;
    struct parg_definer *definer;
    struct dts_partition_entry_simple *entry;
    for (unsigned i = 0; i < dhelper.count; ++i) {
        definer = dhelper.definers + i;
        entry = dparts->partitions + i;
        strncpy(entry->name, definer->name, MAX_PARTITION_NAME_LENGTH);
        entry->size = definer->size;
        entry->mask = definer->masks;
    }
    fputs("DTS dclone parse: New DTB partitions:\n", stderr);
    dts_report_partitions_simple(dparts);
    return 0;
}

int
dts_get_phandles(
    struct dts_phandle_list * const plist,
    uint8_t const * const           dts,
    uint32_t const                  max_offset,
    uint32_t const                  offset_phandle,
    uint32_t const                  offset_linux_phandle
){
    uint8_t const *const start = dts_skip_nop(dts);
    if (!start) {
        fputs("DTS get phandles: Node does not start properly\n", stderr);
        return -1;
    }
    memset(plist, 0, sizeof *plist);
    plist->phandles = malloc(128 * sizeof *plist->phandles);
    if (!plist->phandles) {
        fputs("DTS get phandles: Failed to allocate memory for phandle list\n", stderr);
        return 1;
    }
    memset(plist->phandles, 0, 128 * sizeof *plist->phandles);
    plist->allocated_count = 128;
    if (!dts_get_phandles_recursive(plist, start + 4, max_offset - (start - dts) - 4, offset_phandle, offset_linux_phandle)) {
        free(plist->phandles);
        fputs("DTS get phandles: Failed to get phandle list\n", stderr);
        return 2;
    }
    // for (unsigned i = 0; i < plist->allocated_count; ++i) {
    //     printf("phandle %u: %u\n", i, plist->phandles[i]);
    // }
    if (dts_phandle_list_finish(plist)) {
        free(plist->phandles);
        fputs("DTS get phandles: Failed to finish phandle list\n", stderr);
        return 3;
    }
    return 0;
}

int
dts_drop_partitions_phandles(
    struct dts_phandle_list * const             plist,
    struct dts_partitions_helper const * const  phelper
){
    if (!plist || !phelper || !plist->phandles) {
        return -1;
    }
    uint8_t const step = plist->have_linux_phandle ? 2 : 1;
    struct dts_partition_entry const *dts_part;
    uint32_t phandle;
    for (uint32_t i = 0; i < phelper->partitions_count; ++i) {
        if (!(phandle = (dts_part = phelper->partitions + i)->phandle)) {
            continue;
        }
        plist->phandles[phandle] -= step;
        if (plist->phandles[phandle]) {
            fprintf(stderr, "DTS drop partitions phandles: Phandle 0x%x still used after droping it from partition %s, this is impossile, give up\n", phandle, dts_part->name);
            return 1;
        }
        fprintf(stderr, "DTS drop partitions phandles: Phandle 0x%x previously used by partition %s can be used now\n", phandle, dts_part->name);
    }
    if ((phandle = phelper->phandle_root)) {
        plist->phandles[phandle] -= step;
        if (plist->phandles[phandle]) {
            fprintf(stderr, "DTS drop partitions phandles: Phandle 0x%x still used after dropping it from partitions root node, this is impossile, give up\n", phandle);
            return 5;
        }
        fprintf(stderr, "DTS drop partitions phandles: Phandle 0x%x previously used by partitions root node can be used now\n", phandle);
    }
    return 0;
}

uint32_t
dts_assign_available_phanle(
    struct dts_phandle_list * const plist
){
    if (!plist || !plist->phandles || !plist->allocated_count) {
        return 0;
    }
    for (uint32_t i = 1;; ++i){
        while (i >= plist->allocated_count) {
            if (dts_phandle_list_realloc(plist)) {
                fputs("DTS assign available phandle: Failed to re-allocate memory\n", stderr);
                return 0;
            }
        }
        if (!plist->phandles[i]) {
            plist->phandles[i] += (plist->have_linux_phandle) ? 2 : 1;
            return i;
        }
    }
}

void
dts_add_property_be32(
    uint32_t * * const  current,
    uint32_t const  name_off_be32,
    uint32_t const  value_be32
){
    *((*current)++) = DTS_PROP_ACTUAL;
    *((*current)++) = DTS_BE_4;
    *((*current)++) = name_off_be32;
    *((*current)++) = value_be32;   
}


int
dts_compose_partitions_node(
    uint8_t * * const                                   node,
    size_t * const                                      len_node,
    struct dts_phandle_list * const                     plist,
    struct dts_partitions_helper_simple const * const   phelper,
    struct stringblock_helper * const                   shelper,
    off_t const                                         offset_phandle,
    off_t const                                         offset_linux_phandle
){
    if (!node || !len_node || !plist || !phelper || !shelper || !plist->phandles || !plist->allocated_count || !phelper->partitions_count || offset_phandle < 0 || (plist->have_linux_phandle && offset_linux_phandle < 0)) {
        fputs("DTS compose partitions node: Illegal arguments\n", stderr);
        return -1;
    }
    *node = NULL;
    uint32_t const offsets[6] = {
        stringblock_append_string_safely(shelper, "parts", 0),
        stringblock_append_string_safely(shelper, "pname", 0),
        stringblock_append_string_safely(shelper, "size", 0),
        stringblock_append_string_safely(shelper, "mask", 0),
        offset_phandle,
        offset_linux_phandle
    };
    uint32_t const offsets_be32[6] = {
        bswap_32(offsets[0]),
        bswap_32(offsets[1]),
        bswap_32(offsets[2]),
        bswap_32(offsets[3]),
        bswap_32(offsets[4]),
        bswap_32(offsets[5])
    };
    struct dts_compose_partition_helper chelpers[MAX_PARTITIONS_COUNT] = {0};
    struct dts_compose_partition_helper *chelper;
    struct dts_partition_entry_simple const *dentry;
    char partn[] = "part-NN";
    // Basic length of the node, excluding the start BEGIN_NODE and end END_NODE, 12 for partitions\0\0\0 as name, len 16 property (4 PROP_NODE, 4 len_prop, 4 name_off, 4 u32 value) for: 1 for parts, 1 for each part-N (storing phandle), 1 for phandle, optional 1 for linux,phandle. Then basic length of the partition sub-node, 8 (4 BEGIN_NODE + 4 END_NODE) + 12 (4 PROP_NODE, 4 len_prop, 4 name_off) for 4 or 5 props (pname, size, mask, phandle, optionally linux,phandle) + 8 for u64 size + 4 for u32 mask + 4 for u32 phandle + 4 optionally for u32 linux,phandle
    *len_node = 12 + 16 * (1 + phelper->partitions_count + 1 + plist->have_linux_phandle) + phelper->partitions_count * (8 + 12 * (4 + plist->have_linux_phandle) + 8 + 4 + 4 + 4 * plist->have_linux_phandle);
    for (uint32_t i = 0; i < phelper->partitions_count; ++i) {
        dentry = phelper->partitions + i;
        memset(partn + 5, 0, 3);
        snprintf(partn + 5, 3, "%u", i % 100);
        chelper = chelpers + i;
        chelper->len_name = strlen(dentry->name);
        chelper->len_pname = chelper->len_name + 1;
        chelper->len_node_name = util_nearest_upper_bound_ulong(chelper->len_pname, 4);
        chelper->offset_partn = stringblock_append_string_safely(shelper, partn, 0);
        chelper->phandle = dts_assign_available_phanle(plist);
        chelper->phandle_be32 = bswap_32(chelper->phandle);
        *len_node += 2 * chelper->len_node_name;
    }
    uint32_t const phandle_root = dts_assign_available_phanle(plist);
    uint32_t const phandle_root_be32 = bswap_32(phandle_root);
    *node = malloc(*len_node); // Excluding the beginning BEGIN_NODE and endding END_NODE
    if (!*node) {
        return 1;
    }
    memcpy(*node, dts_partitions_node_start, DTS_PARTITIONS_NODE_START_LENGTH);
    uint32_t *current = (uint32_t *)(*node + DTS_PARTITIONS_NODE_START_LENGTH);
    dts_add_property_be32(&current, offsets_be32[DTS_ESSENTIAL_OFFSET_PARTS], bswap_32(phelper->partitions_count));
    for (uint32_t i = 0; i < phelper->partitions_count; ++i) {
        chelper = chelpers + i;
        dts_add_property_be32(&current, bswap_32(chelper->offset_partn), bswap_32(chelper->phandle));
    }
    dts_add_property_be32(&current, offsets_be32[DTS_ESSENTIAL_OFFSET_PHANDLE], phandle_root_be32);
    if (plist->have_linux_phandle) {
        dts_add_property_be32(&current, offsets_be32[DTS_ESSENTIAL_OFFSET_LINUX_PHANDLE], phandle_root_be32);
    }
    for (uint32_t i = 0; i < phelper->partitions_count; ++i) {
        dentry = phelper->partitions + i;
        chelper = chelpers + i;
        // node
        *(current++) = DTS_BEGIN_NODE_ACTUAL;
        strncpy((char *)current, dentry->name, chelper->len_pname);
        current = (uint32_t *)((uint8_t *)current + chelper->len_node_name);
        // pname
        *(current++) = DTS_PROP_ACTUAL;
        *(current++) = bswap_32(chelper->len_pname);
        *(current++) = offsets_be32[DTS_ESSENTIAL_OFFSET_PNAME];
        strncpy((char *)current, dentry->name, chelper->len_pname);
        current = (uint32_t *)((uint8_t *)current + chelper->len_node_name);
        // size
        *(current++) = DTS_PROP_ACTUAL;
        *(current++) = DTS_BE_8;
        *(current++) = offsets_be32[DTS_ESSENTIAL_OFFSET_SIZE];
        *(current++) = bswap_32(dentry->size >> 32); // Force cut 
        *(current++) = bswap_32(dentry->size);
        // mask
        dts_add_property_be32(&current, offsets_be32[DTS_ESSENTIAL_OFFSET_MASK], bswap_32(dentry->mask));
        // phandle
        dts_add_property_be32(&current, offsets_be32[DTS_ESSENTIAL_OFFSET_PHANDLE], chelper->phandle_be32);
        // linux,phandle
        if (plist->have_linux_phandle) {
            dts_add_property_be32(&current, offsets_be32[DTS_ESSENTIAL_OFFSET_LINUX_PHANDLE], chelper->phandle_be32);
        }
        *(current++) = DTS_END_NODE_ACTUAL;
    }
    // printf("Start: %p, length %lx, current %p\n", *node, *len_node, current);
    return 0;
}

size_t
dts_get_node_full_length(
    uint8_t const * const node,
    uint32_t const         max_offset
){
    if (!node) {
        return 0;
    }
    uint32_t const offset_child = dts_skip_node(node, max_offset);
    if (!offset_child) {
        return 0;
    }
    return 4 * (offset_child + 2);
}

int
dts_partitions_helper_to_simple(
    struct dts_partitions_helper_simple * const simple,
    struct dts_partitions_helper const * const  generic
){
    if (!simple || !generic || !generic->partitions_count) {
        return -1;
    }
    simple->partitions_count = generic->partitions_count;
    for (unsigned i = 0; i < simple->partitions_count; ++i) {
        simple->partitions[i] = *(struct dts_partition_entry_simple *)(generic->partitions + i);
    }
    return 0;
}

struct dts_partition_entry_simple *
dts_dedit_part_select(
    struct parg_modifier const * const modifier,
    struct dts_partitions_helper_simple * const dparts
){
    switch (modifier->select) {
        case PARG_SELECT_NAME:
            for (unsigned i = 0; i < dparts->partitions_count; ++i) {
                if (!strncmp(modifier->select_name, dparts->partitions[i].name, MAX_PARTITION_NAME_LENGTH)) {
                    return dparts->partitions + i;
                }
            }
            return NULL;
        case PARG_SELECT_RELATIVE:
            if (modifier->select_relative >= 0) {
                if ((unsigned)modifier->select_relative + 1 > dparts->partitions_count) {
                    return NULL;
                }
                return dparts->partitions + modifier->select_relative;
            } else {
                if (abs(modifier->select_relative) > dparts->partitions_count) {
                    return NULL;
                }
                return dparts->partitions + dparts->partitions_count + modifier->select_relative;
            }
    }
    return NULL;
}

void
dts_dedit_report_failed_select(
    struct parg_modifier const * const modifier
){
    switch (modifier->select) {
        case PARG_SELECT_NAME:
            fprintf(stderr, "DTS deidt report failed select: No partition selected by Name selector %s\n", modifier->select_name);
            break;
        case PARG_SELECT_RELATIVE:
            fprintf(stderr, "DTS deidt report failed select: No partition selected by Relative selector %d\n", modifier->select_relative);
            break;
    }
}

int
dts_dedit_adjust(
    struct parg_modifier const * const          modifier,
    struct dts_partition_entry_simple * const   dpart
){
    
    if (modifier->modify_name == PARG_MODIFY_DETAIL_SET) {
        strncpy(dpart->name, modifier->name, MAX_PARTITION_NAME_LENGTH);
    }
    switch (modifier->modify_size) {
        case PARG_MODIFY_DETAIL_ADD:
            dpart->size += modifier->size;
            break;
        case PARG_MODIFY_DETAIL_SUBSTRACT:
            if (dpart->size < modifier->size) {
                return 1;
            }
            dpart->size -= modifier->size;
            break;
        case PARG_MODIFY_DETAIL_SET:
            dpart->size = modifier->size;
            break;
        default:
            break;
    }
    if (modifier->modify_masks == PARG_MODIFY_DETAIL_SET) {
        dpart->mask = modifier->masks;
    }
    return 0;
}

int
dts_dedit_place(
    struct parg_modifier const * const          modifier,
    struct dts_partitions_helper_simple * const dparts,
    struct dts_partition_entry_simple * const   dpart
){
    int place_target;
    switch (modifier->modify_place) {
        case PARG_MODIFY_PLACE_ABSOLUTE:
            if (modifier->place >= 0) {
                place_target = modifier->place;
            } else {
                place_target = dparts->partitions_count + modifier->place;
            }
            break;
        case PARG_MODIFY_PLACE_RELATIVE:
            place_target = dpart - dparts->partitions + modifier->place;
            break;
        default:
            fputs("DTS dedit place: Illegal place method\n", stderr);
            return -1;
    }
    if (place_target < 0 || (unsigned)place_target >= dparts->partitions_count) {
        fprintf(stderr, "DTS dedit place: Target place %i overflows (minumum 0 as start, maximum %u as end)\n", place_target, dparts->partitions_count);
        return 1;
    }
    struct dts_partition_entry_simple * const dpart_target = dparts->partitions + place_target;
    if (dpart_target > dpart) {
        struct dts_partition_entry_simple const dpart_buffer = *dpart;
        for (struct dts_partition_entry_simple *dpart_hot = dpart; dpart_hot < dpart_target; ++dpart_hot) {
            *(dpart_hot) = *(dpart_hot + 1);
        }
        *dpart_target = dpart_buffer;
    } else if (dpart_target < dpart) {
        struct dts_partition_entry_simple const dpart_buffer = *dpart;
        for (struct dts_partition_entry_simple *dpart_hot = dpart; dpart_hot > dpart_target; --dpart_hot) {
            *(dpart_hot) = *(dpart_hot + 1);
        }
        *dpart_target = dpart_buffer;
    }
    return 0;
}

int
dts_dedit_parse(
    struct dts_partitions_helper_simple * const dparts,
    int const                                   argc,
    char const * const * const                  argv
){
    if (argc <= 0 || !argv || !dparts || !dparts->partitions_count) {
        fputs("DTS dedit parse: Illegal arguments\n", stderr);
        return -1;
    }
    struct parg_editor_helper ehelper;
    if (parg_parse_dedit_mode(&ehelper, argc, argv)) {
        fputs("DTS dedit parse: Failed to parse argument\n", stderr);
        return 1;
    }
    if (!ehelper.count) {
        free(ehelper.editors);
        fputs("DTS dedit parse: No editor\n", stderr);
        return 2;
    }
    fputs("DTS dedit parse: Table before editting:\n", stderr);
    dts_report_partitions_simple(dparts);
    struct parg_editor *editor;
    struct parg_definer *definer;
    struct parg_modifier *modifier;
    struct dts_partition_entry_simple *dpart, *dpart_hot;
    for (unsigned i = 0; i < ehelper.count; ++i) {
        editor = ehelper.editors + i;
        if (editor->modify) {
            modifier = &editor->modifier;
            if (!(dpart = dts_dedit_part_select(modifier, dparts))) {
                dts_dedit_report_failed_select(modifier);
                fputs("DTS dedit parse: Failed selector\n", stderr);
                return 3;
            }
            switch (modifier->modify_part) {
                case PARG_MODIFY_PART_ADJUST:
                    if (dts_dedit_adjust(modifier, dpart)) {
                        fputs("DTS dedit parse: Failed to adjust partition detail\n", stderr);
                        return 4;
                    }
                    break;
                case PARG_MODIFY_PART_DELETE:
                    if (dparts->partitions_count == 0) {
                        return 1;
                    }
                    for (dpart_hot = dpart; dpart_hot - dparts->partitions < dparts->partitions_count - 1; ++dpart_hot) {
                        *dpart_hot = *(dpart_hot + 1);
                    }
                    --dparts->partitions_count;
                    break;
                case PARG_MODIFY_PART_CLONE:
                    if (dparts->partitions_count >= MAX_PARTITIONS_COUNT) {
                        fputs("DTS dedit parse: Trying to clone when partition count overflowed, refuse to continue\n", stderr);
                        return 1;
                    }
                    dpart_hot = dparts->partitions + dparts->partitions_count++;
                    *dpart_hot = *dpart;
                    strncpy(dpart_hot->name, modifier->name, MAX_PARTITION_NAME_LENGTH);
                    break;
                case PARG_MODIFY_PART_PLACE:
                    if (dts_dedit_place(modifier, dparts, dpart)) {
                        fputs("DTS dedit parse: Failed to place partition\n", stderr);
                        return 1;
                    }
                    break;
            }
        } else {
            definer = &editor->definer;
            dpart = dparts->partitions + dparts->partitions_count;
            if (++dparts->partitions_count > MAX_PARTITIONS_COUNT) {
                fputs("DTS dedit parse: Trying to define partition when partition count overflowed, refuse to continue\n", stderr);
                return 1;
            }
            strncpy(dpart->name, definer->name, MAX_PARTITION_NAME_LENGTH);
            dpart->size = definer->size;
            dpart->mask = definer->masks;
        }
    }
    free(ehelper.editors);
    fputs("DTS dedit parse: Table after editting:\n", stderr);
    dts_report_partitions_simple(dparts);
    return 0;
}