/* Self */

#include "ept.h"

/* System */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* Local */

#include "cli.h"
#include "io.h"
#include "parg.h"
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

uint32_t const
    ept_header_version_uint32[] = {
        EPT_HEADER_VERSION_UINT32_0,
        EPT_HEADER_VERSION_UINT32_1,
        EPT_HEADER_VERSION_UINT32_2
    };

struct ept_table const 
    ept_table_empty = {
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

static inline
uint32_t
ept_checksum_partitions(
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

void
ept_checksum_table(
    struct ept_table * const    table
){
    table->checksum = ept_checksum_partitions(table->partitions, table->partitions_count);
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
        const uint32_t checksum = ept_checksum_partitions(((const struct ept_table *)header)->partitions, header->partitions_count);
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
ept_valid_table(
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
    if (!table) {
        return;
    }
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
    uint32_t const block = ept_get_minimum_block(table);
    char suffix;
    double block_h = util_size_to_human_readable(block, &suffix);
    fprintf(stderr, "table report: Minumum block in table: 0x%x, %u, %lf%c\n", block, block, block_h, suffix);
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

int
ept_table_from_dts_partitions_helper(
    struct ept_table * const                    table,
    struct dts_partitions_helper const * const  phelper, 
    uint64_t const                              capacity
){
    if (!phelper || !phelper->partitions_count) {
        fputs("EPT table from DTS partitions helper: Helper invalid or does not contain valid partitions\n", stderr);
        return 1;
    }
    memcpy(table, &ept_table_empty, sizeof *table);
    const struct dts_partition_entry *part_dtb;
    struct ept_partition *part_table;
    bool replace;
    for (uint32_t i=0; i<phelper->partitions_count; ++i) {
        part_dtb = phelper->partitions + i;
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
        fputs("EPT table from DTS partitions helper: Failed to fill in offsets\n", stderr);
        return 2;
    }
    ept_checksum_table(table);
    return 0;
}

int
ept_table_from_dtb(
    struct ept_table * const    table,
    uint8_t const * const       dtb,
    size_t const                dtb_size,
    uint64_t const              capacity
){
    struct dts_partitions_helper phelper;
    if (!dtb_get_partitions(&phelper, dtb, dtb_size)) {
        return 1;
    }
    if (ept_table_from_dts_partitions_helper(table, &phelper, capacity)) {
        return 2;
    }
    return 0;
}

int
ept_compare_partition(
    struct ept_partition const * const  part_a,
    struct ept_partition const * const  part_b
){
    if (!part_a || !part_b) {
        fputs("EPT compare partitions: Not both partitions exist\n", stderr);
        return -1;
    }
    if (strncmp(part_a->name, part_b->name, MAX_PARTITION_NAME_LENGTH)) {
        fputs("EPT compare partitions: Names different\n", stderr);
        return 1;
    }
    if (part_a->offset != part_b->offset) {
        fputs("EPT compare partitions: Offset different\n", stderr);
        return 2;
    }
    if (part_a->size != part_b->size) {
        fputs("EPT compare partitions: Size different\n", stderr);
        return 4;
    }
    if (part_a->mask_flags != part_b->mask_flags) {
        fputs("EPT compare partitions: Masks different\n", stderr);
        return 8;
    }
    // fputs("EPT compare partitions: All same\n", stderr);
    return 0;
}

int
ept_compare_table(
    struct ept_table const * const  table_a,
    struct ept_table const * const  table_b
){
    if (!table_a || !table_b || table_a->magic_uint32 != EPT_HEADER_MAGIC_UINT32 || table_b->magic_uint32 != EPT_HEADER_MAGIC_UINT32 || !table_a->partitions_count || !table_b->partitions_count) {
        fputs("EPT compare: not both tables are valid and contain valid partitions\n", stderr);
        return -1;
    }
    int r = 128 * (table_a->version_uint32[0] != table_b->version_uint32[0]) + 256 * (table_a->version_uint32[1] != table_b->version_uint32[1]) + 512 * (table_a->version_uint32[2] != table_b->version_uint32[2]) + 1024 * (table_a->partitions_count != table_b->partitions_count);
    struct ept_partition const *part_a, *part_b;
    for (unsigned i = 0; i < MAX_PARTITIONS_COUNT; ++i) {
        part_a = table_a->partitions + i;
        part_b = table_b->partitions + i;
        r += ept_compare_partition(part_a, part_b);
    }
    return r;
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

int
ept_read(
    struct ept_table *  table,
    int const           fd,
    size_t const        size
){
    if (!table) {
        return 1;
    }
    if (size < sizeof *table) {
        fputs("EPT read: Input size too small\n", stderr);
        return 2;
    }
    if (io_read_till_finish(fd, table, sizeof *table)) {
        fputs("EPT read: Failed to read into buffer", stderr);
        return 3;
    }
    return 0;
}

int
ept_read_and_report(
    struct ept_table * const    table,
    int const                   fd,
    size_t const                size
){
    if (!table) {
        return 1;
    }
    if (ept_read(table, fd, size)) {
        return 2;
    }
    ept_report(table);
    return 0;
}

int
ept_create_pedantic(
    struct ept_table * const    table,
    size_t const                capacity
){
    *table = ept_table_empty;
    if (ept_pedantic_offsets(table, capacity)) {
        fputs("EPT create pedantic: Failed\n", stderr);
        return 1;
    }
    return 0;
}

int
ept_is_not_pedantic(
    struct ept_table const * const  table
){
    if (!table || table->partitions_count < 4) {
        fputs("EPT is not pedantic: Table invalid or partitions count too few, not pedantic\n", stderr);
        return -1;
    }
    size_t const capacity = ept_get_capacity(table);
    if (!capacity) {
        fputs("EPT is not pedantic: Failed to get capacity\n", stderr);
        return -2;
    }
    struct ept_table table_pedantic = ept_table_empty;
    if (ept_create_pedantic(&table_pedantic, capacity)) {
        fputs("EPT is not pedantic: Failed to create pedantic reference table\n", stderr);
        return -3;
    }
    if (ept_compare_partition(table->partitions, table_pedantic.partitions)) {
        fputs("EPT is not pedantic: Part 1 does not met with the pedantic bootloader\n", stderr);
        return 1;
    }
    if (ept_compare_partition(table->partitions + 1, table_pedantic.partitions + 1)) {
        fputs("EPT is not pedantic: Part 2 does not met with the pedantic reserved\n", stderr);
        return 2;
    }
    if (strncmp((table->partitions + 2)->name, "cache", MAX_PARTITION_NAME_LENGTH)) {
        fputs("EPT is not pedantic: Part 3 is not cache\n", stderr);
        return 3;
    }
    if (strncmp((table->partitions + 3)->name, "env", MAX_PARTITION_NAME_LENGTH)) {
        fputs("EPT is not pedantic: Part 3 is not env\n", stderr);
        return 4;
    }
    size_t offset;
    struct ept_partition const *current = table->partitions + 1;
    for (uint32_t i = 2; i < table->partitions_count; ++i) {
        offset = current->offset + current->size + cli_options.gap_partition;
        current = table->partitions + i;
        if (current->offset != offset) {
            fprintf(stderr, "EPT is not pedantic: Part %u (%s) has a non-pedantic offset\n", i, current->name);
            return 10 + i;
        }
    }
    return 0;
}

int
ept_eclone_parse(
    int const                   argc,
    char const * const * const  argv,
    struct ept_table * const    table,
    size_t const                capacity
){
    struct parg_definer_helper *dhelper = parg_parse_eclone_mode(argc, argv);
    if (!dhelper) {
        fputs("EPT eclone parse: Failed to parse PARGS\n", stderr);
        return 1;
    }
    *table = ept_table_empty;
    table->partitions_count = dhelper->count;
    struct parg_definer *definer;
    struct ept_partition *part;
    size_t part_end;
    for (unsigned i = 0; i < dhelper->count; ++i) {
        definer = dhelper->definers + i;
        part = table->partitions + i;
        strncpy(part->name, definer->name, MAX_PARTITION_NAME_LENGTH);
        part->offset = definer->offset;
        part->size = definer->size;
        part_end = part->offset + part->size;
        if (part_end > capacity) {
            part->size -= (part_end - capacity);
            fprintf(stderr, "EPT eclone parse: Warning, part %u (%s) overflows, shrink its size to %lu\n", i, part->name, part->size);
        }
        part->mask_flags = definer->masks;
    }
    parg_free_definer_helper(&dhelper);
    ept_checksum_table(table);
    fputs("EPT eclone parse: New EPT:\n", stderr);
    ept_report(table);
    size_t const capacity_new = ept_get_capacity(table);
    if (capacity_new > capacity) {
        fputs("EPT eclone parse: New table max part end larger than capacity, refuse to continue:\n", stderr);
        return 2;
    } else if (capacity_new < capacity) {
        fputs("EPT eclone parse: Warning, new table max part end smaller than capcity, this may result in unexpected behaviour:\n", stderr);
    }
    return 0;
}

static inline
void
ept_snapshot_decimal(
    struct ept_table const * const  table
){
    fputs("EPT snapshot decimal:\n", stderr);
    struct ept_partition const *const part_start = table->partitions;
    struct ept_partition const *part_current;
    for (uint32_t i = 0; i < table->partitions_count; ++i) {
        part_current = part_start + i;
        printf("%s:%lu:%lu:%u ", part_current->name, part_current->offset, part_current->size, part_current->mask_flags);
    }
    fputc('\n', stdout);
}

static inline
void
ept_snapshot_hex(
    struct ept_table const * const  table
){
    fputs("EPT snapshot hex:\n", stderr);
    struct ept_partition const *const part_start = table->partitions;
    struct ept_partition const *part_current;
    for (uint32_t i = 0; i < table->partitions_count; ++i) {
        part_current = part_start + i;
        printf("%s:0x%lx:0x%lx:%u ", part_current->name, part_current->offset, part_current->size, part_current->mask_flags);
    }
    fputc('\n', stdout);
}

static inline
void
ept_snapshot_human(
    struct ept_table const * const  table
){
    fputs("EPT snapshot human:\n", stderr);
    struct ept_partition const *const part_start = table->partitions;
    struct ept_partition const *part_current;
    size_t offset;
    char suffix_offset;
    size_t size;
    char suffix_size;
    for (uint32_t i = 0; i < table->partitions_count; ++i) {
        part_current = part_start + i;
        offset = util_size_to_human_readable_int(part_current->offset, &suffix_offset);
        size = util_size_to_human_readable_int(part_current->size, &suffix_size);
        printf("%s:%lu%c:%lu%c:%u ", part_current->name, offset, suffix_offset, size, suffix_size, part_current->mask_flags);
    }
    fputc('\n', stdout);
}

int
ept_snapshot(
    struct ept_table const * const  table
){
    if (!table || !table->partitions_count) {
        fputs("EPT snapshot: EPT invalid\n", stderr);
        return 1;
    }
    ept_snapshot_decimal(table);
    ept_snapshot_hex(table);
    ept_snapshot_human(table);
    return 0;
}

int
ept_table_to_dts_partitions_helper(
    struct ept_table const * const              table,
    struct dts_partitions_helper_simple * const dparts,
    uint64_t const                              capacity
){
    if (!table || !table->partitions_count || !dparts || !capacity) {
        return 1;
    }
    struct ept_table table_pedantic;
    if (ept_create_pedantic(&table_pedantic, capacity)) {
        return 2;
    }
    dparts->partitions_count = 0;
    struct ept_partition const *part_source, *part_pedantic;
    struct dts_partition_entry_simple *dpart;
    bool copy;
    for (unsigned i = 0; i < table->partitions_count; ++i) {
        part_source = table->partitions + i;
        if (i < 4) {
            part_pedantic = table_pedantic.partitions + i;
            if (ept_compare_partition(part_source, part_pedantic)) {
                copy = true;
            } else {
                copy = false;
            }
        } else {
            copy = true;
        }
        if (copy) {
            dpart = dparts->partitions + dparts->partitions_count++;
            strncpy(dpart->name, part_source->name, MAX_PARTITION_NAME_LENGTH);
            dpart->mask = part_source->mask_flags;
            dpart->size = part_source->size;
            if (part_source->offset + part_source->size >= capacity) {
                dpart->size = (uint64_t)-1;
            }
        }   
    }
    return 0;
}

uint32_t
ept_get_minimum_block(
    struct ept_table const * const  table
){
    uint32_t block = EPT_PARTITION_BOOTLOADER_SIZE;
    bool change;
    struct ept_partition const *part;
#ifdef EPT_GET_MINUMUM_BLOCK_AVOID_LAST_SIZE
    uint32_t const parts_count = table->partitions_count - 1;
    struct ept_partition const *const part_last = table->partitions + parts_count;
#endif
    while (block) {
        change = false;
        for (uint32_t i = 0; i < table->partitions_count; ++i) {
            part = table->partitions + i;
            if (part->offset % block) {
                fprintf(stderr, "EPT get minumum block: Shift down block size from 0x%x due to part %u (%s)'s offset 0x%lx\n", block, i + 1, part->name, part->offset);
                block >>= 1;
                change = true;
            }
            if (part->size % block) {
                fprintf(stderr, "EPT get minumum block: Shift down block size from 0x%x due to part %u (%s)'s size 0x%lx\n", block, i + 1, part->name, part->size);
                block >>= 1;
                change = true;
            }
        }
#ifdef EPT_GET_MINUMUM_BLOCK_AVOID_LAST_SIZE
        if (part_last->offset % block) {
            fprintf(stderr, "EPT get minumum block: Shift down block size from 0x%x due to part %u (%s)'s offset 0x%lx\n", block, table->partitions_count, part_last->name, part_last->offset);
            block >>= 1;
            change = true;
        }
#endif
        if (!change) {
            break;
        }
    }
    return block;
}

#define EPT_PARTITION_ESSENTIAL_COUNT 8
char const ept_partition_essential_names[EPT_PARTITION_ESSENTIAL_COUNT][MAX_PARTITION_NAME_LENGTH] = {
    EPT_PARTITION_BOOTLOADER_NAME,
    EPT_PARTITION_RESERVED_NAME,
    EPT_PARTITION_ENV_NAME,
    "logo",
    "misc",
    "dtb",
    "dtbo",
    "dtb_a"
};

bool
ept_is_partition_essential(
    struct ept_partition const * const  part
){
    if (!part) {
        return false;
    }
    for (unsigned i = 0; i < EPT_PARTITION_ESSENTIAL_COUNT; ++i) {
        if (!strncmp(part->name, ept_partition_essential_names[i], MAX_PARTITION_NAME_LENGTH)) {
            return true;
        }
    }
    return false;
}

int
ept_migrate_plan(
    struct io_migrate_helper *      mhelper,
    struct ept_table const * const  source,
    struct ept_table const * const  target,
    bool const                      all
){
    if (!mhelper || !source || !target || !source->partitions_count || !target->partitions_count) {
        return -1;
    }
    size_t const block_source = ept_get_minimum_block(source);
    size_t const block_target = ept_get_minimum_block(target);
    if (!block_source || !block_target) {
        return 1;
    }
    mhelper->block = (block_source > block_target) ? block_target : block_source;
    size_t const capacity_source = ept_get_capacity(source);
    size_t const capacity_target = ept_get_capacity(target);
    size_t const capacity = (capacity_source > capacity_target) ? capacity_source : capacity_target;
    if (capacity % mhelper->block) {
        return 2;
    }
    mhelper->count = capacity / mhelper->block;
    size_t const block_max_source = capacity_source / mhelper->block;
    size_t const block_max_target = capacity_target / mhelper->block;
    size_t const size_entries = mhelper->count * sizeof *mhelper->entries;
    mhelper->entries = malloc(size_entries);
    if (!mhelper->entries) {
        return 3;
    }
    memset(mhelper->entries, 0, size_entries);
    uint32_t i, j, k;
    struct ept_partition const *part_source, *part_target;
    uint32_t entry_start, entry_count, entry_end, target_start;
    struct io_migrate_entry *mentry;
    uint32_t blocks = 0;
    fprintf(stderr, "EPT migrate plan: Start planning, using block size 0x%x (source 0x%lx, target 0x%lx), count %u\n", mhelper->block, block_source, block_target, mhelper->count);
    for (i = 0; i < source->partitions_count; ++i){
        part_source = source->partitions + i;
        if (all || (!all && ept_is_partition_essential(part_source))) {
            for (j = 0; j < target->partitions_count; ++j) {
                part_target = target->partitions + j;
                // fprintf(stderr, "EPT migrate plan: Checking source %s against target %s\n", part_source->name, part_target->name);
                if (!strncmp(part_source->name, part_target->name, MAX_PARTITION_NAME_LENGTH) && part_source->offset != part_target->offset) {
                    entry_start = part_source->offset / mhelper->block;
                    entry_count = ((part_source->size > part_target->size) ? part_target->size : part_source->size) / mhelper->block;
                    entry_end = entry_start + entry_count;
                    if (entry_end > block_max_source) {
                        entry_end = block_max_source;
                        if (entry_end <= entry_start) {
                            continue;
                        }
                        entry_count = entry_end - entry_start;
                        fprintf(stderr, "EPT migrate plan: Warning, expected migrate end point of part %s exceeds the capacity of source drive, shrinked migrate block count, this may result in partition damaged since it will be incomplete\n", part_source->name);
                    }
                    target_start = part_target->offset / mhelper->block;
                    // printf("%08x, %08x, %08x\n", entry_start, entry_count, entry_end);
                    fprintf(stderr, "EPT migrate plan: Part %s (%u of %u in old table, %u of %u in new table) should be migrated, from offset 0x%lx(block %u) to 0x%lx(block %u), block count %u\n", part_source->name, i + 1, source->partitions_count, j + 1, target->partitions_count, part_source->offset, entry_start, part_target->offset, target_start, entry_count);
                    for (k = entry_start; k < entry_end; ++k) {
                        // puts("1");
                        mentry = mhelper->entries + k;
                        mentry->target = target_start + k;
                        if (mentry->target > block_max_target - 1) {
                            fprintf(stderr, "EPT migrate plan: Warning, expected migrate end point of part %s exceeds the capacity of target drive, shrinked migrate block count, this may result in partition damaged since it will be incomplete\n", part_target->name);
                            mentry->target = 0;
                            break;
                        }
                        mentry->pending = true;
                    }
                    blocks += entry_count;
                }
            }
        }
    }
    // for (uint32_t i = 0; i < mhelper->count; ++i) {
    //     if ((mhelper->entries + i)->pending) {
    //         printf("%u\n", i);
    //     }
    // }
    char suffix_each, suffix_total;
    double const size_each_d = util_size_to_human_readable(mhelper->block, &suffix_each);
    size_t const size_total = (size_t)blocks * (size_t)mhelper->block;
    double const size_total_d = util_size_to_human_readable(size_total, &suffix_total);
    fprintf(stderr, "EPT migrate plan: %u blocks should be migrated, each size 0x%x (%lf%c), total size 0x%lx (%lf%c). In the worst scenario you will need the SAME amount of memory for the migration. If you are using migartion=all mode, consider changing it to migrate=essential, or migarate=no\n", blocks, mhelper->block, size_each_d, suffix_each, size_total, size_total_d, suffix_total);
    return 0;
}

/* ept.c: eMMC Partition Table related functions */