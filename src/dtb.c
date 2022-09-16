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

/* Function */

uint32_t
dtb_checksum(
    struct dtb_partition const * const  dtb
){
    uint32_t checksum = 0;
    uint32_t const * const dtb_as_uint = (uint32_t const *)dtb;
    for (uint32_t i=0; i<DTB_PARTITION_CHECKSUM_COUNT; ++i) {
        checksum += dtb_as_uint[i];
    }
    fprintf(stderr, "DTB checksum: Calculated %08x, Recorded %08x\n", checksum, dtb->checksum);
    return checksum;
}

static inline
struct dtb_header
dtb_header_swapbytes(
    struct dtb_header const * const dh
){
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

static inline
uint32_t
dtb_get_multi_header_property_length(
    struct dtb_multi_header const *const    header
){
    switch(header->version) {
        case 1:
            return 4;
        case 2:
            return 16;
        default:
            fprintf(stderr, "DTB parse multi entries: version not supported, only v1 and v2 supported yet the version is %"PRIu32"\n", header->version);
            return 0;
    }
}

static inline
void
dtb_pasre_multi_entries_each(
    uint8_t const * const                   dtb,
    struct dtb_multi_entries_helper *const  mhelper,
    uint32_t const                          len_property,
    uint32_t const                          i
){
    uint32_t const entry_offset = 12 + (len_property * 3 + 8) * i;
    uint32_t const prop_offset = entry_offset + len_property * 3;
    mhelper->entries[i].offset = *(const uint32_t *)(dtb + prop_offset);
    mhelper->entries[i].size = *(const uint32_t *)(dtb + prop_offset + 4);
    mhelper->entries[i].dtb = (uint8_t *)dtb + mhelper->entries[i].offset;
    char target[3][12] = {0};
    for (int j = 0; j < 3; ++j) {
        for (uint32_t k = 0; k < len_property; k += 4) {
            char *const str_hot = target[j] + k;
            char *const header_hot = (char *)dtb + entry_offset + len_property * j + k;
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

int
dtb_parse_multi_entries(
    struct dtb_multi_entries_helper * const mhelper,
    uint8_t const * const                   dtb
){
    if (!mhelper || !dtb) {
        return 1;
    }
    struct dtb_multi_header const *const header = (struct dtb_multi_header const *)dtb;
    if (header->magic != DTB_MAGIC_MULTI) {
        fputs("DTB parse multi entries: given dtb's magic is not correct\n", stderr);
        return 2;
    }
    uint32_t const len_property = dtb_get_multi_header_property_length(header);
    if (!len_property) {
        return 3;
    }
    mhelper->version = header->version;
    mhelper->entry_count = header->entry_count;
    mhelper->entries = malloc(sizeof(struct dtb_multi_entry) * mhelper->entry_count);
    if (!mhelper->entries) {
        fputs("DTB parse multi entries: failed to allocate memory for entries\n", stderr);
        return 4;
    }
    for (uint32_t i = 0; i<mhelper->entry_count; ++i) {
        dtb_pasre_multi_entries_each(dtb, mhelper, len_property, i);
    }
    return 0;
}

int
dtb_get_partitions(
    struct dts_partitions_helper * const    phelper,
    uint8_t const * const                   dtb, 
    size_t const                            size
){
    if (!dtb) {
        return 1;
    }
    struct dtb_header dh = dtb_header_swapbytes((struct dtb_header *)dtb);
    if (dh.off_dt_strings + dh.size_dt_strings > size) {
        fputs("DTB get partitions: dtb end point overflows\n", stderr);
        printf("End: %u, Size: %zu\n", dh.off_dt_strings + dh.size_dt_strings, size);
        return 2;
    }
    const uint8_t *const node = DTB_GET_PARTITIONS_NODE_FROM_DTS(dtb + dh.off_dt_struct, dh.size_dt_struct);
    if (!node) {
        fputs("DTB get partitions: partitions node does not exist in dtb\n", stderr);
        return 3;
    }
    struct stringblock_helper shelper;
    shelper.length = dh.size_dt_strings;
    shelper.allocated_length = dh.size_dt_strings;
    shelper.stringblock = (char *)(dtb + dh.off_dt_strings);
    if (!dts_get_partitions_from_node(phelper, node, &shelper)) {
        fputs("DTB get partitions: failed to get partitions\n", stderr);
        return 4;
    }
    dts_sort_partitions(phelper);
    return 0;
}

enum dtb_type
dtb_identify_type(
    uint8_t const * const dtb
){
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

struct dts_phandle_list *
dtb_get_phandles(
    uint8_t const * const   dtb,
    size_t const            size
){
    struct dtb_header dh = dtb_header_swapbytes((struct dtb_header *)dtb);
    if (dh.totalsize > size) {
        return NULL;
    }
    uint32_t const *current = (uint32_t const*)(dtb + dh.off_dt_struct);
    uint32_t max_offset = dh.size_dt_struct;
    while (*current == DTS_NOP_ACTUAL) {
        ++current;
        max_offset -= 4;
    }
    if (*current != DTS_BEGIN_NODE_ACTUAL) {
        fputs("DTS get phandles: Root node does not start properly", stderr);
        return NULL;
    }
    off_t const offset_phandle = stringblock_find_string_raw((const char*)dtb + dh.off_dt_strings, dh.size_dt_strings, "phandle");
    if (offset_phandle < 0) {
        return NULL;
    }
    off_t const offset_linux_phandle = stringblock_find_string_raw((const char*)dtb + dh.off_dt_strings, dh.size_dt_strings, "linux,phandle");
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

uint8_t *
dtb_partition_choose_correct(
    uint8_t *const buffer
){
    struct dtb_partition const *dtb = (struct dtb_partition const*)buffer;
    if (dtb_checksum(dtb) == dtb->checksum) {
        fputs("DTB read: Using first 256K in DTB partition\n", stderr);
        return buffer;
    }
    ++dtb;
    if (dtb_checksum(dtb) == dtb->checksum) {
        fputs("DTB read: Using second 256K in DTB partition\n", stderr);
        return buffer + DTB_PARTITION_SIZE;
    }
    fputs("DTB read: Both copies in DTB partition invalid, using first one\n", stderr);
    return buffer;
}

uint8_t *
dtb_identify_and_redirect_buffer(
    struct dtb_buffer_helper *const bhelper,
    uint8_t       * const           buffer,
    size_t const                    size_max
){
    if (!buffer || !buffer[0]) {
        return NULL;
    }
    switch (dtb_identify_type(buffer)) {
        case DTB_TYPE_PLAIN:
            bhelper->type_main = DTB_TYPE_PLAIN;
            return buffer;
        case DTB_TYPE_MULTI:
            bhelper->type_main = DTB_TYPE_MULTI;
            return buffer;
        case DTB_TYPE_GZIPPED: {
            bhelper->type_main = DTB_TYPE_GZIPPED;
            uint8_t *gbuffer;
            if (!gzip_unzip(buffer, size_max, &gbuffer)) {
                fputs("DTB read partitions and report: Failed to unzipped gzipped DTB\n", stderr);
                return NULL;
            }
            switch (dtb_identify_type(gbuffer)) {
                case DTB_TYPE_PLAIN:
                    bhelper->type_sub = DTB_TYPE_PLAIN;
                    break;
                case DTB_TYPE_MULTI:
                    bhelper->type_sub = DTB_TYPE_MULTI;
                    break;
                default:
                    fputs("DTB read partitions and report: Gzipped DTB does not contain valid plain/multi DTB\n", stderr);
                    free(gbuffer);
                    return NULL;
            }
            return gbuffer;
        }
        default:
            fprintf(stderr, "DTB read partitions and report: Unrecognizable DTB, magic %08x\n", *(uint32_t *)buffer);
            return NULL;
    }
}

static inline
size_t
dtb_get_size(
    uint8_t const * const   dtb
){
    return bswap_32(((struct dtb_header *)dtb)->totalsize);
}

static inline
uint8_t *
dtb_get_dts(
    uint8_t const * const   dtb
){
    return (uint8_t *)dtb + bswap_32(((struct dtb_header *)dtb)->off_dt_struct);
}

static inline
uint32_t
dtb_get_dts_size(
    uint8_t const * const   dtb
){
    return bswap_32(((struct dtb_header *)dtb)->size_dt_struct);
}

static inline
void
dtb_complete_stringblock_helper(
    uint8_t const * const       dtb,
    struct stringblock_helper * shelper
){
    struct dtb_header const *const dh = (struct dtb_header *)dtb;
    shelper->stringblock = (char *)dtb + bswap_32(dh->off_dt_strings);
    shelper->length = bswap_32(dh->size_dt_strings);
    shelper->allocated_length = shelper->length;
}

static inline
int
dtb_get_target(
    uint8_t const * const   dtb,
    char * const            target
){
    struct stringblock_helper shelper;
    dtb_complete_stringblock_helper(dtb, &shelper);
    off_t off_target = stringblock_find_string(&shelper, "amlogic-dt-id");
    if (off_target < 0) {
        return 1;
    }
    uint8_t const * const dts = dtb_get_dts(dtb);
    uint32_t const size_dts = dtb_get_dts_size(dtb);
    uint8_t const * node = dts_get_node_from_path(dts, size_dts, "/", 1);
    if (!node) {
        return 2;
    }
    dts_get_property_string(node, off_target, target, 36);
    return 0;
}

static inline
int
dtb_entry_split_target_string(
    struct dtb_buffer_entry * const entry
){
    char buffer[36] = {0};
    strncpy(buffer, entry->target, 36);
    char *key[3] = {buffer, NULL};
    unsigned key_id = 0;
    for (unsigned i = 0; i < 36; ++i) {
        if (buffer[i] == '_') {
            buffer[i] = '\0';
            key[++key_id] = buffer + i + 1;
        }
        if (key_id == 2) {
            break;
        }
    }
    if (key_id != 2) {
        return 1;
    }
    strncpy(entry->soc, key[0], 12);
    strncpy(entry->platform, key[1], 12);
    strncpy(entry->variant, key[2], 12);
    return 0;
}

static inline
int
dtb_parse_entry(
    struct dtb_buffer_entry * const entry,
    uint8_t const * const           buffer
){
    entry->size = dtb_get_size(buffer);
    entry->buffer = malloc(entry->size);
    if (!entry->buffer) {
        return 1;
    }
    memcpy(entry->buffer, buffer, entry->size);
    if (dtb_get_target(entry->buffer, entry->target)) {
        return 2;
    }
    if (dtb_entry_split_target_string(entry)) {
        return 3;
    }
    if (!dtb_get_partitions(&(entry->phelper), entry->buffer, entry->size)) {
        // free(entry->buffer);
        return -1;
    }
    return 0;
}

static inline
int
dtb_get_read_size(
    bool const      checksum,
    size_t const    size_max,
    size_t          *size_read,
    size_t          *size_dtb
){
    if (checksum) {
        *size_read = DTB_PARTITION_SIZE * 2;
        if (*size_read > size_max) {
            fputs("DTB read partitions and report: Remaning data size smaller than minimum required for DTB checksum\n", stderr);
            return 1;
        }
        *size_dtb = DTB_PARTITION_DATA_SIZE;
    } else {
        if (size_max > DTB_PARTITION_DATA_SIZE) {
            *size_read = DTB_PARTITION_DATA_SIZE;
        } else {
            *size_read = size_max;
        }
        *size_dtb = *size_read;
    }
    return 0;
}

void
dtb_free_buffer_helper(
    struct dtb_buffer_helper * const  bhelper
){
    if (bhelper && bhelper->dtb_count) {
        for (unsigned i = 0; i < bhelper->dtb_count; ++i) {
            free(bhelper->dtbs[i].buffer);
        }
        free(bhelper->dtbs);
    }
}

int
dtb_read_into_buffer_helper(
    struct dtb_buffer_helper *  bhelper,
    int const                   fd,
    size_t const                size_max,
    bool const                  should_checksum
){
    if (!bhelper) {
        return 1;
    }
    memset(bhelper, 0, sizeof(struct dtb_buffer_helper));
    size_t size_read, size_dtb;
    if (dtb_get_read_size(should_checksum, size_max, &size_read, &size_dtb)) {
        fputs("DTB read into buffer helper: Failed to get size to read\n", stderr);
        return 2;
    }
    uint8_t *const buffer_read = malloc(size_read);
    if (!buffer_read) {
        fputs("DTB read into buffer helper: Failed to allocate memory for read buffer\n", stderr);
        return 3;
    }
    if (io_read_till_finish(fd, buffer_read, size_read)) {
        fputs("DTB read into buffer helper: Failed to read into buffer buffer\n", stderr);
        free(buffer_read);
        return 4;
    }
    uint8_t *buffer_use = should_checksum ? dtb_partition_choose_correct(buffer_read) : buffer_read;
    if (!(buffer_use = dtb_identify_and_redirect_buffer(bhelper, buffer_use, size_dtb))) {
        fputs("DTB read into buffer helper: Failed to identify DTB type\n", stderr);
        free(buffer_read);
        return 5;
    }
    if (bhelper->type_main == DTB_TYPE_MULTI || bhelper->type_sub == DTB_TYPE_MULTI) {
        struct dtb_multi_entries_helper mhelper;
        if (dtb_parse_multi_entries(&mhelper, buffer_use) || !mhelper.entry_count) {
            fputs("DTB read into buffer helper: Failed to get multi-DTB helper\n", stderr);
            free(buffer_read);
            return 6;
        }
        bhelper->dtb_count = mhelper.entry_count;
        if (!(bhelper->dtbs = malloc(sizeof(struct dtb_buffer_entry) * bhelper->dtb_count))) {
            fputs("DTB read into buffer helper: Failed to allocate memory for multi DTBs\n", stderr);
            free(buffer_read);
            return 7;
        }
        for (unsigned i = 0; i < bhelper->dtb_count; ++i) {
            if (dtb_parse_entry(bhelper->dtbs + i, mhelper.entries[i].dtb) > 0) {
                fprintf(stderr, "DTB read into buffer helper: Failed to parse entry %u of %u\n", i + 1, bhelper->dtb_count);
                for (unsigned j = 0; j < i; ++j) {
                    free(bhelper->dtbs[j].buffer);
                }
                free(bhelper->dtbs);
                free(buffer_read);
                return 8;
            }
            if (strncmp((bhelper->dtbs + i)->target, mhelper.entries[i].target, 36)) {
                fprintf(stderr, "DTB read into buffer helper: Target name in header is different from amlogic-dt-id in DTS: %s != %s\n", mhelper.entries[i].target, (bhelper->dtbs + i)->target);
                for (unsigned j = 0; j <= i; ++j) {
                    free(bhelper->dtbs[j].buffer);
                }
                free(bhelper->dtbs);
                free(buffer_read);
                return 9;
            }
        }
    } else {
        bhelper->dtb_count = 1;
        if (!(bhelper->dtbs = malloc(sizeof(struct dtb_buffer_entry)))) {
            fputs("DTB read into buffer helper: Failed to allocate memory for DTB\n", stderr);
            free(buffer_read);
            return 10;
        }
        if (dtb_parse_entry(bhelper->dtbs, buffer_use) > 0) {
            fputs("DTB read into buffer helper: Failed to parse the only entry\n", stderr);
            free(bhelper->dtbs);
            free(buffer_read);
            return 11;
        }
    }
    if (bhelper->type_main == DTB_TYPE_GZIPPED) {
        free(buffer_use);
    }
    free(buffer_read);
    return 0;
}

int
dtb_check_buffers_partitions(
    struct dtb_buffer_helper const * const  bhelper
){
    if (!bhelper || !bhelper->dtb_count) {
        return -1;
    }
    if (bhelper->dtb_count == 1) {
        if (!bhelper->dtbs->phelper.partitions_count) {
            return -2;
        }
        return 0;
    }
    struct dts_partitions_helper const *phelper_a, *phelper_b;
    struct dts_partition_entry const *entry_a, *entry_b;
    for (unsigned i = 0; i < bhelper->dtb_count; ++i) {
        phelper_a = &((bhelper->dtbs+i)->phelper);
        for (unsigned j = i; j < bhelper->dtb_count; ++j) {
            phelper_b = &((bhelper->dtbs+j)->phelper);
            if (phelper_a->partitions_count != phelper_b->partitions_count) {
                return 1;
            }
            if (!phelper_a->partitions_count) { // Essentially should mean both 0
                return -2;
            }
            if (phelper_a->partitions_count > MAX_PARTITIONS_COUNT) {
                return -2;
            }
            for (unsigned k = 0; k <phelper_a->partitions_count; ++k) {
                entry_a = phelper_a->partitions + k;
                entry_b = phelper_a->partitions + k;
                if (strncmp(entry_a->name, entry_b->name, MAX_PARTITION_NAME_LENGTH) || entry_a->size != entry_b->size || entry_a->mask != entry_b->mask) {
                    return 2;
                }
            }
        }
    }
    return 0;
}

int
dtb_read_into_buffer_helper_and_report(
    struct dtb_buffer_helper *  bhelper,
    int const                   fd,
    size_t const                size_max,
    bool const                  checksum
){
    if (!bhelper) {
        return 1;
    }
    if (dtb_read_into_buffer_helper(bhelper, fd, size_max, checksum)) {
        return 2;
    }
    for (unsigned i = 0; i < bhelper->dtb_count; ++i) {
        fprintf(stderr, "DTB read into buffer helper and report: DTB %u of %u", i + 1, bhelper->dtb_count);
        dts_report_partitions(&bhelper->dtbs[i].phelper);
    }
    return 0;
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
