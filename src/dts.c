#include "dts.h"

#include <byteswap.h>
#include <string.h>

#include "util.h"

#define DTS_PARTITIONS_NODE_START_LENGTH    12U

static const uint8_t dts_partitions_node_start[DTS_PARTITIONS_NODE_START_LENGTH] = "partitions";

struct dts_stringblock_essential_offsets {
    off_t parts, pname, size, mask, phandle, linux_phandle;
};

struct {
    uint32_t len;
    const uint8_t *value;
} dts_property;

static uint32_t dts_skip_node(const uint8_t *const node, const uint32_t max_offset) {
    const uint32_t count = max_offset / 4;
    if (!count) {
        return 0;
    }
    const size_t len_name = strlen((const char *)node) + 1;
    const uint32_t *const start = (uint32_t *)node + len_name / 4 + (bool)(len_name % 4);
    const uint32_t *current;
    uint32_t len_prop;
    uint32_t offset_child;
    for (uint32_t i = 0; i < count; ++i) {
        current = start + i;
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
                return i + start - (const uint32_t *)node;
            case DTS_PROP_ACTUAL:
                len_prop = bswap_32(*(current+1));
                i += 2 + len_prop / 4;
                if (len_prop % 4) {
                    ++i;
                }
                break;
            case DTS_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "DTS skip node: Invalid token %"PRIu32"\n", bswap_32(*current));
                return 0;
        }
    }
    fputs("DTS skip node: Node not properly ended\n", stderr);
    return 0;
}

uint32_t dts_get_node(const uint8_t *const node, const uint32_t max_offset, const char *const name, const uint32_t layers, const uint8_t **const target) {
    const uint32_t count = max_offset / 4;
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
    const size_t len_name = strlen((const char *)node) + 1;
    const uint32_t *const start = (const uint32_t *)node + len_name / 4 + (bool)(len_name % 4);
    const uint32_t *current;
    uint32_t len_prop;
    uint32_t offset_child;
    for (uint32_t i = 0; i < count; ++i) {
        current = start + i;
        switch (*current) {
            case DTS_BEGIN_NODE_ACTUAL:
                offset_child = dts_get_node((const uint8_t *)(current + 1), max_offset - 4*i, name + len_name, layers - 1, target);
                if (offset_child) {
                    i += offset_child + 1;
                } else {
                    fputs("DTS get node: Failed to travel through child node\n", stderr);
                    return 0;
                }
                break;
            case DTS_END_NODE_ACTUAL:
                return i + start - (const uint32_t *)node;
            case DTS_PROP_ACTUAL:
                len_prop = bswap_32(*(current+1));
                i += 2 + len_prop / 4;
                if (len_prop % 4) {
                    ++i;
                }
                break;
            case DTS_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "DTS get node: Invalid token %"PRIu32"\n", bswap_32(*current));
                return 0;
        }
    }
    fputs("DTS get node: Node not properly ended\n", stderr);
    return 0;
}

static int dts_get_property_actual(const uint8_t *const node, const uint32_t property_offset) {
    const size_t len_name = strlen((const char *)node) + 1;
    const uint32_t *const start = (uint32_t *)node + len_name / 4 + (bool)(len_name % 4);
    const uint32_t *current;
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
                i += 2 + len_prop / 4;
                if (len_prop % 4) {
                    ++i;
                }
                break;
            case DTS_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "DTS get property: Invalid token %"PRIu32"\n", bswap_32(*current));
                return 1;
        }
    }
}

int dts_get_property(const uint8_t *const node, const struct stringblock_helper *const shelper, const char *const property) {
    const off_t property_offset = stringblock_find_string(shelper, property);
    if (property_offset < 0) {
        return 1;
    }
    return dts_get_property_actual(node, property_offset);
}

uint8_t *dts_get_node_from_path(const uint8_t *const dts, const uint32_t max_offset, const char *const path, const size_t len_path) {
    if (!dts) {
        fputs("DTS get node from path: No dtb to lookup\n", stderr);
        return NULL;
    }
    if (!max_offset) {
        fputs("DTS get node from path: Max offset invalid\n", stderr);
        return NULL;
    }
    if (max_offset % 4) {
        fputs("DTS get node from path: Offset is not multiply of 4\n", stderr);
        return NULL;
    }
    if (!path) {
        fputs("DTS get node from path: No path to lookup\n", stderr);
        return NULL;
    }
    if (!path[0]) {
        fputs("DTS get node from path: Empty path to lookup\n", stderr);
        return NULL;
    }
    if (path[0] != '/') {
        fputs("DTS get node from path: Path does not start with /\n", stderr);
        return NULL;
    }
    size_t len_path_actual;
    if (len_path) {
        len_path_actual = len_path;
    } else {
        if (!(len_path_actual = strlen(path))) { // This should not happen
            fputs("DTS get node from path: Empty path to lookup\n", stderr);
            return NULL;
        }
    }
    const uint32_t *current = (const uint32_t *)dts;
    while (*current == DTS_NOP_ACTUAL) {
        ++current;
    }
    if (*current != DTS_BEGIN_NODE_ACTUAL) {
        fputs("DTS get node from path: Node does not start properly", stderr);
        return NULL;
    }
    if (len_path_actual == 1) {
        fputs("DTS get node from path: Early quit for root node\n", stderr);
        return (uint8_t *)(current + 1);
    }
    char *const path_actual = strdup(path);
    if (!path_actual) {
        fputs("DTS get node from path: Failed to dup path\n", stderr);
        return NULL;
    }
    unsigned layers = 0;
    for (size_t i = 0; i<len_path_actual; ++i) {
        switch (path_actual[i]) {
            case '/':
                path_actual[i] = '\0';
                ++layers;
                break;
            case '\0': // This should not happend
                fputs("DTS get node from path: Path ends prematurely\n", stderr);
                free(path_actual);
                return NULL;
        }
    }
    uint8_t *target = NULL;
    uint32_t r = dts_get_node((const uint8_t *)(current + 1), max_offset - 4, path_actual, layers, (const uint8_t **)&target);
    free(path_actual);
    if (r) {
        return target;
    } else {
        fputs("DTS get node from path: Error occured, can not find\n", stderr);
        return NULL;
    }
}


static inline off_t dts_stringblock_essential_offset_get(const struct stringblock_helper *const shelper, const char *const name, uint32_t *const invalids) {
    const off_t offset = stringblock_find_string(shelper, name);
    if (offset < 0) {
        ++(*invalids);
        fprintf(stderr, "DTS stringblock essential offset: can not find %s in stringblock\n", name);
    }
    return offset;
}

struct dts_partitions_helper *dts_get_partitions_from_node(const uint8_t *const node, const struct stringblock_helper *const shelper) {
    if (memcmp(node, dts_partitions_node_start, DTS_PARTITIONS_NODE_START_LENGTH)) {
        fputs("DTB get partitions: node does not start properly\n", stderr);
        return NULL;
    }
    uint32_t offset_invalids = 0;
    uint32_t optional_invalids = 0;
    const struct dts_stringblock_essential_offsets offsets = {
        dts_stringblock_essential_offset_get(shelper, "parts", &offset_invalids),
        dts_stringblock_essential_offset_get(shelper, "pname", &offset_invalids),
        dts_stringblock_essential_offset_get(shelper, "size", &offset_invalids),
        dts_stringblock_essential_offset_get(shelper, "mask", &offset_invalids),
        dts_stringblock_essential_offset_get(shelper, "phandle", &offset_invalids),
        dts_stringblock_essential_offset_get(shelper, "linux,phandle", &optional_invalids)
    };
    if (offset_invalids) {
        return NULL;
    }
    struct dts_partitions_helper *const phelper = malloc(sizeof(struct dts_partitions_helper));
    if (!phelper) {
        fputs("DTB get partitions: failed to allocate memory for partitions helper\n", stderr);
        return NULL;
    }
    memset(phelper, 0, sizeof(struct dts_partitions_helper));
    const uint32_t *const start = (const uint32_t *)(node + DTS_PARTITIONS_NODE_START_LENGTH);
    const uint32_t *current;
    uint32_t len_prop, name_off;
    size_t len_node_name;
    bool in_partition = false;
    struct dts_partition_entry *partition = NULL;
    unsigned long phandle_id;
    for (uint32_t i = 0; ; ++i) {
        current = start + i;
        switch (*current) {
            case DTS_BEGIN_NODE_ACTUAL:
                if (in_partition) {
                    fputs("DTB get partitions: encountered sub node inside partition, which is impossible\n", stderr);
                    free(phelper);
                    return NULL;
                } else {
                    if (phelper->partitions_count == MAX_PARTITIONS_COUNT) {
                        fputs("DTB get partitions: partitions count exceeds maximum\n", stderr);
                        free(phelper);
                        return NULL;
                    }
                    partition = phelper->partitions + phelper->partitions_count++;
                    len_node_name = strlen((const char *)(current + 1));
                    if (len_node_name > 15) {
                        fprintf(stderr, "DTB get partitions: partition name '%s' too long\n", (const char *)(current + 1));
                        free(phelper);
                        return NULL;
                    }
                    i += (len_node_name + 1) / 4;
                    if ((len_node_name + 1) % 4) {
                        ++i;
                    }
                    strncpy(partition->name, (const char *)(current + 1), len_node_name);
                    in_partition = true;
                }
                break;
            case DTS_END_NODE_ACTUAL:
                if (in_partition) {
                    if (partition && partition->phandle && partition->linux_phandle && partition->phandle != partition->linux_phandle) {
                        fprintf(stderr, "DTB get partitions: partition '%s' has different phandle (%"PRIu32") and linux,phandle (%"PRIu32")\n", partition->name, partition->phandle, partition->linux_phandle);
                        free(phelper);
                        return NULL;
                    }
                    in_partition = false;
                } else {
                    return phelper;
                }
                break;
            case DTS_PROP_ACTUAL:
                len_prop = bswap_32(*(current+1));
                name_off = bswap_32(*(current+2));
                i += 2 + len_prop / 4;
                if (len_prop % 4) {
                    ++i;
                }
                if (in_partition) {
                    if (name_off == offsets.pname) {
                        if (len_prop > 16) {
                            fprintf(stderr, "DTB get partitions: partition name '%s' too long\n", (const char *)(current + 3));
                            free(phelper);
                            return NULL;
                        }
                        if (strcmp(partition->name, (const char *)(current + 3))) {
                            fprintf(stderr, "DTB get partitions: pname property %s different from partition node name %s\n", (const char *)(current + 3), partition->name);
                            free(phelper);
                            return NULL;
                        }
                    } else if (name_off == offsets.size) {
                        if (len_prop == 8) {
                            partition->size = ((uint64_t)bswap_32(*(current+3)) << 32) | (uint64_t)bswap_32(*(current+4));
                        } else {
                            fputs("DTB get partitions: partition size is not of length 8\n", stderr);
                            free(phelper);
                            return NULL;
                        }
                    } else if (name_off == offsets.mask) {
                        if (len_prop == 4) {
                            partition->mask = bswap_32(*(current+3));
                        } else {
                            fputs("DTB get partitions: partition mask is not of length 4\n", stderr);
                            free(phelper);
                            return NULL;
                        }
                    } else if (name_off == offsets.phandle) {
                        if (len_prop == 4) {
                            partition->phandle = bswap_32(*(current+3));
                        } else {
                            fputs("DTB get partitions: partition phandle is not of length 4\n", stderr);
                            free(phelper);
                            return NULL;
                        }
                    } else if (name_off == offsets.linux_phandle) {
                        if (len_prop == 4) {
                            partition->linux_phandle = bswap_32(*(current+3));
                        } else {
                            fputs("DTB get partitions: partition phandle is not of length 4\n", stderr);
                            free(phelper);
                            return NULL;
                        }
                    } else {
                        fprintf(stderr, "DTB get partitions: invalid property for partition, ignored: %s\n", shelper->stringblock+name_off);
                        free(phelper);
                        return NULL;
                    }
                } else {
                    if (len_prop == 4) {
                        if (name_off == offsets.parts) {
                            phelper->record_count = bswap_32(*(current+3));
                        } else if (name_off == offsets.phandle) {
                            phelper->phandle_root = bswap_32(*(current+3));
                        } else if (name_off == offsets.linux_phandle) {
                            phelper->linux_phandle_root = bswap_32(*(current+3));
                        } else {
                            if (strncmp(shelper->stringblock + name_off, "part-", 5)) {
                                fprintf(stderr, "DTB get partitions: invalid propertey '%s' in partitions node\n", shelper->stringblock + name_off);
                                free(phelper);
                                return NULL;
                            } else {
                                phandle_id = strtoul(shelper->stringblock + name_off + 5, NULL , 10);
                                if (phandle_id > MAX_PARTITIONS_COUNT - 1) {
                                    fprintf(stderr, "DTB get partitions: invalid part id %lu in partitions node\n", phandle_id);
                                    free(phelper);
                                    return NULL;
                                }
                                phelper->phandles[phandle_id] = bswap_32(*(current+3));
                            }
                        }
                    } else {
                        fprintf(stderr, "DTB get partitions: %s property of partitions node is not of length 4\n", shelper->stringblock + name_off);
                        free(phelper);
                        return NULL;
                    }
                }
                break;
            case DTS_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "DTB get partitions: invalid token 0x%08x\n", bswap_32(*current));
                free(phelper);
                return NULL;
        }
    }
}

int dts_sort_partitions(struct dts_partitions_helper *const phelper) {
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
    fputs("DTB sort partitions: partitions now in part-num order defined in partitions node's properties\n", stderr);
    return 0;
}

void dts_report_partitions(const struct dts_partitions_helper *const phelper) {
    fprintf(stderr, "DTB report partitions: %u partitions in the DTB:\n=======================================================\nID| name            |            size|(   human)| masks\n-------------------------------------------------------\n", phelper->partitions_count);
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

uint32_t dts_get_phandles_recursive(const uint8_t *const node, const uint32_t max_offset, const uint32_t offset_phandle, const uint32_t offset_linux_phandle, struct dts_phandle_list *plist) {
    const uint32_t count = max_offset / 4;
    if (!count) {
        return 0;
    }
    const size_t len_name = strlen((const char *)node) + 1;
    const uint32_t *const start = (uint32_t *)node + len_name / 4 + (bool)(len_name % 4);
    const uint32_t *current;
    uint32_t len_prop, name_off;
    uint32_t offset_child;
    uint32_t phandle;
    uint8_t *buffer;
    for (uint32_t i = 0; i < count; ++i) {
        current = start + i;
        switch (*current) {
            case DTS_BEGIN_NODE_ACTUAL:
                offset_child = dts_get_phandles_recursive((const uint8_t *)(current + 1), max_offset - 4*i, offset_phandle, offset_linux_phandle, plist);
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
                len_prop = bswap_32(*(current+1));
                name_off = bswap_32(*(current+2));
                i += 2 + len_prop / 4;
                if (len_prop % 4) {
                    ++i;
                }
                if (name_off == offset_phandle || name_off == offset_linux_phandle) {
                    if (len_prop == 4) {
                        phandle = bswap_32(*(current+3));
                        while (phandle >= plist->allocated_count) {
                            buffer = realloc(plist->phandles, sizeof(uint8_t)*plist->allocated_count*2);
                            if (buffer) {
                                memset(buffer + plist->allocated_count, 0, sizeof(uint8_t)*plist->allocated_count);
                                plist->phandles = buffer;
                            } else {
                                fputs("DTS get phandles recursive: Failed to re-allocate memory\n", stderr);
                                return 0;
                            }
                            plist->allocated_count *= 2;
                        }
                        ++(plist->phandles[phandle]);
                    } else {
                        fputs("DTS get phandles recursive: phandle not of length 4\n", stderr);
                        return 0;
                    }
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

int dts_phandle_list_finish(struct dts_phandle_list *const plist) {
    bool init = false;
    bool have_1 = false;
    bool have_2 = false;
    for (uint32_t i = 0; i < plist->allocated_count; ++i) {
        if (plist->phandles[i]) {
            switch (plist->phandles[i]) {
                case 1:
                    have_1 = true;
                    break;
                case 2:
                    have_2 = true;
                    break;
                default:
                    fprintf(stderr, "DTS phandle list finish: phandle %u/%x appears %u times, which is illegal\n", i, i, plist->phandles[i]);
                    return 1;
            }
            if (init)  {
                if (i<plist->min_phandle) {
                    plist->min_phandle = i;
                }
                if (i>plist->max_phandle) {
                    plist->max_phandle = i;
                }
            } else {
                init = true;
                plist->min_phandle = i;
                plist->max_phandle = i;
            }
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
