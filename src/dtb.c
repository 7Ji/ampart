#include "dtb_p.h"

uint32_t dtb_checksum(const struct dtb_partition * const dtb) {
    uint32_t checksum = 0;
    const uint32_t *const dtb_as_uint = (const uint32_t *)dtb;
    for (uint32_t i=0; i<DTB_PARTITION_CHECKSUM_COUNT; ++i) {
        checksum += dtb_as_uint[i];
    }
    return checksum;
}

static inline struct dtb_header dtb_header_swapbytes(const struct dtb_header *const dh) {
    struct dtb_header dh_new = {
        bswap_32(dh->magic),
        {bswap_32(dh->totalsize)},
        bswap_32(dh->off_dt_struct),
        {bswap_32(dh->off_dt_strings)},
        bswap_32(dh->off_mem_rsvmap),
        bswap_32(dh->version),
        bswap_32(dh->last_comp_version),
        bswap_32(dh->boot_cpuid_phys),
        bswap_32(dh->size_dt_strings),
        {bswap_32(dh->size_dt_struct)},
    };
    return dh_new;
}

static uint32_t dtb_skip_node(const uint8_t *const node, const uint32_t max_offset) {
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
            case DTB_FDT_BEGIN_NODE_ACTUAL:
                offset_child = dtb_skip_node((const uint8_t *)(current + 1), max_offset - 4*i);
                if (offset_child) {
                    i += offset_child + 1;
                } else {
                    fputs("DTB skip node: Failed to recursively skip child node\n", stderr);
                    return 0;
                }
                break;
            case DTB_FDT_END_NODE_ACTUAL:
                return i + start - (const uint32_t *)node;
            case DTB_FDT_PROP_ACTUAL:
                len_prop = bswap_32(*(current+1));
                i += 2 + len_prop / 4;
                if (len_prop % 4) {
                    ++i;
                }
                break;
            case DTB_FDT_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "DTB skip node: Invalid token %"PRIu32"\n", bswap_32(*current));
                return 0;
        }
    }
    fputs("DTB skip node: Node not properly ended\n", stderr);
    return 0;
}

uint32_t dtb_get_node(const uint8_t *const node, const uint32_t max_offset, const char *const name, const uint32_t layers, const uint8_t **const target) {
    const uint32_t count = max_offset / 4;
    if (!count) {
        return 0;
    }
    if (strcmp((const char *)node, name)) {
        return dtb_skip_node(node, max_offset);
    }
    if (!layers) {
        *target = node;
        return dtb_skip_node(node, max_offset);
    }
    const size_t len_name = strlen((const char *)node) + 1;
    const uint32_t *const start = (const uint32_t *)node + len_name / 4 + (bool)(len_name % 4);
    const uint32_t *current;
    uint32_t len_prop;
    uint32_t offset_child;
    for (uint32_t i = 0; i < count; ++i) {
        current = start + i;
        switch (*current) {
            case DTB_FDT_BEGIN_NODE_ACTUAL:
                offset_child = dtb_get_node((const uint8_t *)(current + 1), max_offset - 4*i, name + len_name, layers - 1, target);
                if (offset_child) {
                    i += offset_child + 1;
                } else {
                    fputs("DTB get node: Failed to travel through child node\n", stderr);
                    return 0;
                }
                break;
            case DTB_FDT_END_NODE_ACTUAL:
                return i + start - (const uint32_t *)node;
            case DTB_FDT_PROP_ACTUAL:
                len_prop = bswap_32(*(current+1));
                i += 2 + len_prop / 4;
                if (len_prop % 4) {
                    ++i;
                }
                break;
            case DTB_FDT_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "DTB get node: Invalid token %"PRIu32"\n", bswap_32(*current));
                return 0;
        }
    }
    fputs("DTB get node: Node not properly ended\n", stderr);
    return 0;
}

static int dtb_get_property_actual(const uint8_t *const node, const uint32_t property_offset) {
    const size_t len_name = strlen((const char *)node) + 1;
    const uint32_t *const start = (uint32_t *)node + len_name / 4 + (bool)(len_name % 4);
    const uint32_t *current;
    uint32_t len_prop, name_off;
    for (uint32_t i = 0; ; ++i) {
        current = start + i;
        switch (*current) {
            case DTB_FDT_BEGIN_NODE_ACTUAL:
                fputs("DTB get property actual: child node starts, property not found, give up\n", stderr);
                return 1;
            case DTB_FDT_END_NODE_ACTUAL:
                fputs("DTB get property actual: node ends, give up\n", stderr);
                return 1;
            case DTB_FDT_PROP_ACTUAL:
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
            case DTB_FDT_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "DTB get property: Invalid token %"PRIu32"\n", bswap_32(*current));
                return 1;
        }
    }
}

int dtb_get_property(const uint8_t *const node, const struct stringblock_helper *const shelper, const char *const property) {
    const off_t property_offset = stringblock_find_string(shelper, property);
    if (property_offset < 0) {
        return 1;
    }
    return dtb_get_property_actual(node, property_offset);
}

struct dtb_multi_entries_helper *dtb_parse_multi_entries(const uint8_t *const dtb) {
    const struct dtb_multi_header *const header = (const struct dtb_multi_header *)dtb;
    if (header->magic != DTB_MAGIC_MULTI) {
        fputs("DTB parse multi entries: given dtb's magic is not correct\n", stderr);
        return NULL;
    }
    uint32_t len_property;
    switch(header->version) {
        case 1:
            len_property = 4;
            break;
        case 2:
            len_property = 16;
            break;
        default:
            fprintf(stderr, "DTB parse multi entries: version not supported, only v1 and v2 supported yet the version is %"PRIu32"\n", header->version);
            return NULL;
    }
    struct dtb_multi_entries_helper *const mhelper = malloc(sizeof(struct dtb_multi_entries_helper));
    if (!mhelper) {
        fputs("DTB parse multi entries: failed to allocate memory for entries helper\n", stderr);
        return NULL;
    }
    mhelper->entry_count = header->entry_count;
    mhelper->entries = malloc(sizeof(struct dtb_multi_entry) * mhelper->entry_count);
    if (!mhelper->entries) {
        fputs("DTB parse multi entries: failed to allocate memory for entries\n", stderr);
        free(mhelper);
        return NULL;
    }
    uint32_t prop_offset;
    for (uint32_t i = 0; i<mhelper->entry_count; ++i) {
        prop_offset = 12 + (len_property * 3 + 8) * i + len_property * 3;
        mhelper->entries[i].offset = *(const uint32_t *)(dtb + prop_offset);
        mhelper->entries[i].size = *(const uint32_t *)(dtb + prop_offset + 4);
        mhelper->entries[i].dtb = (uint8_t *)dtb + mhelper->entries[i].offset;
    }
    return mhelper;
#if 0
    char soc_v1[4];
    char platform_v1[4];
    char variant_v1[4];
    char soc_v2[12];
    char platform_v2[12];
    char variant_v2[12];
    char *soc, *platform, *variant;
    if (header->version == 1) {
        soc = soc_v1;
        platform = platform_v1;
        variant = variant_v1;
    } else {
        soc = soc_v2;
        platform = platform_v2;
        variant = variant_v2;
    }
#endif
}

uint8_t *dtb_get_node_with_path_from_dts(const uint8_t *const dts, const uint32_t max_offset, const char *const path, const size_t len_path) {
    if (!dts) {
        fputs("DTB get node from path: No dtb to lookup\n", stderr);
        return NULL;
    }
    if (!max_offset) {
        fputs("DTB get node from path: Max offset invalid\n", stderr);
        return NULL;
    }
    if (max_offset % 4) {
        fputs("DTB get node from path: Offset is not multiply of 4\n", stderr);
        return NULL;
    }
    if (!path) {
        fputs("DTB get node from path: No path to lookup\n", stderr);
        return NULL;
    }
    if (!path[0]) {
        fputs("DTB get node from path: Empty path to lookup\n", stderr);
        return NULL;
    }
    if (path[0] != '/') {
        fputs("DTB get node from path: Path does not start with /\n", stderr);
        return NULL;
    }
    size_t len_path_actual;
    if (len_path) {
        len_path_actual = len_path;
    } else {
        if (!(len_path_actual = strlen(path))) { // This should not happen
            fputs("DTB get node from path: Empty path to lookup\n", stderr);
            return NULL;
        }
    }
    const uint32_t *current = (const uint32_t *)dts;
    while (*current == DTB_FDT_NOP_ACTUAL) {
        ++current;
    }
    if (*current != DTB_FDT_BEGIN_NODE_ACTUAL) {
        fputs("DTB get node from path: Node does not start properly", stderr);
        return NULL;
    }
    if (len_path_actual == 1) {
        fputs("DTB get node from path: Early quit for root node", stderr);
        return (uint8_t *)(current + 1);
    }
    char *const path_actual = strdup(path);
    if (!path_actual) {
        fputs("DTB get node from path: Failed to dup path\n", stderr);
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
                fputs("DTB get node from path: Path ends prematurely\n", stderr);
                free(path_actual);
                return NULL;
        }
    }
    uint8_t *target = NULL;
    uint32_t r = dtb_get_node((const uint8_t *)(current + 1), max_offset - 4, path_actual, layers, (const uint8_t **)&target);
    free(path_actual);
    if (r) {
        return target;
    } else {
        fputs("DTB get node from path: Error occured, can not find", stderr);
        return NULL;
    }
}

static inline off_t dtb_stringblock_essential_offset_get(const struct stringblock_helper *const shelper, const char *const name, uint32_t *const invalids) {
    const off_t offset = stringblock_find_string(shelper, name);
    if (offset < 0) {
        ++(*invalids);
        fprintf(stderr, "DTB stringblock essential offset: can not find %s in stringblock\n", name);
    }
    return offset;
}

struct dts_partitions_helper *dtb_get_partitions_from_node(const uint8_t *const node, const struct stringblock_helper *const shelper) {
    if (memcmp(node, dtb_partitions_node_start, DTB_PARTITIONS_NODE_START_LENGTH)) {
        fputs("DTB get partitions: node does not start properly\n", stderr);
        return NULL;
    }
    uint32_t offset_invalids = 0;
    uint32_t optional_invalids = 0;
    const struct dtb_stringblock_essential_offsets offsets = {
        dtb_stringblock_essential_offset_get(shelper, "parts", &offset_invalids),
        dtb_stringblock_essential_offset_get(shelper, "pname", &offset_invalids),
        dtb_stringblock_essential_offset_get(shelper, "size", &offset_invalids),
        dtb_stringblock_essential_offset_get(shelper, "mask", &offset_invalids),
        dtb_stringblock_essential_offset_get(shelper, "phandle", &offset_invalids),
        dtb_stringblock_essential_offset_get(shelper, "linux,phandle", &optional_invalids)
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
    const uint32_t *const start = (const uint32_t *)(node + DTB_PARTITIONS_NODE_START_LENGTH);
    const uint32_t *current;
    uint32_t len_prop, name_off;
    size_t len_node_name;
    bool in_partition = false;
    struct dts_partition_entry *partition = NULL;
    unsigned long phandle_id;
    for (uint32_t i = 0; ; ++i) {
        current = start + i;
        switch (*current) {
            case DTB_FDT_BEGIN_NODE_ACTUAL:
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
            case DTB_FDT_END_NODE_ACTUAL:
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
            case DTB_FDT_PROP_ACTUAL:
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
            case DTB_FDT_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "DTB get partitions: invalid token 0x%08x\n", bswap_32(*current));
                free(phelper);
                return NULL;
        }
    }
}

int dtb_sort_partitions(struct dts_partitions_helper *const phelper) {
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

struct dts_partitions_helper *dtb_get_partitions(const uint8_t *const dtb, const size_t size) {
    struct dtb_header dh = dtb_header_swapbytes((struct dtb_header *)dtb);
    if (dh.off_dt_strings + dh.size_dt_strings > size) {
        fputs("DTB get partitions: dtb end point overflows\n", stderr);
        printf("End: %u, Size: %zu\n", dh.off_dt_strings + dh.size_dt_strings, size);
        return NULL;
    }
    const uint8_t *const node = DTB_GET_PARTITIONS_NODE_FROM_DTS(dtb + dh.off_dt_struct, dh.size_dt_struct);
    if (!node) {
        fputs("DTB get partitions: partitions node does not exist in dtb\n", stderr);
        return NULL;
    }
    struct stringblock_helper shelper;
    shelper.length = dh.size_dt_strings;
    shelper.allocated_length = dh.size_dt_strings;
    shelper.stringblock = (char *)(dtb + dh.off_dt_strings);
    struct dts_partitions_helper *const phelper = dtb_get_partitions_from_node(node, &shelper);
    if (!phelper) {
        fputs("DTB get partitions: failed to get partitions\n", stderr);
        return NULL;
    }
    dtb_sort_partitions(phelper);
    return phelper;
}

void dtb_report_partitions(const struct dts_partitions_helper *const phelper) {
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

enum dtb_type dtb_identify_type(const uint8_t *const dtb) {
    switch (*(uint32_t *)dtb) {
        case DTB_MAGIC_PLAIN:
            return DTB_TYPE_PLAIN;
        case DTB_MAGIC_MULTI:
            return DTB_TYPE_MULTI;
        default:
            if (*(uint16_t *)dtb == GZIP_MAGIC) {
                return DTB_TYPE_GZIPPED;
            }
            fputs("DTB identify type: DTB type invalid\n", stderr);
            return DTB_TYPE_INVALID;
    }
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
            case DTB_FDT_BEGIN_NODE_ACTUAL:
                offset_child = dts_get_phandles_recursive((const uint8_t *)(current + 1), max_offset - 4*i, offset_phandle, offset_linux_phandle, plist);
                if (offset_child) {
                    i += offset_child + 1;
                } else {
                    fputs("DTS get phandles recursive: Failed to recursively get phandles from child node\n", stderr);
                    return 0;
                }
                break;
            case DTB_FDT_END_NODE_ACTUAL:
                return i + start - (const uint32_t *)node;
            case DTB_FDT_PROP_ACTUAL:
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
            case DTB_FDT_NOP_ACTUAL:
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
        // if (plist->have_linux_phandle) {
        //     fputs("DTS phandle list finish: All phandles appears once yet there is 'linux,phandle', which is illegal\n", stderr);
        //     return 4;
        // }
    } else if (have_2) {
        plist->have_linux_phandle = true;
        // if (!plist->have_linux_phandle) {
        //     fputs("DTS phandle list finish: All phandles appears more than once yet there is no 'linux,phandle', which is illegal\n", stderr);
        //     return 5;
        // }
    }
    return 0;
}

struct dts_phandle_list *dtb_get_phandles(const uint8_t *const dtb, const size_t size) {
    struct dtb_header dh = dtb_header_swapbytes((struct dtb_header *)dtb);
    if (dh.totalsize > size) {
        return NULL;
    }
    const uint32_t *current = (const uint32_t *)(dtb + dh.off_dt_struct);
    uint32_t max_offset = dh.size_dt_struct;
    while (*current == DTB_FDT_NOP_ACTUAL) {
        ++current;
        max_offset -= 4;
    }
    if (*current != DTB_FDT_BEGIN_NODE_ACTUAL) {
        fputs("DTS get phandles: Root node does not start properly", stderr);
        return NULL;
    }
    const off_t offset_phandle = stringblock_find_string_raw((const char*)dtb + dh.off_dt_strings, dh.size_dt_strings, "phandle");
    if (offset_phandle < 0) {
        return NULL;
    }
    const off_t offset_linux_phandle = stringblock_find_string_raw((const char*)dtb + dh.off_dt_strings, dh.size_dt_strings, "linux,phandle");
    struct dts_phandle_list *plist = malloc(sizeof(struct dts_phandle_list));
    if (!plist) {
        return NULL;
    }
    plist->phandles = malloc(sizeof(uint8_t) * 16);
    if (!plist->phandles) {
        free(plist);
        return NULL;
    }
    memset(plist->phandles, 0, sizeof(uint8_t) * 16);
    plist->allocated_count = 16;
    // plist->have_duplicate_phandle = false;
    plist->have_linux_phandle = false;
    if (!dts_get_phandles_recursive(
        (const uint8_t *)(current + 1), 
        max_offset, 
        (offset_phandle <= INT32_MAX) ? (uint32_t) offset_phandle : UINT32_MAX, // It's impossible to be smaller than 0
        (offset_linux_phandle >= 0 && offset_linux_phandle <= INT32_MAX) ? (uint32_t) offset_linux_phandle : UINT32_MAX,
        plist
    ) || dts_phandle_list_finish(plist)) {
        free(plist->phandles);
        free(plist);
        return NULL;
    }
    return plist;
}


// uint32_t enter_node(uint32_t layer, uint32_t offset_phandle, uint8_t *dtb, uint32_t offset, uint32_t end_offset, struct dtb_header *dh) {
//     if (offset >= end_offset) {
//         puts("ERROR: Trying to enter a node exceeding end point");
//         return 0;
//     }
//     // printf("Entering node at %"PRIu32" (%"PRIx32")\n", offset, offset);
//     uint32_t *start = (uint32_t *)(dtb + offset);
//     uint32_t *current;
//     uint32_t i;
//     uint32_t count = (end_offset - offset) / 4;
//     // printf("Will do scan for %"PRIu32" times\n", count);
//     uint32_t sub_length = 0;
//     uint32_t nameoff;
//     uint32_t len_prop;
//     size_t len_node_name;
//     char *node_name;
//     bool in_node = false;
//     for (i=0; i<count; ++i) { // Skip the first begin node 01
//         current = start + i;
//         switch (*current) {
//             case DTB_FDT_BEGIN_NODE_ACTUAL:
//                 if (in_node) {
//                     // puts("Encountered sub node");
//                     sub_length = enter_node(layer + 1,offset_phandle, dtb, offset+4*i, end_offset, dh);
//                     if (!sub_length) {
//                         puts("ERROR: sub node no length");
//                         return 0;
//                     }
//                     i += sub_length;
//                 } else {
//                     prepare_layer(layer);
//                     // puts("Node actual start");
//                     node_name = (char *)(current+1);
//                     printf("/%s\n", node_name);
//                     len_node_name = strlen(node_name);
//                     i += len_node_name / 4;
//                     if (len_node_name % 4) {
//                         i += 1;
//                     }
//                     in_node = true;
//                 }
//                 break;
//             case DTB_FDT_PROP_ACTUAL:
//                 len_prop = bswap_32(*(current+1));
//                 i += 2 + len_prop / 4;
//                 if (len_prop % 4) {
//                     i += 1;
//                 }
//                 prepare_layer(layer);
//                 nameoff = bswap_32(*(current+2));
//                 printf("->%s\n", dtb+(dh->off_dt_strings)+nameoff);
//                 if (nameoff == offset_phandle) {
//                     // prepare_layer(layer);
//                     printf("==>Phandle: %"PRIu32"\n", bswap_32(*(current+3)));
//                 }
//                 break;
//             case DTB_FDT_NOP_ACTUAL: // Nothing to do
//                 break;
//             case DTB_FDT_END_NODE_ACTUAL:
//                 if (in_node) {
//                     // printf("Exiting node at %"PRIx32"\n", offset+4*i);
//                     return i;
//                 } else {
//                     puts("Error: node ends without starting");
//                     return 0;
//                 }
//         }
//     }
//     return 0;
// }
#if 0
int main(int argc, char **argv) {
    if (argc <= 1) {
        puts("File not given");
        return 1;
    }
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("open(): Failed to open, errno: %d", errno);
        return 2;
    }
    struct stat st_dtb;
    if (fstat(fd, &st_dtb)) {
        printf("fstat(): Failed to stat file, errno: %d", errno);
        return 3;
    }
    printf("Size is %ld\n", st_dtb.st_size);
    unsigned long sz_dtb = st_dtb.st_size;
    uint8_t buffer[sz_dtb];
    if (read(fd, buffer, sz_dtb) - sz_dtb != 0) {
        printf("read(): Read length mismatch, errno: %d", errno);
        return 4;
    }
    close(fd);
    report_dtb_header((struct dtb_header *)buffer);
    struct dtb_header dh = dtb_header_swapbytes((struct dtb_header *)buffer);
    report_dtb_header_no_swap(&dh);
    if (dh.size_dt_struct % 4) {
        puts("Size of dt struct is not multiply of 4");
        return 5;
    }
    if (dh.off_dt_struct % 4) {
        puts("Offset of dt struct is not multiply of 4");
        return 6;
    }
    // uint32_t *dt_begin = (uint32_t *)(buffer + dh.off_dt_struct);
    // uint32_t *dt_hot;
    // // long diff;
    struct stringblock_helper shelper;
    shelper.length = dh.size_dt_strings;
    shelper.allocated_length = shelper.length;
    shelper.stringblock = (char *)(buffer + dh.off_dt_strings);
    // shelper.allocated_length = util_nearest_upper_bound_long(dh.size_dt_strings, 0x1000, 1);
    // shelper.stringblock = malloc(shelper.allocated_length);
    // if (!shelper.stringblock) {
    //     puts("Failed to allocate memory for stringblock");
    //     return 7;
    // }
    dtb_skip_node(buffer + dh.off_dt_struct + 4, dh.size_dt_struct - 4);
    uint8_t * r = dtb_get_node_from_path(buffer + dh.off_dt_struct, dh.size_dt_struct, "/partitions", 0);
    if (r){
        puts((const char *)r);
        printf("%lx\n", r - buffer);
        struct dts_partitions_helper *phelper = dtb_get_partitions(r, &shelper);
        if (phelper) {
            dtb_report_partitions(phelper);
            dtb_sort_partitions(phelper);
            // struct dts_partition_entry *part;
            // for (uint32_t i = 0; i<phelper->partitions_count; ++i) {
            //     part = phelper->partitions + i;
            //     printf("Part %u: name %s, size %lu, mask %u, phandle %u\n", i, part->name, part->size, part->mask, part->phandle);
            //     printf(" - The same id with phandle %u, ", phelper->phandles[i]);
            //     if (phelper->phandles[i] == part->phandle) {
            //         puts("same");
            //     } else {
            //         puts("different");
            //     }
            // }
            dtb_report_partitions(phelper);
            free(phelper);
        }
    }
    return 0;

    // shelper.allocated_length = dh.size_dt_strings;
    // shelper.stringblock = (char *)(buffer + dh.off_dt_strings);
    // off_t off_pname = stringblock_find_string(&shelper, "pname");
    // off_t off_size = stringblock_find_string(&shelper, "size");
    // off_t off_mask = stringblock_find_string(&shelper, "mask");
    // off_t off_phandle = stringblock_find_string(&shelper, "phandle");
    // printf("%lx, %lx, %lx, %lx\n", off_pname, off_size, off_mask, off_phandle);
    // uint32_t offset = 0;
    // uint8_t *partitions_p = dtb_get_node_from_path(buffer + dh.off_dt_struct, dh.size_dt_struct, "/partitions", 0, &offset);
    // if (partitions_p) {
    //     puts("Found");
    //     puts(partitions_p + 4);
    // } else {
    //     puts("Not found");
    // }


    


    return 0;
    // off_t offset = dtb_search_node(buffer, "vbmeta_system_", 0);
    // printf("Offset of partitions node %ld, %lx, %ld\n", offset, offset, dh.off_dt_struct+offset);
    // return 0;
    // unsigned int offset_phandle = find_phandle((char *)buffer + dh.off_dt_strings, dh.size_dt_strings);
    // printf("Offset of phandle is %u\n", offset_phandle);
    // if (!offset_phandle) {
    //     printf("Failed to find string offset for phandle");
    //     return 1;
    // }
    // enter_node(0, offset_phandle, buffer, dh.off_dt_struct, dh.off_dt_struct + dh.size_dt_struct, &dh);
    // for (unsigned long i=0; i<dh.size_dt_struct/4; ++i) {
    //     dt_hot = dt_begin + i;
    //     diff = (uint8_t *)dt_hot - buffer;
    //     switch (*dt_hot) {
    //         case FDT_BEGIN_NODE_ACTUAL:
    //             printf("Node beginning at offset %ld, %lx\n", diff, diff);
    //             break;
    //         case FDT_END_NODE_ACTUAL:
    //             printf("Node endding at offset %ld, %lx\n", diff, diff);
    //             break;
    //     }
    // }
    return 0;
}
#endif