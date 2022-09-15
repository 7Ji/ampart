/* Self */

#include "dtb.h"

/* System */

#include <byteswap.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* Local */

#include "dts.h"
#include "gzip.h"
#include "io.h"
#include "stringblock.h"
#include "util.h"

/* Definition */

#define DTB_PARTITION_CHECKSUM_COUNT   (DTB_PARTITION_SIZE - 4U) >> 2U

/* Macro */

#define DTB_GET_PARTITIONS_NODE_FROM_DTS(dts, max_offset) \
    dts_get_node_from_path(dts, max_offset, "/partitions", 11)

/* Struct */

struct
    dtb_buffer {
        uint8_t *res; // Free on this
        uint8_t *buffer;
        struct dts_partitions_helper *partitions;
    };

struct
    dtb_buffers {
        struct dtb_buffer *dtbs;
        unsigned dtb_count;
        enum dtb_type type_main;
        enum dtb_type type_sub;
    };

/* Variable */
struct dtb_buffers dtb_buffers = {0};

/* Function */

uint32_t dtb_checksum(const struct dtb_partition * const dtb) {
    uint32_t checksum = 0;
    const uint32_t *const dtb_as_uint = (const uint32_t *)dtb;
    for (uint32_t i=0; i<DTB_PARTITION_CHECKSUM_COUNT; ++i) {
        checksum += dtb_as_uint[i];
    }
    fprintf(stderr, "DTB checksum: Calculated %08x, Recorded %08x\n", checksum, dtb->checksum);
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
    mhelper->version = header->version;
    mhelper->entry_count = header->entry_count;
    mhelper->entries = malloc(sizeof(struct dtb_multi_entry) * mhelper->entry_count);
    if (!mhelper->entries) {
        fputs("DTB parse multi entries: failed to allocate memory for entries\n", stderr);
        free(mhelper);
        return NULL;
    }
    uint32_t entry_offset, prop_offset;
    char target[3][12] = {0};
    char *str_hot, *header_hot;
    for (uint32_t i = 0; i<mhelper->entry_count; ++i) {
        entry_offset = 12 + (len_property * 3 + 8) * i;
        prop_offset = entry_offset + len_property * 3;
        mhelper->entries[i].offset = *(const uint32_t *)(dtb + prop_offset);
        mhelper->entries[i].size = *(const uint32_t *)(dtb + prop_offset + 4);
        mhelper->entries[i].dtb = (uint8_t *)dtb + mhelper->entries[i].offset;
        memset(target, 0, 36);
        for (int j = 0; j < 3; ++j) {
            for (uint32_t k = 0; k < len_property; k += 4) {
                str_hot = target[j] + k;
                header_hot = (char *)dtb + entry_offset + len_property * j + k;
                if (*(uint32_t *)header_hot) {
                    str_hot[0] = header_hot[3];
                    str_hot[1] = header_hot[2];
                    str_hot[2] = header_hot[1];
                    str_hot[3] = header_hot[0];
                } else {
                    *(uint32_t *)str_hot = 0;
                }
            }
            for (int k = 0; k < 12; ++k) {
                if (target[j][k] == ' ') {
                    target[j][k] = '\0';
                }
            }
        }
        strncpy(mhelper->entries[i].soc, target[0], 12);
        strncpy(mhelper->entries[i].platform, target[1], 12);
        strncpy(mhelper->entries[i].variant, target[2], 12);
        snprintf(mhelper->entries[i].target, 36, "%s_%s_%s", target[0], target[1], target[2]);
        fprintf(stderr, "DTB parse multi entries: Entry %uth of %u, %s, for SoC %s, platform %s, variant %s\n", i+1, mhelper->entry_count, mhelper->entries[i].target, mhelper->entries[i].soc, mhelper->entries[i].platform, mhelper->entries[i].variant);
    }
    return mhelper;
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
    struct dts_partitions_helper *const phelper = dts_get_partitions_from_node(node, &shelper);
    if (!phelper) {
        fputs("DTB get partitions: failed to get partitions\n", stderr);
        return NULL;
    }
    dts_sort_partitions(phelper);
    return phelper;
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

struct dts_phandle_list *dtb_get_phandles(const uint8_t *const dtb, const size_t size) {
    struct dtb_header dh = dtb_header_swapbytes((struct dtb_header *)dtb);
    if (dh.totalsize > size) {
        return NULL;
    }
    const uint32_t *current = (const uint32_t *)(dtb + dh.off_dt_struct);
    uint32_t max_offset = dh.size_dt_struct;
    while (*current == DTS_NOP_ACTUAL) {
        ++current;
        max_offset -= 4;
    }
    if (*current != DTS_BEGIN_NODE_ACTUAL) {
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

uint32_t dtb_compare_partitions(struct dts_partitions_helper *phelper_a, struct dts_partitions_helper *phelper_b) {
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
        struct dts_partition_entry *part_a, *part_b;
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

int dtb_read_partitions_and_report(const int fd, const size_t size_max, const bool checksum) {
    size_t size_read, size_dtb;
    if (checksum) {
        size_read = DTB_PARTITION_SIZE * 2;
        if (size_read > size_max) {
            fputs("DTB read partitions and report: Remaning data size smaller than minimum required for DTB checksum\n", stderr);
            return 1;
        }
        size_dtb = DTB_PARTITION_DATA_SIZE;
    } else {
        if (size_max > DTB_PARTITION_DATA_SIZE) {
            size_read = DTB_PARTITION_DATA_SIZE;
        } else {
            size_read = size_max;
        }
        size_dtb = size_read;
    }
    uint8_t *buffer_read = malloc(size_read);
    if (!buffer_read) {
        fputs("DTB read partitions and report: Failed to allocate memroy\n", stderr);
        return 2;
    }
    if (io_read_till_finish(fd, buffer_read, size_read)) {
        fputs("DTB read partitions and report: Failed to read into buffer\n", stderr);
        free(buffer_read);
        return 3;
    }
    uint8_t *buffer;
    if (checksum) {
        const struct dtb_partition *dtb_partition_a = (const struct dtb_partition *)buffer_read;
        const struct dtb_partition *dtb_partition_b = dtb_partition_a + 1;
        bool correct_a = dtb_checksum(dtb_partition_a) == dtb_partition_a->checksum;
        bool correct_b = dtb_checksum(dtb_partition_b) == dtb_partition_b->checksum;
        if (correct_a) {
            fputs("DTB read partitions and report: Using first 256K in DTB partition\n", stderr);
            buffer = (uint8_t *)dtb_partition_a;
        } else if (correct_b) {
            fputs("DTB read partitions and report: Using second 256K in DTB partition\n", stderr);
            buffer = (uint8_t *)dtb_partition_b;
        } else {
            fputs("DTB read partitions and report: Both copies in DTB partition invalid, using first one\n", stderr);
            buffer = (uint8_t *)dtb_partition_a;
        }
    } else {
        buffer = buffer_read;
    }
    uint8_t *buffer_gzip = NULL;
    bool multi = false;
    switch (dtb_identify_type(buffer)) {
        case DTB_TYPE_PLAIN:
            break;
        case DTB_TYPE_MULTI:
            multi = true;
            break;
        case DTB_TYPE_GZIPPED:
            if (!gzip_unzip(buffer, size_dtb, &buffer_gzip)) {
                free(buffer_read);
                fputs("DTB read partitions and report: Failed to unzipped gzipped DTB\n", stderr);
                return 4;
            }
            buffer = buffer_gzip;
            switch (*(uint32_t *)buffer) {
                case DTB_MAGIC_PLAIN:
                    break;
                case DTB_MAGIC_MULTI:
                    multi = true;
                    break;
                default:
                    free(buffer_gzip);
                    free(buffer_read);
                    fputs("DTB read partitions and report: Gzipped DTB does not contain valid plain/multi DTB\n", stderr);
                    return 5;
            }
            break;
        default:
            fprintf(stderr, "DTB read partitions and report: Unrecognizable DTB, magic %08x\n", *(uint32_t *)buffer);
            free(buffer_read);
            return 6;
    }
    struct dts_partitions_helper *phelper;
    bool missing_partitions = false;
    uint32_t different_partitions = 0;
    if (multi) {
        struct dtb_multi_entries_helper *mhelper = dtb_parse_multi_entries(buffer);
        if (!mhelper) {
            free(buffer_read);
            if (buffer_gzip) {
                free(buffer_gzip);
            }
            fputs("DTB read partitions and report: Failed to parse multi DTB\n", stderr);
            return 7;
        }
        struct dts_partitions_helper *phelpers = malloc(sizeof(struct dts_partitions_helper) * mhelper->entry_count);
        if (!phelpers) {
            return 8;
        }
        memset(phelpers, 0, sizeof(struct dts_partitions_helper) * mhelper->entry_count);
        for (uint32_t i = 0; i < mhelper->entry_count; ++i) {
            fprintf(stderr, "DTB read partitions and report: parsing %uth of %u, for %s (SoC %s, platform %s, variant %s)\n", i+1, mhelper->entry_count, mhelper->entries[i].target, mhelper->entries[i].soc, mhelper->entries[i].platform, mhelper->entries[i].variant);
            if ((phelper = dtb_get_partitions(mhelper->entries[i].dtb, mhelper->entries[i].size))) {
                phelpers[i] = *phelper;
                dts_report_partitions(phelper);
                free(phelper);
            } else {
                fprintf(stderr, "DTB read partitions and report: Failed to read partitions in the %uth of %u DTB\n", i+1, mhelper->entry_count);
                missing_partitions = true;
            }
        }
        struct dts_partitions_helper *phelper_a;
        struct dts_partitions_helper *phelper_b;
        for (uint32_t i = 0; i < mhelper->entry_count; ++i) {
            phelper_a = phelpers + i;
            for (uint32_t j = i + 1; j < mhelper->entry_count; ++j) {
                phelper_b = phelpers + j;
                if (dtb_compare_partitions(phelper_a, phelper_b)) {
                    fprintf(stderr, "DTB read partitions and report: %dth DTB and %dth DTB are different:\n", i, j);
                    dts_report_partitions(phelper_a);
                    dts_report_partitions(phelper_b);
                    ++different_partitions;
                }   
            }
        }
        if (!different_partitions) {
            fputs("DTB read partitions and report: No different partitions\n", stderr);
        }
        free(phelpers);
    } else {
        if ((phelper = dtb_get_partitions(buffer, size_dtb))) {
            dts_report_partitions(phelper);
            free(phelper);
        } else {
            fputs("DTB read partitions and report: Failed to read partitions in the plain DTB\n", stderr);
            missing_partitions = true;
        }
    }
    free(buffer_read);
    if (buffer_gzip) {
        free(buffer_gzip);
    }
    return 0 - 128 * missing_partitions - different_partitions;
}

int dtb_replace_partitions() {
    return 0;
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
//             case DTS_BEGIN_NODE_ACTUAL:
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
            dts_report_partitions(phelper);
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
            dts_report_partitions(phelper);
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
