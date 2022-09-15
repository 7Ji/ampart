/* Self */

#include "ept.h"

/* System */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* Local */

#include "cli.h"
#include "io.h"
#include "util.h"

/* Definition */

#define EPT_PART_MAXIMUM  32

#define EPT_PART_MASKS_BOOT   1
#define EPT_PART_MASKS_SYSTEM 2
#define EPT_PART_MASKS_DATA   4


#define EPT_HEADER_MAGIC_STRING         "MPT"
#define EPT_HEADER_VERSION_STRING       "01.00.00"
#define EPT_HEADER_VERSION_UINT32_0     (uint32_t)0x302E3130U
#define EPT_HEADER_VERSION_UINT32_1     (uint32_t)0x30302E30U
#define EPT_HEADER_VERSION_UINT32_2     (uint32_t)0x00000000U

/* Variable */

uint32_t const          ept_header_version_uint32[] = {
    EPT_HEADER_VERSION_UINT32_0,
    EPT_HEADER_VERSION_UINT32_1,
    EPT_HEADER_VERSION_UINT32_2
};

struct ept_table const  ept_table_empty = {
    {
        {
            { .magic_uint32 = EPT_HEADER_MAGIC_UINT32 },
            { .version_uint32 = {EPT_HEADER_VERSION_UINT32_0, EPT_HEADER_VERSION_UINT32_1, EPT_HEADER_VERSION_UINT32_2} },
            4U,
            0U
        }
    },
    {
        {
            {
                EPT_PARTITION_BOOTLOADER_NAME,
                EPT_PARTITION_BOOTLOADER_SIZE,
                0U,
                0U,
                0U
            },
            /*
            Layout of reserved partition:
            0x000000 - 0x003fff: partition table
            0x004000 - 0x03ffff: storage key area	(16k offset & 256k size)
            0x400000 - 0x47ffff: dtb area  (4M offset & 512k size)
            0x480000 - 64MBytes: resv for other usage.
            */
            {
                EPT_PARTITION_RESERVED_NAME,
                EPT_PARTITION_RESERVED_SIZE,
                0U,
                0U,
                0U
            },
            {
                EPT_PARTITION_CACHE_NAME,
                EPT_PARTITION_CACHE_SIZE,
                0U,
                0U,
                0U
            },
            {
                EPT_PARTITION_ENV_NAME,
                EPT_PARTITION_ENV_SIZE,
                0U,
                0U,
                0U
            },
            {{0U},0U,0U,0U,0U}
        }
    }
};

/* Function */

uint32_t
ept_checksum(
    struct ept_partition const * const  partitions, 
    int const                           partitions_count
){
    int i, j;
    uint32_t checksum = 0, *p;
    for (i = 0; i < partitions_count; i++) { // This is utterly wrong, but it's how amlogic does. So we have to stick with the glitch algorithm that only calculates 1 partition if we want ampart to work
        p = (uint32_t *)partitions;
        for (j = sizeof(struct ept_partition)/4; j > 0; --j) {
            checksum += *p;
            p++;
        }
    }
    return checksum;
}

int
ept_valid_header(
    struct ept_header const * const header
){
    int ret = 0;
    if (header->partitions_count > 32) {
        fprintf(stderr, "EPT valid header: Partitions count invalid, only integer 0~32 is acceppted, actual: %d\n", header->partitions_count);
        ++ret;
    }
    if (header->magic_uint32 != EPT_HEADER_MAGIC_UINT32) {
        fprintf(stderr, "EPT valid header: Magic invalid, expect: %"PRIx32", actual: %"PRIx32"\n", header->magic_uint32, EPT_HEADER_MAGIC_UINT32);
        ++ret;
    }
    bool version_invalid = false;
    for (unsigned short i=0; i<3; ++i) {
        if (header->version_uint32[i] != ept_header_version_uint32[i]) {
            version_invalid = true;
            break;
        }
    }
    if (version_invalid) {
        fprintf(stderr, "EPT valid header: Version invalid, expect (little endian) 0x%08x%08x%08x ("EPT_HEADER_VERSION_STRING"), actual: 0x%08"PRIx32"%08"PRIx32"%08"PRIx32"\n", EPT_HEADER_VERSION_UINT32_2, EPT_HEADER_VERSION_UINT32_1, EPT_HEADER_VERSION_UINT32_0, header->version_uint32[2], header->version_uint32[1], header->version_uint32[0]);
        ret += 1;
    }
    if (header->partitions_count < 32) { // If it's corrupted it may be too large
        const uint32_t checksum = ept_checksum(((const struct ept_table *)header)->partitions, header->partitions_count);
        if (header->checksum != checksum) {
            fprintf(stderr, "EPT valid header: Checksum mismatch, calculated: %"PRIx32", actual: %"PRIx32"\n", checksum, header->checksum);
            ret += 1;
        }
    }
    return ret;
}

unsigned int
ept_valid_partition_name(
    char const * const  name
){
    unsigned int ret = 0;
    unsigned int i;
    bool term = true;
    for (i = 0; i < 16; ++i) {
        switch (name[i]) {
            case '\0':
            case '-':
            case '0'...'9':
            case 'A'...'Z':
            case '_':
            case 'a'...'z':
                break;
            default:
                ++ret;
                break;
        }
        if (!name[i]) {
            break;
        }
    }
    if (i == 16 && name[15]) { // Does not properly end
        term = false;
    }
    if (ret) {
        fprintf(stderr, "EPT valid partition name: %u illegal characters found in partition name '%s'", ret, name);
        if (term) {
            fputc('\n', stderr);
        } else {
            fputs(", and it does not end properly\n", stderr);
        }
    }
    return ret;
}


int 
ept_valid_partition(
    struct ept_partition const * const  part
){
    if (ept_valid_partition_name(part->name)) {
        return 1;
    }
    return 0;
}

int 
ept_valid(
    struct ept_table const * const  table
){
    int ret = ept_valid_header((const struct ept_header *)table);
    const uint32_t valid_count = table->partitions_count < 32 ? table->partitions_count : 32;
    for (uint32_t i=0; i<valid_count; ++i) {
        ret += ept_valid_partition(table->partitions+i);
    }
    return ret;
}

void 
ept_report(
    struct ept_table const * const  table
){
    fprintf(stderr, "table report: %d partitions in the table:\n===================================================================================\nID| name            |          offset|(   human)|            size|(   human)| masks\n-----------------------------------------------------------------------------------\n", table->partitions_count);
    const struct ept_partition *part;
    double num_offset, num_size;
    char suffix_offset, suffix_size;
    uint64_t last_end = 0, diff;
    for (uint32_t i=0; i<table->partitions_count; ++i) {
        part = table->partitions + i;
        if (part->offset > last_end) {
            diff = part->offset - last_end;
            num_offset = util_size_to_human_readable(diff, &suffix_offset);
            fprintf(stderr, "    (GAP)                                        %16"PRIx64" (%7.2lf%c)\n", diff, num_offset, suffix_offset);
        } else if (part->offset < last_end) {
            diff = last_end - part->offset;
            num_offset = util_size_to_human_readable(diff, &suffix_offset);
            fprintf(stderr, "    (OVERLAP)                                    %16"PRIx64" (%7.2lf%c)\n", diff, num_offset, suffix_offset);
        }
        num_offset = util_size_to_human_readable(part->offset, &suffix_offset);
        num_size = util_size_to_human_readable(part->size, &suffix_size);
        fprintf(stderr, "%2d: %-16s %16"PRIx64" (%7.2lf%c) %16"PRIx64" (%7.2lf%c) %6"PRIu32"\n", i, part->name, part->offset, num_offset, suffix_offset, part->size, num_size, suffix_size, part->mask_flags);
        last_end = part->offset + part->size;
    }
    fputs("===================================================================================\n", stderr);
    return;
}

static inline
int 
ept_pedantic_offsets(
    struct ept_table * const    table,
    uint64_t const              capacity
){
    if (table->partitions_count < 4) {
        fputs("EPT pedantic offsets: refuse to fill-in offsets for heavily modified table (at least 4 partitions should exist)\n", stderr);
        return 1;
    }
    if (strncmp(table->partitions[0].name, EPT_PARTITION_BOOTLOADER_NAME, MAX_PARTITION_NAME_LENGTH) || strncmp(table->partitions[1].name, EPT_PARTITION_RESERVED_NAME, MAX_PARTITION_NAME_LENGTH) || strncmp(table->partitions[2].name, EPT_PARTITION_CACHE_NAME, MAX_PARTITION_NAME_LENGTH) || strncmp(table->partitions[3].name, EPT_PARTITION_ENV_NAME, MAX_PARTITION_NAME_LENGTH)) {
        fprintf(stderr, "EPT pedantic offsets: refuse to fill-in offsets for a table where the first 4 partitions are not bootloader, reserved, cache, env (%s, %s, %s, %s instead)\n", table->partitions[0].name, table->partitions[1].name, table->partitions[2].name, table->partitions[3].name);
        return 2;
    }
    table->partitions[0].offset = 0;
    table->partitions[1].offset = table->partitions[0].size + cli_options.gap_reserved;
    struct ept_partition *part_current;
    struct ept_partition *part_last = table->partitions + 1;
    for (uint32_t i=2; i<table->partitions_count; ++i) {
        part_current= table->partitions + i;
        part_current->offset = part_last->offset + part_last->size + cli_options.gap_partition;
        if (part_current->offset > capacity) {
            fprintf(stderr, "EPT pedantic offsets: partition %"PRIu32" (%s) overflows, its offset (%"PRIu64") is larger than eMMC capacity (%zu)\n", i, part_current->name, part_current->offset, capacity);
            return 3;
        }
        if (part_current->size == (uint64_t)-1 || part_current->offset + part_current->size > capacity) {
            part_current->size = capacity - part_current->offset;
        }
        part_last = part_current;
    }
    fputs("EPT pedantic offsets: layout now compatible with Amlogic's u-boot\n", stderr);
    return 0;
}

struct ept_table *
ept_complete_dtb(
    struct dts_partitions_helper const * const  dhelper, 
    uint64_t const                              capacity
){
    if (!dhelper) {
        return NULL;
    }
    struct ept_table *const table = malloc(sizeof(struct ept_table));
    if (!table) {
        return NULL;
    }
    memcpy(table, &ept_table_empty, sizeof(struct ept_table));
    const struct dts_partition_entry *part_dtb;
    struct ept_partition *part_table;
    bool replace;
    for (uint32_t i=0; i<dhelper->partitions_count; ++i) {
        part_dtb = dhelper->partitions + i;
        replace = false;
        for (uint32_t j=0; j<table->partitions_count; ++j) {
            part_table = table->partitions + j;
            if (!strncmp(part_dtb->name, part_table->name, MAX_PARTITION_NAME_LENGTH)) {
                part_table->size = part_dtb->size;
                part_table->mask_flags = part_dtb->mask;
                replace = true;
                break;
            }
        }
        if (!replace) {
            part_table = table->partitions + table->partitions_count++;
            strncpy(part_table->name, part_dtb->name, MAX_PARTITION_NAME_LENGTH);
            part_table->size = part_dtb->size;
            part_table->mask_flags = part_dtb->mask;
        }
    }
    if (ept_pedantic_offsets(table, capacity)) {
        free(table);
        return NULL;
    }
    return table;
}

struct ept_table *
ept_from_dtb(
    uint8_t const * const   dtb,
    size_t const            dtb_size,
    uint64_t const          capacity
){
    struct dts_partitions_helper *const dhelper = dtb_get_partitions(dtb, dtb_size);
    if (!dhelper) {
        return NULL;
    }
    // dtb_report_partitions(dhelper);
    struct ept_table *const table = ept_complete_dtb(dhelper, capacity);
    free(dhelper);
    if (!table) {
        return NULL;
    }
    table->checksum = ept_checksum(table->partitions, table->partitions_count);
    // ept_report(table);
    // printf("%d\n", ept_valid(table));
    return table;
}

int
ept_compare(
    struct ept_table const * const  ept_a,
    struct ept_table const * const  ept_b
){
    if (!(ept_a && ept_b)) {
        fputs("EPT compare: not both tables are valid\n", stderr);
        return -1;
    }
    return (memcmp(ept_a, ept_b, sizeof(struct ept_table)));
}

uint64_t 
ept_get_capacity(
    struct ept_table const * const  table
){
    if (!table || !table->partitions_count) {
        return 0;
    }
    uint64_t capacity = 0;
    uint64_t end;
    const struct ept_partition *part;
    for (uint32_t i = 0; i < table->partitions_count; ++i) {
        part = table->partitions + i;
        end = part->offset + part->size;
        if (end > capacity) {
            capacity = end;
        }
    }
    return capacity;
}

struct ept_table *
ept_read(
    int const       fd,
    size_t const    size
){
    if (size < sizeof(struct ept_table)) {
        fputs("EPT read: Input size too small\n", stderr);
        return NULL;
    }
    struct ept_table *table = malloc(sizeof(struct ept_table));
    if (table == NULL) {
        fputs("EPT read: Failed to allocate memory for partition table\n", stderr);
        return NULL;
    }
    if (io_read_till_finish(fd, table, sizeof(struct ept_table))) {
        fputs("EPT read: Failed to read into buffer", stderr);
        free(table);
        return NULL;
    }
    return table;
}

struct ept_table *
ept_read_and_report(
    int const       fd,
    size_t const    size
){
    struct ept_table *table = ept_read(fd, size);
    if (table) {
        ept_report(table);
        return table;
    } else {
        return NULL;
    }
}

/* ept.c: eMMC Partition Table related functions */