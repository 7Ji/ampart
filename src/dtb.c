/* Self */

#include "dtb.h"

/* System */

#include <byteswap.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Local */

#include "common.h"
#include "dts.h"
#include "gzip.h"
#include "io.h"
#include "stringblock.h"
#include "util.h"

/* Definition */

#define DTB_PARTITION_CHECKSUM_COUNT    (DTB_PARTITION_SIZE - 4U) >> 2U
#define DTB_PAGE_SIZE                   0x800U
#define DTB_MULTI_HEADER_ENTRY_LENGTH_v2    (DTB_MULTI_HEADER_PROPERTY_LENGTH_V2 * 3 + 8)
#define DTB_PARTITION_VERSION           1
#define DTB_PARTITION_MAGIC             0x00447E41U
#define DTB_WEBREPORT_ARG_MAXLEN        0x800U

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
    prln_info("calculated %08x, recorded %08x", checksum, dtb->checksum);
    return checksum;
}

void
dtb_checksum_partition(
    struct dtb_partition * const  dtb
){
    dtb->checksum = 0;
    uint32_t const * const dtb_as_uint = (uint32_t const *)dtb;
    for (uint32_t i=0; i<DTB_PARTITION_CHECKSUM_COUNT; ++i) {
        dtb->checksum += dtb_as_uint[i];
    }
    prln_info("calculated %08x", dtb->checksum);
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
            return DTB_MULTI_HEADER_PROPERTY_LENGTH_V1;
        case 2:
            return DTB_MULTI_HEADER_PROPERTY_LENGTH_V2;
        default:
            prln_error("version not supported, only v1 and v2 supported yet the version is %"PRIu32"", header->version);
            return 0;
    }
}

static inline
void
dtb_parse_multi_entries_each(
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
    char target[3][DTB_MULTI_HEADER_PROPERTY_LENGTH_V2] = {0};
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
        for (unsigned k = 0; k < DTB_MULTI_HEADER_PROPERTY_LENGTH_V2; ++k) {
            if (target[j][k] == ' ') {
                target[j][k] = '\0';
            }
        }
    }
    strncpy(mhelper->entries[i].soc, target[0], DTB_MULTI_HEADER_PROPERTY_LENGTH_V2);
    strncpy(mhelper->entries[i].platform, target[1], DTB_MULTI_HEADER_PROPERTY_LENGTH_V2);
    strncpy(mhelper->entries[i].variant, target[2], DTB_MULTI_HEADER_PROPERTY_LENGTH_V2);
    snprintf(mhelper->entries[i].target, DTB_MULTI_TARGET_LENGTH_V2, "%s_%s_%s", target[0], target[1], target[2]);
    prln_info("entry %uth of %u, %s, for SoC %s, platform %s, variant %s", i+1, mhelper->entry_count, mhelper->entries[i].target, mhelper->entries[i].soc, mhelper->entries[i].platform, mhelper->entries[i].variant);
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
        prln_error("given dtb's magic is not correct, expecting %08x, got %08x", DTB_MAGIC_MULTI, header->magic);
        return 2;
    }
    uint32_t const len_property = dtb_get_multi_header_property_length(header);
    if (!len_property) {
        return 3;
    }
    mhelper->version = header->version;
    mhelper->entry_count = header->entry_count;
    mhelper->entries = malloc(mhelper->entry_count * sizeof *mhelper->entries);
    if (!mhelper->entries) {
        prln_error_with_errno("failed to allocate memory for entries");
        return 4;
    }
    for (uint32_t i = 0; i<mhelper->entry_count; ++i) {
        dtb_parse_multi_entries_each(dtb, mhelper, len_property, i);
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
        prln_error("dtb end point overflows, end: 0x%x, size: 0x%lx", dh.off_dt_strings + dh.size_dt_strings, size);
        return 2;
    }
    if (!(phelper->node = DTB_GET_PARTITIONS_NODE_FROM_DTS(dtb + dh.off_dt_struct, dh.size_dt_struct))) {
        prln_error("partitions node does not exist in dtb");
        return 3;
    }
    struct stringblock_helper const shelper = {
        .stringblock = (char *)(dtb + dh.off_dt_strings),
        .length = dh.size_dt_strings,
        .allocated_length = dh.size_dt_strings
    };
    if (dts_get_partitions_from_node(phelper, &shelper)) {
        prln_error("failed to get partitions");
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
            prln_error("DTB type invalid");
            return DTB_TYPE_INVALID;
    }
}

uint8_t *
dtb_partition_choose_correct(
    uint8_t *const buffer
){
    struct dtb_partition const *dtb = (struct dtb_partition const*)buffer;
    if (dtb_checksum(dtb) == dtb->checksum) {
        prln_info("using first 256K in DTB partition");
        return buffer;
    }
    ++dtb;
    if (dtb_checksum(dtb) == dtb->checksum) {
        prln_info("using second 256K in DTB partition");
        return buffer + DTB_PARTITION_SIZE;
    }
    prln_info("both copies in DTB partition invalid, using first one");
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
                prln_error("failed to unzipped gzipped DTB");
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
                    prln_error("gzipped DTB does not contain valid plain/multi DTB");
                    free(gbuffer);
                    return NULL;
            }
            return gbuffer;
        }
        default:
            prln_error("unrecognizable DTB, magic %08x. Maybe your DTB is encrypted? You can follow the method documented on my blog to try to decrypt it before using ampart: https://7ji.github.io/crack/2023/01/08/decrypt-aml-dtb.html", *(uint32_t *)buffer);
            return NULL;
    }
}

static inline
size_t
dtb_get_size(
    uint8_t const * const   dtb
){
    size_t const size = bswap_32(((struct dtb_header *)dtb)->totalsize);
    prln_info("size recorded in header is 0x%lx", size);
    return size;
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
    prln_info("target is %s", target);
    return 0;
}

static inline
int
dtb_entry_split_target_string(
    struct dtb_buffer_entry * const entry
){
    char buffer[DTB_MULTI_TARGET_LENGTH_V2] = {0};
    strncpy(buffer, entry->target, DTB_MULTI_TARGET_LENGTH_V2);
    char *key[3] = {buffer, NULL};
    unsigned key_id = 0;
    for (unsigned i = 0; i < DTB_MULTI_TARGET_LENGTH_V2; ++i) {
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
    strncpy(entry->soc, key[0], DTB_MULTI_HEADER_PROPERTY_LENGTH_V2);
    strncpy(entry->platform, key[1], DTB_MULTI_HEADER_PROPERTY_LENGTH_V2);
    strncpy(entry->variant, key[2], DTB_MULTI_HEADER_PROPERTY_LENGTH_V2);
    prln_info("soc %s, platform %s, variant %s", entry->soc, entry->platform, entry->variant);
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
        prln_error("failed to allocate memory for buffer");
        return 1;
    }
    memcpy(entry->buffer, buffer, entry->size);
    if (dtb_get_target(entry->buffer, entry->target)) {
        prln_error("failed get target name");
        return 2;
    }
    if (dtb_entry_split_target_string(entry)) {
        prln_error("failed split target string into SoC, platform and variant");
        return 3;
    }
    if (dtb_get_partitions(&(entry->phelper), entry->buffer, entry->size)) {
        prln_error("failed to get partitions in DTB");
        entry->has_partitions = false;
        return -1;
    }
    entry->has_partitions = true;
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
            prln_error("remaning data size smaller than minimum required for DTB checksum");
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
    if (bhelper && bhelper->dtb_count && bhelper->dtbs) {
        for (unsigned i = 0; i < bhelper->dtb_count; ++i) {
            if (bhelper->dtbs[i].buffer) {
                free(bhelper->dtbs[i].buffer);
            }
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
    memset(bhelper, 0, sizeof *bhelper);
    size_t size_read, size_dtb;
    if (dtb_get_read_size(should_checksum, size_max, &size_read, &size_dtb)) {
        prln_error("failed to get size to read");
        return 2;
    }
    uint8_t *const buffer_read = malloc(size_read);
    if (!buffer_read) {
        prln_error_with_errno("failed to allocate memory for read buffer");
        return 3;
    }
    if (io_read_till_finish(fd, buffer_read, size_read)) {
        prln_error("failed to read into buffer buffer");
        free(buffer_read);
        return 4;
    }
    uint8_t *buffer_use = should_checksum ? dtb_partition_choose_correct(buffer_read) : buffer_read;
    if (!(buffer_use = dtb_identify_and_redirect_buffer(bhelper, buffer_use, size_dtb))) {
        prln_error("failed to identify DTB type");
        free(buffer_read);
        return 5;
    }
    if (bhelper->type_main == DTB_TYPE_MULTI || bhelper->type_sub == DTB_TYPE_MULTI) {
        struct dtb_multi_entries_helper mhelper;
        if (dtb_parse_multi_entries(&mhelper, buffer_use) || !mhelper.entry_count) {
            prln_error("failed to get multi-DTB helper");
            free(buffer_read);
            return 6;
        }
        bhelper->dtb_count = mhelper.entry_count;
        bhelper->multi_version = mhelper.version;
        if (!(bhelper->dtbs = malloc(bhelper->dtb_count * sizeof *bhelper->dtbs))) {
            prln_error_with_errno("failed to allocate memory for multi DTBs");
            free(buffer_read);
            return 7;
        }
        memset(bhelper->dtbs, 0, bhelper->dtb_count * sizeof *bhelper->dtbs);
        for (unsigned i = 0; i < bhelper->dtb_count; ++i) {
            if (dtb_parse_entry(bhelper->dtbs + i, mhelper.entries[i].dtb) > 0) {
                prln_error("failed to parse entry %u of %u", i + 1, bhelper->dtb_count);
                for (unsigned j = 0; j < i; ++j) {
                    free(bhelper->dtbs[j].buffer);
                }
                free(bhelper->dtbs);
                free(buffer_read);
                bhelper->dtbs = NULL;
                bhelper->dtb_count = 0;
                return 8;
            }
            if (strncmp((bhelper->dtbs + i)->target, mhelper.entries[i].target, DTB_MULTI_HEADER_PROPERTY_LENGTH_V2 * 3)) {
                prln_error("target name in header is different from amlogic-dt-id in DTS: %s != %s", mhelper.entries[i].target, (bhelper->dtbs + i)->target);
                for (unsigned j = 0; j <= i; ++j) {
                    free(bhelper->dtbs[j].buffer);
                }
                free(bhelper->dtbs);
                free(buffer_read);
                bhelper->dtbs = NULL;
                bhelper->dtb_count = 0;
                return 9;
            }
        }
    } else {
        bhelper->dtb_count = 1;
        if (!(bhelper->dtbs = malloc(sizeof *bhelper->dtbs))) {
            prln_error("failed to allocate memory for DTB");
            free(buffer_read);
            return 10;
        }
        if (dtb_parse_entry(bhelper->dtbs, buffer_use) > 0) {
            prln_error("failed to parse the only entry");
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
        prln_info("DTB %u of %u", i + 1, bhelper->dtb_count);
        if (bhelper->dtbs[i].has_partitions) {
            dts_report_partitions(&bhelper->dtbs[i].phelper);
        }
    }
    return 0;
}

// int dtb_replace_partitions() {
//     return 0;
// }

static inline
void
dtb_snapshot_decimal(
    struct dts_partitions_helper const * const phelper
){
    struct dts_partition_entry const *const part_start = phelper->partitions;
    struct dts_partition_entry const *part_current;
    uint32_t const pcount = util_safe_partitions_count(phelper->partitions_count);
    uint32_t const pcount_has_space = pcount - 1;
    prln_info("DTB snapshot decimal:");
    for (unsigned i = 0; i < pcount; ++i) {
        part_current = part_start + i;
        if (part_current->size == (uint64_t)-1) {
            printf("%s::-1:%u", part_current->name, part_current->mask);
        } else {
            printf("%s::%lu:%u", part_current->name, part_current->size, part_current->mask);
        }
        if (i != pcount_has_space) {
            fputc(' ', stdout);
        }
    }
    putc('\n', stdout);
}

static inline
void
dtb_snapshot_hex(
    struct dts_partitions_helper const * const phelper
){
    struct dts_partition_entry const *const part_start = phelper->partitions;
    struct dts_partition_entry const *part_current;
    uint32_t const pcount = util_safe_partitions_count(phelper->partitions_count);
    uint32_t const pcount_has_space = pcount - 1;
    prln_info("DTB snapshot hex:");
    for (unsigned i = 0; i < pcount; ++i) {
        part_current = part_start + i;
        if (part_current->size == (uint64_t)-1) {
            printf("%s::-1:%u", part_current->name, part_current->mask);
        } else {
            printf("%s::0x%lx:%u", part_current->name, part_current->size, part_current->mask);
        }
        if (i != pcount_has_space) {
            fputc(' ', stdout);
        }
    }
    putc('\n', stdout);
}

static inline
void
dtb_snapshot_human(
    struct dts_partitions_helper const * const phelper
){
    prln_info("DTB snapshot human:");
    struct dts_partition_entry const *const part_start = phelper->partitions;
    struct dts_partition_entry const *part_current;
    uint32_t const pcount = util_safe_partitions_count(phelper->partitions_count);
    uint32_t const pcount_has_space = pcount - 1;
    size_t size;
    char suffix;
    for (unsigned i = 0; i < pcount; ++i) {
        part_current = part_start + i;
        if (part_current->size == (uint64_t)-1) {
            printf("%s::-1:%u", part_current->name, part_current->mask);
        } else {
            size = util_size_to_human_readable_int(part_current->size, &suffix);
            printf("%s::%lu%c:%u", part_current->name, size, suffix, part_current->mask);
        }
        if (i != pcount_has_space) {
            fputc(' ', stdout);
        }
    }
    putc('\n', stdout);
}

int
dtb_snapshot(
    struct dtb_buffer_helper const * const  bhelper
){
    if (!bhelper || !bhelper->dtb_count || !bhelper->dtbs->phelper.partitions_count) {
        prln_error("DTB invalid");
        return 1;
    }
    struct dts_partitions_helper const *const phelper = &bhelper->dtbs->phelper;
    dtb_snapshot_decimal(phelper);
    dtb_snapshot_hex(phelper);
    dtb_snapshot_human(phelper);
    return 0;
}

int
dtb_webreport(
    struct dtb_buffer_helper const * const  bhelper,
    char * const                            arg_dsnapshot,
    uint32_t * const                        len_dsnapshot
){
    if (!bhelper || !bhelper->dtb_count || !bhelper->dtbs->phelper.partitions_count) {
        prln_error("DTB invalid");
        return 1;
    }
    char *current = arg_dsnapshot;
    uint32_t len_available = DTB_WEBREPORT_ARG_MAXLEN;
    // For each partition
    struct dts_partitions_helper const *const phelper = &bhelper->dtbs->phelper;
    uint32_t const pcount = util_safe_partitions_count(phelper->partitions_count);
    struct dts_partition_entry const *pentry;
    int len_this;
    bool started = false;
    for (uint32_t i = 0; i < pcount; ++i) {
        if (started) {
            if (len_available <= 4) {
                prln_error("not enough space for connector");
                return 2;
            }
            strncpy(current, "\%20", 3);
            current += 3;
            len_available -= 3;
        }
        pentry = phelper->partitions + i;
        len_this = 
            pentry->size == (uint64_t)-1 ?
                snprintf(current, len_available, "%s::-1:%u", pentry->name, pentry->mask) :
                snprintf(current, len_available, "%s::%lu:%u", pentry->name, pentry->size, pentry->mask);
        if (len_this >= 0 && (uint32_t)len_this >= len_available) {
            prln_error("argument truncated");
            return 3;
        } else if (len_this < 0) {
            prln_error("output error encountered");
            return 4;
        }
        len_available -= len_this;
        current += len_this;
        started = true;
    }
    *len_dsnapshot = DTB_WEBREPORT_ARG_MAXLEN - len_available;
    return 0;
}

int
dtb_buffer_entry_implement_partitions(
    struct dtb_buffer_entry * const                     new,
    struct dtb_buffer_entry const * const               old,
    struct dts_partitions_helper_simple const * const   phelper
){
    if (!old || !new || !phelper || !old->buffer || !phelper->partitions_count) {
        prln_error("invalid arguments");
        return -1;
    }
    struct dtb_header const dh = dtb_header_swapbytes((struct dtb_header *)old->buffer);
    struct stringblock_helper shelper = {
        .stringblock = (char *)old->buffer + dh.off_dt_strings,
        .length = dh.size_dt_strings,
        .allocated_length = dh.size_dt_strings
    };
    off_t const offset_phandle = stringblock_find_string(&shelper, "phandle");
    if (offset_phandle < 0) {
        prln_error("failed to get offset of phandle");
        return 2;
    }
    off_t const offset_linux_phandle = stringblock_find_string(&shelper, "linux,phandle");
    if (offset_linux_phandle < 0) {
        prln_warn("failed to get offset of linux,phandle");
    }
    struct dts_phandle_list plist = {0};
    if (dts_get_phandles(&plist, old->buffer + dh.off_dt_struct, dh.size_dt_struct, offset_phandle, offset_linux_phandle)) {
        prln_error("failed to get phandles");
        return 3;
    }
    if (old->has_partitions && dts_drop_partitions_phandles(&plist, &old->phelper)) {
        prln_error("failed to drop phandles occupied by partitions node");
        free(plist.entries);
        return 4;
    }
    shelper.allocated_length = util_nearest_upper_bound_with_multiply_long(shelper.length, 0x1000, 2);
    char *const sbuffer = malloc(shelper.allocated_length * sizeof *sbuffer);
    if (!sbuffer) {
        free(plist.entries);
        return 5;
    }
    memcpy(sbuffer, shelper.stringblock, shelper.length * sizeof *shelper.stringblock);
    memset(sbuffer + shelper.length, 0, (shelper.allocated_length - shelper.length) * sizeof *sbuffer);
    shelper.stringblock = sbuffer;
    uint8_t *node = NULL;
    size_t len_node = 0;
    if (dts_compose_partitions_node(&node, &len_node, &plist, phelper, &shelper, offset_phandle, offset_linux_phandle) || !node) {
        prln_error("failed to compose new partitions node");
        free(shelper.stringblock);
        free(plist.entries);
        return 5;
    }
    uint8_t *node_start, *end_start;
    size_t len_existing_node;
    if (old->phelper.node) {
        node_start = old->phelper.node - 4; // For the sake of DTS calling, it starts at the node's name instead of the BEGIN_NODE token, we get that 4 byte back here
        len_existing_node = dts_get_node_full_length(old->phelper.node, dh.size_dt_struct);
        end_start = node_start + len_existing_node;
    } else {
        node_start = old->buffer + dh.off_dt_struct + dh.size_dt_struct - 8; 
        // e.g. 0x38 start, 0x11368 size, then start + size is 0x113a0, this address is after the acutal end (essentially the start of DT sting, so we need to go backwards by 8 (pass 4 for END, pass 4 for END_NODE))
        if (*(uint32_t*)node_start != DTS_END_NODE_ACTUAL || *(uint32_t *)(node_start + 4) != DTS_END_ACTUAL) {
            free(node);
            free(shelper.stringblock);
            free(plist.entries);
            return 6;
        }
        len_existing_node = 0;
        end_start = node_start;
        /* Now we are at the last END_NODE, this is the END_NODE for the root node, as partitions is a direct child node under the root node, our START_NODE...END_NODE should write to here, and pushing the exising data starting with this END_NODE to the further beyond
        | - - - - | - - - - | - - - - | - - - - |
          . . . .   . . . .   END_ROOT     END
                                 ^
                                 |node_start
                                 v

        | - - - - | - - - - | - - - - | - - - - | - - - - | - - - - | - - - - |
          . . . .   . . . .  [BEGIN_NODE . . . .  END_NODE] END_ROOT    END
        */
    }
    size_t const size_before = node_start - old->buffer; // Before the new BEGIN_NODE token
    size_t const size_after = old->buffer + dh.off_dt_struct + dh.size_dt_struct - end_start;
    size_t const size_dt_struct = size_before - dh.off_dt_struct + size_after + len_node + 8;
    size_t const offset_dt_strings = dh.off_dt_strings + size_dt_struct - dh.size_dt_struct; 
    size_t const size_new = util_nearest_upper_bound_ulong(old->size - dh.size_dt_struct + size_dt_struct - dh.size_dt_strings + shelper.length, 4);
    prln_info("old DTB size 0x%lx, existing partitions node in it size is 0x%lx, DT struct offset 0x%x, size 0x%x; new node size 0x%lx, will insert between %p (off 0x%lx) and %p (off 0x%lx), size before insertion is 0x%lx, size after insertion is 0x%lx, size of new DT struct is 0x%lx, new DT strings offset 0x%lx, size 0x%lx; total size of new DTB is 0x%lx", old->size, len_existing_node, dh.off_dt_struct, dh.size_dt_struct, len_node, node_start, node_start - old->buffer, end_start, end_start - old->buffer, size_before, size_after, size_dt_struct, offset_dt_strings, shelper.length, size_new);
    uint8_t *dbuffer = malloc(size_new * sizeof *dbuffer);
    if (!dbuffer) {
        free(node);
        free(shelper.stringblock);
        free(plist.entries);
        return 7;
    }
    memset(dbuffer, 0, size_new * sizeof *dbuffer);
    uint8_t *offset_hot = dbuffer;
    memcpy(offset_hot, old->buffer, size_before);
    *(uint32_t *)(offset_hot += size_before) = DTS_BEGIN_NODE_ACTUAL;
    memcpy((offset_hot += 4), node, len_node);
    *(uint32_t *)(offset_hot += len_node) = DTS_END_NODE_ACTUAL;
    memcpy((offset_hot += 4), end_start, size_after);
    memcpy((offset_hot += size_after), shelper.stringblock, shelper.length);
    struct dtb_header *dh_new = (struct dtb_header *)dbuffer;
    dh_new->totalsize = bswap_32(size_new);
    dh_new->size_dt_struct = bswap_32(size_dt_struct);
    dh_new->off_dt_strings = bswap_32(offset_dt_strings);
    dh_new->size_dt_strings = bswap_32(shelper.length);
    free(node);
    free(shelper.stringblock);
    free(plist.entries);
    int r = dtb_parse_entry(new, dbuffer);
    free(dbuffer);
    if (r) {
        prln_error("failed to convert to buffer entry");
        return 8;
    }
    if (!new->has_partitions || !new->phelper.partitions_count) {
        prln_error("new buffer entry does not have partitions node, which is impossible");
        free(new->buffer);
        new->buffer = NULL;
        return 9;
    }
    if (dts_compare_partitions_mixed(&new->phelper, phelper)) {
        prln_error("reconstructed DTB yeilds different partitions:");
        free(new->buffer);
        new->buffer = NULL;
        return 10;
    }
    prln_error("constructed DTB yields same partitions, all good");
    return 0;
}

int
dtb_buffer_helper_not_all_have_buffer(
    struct dtb_buffer_helper const * const  bhelper
){
    if (!bhelper || !bhelper->dtb_count) {
        return -1;
    }
    int r = 0;
    for (unsigned i = 0; i < bhelper->dtb_count; ++i) {
        if (!bhelper->dtbs[i].buffer) {
            ++r;
        }
    }
    return r;
}

int
dtb_buffer_helper_implement_partitions(
    struct dtb_buffer_helper * const                    new,
    struct dtb_buffer_helper const * const              old,
    struct dts_partitions_helper_simple const * const   phelper
){
    if (!old || !new || !phelper || !old->dtb_count || dtb_buffer_helper_not_all_have_buffer(old) || !phelper->partitions_count) {
        prln_error("invalid arguments");
        return -1;
    }
    new->dtb_count = old->dtb_count;
    if (!(new->dtbs = malloc(new->dtb_count * sizeof *new->dtbs))) {
        prln_error("failed to allocate memory for entries");
        new->dtb_count = 0;
        return 1;
    }
    new->type_main = old->type_main;
    new->type_sub = old->type_sub;
    new->multi_version = old->multi_version;
    for (unsigned i = 0; i < new->dtb_count; ++i) {
        struct dtb_buffer_entry const *const entry_old = old->dtbs + i;
        struct dtb_buffer_entry *const entry_new = new->dtbs + i;
        if (dtb_buffer_entry_implement_partitions(entry_new, entry_old, phelper)) {
            prln_error("failed to implement new partitions into DTB %u of %u", i + 1, new->dtb_count);
            for (unsigned j = 0; j < i; ++j) {
                free((new->dtbs + j)->buffer);
            }
            return 2;
        }
    }
    return 0;
}

int
dtb_buffer_entry_remove_partitions(
    struct dtb_buffer_entry * const                     new,
    struct dtb_buffer_entry const * const               old
){
    if (!old || !new || !old->buffer) {
        prln_error("invalid arguments");
        return -1;
    }
    memset(new, 0, sizeof *new);
    if (!old->phelper.node) {
        if (!(new->buffer = malloc(old->size))) {
            return 1;
        }
        new->size = old->size;
        memcpy(new->buffer, old->buffer, new->size);
        memcpy(new->target, old->target, sizeof new->target);
        memcpy(new->soc, old->soc, sizeof new->soc);
        memcpy(new->platform, old->platform, sizeof new->platform);
        memcpy(new->variant, old->variant, sizeof new->variant);
        return 0;
    }
    struct dtb_header const dh = dtb_header_swapbytes((struct dtb_header *)old->buffer);
    uint8_t *const node_start = old->phelper.node - 4;
    size_t len_existing_node =  dts_get_node_full_length(old->phelper.node, dh.size_dt_struct);
    if (!len_existing_node) {
        return 2;
    }
    uint8_t *const end_start = node_start + len_existing_node;
    size_t const size_before = node_start - old->buffer;
    size_t const size_after = old->buffer + dh.off_dt_struct + dh.size_dt_struct - end_start;
    size_t const size_dt_struct = size_before - dh.off_dt_struct + size_after;
    size_t const offset_dt_strings = dh.off_dt_strings + size_dt_struct - dh.size_dt_struct; 
    size_t const size_new = util_nearest_upper_bound_ulong(old->size - dh.size_dt_struct + size_dt_struct, 4);
    uint8_t *dbuffer = malloc(size_new * sizeof *dbuffer);
    if (!dbuffer) {
        return 3;
    }
    memset(dbuffer, 0, size_new * sizeof *dbuffer);
    memcpy(dbuffer, old->buffer, size_before);
    memcpy(dbuffer + size_before, end_start, old->buffer + old->size - end_start);
    struct dtb_header *dh_new = (struct dtb_header *)dbuffer;
    dh_new->totalsize = bswap_32(size_new);
    dh_new->size_dt_struct = bswap_32(size_dt_struct);
    dh_new->off_dt_strings = bswap_32(offset_dt_strings);
    dh_new->size_dt_strings = bswap_32(dh.size_dt_strings);
    int r = dtb_parse_entry(new, dbuffer);
    free(dbuffer);
    if (r > 0) {
        prln_error("failed to convert to buffer entry");
        return 4;
    }
    if (new->has_partitions || new->phelper.partitions_count) {
        prln_error("result DTB has partitions, give up");
        free(new->buffer);
        new->buffer = NULL;
        return 5;
    }
    prln_error("constructed DTB does not have partition, all good");
    return 0;
}

int
dtb_buffer_helper_remove_partitions(
    struct dtb_buffer_helper * const                    new,
    struct dtb_buffer_helper const * const              old
){
    if (!old || !new || !old->dtb_count || dtb_buffer_helper_not_all_have_buffer(old)) {
        prln_error("invalid arguments");
        return -1;
    }
    new->dtb_count = old->dtb_count;
    if (!(new->dtbs = malloc(new->dtb_count * sizeof *new->dtbs))) {
        prln_error("failed to allocate memory for entries");
        new->dtb_count = 0;
        return 1;
    }
    new->type_main = old->type_main;
    new->type_sub = old->type_sub;
    new->multi_version = old->multi_version;
    for (unsigned i = 0; i < new->dtb_count; ++i) {
        struct dtb_buffer_entry const *const entry_old = old->dtbs + i;
        struct dtb_buffer_entry *const entry_new = new->dtbs + i;
        if (dtb_buffer_entry_remove_partitions(entry_new, entry_old)) {
            prln_error("failed to remove partitions into DTB %u of %u", i + 1, new->dtb_count);
            for (unsigned j = 0; j < i; ++j) {
                free((new->dtbs + j)->buffer);
            }
            return 2;
        }
    }
    return 0;
}

char
dtb_pedantic_multi_entry_char(
    char c
){
    if (c == '\0') {
        return ' ';
    } else {
        return c;
    }
}

int
dtb_combine_multi_dtb(
    uint8_t * * const dtb,
    size_t *size,
    struct dtb_buffer_helper const * const  bhelper
){
    if (!dtb || !size || !bhelper || !bhelper->dtb_count || !bhelper->multi_version || dtb_buffer_helper_not_all_have_buffer(bhelper)) {
        prln_error("illegal arguments");
        return -1;
    }
    size_t property_length;
    switch (bhelper->multi_version) {
        case 1:
            property_length = DTB_MULTI_HEADER_PROPERTY_LENGTH_V1;
            break;
        case 2:
            property_length = DTB_MULTI_HEADER_PROPERTY_LENGTH_V2;
            break;
        default:
            printf("%u", bhelper->multi_version);
            prln_error("illegal multi DTB version");
            return 1;
    }
    size_t const entry_length = property_length * 3 + 8;
    *size = util_nearest_upper_bound_ulong(12 + entry_length * bhelper->dtb_count, DTB_PAGE_SIZE);
    size_t *offsets = malloc(bhelper->dtb_count * sizeof *offsets);
    if (!offsets) {
        return 2;
    }
    size_t *sizes = malloc(bhelper->dtb_count * sizeof *offsets);
    if (!sizes) {
        free(offsets);
        return 3;
    }
    for (unsigned i = 0; i < bhelper->dtb_count; ++i) {
        offsets[i] = *size;
        sizes[i] = util_nearest_upper_bound_ulong(bhelper->dtbs[i].size, DTB_PAGE_SIZE);
        *size += sizes[i];
    }
    if (!(*dtb = malloc(*size * sizeof **dtb))) {
        free(sizes);
        free(offsets);
        return 3;
    }
    memset(*dtb, 0, *size);
    uint32_t *current = (uint32_t *)(*dtb);
    *(current++) = DTB_MAGIC_MULTI;
    *(current++) = bhelper->multi_version;
    *(current++) = bhelper->dtb_count;
    char *char_current;
    struct dtb_buffer_entry *bentry;
    char *properties[3];
    char *property;
    for (unsigned i = 0; i < bhelper->dtb_count; ++i) {
        bentry = bhelper->dtbs + i;
        properties[0] = bentry->soc;
        properties[1] = bentry->platform;
        properties[2] = bentry->variant;
        char_current = (char *)current;
        for (unsigned j = 0; j < 3; ++j) {
            property = properties[j];
            for (unsigned k = 0; k < property_length; k+=4) {
                *(char_current++) = dtb_pedantic_multi_entry_char(property[k+3]);
                *(char_current++) = dtb_pedantic_multi_entry_char(property[k+2]);
                *(char_current++) = dtb_pedantic_multi_entry_char(property[k+1]);
                *(char_current++) = dtb_pedantic_multi_entry_char(property[k]);
            }
        }
        current = (uint32_t *)char_current;
        *(current++) = offsets[i];
        *(current++) = sizes[i];
    }
    for (unsigned i = 0; i < bhelper->dtb_count; ++i) {
        bentry = bhelper->dtbs + i;
        memcpy(*dtb + offsets[i], bentry->buffer, bentry->size);
    }
    free(sizes);
    free(offsets);
    return 0;
}

int
dtb_compose(
    uint8_t * * const dtb,
    size_t *size,
    struct dtb_buffer_helper const * const bhelper,
    struct dts_partitions_helper_simple const * const   phelper
){
    if (!dtb || !bhelper || !bhelper->dtb_count || dtb_buffer_helper_not_all_have_buffer(bhelper)) {
        return -1;
    }
    *dtb = NULL;
    *size = 0;
    struct dtb_buffer_helper bhelper_new;
    if (phelper && phelper->partitions_count) {
        if (dtb_buffer_helper_implement_partitions(&bhelper_new, bhelper, phelper)) {
            prln_error("failed to implement partitions into DTB");
            return 1;
        }
    } else {
        if (dtb_buffer_helper_remove_partitions(&bhelper_new, bhelper)) {
            prln_error("failed to remove partitions from DTB");
            return 1;
        }
    }
    if (!bhelper_new.dtb_count) {
        prln_error("new DTB count 0, which is impossible, refuse to work");
        dtb_free_buffer_helper(&bhelper_new);
        return 2;
    }
    if (bhelper_new.dtb_count == 1) {
        if (!(*dtb = malloc(bhelper_new.dtbs->size * sizeof **dtb))) {
            prln_error("failed to duplicate plain DTB");
            dtb_free_buffer_helper(&bhelper_new);
            return 3;
        }
        memcpy(*dtb, bhelper_new.dtbs->buffer, bhelper_new.dtbs->size);
        *size = bhelper_new.dtbs->size;
    } else {
        if (dtb_combine_multi_dtb(dtb, size, &bhelper_new)) {
            prln_error("failed to compose multi-DTB");
            dtb_free_buffer_helper(&bhelper_new);
            return 4;
        }
    }
    if (*size > DTB_PARTITION_DATA_SIZE) {
        prln_error("DTB size too large (0x%lx), trying to gzip it", *size);
        uint8_t *buffer;
        if (!(*size = gzip_zip(*dtb, *size, &buffer))) {
            prln_error("failed to compose Gzipped multi-DTB");
            free(*dtb);
            dtb_free_buffer_helper(&bhelper_new);
            return 6;
        }
        if (*size > DTB_PARTITION_DATA_SIZE) {
            prln_error("gzipped DTB still too large (0x%lx), giving up", *size);
            free(buffer);
            free(*dtb);
            dtb_free_buffer_helper(&bhelper_new);
            return 6;
        }
        free(*dtb);
        *dtb = buffer;
    }
    dtb_free_buffer_helper(&bhelper_new);
    return 0;
}

void
dtb_finish_partition(
    struct dtb_partition * const    part
){
    part->magic = DTB_PARTITION_MAGIC;
    part->version = DTB_PARTITION_VERSION;
    part->timestamp = time(NULL);
    dtb_checksum_partition(part);
}

int
dtb_as_partition(
    uint8_t * * const   dtb,
    size_t * const      size
){
    if (!dtb || !*dtb || !size || !*size) {
        prln_error("illegal arguments");
        return -1;
    }
    if (*size > DTB_PARTITION_DATA_SIZE) {
        prln_error("data too large");
        return 1;
    }
    struct dtb_partition *part = malloc(2 * sizeof *part);
    if (!part) {
        prln_error("failed to allocate memory for DTB partition");
        return 2;
    }
    memcpy(part, *dtb, *size);
    memset((uint8_t *)part + *size, 0, sizeof *part - *size);
    dtb_finish_partition(part);
    memcpy(part + 1, part, sizeof *part);
    free(*dtb);
    *dtb = (uint8_t *)part;
    *size = 2 * sizeof *part;
    return 0;
}