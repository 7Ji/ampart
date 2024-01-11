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

#define EPT_PARTITION_ESSENTIAL_COUNT 17
#define EPT_PARTITION_CRITICAL_COUNT 2

#define EPT_WEBREPORT_ARG           "esnapshot="
#define EPT_WEBREPORT_ARG_MAXLEN    0x800

/* Macro */
#define EPT_IS_PARTITION_ESSENTIAL(part) !ept_is_partition_not_essential(part)
#define EPT_IS_PARTITION_CRITICAL(part) !ept_is_partition_not_critical(part)

/* Variable */

uint32_t const
    len_ept_webreport_arg = strlen(EPT_WEBREPORT_ARG);

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

char const 
    ept_partition_essential_names[17][MAX_PARTITION_NAME_LENGTH] = {
        EPT_PARTITION_BOOTLOADER_NAME,
        EPT_PARTITION_RESERVED_NAME,
        EPT_PARTITION_ENV_NAME,
        "logo",
        "misc",
        "boot",
        "boot_a",
        "boot_b",
        "dtbo",
        "dtbo_a",
        "dtbo_b",
        "vbmeta",
        "vbmeta_a",
        "vbmeta_b",
        "vbmeta_system",
        "vbmeta_system_a",
        "vbmeta_system_b"
    };

char const 
    ept_partition_critical_names[EPT_PARTITION_CRITICAL_COUNT][MAX_PARTITION_NAME_LENGTH] = {
        EPT_PARTITION_BOOTLOADER_NAME,
        EPT_PARTITION_RESERVED_NAME
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
    table->checksum = ept_checksum_partitions(table->partitions, util_safe_partitions_count(table->partitions_count));
}

int
ept_valid_header(
    struct ept_header const * const header
){
    int ret = 0;
    if (header->partitions_count > 32) {
        pr_error("EPT valid header: Partitions count invalid, only integer 0~32 is acceppted, actual: %d\n", header->partitions_count);
        ++ret;
    }
    if (header->magic_uint32 != EPT_HEADER_MAGIC_UINT32) {
        pr_error("EPT valid header: Magic invalid, expect: %"PRIx32", actual: %"PRIx32"\n", header->magic_uint32, EPT_HEADER_MAGIC_UINT32);
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
        pr_error("EPT valid header: Version invalid, expect (little endian) 0x%08x%08x%08x ("EPT_HEADER_VERSION_STRING"), actual: 0x%08"PRIx32"%08"PRIx32"%08"PRIx32"\n", EPT_HEADER_VERSION_UINT32_2, EPT_HEADER_VERSION_UINT32_1, EPT_HEADER_VERSION_UINT32_0, header->version_uint32[2], header->version_uint32[1], header->version_uint32[0]);
        ret += 1;
    }
    const uint32_t checksum = ept_checksum_partitions(((const struct ept_table *)header)->partitions, util_safe_partitions_count(header->partitions_count));
    if (header->checksum != checksum) {
        pr_error("EPT valid header: Checksum mismatch, calculated: %"PRIx32", actual: %"PRIx32"\n", checksum, header->checksum);
        ret += 1;
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
        pr_error("EPT valid partition name: %u illegal characters found in partition name '%s'\n", ret, name);
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
    if (!table) {
        return -1;
    }
    int ret = ept_valid_header((const struct ept_header *)table);
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    char unique_names[MAX_PARTITIONS_COUNT][MAX_PARTITION_NAME_LENGTH];
    unsigned name_id = 0;
    unsigned illegal = 0, dups = 0, r;
    bool dup;
    struct ept_partition const *part;
    for (uint32_t i=0; i < pcount; ++i) {
        part = table->partitions + i;
        ret += (r = ept_valid_partition(table->partitions+i));
        if (r) {
            ++illegal;
        }
        dup = false;
        for (unsigned j = 0; j < name_id; ++j) {
            if (!strncmp(part->name, unique_names[j], MAX_PARTITION_NAME_LENGTH)) {
                dup = true;
                ++dups;
            }
        }
        if (!dup) {
            strncpy(unique_names[name_id++], part->name, MAX_PARTITION_NAME_LENGTH);
        }
    }
    pr_error("EPT valid table: %u partitions have illegal names, %u partitions have duplicated names\n", illegal, dups);
    return ret + dups;
}

void 
ept_report(
    struct ept_table const * const  table
){
    if (!table) {
        return;
    }
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    pr_error("EPT report: %d partitions in the table:\n===================================================================================\nID| name            |          offset|(   human)|            size|(   human)| masks\n-----------------------------------------------------------------------------------\n", pcount);
    const struct ept_partition *part;
    double num_offset, num_size;
    char suffix_offset, suffix_size;
    uint64_t last_end = 0, diff;
    for (uint32_t i = 0; i < pcount; ++i) {
        part = table->partitions + i;
        if (part->offset > last_end) {
            diff = part->offset - last_end;
            num_offset = util_size_to_human_readable(diff, &suffix_offset);
            pr_error("    (GAP)                                        %16"PRIx64" (%7.2lf%c)\n", diff, num_offset, suffix_offset);
        } else if (part->offset < last_end) {
            diff = last_end - part->offset;
            num_offset = util_size_to_human_readable(diff, &suffix_offset);
            pr_error("    (OVERLAP)                                    %16"PRIx64" (%7.2lf%c)\n", diff, num_offset, suffix_offset);
        }
        num_offset = util_size_to_human_readable(part->offset, &suffix_offset);
        num_size = util_size_to_human_readable(part->size, &suffix_size);
        pr_error("%2d: %-16s %16"PRIx64" (%7.2lf%c) %16"PRIx64" (%7.2lf%c) %6"PRIu32"\n", i, part->name, part->offset, num_offset, suffix_offset, part->size, num_size, suffix_size, part->mask_flags);
        last_end = part->offset + part->size;
    }
    fputs("===================================================================================\n", stderr);
    uint32_t const block = ept_get_minimum_block(table);
    char suffix;
    double block_h = util_size_to_human_readable(block, &suffix);
    pr_error("EPT report: Minumum block in table: 0x%x, %u, %lf%c\n", block, block, block_h, suffix);
    return;
}

static inline
int 
ept_pedantic_offsets(
    struct ept_table * const    table,
    uint64_t const              capacity
){
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    if (pcount < 4) {
        fputs("EPT pedantic offsets: refuse to fill-in offsets for heavily modified table (at least 4 partitions should exist)\n", stderr);
        return 1;
    }
    if (strncmp(table->partitions[0].name, EPT_PARTITION_BOOTLOADER_NAME, MAX_PARTITION_NAME_LENGTH) || strncmp(table->partitions[1].name, EPT_PARTITION_RESERVED_NAME, MAX_PARTITION_NAME_LENGTH) || strncmp(table->partitions[2].name, EPT_PARTITION_CACHE_NAME, MAX_PARTITION_NAME_LENGTH) || strncmp(table->partitions[3].name, EPT_PARTITION_ENV_NAME, MAX_PARTITION_NAME_LENGTH)) {
        pr_error("EPT pedantic offsets: refuse to fill-in offsets for a table where the first 4 partitions are not bootloader, reserved, cache, env (%s, %s, %s, %s instead)\n", table->partitions[0].name, table->partitions[1].name, table->partitions[2].name, table->partitions[3].name);
        return 2;
    }
    table->partitions[0].offset = 0;
    table->partitions[1].offset = table->partitions[0].size + cli_options.gap_reserved;
    struct ept_partition *part_current;
    struct ept_partition *part_last = table->partitions + 1;
    for (uint32_t i=2; i < pcount; ++i) {
        part_current= table->partitions + i;
        part_current->offset = part_last->offset + part_last->size + cli_options.gap_partition;
        if (part_current->offset > capacity) {
            pr_error("EPT pedantic offsets: partition %"PRIu32" (%s) overflows, its offset (%"PRIu64") is larger than eMMC capacity (%zu)\n", i, part_current->name, part_current->offset, capacity);
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
    if (!phelper) {
        fputs("EPT table from DTS partitions helper: Helper invalid\n", stderr);
        return 1;
    }
    memcpy(table, &ept_table_empty, sizeof *table);
    const struct dts_partition_entry *part_dtb;
    struct ept_partition *part_table;
    bool replace;
    uint32_t const pcount = util_safe_partitions_count(phelper->partitions_count);
    for (uint32_t i=0; i < pcount; ++i) {
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
            if (table->partitions_count >= MAX_PARTITIONS_COUNT) {
                fputs("EPT table from DTS partitions helper: Too many partitions\n", stderr);
                return 2;
            }
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
ept_table_from_dts_partitions_helper_simple(
    struct ept_table * const                            table,
    struct dts_partitions_helper_simple const * const   phelper, 
    uint64_t const                                      capacity
){
    if (!phelper) {
        fputs("EPT table from DTS partitions helper: Helper invalid\n", stderr);
        return 1;
    }
    memcpy(table, &ept_table_empty, sizeof *table);
    const struct dts_partition_entry_simple *part_dtb;
    struct ept_partition *part_table;
    bool replace;
    uint32_t const pcount = util_safe_partitions_count(phelper->partitions_count);
    for (uint32_t i=0; i<pcount; ++i) {
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
            if (table->partitions_count >= MAX_PARTITIONS_COUNT) {
                fputs("EPT table from DTS partitions helper: Too many partitions\n", stderr);
                return 2;
            }
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
    struct ept_partition const *part;
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    for (uint32_t i = 0; i < pcount; ++i) {
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
        fputs("EPT read: Failed to read into buffer\n", stderr);
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
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    for (uint32_t i = 2; i < pcount; ++i) {
        offset = current->offset + current->size + cli_options.gap_partition;
        current = table->partitions + i;
        if (current->offset != offset) {
            pr_error("EPT is not pedantic: Part %u (%s) has a non-pedantic offset\n", i, current->name);
            return 10 + i;
        }
    }
    return 0;
}

int
ept_eclone_parse(
    struct ept_table * const    table,
    int const                   argc,
    char const * const * const  argv,
    size_t const                capacity
){
    if (!table || argc <= 0 || argc > MAX_PARTITIONS_COUNT || !argv || !capacity) {
        fputs("EPT eclone parse: Illegal arguments\n", stderr);
        return -1;
    }
    struct parg_definer_helper_static dhelper;
    if (parg_parse_eclone_mode(&dhelper, argc, argv)) {
        fputs("EPT eclone parse: Failed to parse arguments\n", stderr);
        return 1;
    }
    *table = ept_table_empty;
    table->partitions_count = util_safe_partitions_count(dhelper.count);
    struct parg_definer *definer;
    struct ept_partition *part;
    size_t part_end;
    for (unsigned i = 0; i < table->partitions_count; ++i) {
        definer = dhelper.definers + i;
        part = table->partitions + i;
        strncpy(part->name, definer->name, MAX_PARTITION_NAME_LENGTH);
        part->offset = definer->offset;
        part->size = definer->size;
        part_end = part->offset + part->size;
        if (part_end > capacity) {
            part->size -= (part_end - capacity);
            pr_error("EPT eclone parse: Warning, part %u (%s) overflows, shrink its size to %lu\n", i, part->name, part->size);
        }
        part->mask_flags = definer->masks;
    }
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
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    uint32_t const pcount_has_space = pcount - 1;
    for (uint32_t i = 0; i < pcount; ++i) {
        part_current = part_start + i;
        printf("%s:%lu:%lu:%u", part_current->name, part_current->offset, part_current->size, part_current->mask_flags);
        if (i != pcount_has_space) {
            fputc(' ', stdout);
        }
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
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    uint32_t const pcount_has_space = pcount - 1;
    for (uint32_t i = 0; i < pcount; ++i) {
        part_current = part_start + i;
        printf("%s:0x%lx:0x%lx:%u", part_current->name, part_current->offset, part_current->size, part_current->mask_flags);
        if (i != pcount_has_space) {
            fputc(' ', stdout);
        }
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
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    uint32_t const pcount_has_space = pcount - 1;
    for (uint32_t i = 0; i < pcount; ++i) {
        part_current = part_start + i;
        offset = util_size_to_human_readable_int(part_current->offset, &suffix_offset);
        size = util_size_to_human_readable_int(part_current->size, &suffix_size);
        printf("%s:%lu%c:%lu%c:%u", part_current->name, offset, suffix_offset, size, suffix_size, part_current->mask_flags);
        if (i != pcount_has_space) {
            fputc(' ', stdout);
        }
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
ept_webreport(
    struct ept_table const * const  table,
    char * const                    arg_esnapshot,
    uint32_t * const                len_dsnapshot
) {
    if (!table || !table->partitions_count) {
        fputs("EPT snapshot: EPT invalid\n", stderr);
        return 1;
    }
    // Initial esnapshot= part
    strncpy(arg_esnapshot, EPT_WEBREPORT_ARG, len_ept_webreport_arg);
    char *current = arg_esnapshot + len_ept_webreport_arg;
    uint32_t len_available = EPT_WEBREPORT_ARG_MAXLEN - len_ept_webreport_arg;
    // For each partition
    struct ept_partition const * part;
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    int len_this;
    bool started = false;
    for (uint32_t i = 0; i < pcount; ++i) {
        if (started) {
            if (len_available <= 4) {
                fputs("EPT webreport: Not enough space for connector\n", stderr);
                return 2;
            }
            strncpy(current, "\%20", 3);
            current += 3;
            len_available -= 3;
        }
        part = table->partitions + i;
        len_this = snprintf(current, len_available, "%s:%lu:%lu:%u", part->name, part->offset, part->size, part->mask_flags);
        if (len_this >= 0 && (uint32_t)len_this >= len_available) {
            fputs("EPT webreport: Argument truncated\n", stderr);
            return 3;
        } else if (len_this < 0) {
            fputs("EPT webreport: Output error encountered\n", stderr);
            return 4;
        }
        len_available -= len_this;
        current += len_this;
        started = true;
    }
    *len_dsnapshot = EPT_WEBREPORT_ARG_MAXLEN- len_available;
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
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    for (unsigned i = 0; i < pcount; ++i) {
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
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    while (block) {
        change = false;
        for (uint32_t i = 0; i < pcount; ++i) {
            part = table->partitions + i;
            if (part->offset % block) {
                pr_error("EPT get minumum block: Shift down block size from 0x%x due to part %u (%s)'s offset 0x%lx\n", block, i + 1, part->name, part->offset);
                block >>= 1;
                change = true;
            }
            if (part->size % block) {
                pr_error("EPT get minumum block: Shift down block size from 0x%x due to part %u (%s)'s size 0x%lx\n", block, i + 1, part->name, part->size);
                block >>= 1;
                change = true;
            }
        }
#ifdef EPT_GET_MINUMUM_BLOCK_AVOID_LAST_SIZE
        if (part_last->offset % block) {
            pr_error("EPT get minumum block: Shift down block size from 0x%x due to part %u (%s)'s offset 0x%lx\n", block, table->partitions_count, part_last->name, part_last->offset);
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

int
ept_is_partition_not_critical(
    struct ept_partition const * const part
){
    if (!part) {
        return -1;
    }
    for (unsigned i = 0; i < EPT_PARTITION_CRITICAL_COUNT; ++i) {
        if (!strncmp(part->name, ept_partition_critical_names[i], MAX_PARTITION_NAME_LENGTH)) {
            return 0;
        }
    }
    return 1;
}

int
ept_is_partition_not_essential(
    struct ept_partition const * const  part
){
    if (!part) {
        return -1;
    }
    for (unsigned i = 0; i < EPT_PARTITION_ESSENTIAL_COUNT; ++i) {
        if (!strncmp(part->name, ept_partition_essential_names[i], MAX_PARTITION_NAME_LENGTH)) {
            return 0;
        }
    }
    return 1;
}

int
ept_migrate_plan(
    struct io_migrate_helper *      mhelper,
    struct ept_table const * const  source,
    struct ept_table const * const  target,
    bool const                      all
){
    if (!mhelper || !source || !target || !source->partitions_count || !target->partitions_count) {
        fputs("EPT migrate plan: Illegal arguments\n", stderr);
        return -1;
    }
    if (all) {
        fputs("EPT migrate plan: All partitions will be migrated\n", stderr);
    } else {
        fputs("EPT migrate plan: Only essential partitions will be migrated\n", stderr);
    }
    size_t const block_source = ept_get_minimum_block(source);
    size_t const block_target = ept_get_minimum_block(target);
    if (!(mhelper->block = block_source > block_target ? block_target : block_source)) {
        fputs("EPT migrate plan: Failed to get minumum blocks\n", stderr);
        return 1;
    }
    size_t const capacity_source = ept_get_capacity(source);
    size_t const capacity_target = ept_get_capacity(target);
    if (!capacity_source || !capacity_target) {
        fputs("EPT migrate plan: Failed to get capacity\n", stderr);
        return 2;
    }
    if (capacity_source % mhelper->block || capacity_target %mhelper->block) {
        fputs("EPT migrate plan: Capcity not multiply of minumum blocks\n", stderr);
        return 3;
    }
    mhelper->count = (capacity_source > capacity_target ? capacity_source : capacity_target) / mhelper->block;
    size_t const block_total_source = capacity_source / mhelper->block;
    size_t const block_total_target = capacity_target / mhelper->block;
    if (!block_total_source || !block_total_target) {
        fputs("EPT migrate plan: Block total count illegal, can not be 0\n", stderr);
        return 4;
    }
    size_t const block_id_max_source = block_total_source - 1;
    size_t const block_id_max_target = block_total_target - 1;
    size_t const size_entries = mhelper->count * sizeof *mhelper->entries;
    if (!(mhelper->entries = malloc(size_entries))) {
        fputs("EPT migrate plan: Failed to allocate memory for entries\n", stderr);
        return 5;
    }
    memset(mhelper->entries, 0, size_entries);
    uint32_t i, j, k;
    struct ept_partition const *part_source, *part_target;
    uint32_t entry_start, entry_count, entry_end, target_start, target_end;
    struct io_migrate_entry *mentry;
    uint32_t blocks = 0;
    pr_error("EPT migrate plan: Start planning, using block size 0x%x (source 0x%lx, target 0x%lx), count %u\n", mhelper->block, block_source, block_target, mhelper->count);
    uint32_t const pcount_source = util_safe_partitions_count(source->partitions_count);
    uint32_t const pcount_target = util_safe_partitions_count(target->partitions_count);
    for (i = 0; i < pcount_source; ++i){
        part_source = source->partitions + i;
        if (all || (!all && EPT_IS_PARTITION_ESSENTIAL(part_source))) {
            for (j = 0; j < pcount_target; ++j) {
                part_target = target->partitions + j;
                if (!strncmp(part_source->name, part_target->name, MAX_PARTITION_NAME_LENGTH) && part_source->offset != part_target->offset) {
                    if ((entry_start = part_source->offset / mhelper->block) > block_id_max_source || ((target_start = part_target->offset / mhelper->block) > block_id_max_target)) {
                        fputs("EPT migrate plan: Block ID overflows!\n", stderr);
                        free(mhelper->entries);
                        return 6;
                    }
                    entry_count = (part_source->size > part_target->size ? part_target->size : part_source->size) / mhelper->block;
                    entry_end = entry_start + entry_count;
                    if (entry_end > block_total_source) { // No need, but anyway
                        entry_end = block_total_source;
                        if (entry_end <= entry_start) {
                            continue;
                        }
                        entry_count = entry_end - entry_start;
                        pr_error("EPT migrate plan: Warning, expected migrate end point of part %s exceeds the capacity of source drive, shrinked migrate block count, this may result in partition damaged since it will be incomplete\n", part_source->name);
                    }
                    target_end = target_start + entry_count;
                    if (target_end > block_total_target) {
                        entry_end -= target_end - block_total_target;
                        if (entry_end <= entry_start) {
                            continue;
                        }
                        entry_count = entry_end - target_start;
                        pr_error("EPT migrate plan: Warning, expected migrate end point of part %s exceeds the capacity of target drive, shrinked migrate block count, this may result in partition damaged since it will be incomplete\n", part_source->name);
                    }
                    pr_error("EPT migrate plan: Part %s (%u of %u in old table, %u of %u in new table) should be migrated, from offset 0x%lx(block %u) to 0x%lx(block %u), block count %u\n", part_source->name, i + 1, pcount_source, j + 1, pcount_target, part_source->offset, entry_start, part_target->offset, target_start, entry_count);
                    for (k = 0; k < entry_count; ++k) {
                        mentry = mhelper->entries + entry_start + k;
                        mentry->target = target_start + k;
                        mentry->pending = true;
                    }
                    blocks += entry_count;
                }
            }
        }
    }
    char suffix_each, suffix_total;
    double const size_each_d = util_size_to_human_readable(mhelper->block, &suffix_each);
    size_t const size_total = (size_t)blocks * (size_t)mhelper->block;
    double const size_total_d = util_size_to_human_readable(size_total, &suffix_total);
    pr_error("EPT migrate plan: %u blocks should be migrated, each size 0x%x (%lf%c), total size 0x%lx (%lf%c). In the worst scenario you will need the SAME amount of memory for the migration\n", blocks, mhelper->block, size_each_d, suffix_each, size_total, size_total_d, suffix_total);
    return 0;
}


struct ept_partition *
ept_eedit_part_select(
    struct parg_modifier const * const modifier,
    struct ept_table * const table
){
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    switch (modifier->select) {
        case PARG_SELECT_NAME:
            for (unsigned i = 0; i < pcount; ++i) {
                if (!strncmp(modifier->select_name, table->partitions[i].name, MAX_PARTITION_NAME_LENGTH)) {
                    return table->partitions + i;
                }
            }
            return NULL;
        case PARG_SELECT_RELATIVE:
            if (modifier->select_relative >= 0) {
                if ((unsigned)modifier->select_relative + 1 > pcount) {
                    return NULL;
                }
                return table->partitions + modifier->select_relative;
            } else {
                if ((uint32_t)abs(modifier->select_relative) > pcount) {
                    return NULL;
                }
                return table->partitions + pcount + modifier->select_relative;
            }
    }
    return NULL;
}

void
ept_sanitize_offset(
    struct ept_partition * const    part,
    size_t const                    capacity
){
    if (part->offset > capacity) {
        pr_error("EPT sanitize offset: Warning: part %s offset overflows, limitting it to 0x%lx\n", part->name, capacity);
        part->offset = capacity;
    }
}

void
ept_sanitize_size(
    struct ept_partition * const    part,
    size_t const                    capacity
){
    if (part->offset + part->size > capacity) {
        part->size -= part->offset + part->size - capacity;
        pr_error("EPT sanitize size: Warning: part %s size overflows, limitting it to 0x%lx\n", part->name, part->size);
    }
}

int
ept_eedit_adjust(
    struct parg_modifier const * const          modifier,
    struct ept_partition * const   part,
    size_t const                    capacity
){
    
    if (modifier->modify_name == PARG_MODIFY_DETAIL_SET) {
        strncpy(part->name, modifier->name, MAX_PARTITION_NAME_LENGTH);
    }
    if (parg_adjustor_adjust_u64(&part->offset, modifier->modify_offset, modifier->offset)) {
        pr_error("EPT eedit adjust: Failed to adjust offset of part %s\n", part->name);
        return 1;
    }
    ept_sanitize_offset(part, capacity);
    if (parg_adjustor_adjust_u64(&part->size, modifier->modify_size, modifier->size)) {
        pr_error("EPT eedit adjust: Failed to adjust size of part %s\n", part->name);
        return 2;
    }
    ept_sanitize_size(part, capacity);
    if (modifier->modify_masks == PARG_MODIFY_DETAIL_SET) {
        part->mask_flags = modifier->masks;
    }
    return 0;
}

int
ept_eedit_place(
    struct parg_modifier const * const  modifier,
    struct ept_table * const            table,
    struct ept_partition * const        part
){
    int place_target;
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    switch (modifier->modify_place) {
        case PARG_MODIFY_PLACE_ABSOLUTE:
            if (modifier->place >= 0) {
                place_target = modifier->place;
            } else {
                place_target = pcount + modifier->place;
            }
            break;
        case PARG_MODIFY_PLACE_RELATIVE:
            place_target = part - table->partitions + modifier->place;
            break;
        default:
            fputs("EPT eedit place: Illegal place method\n", stderr);
            return -1;
    }
    if (place_target < 0 || (unsigned)place_target >= pcount) {
        pr_error("EPT eedit place: Target place %i overflows (minumum 0 as start, maximum %u as end)\n", place_target, pcount);
        return 1;
    }
    struct ept_partition * const part_target = table->partitions + place_target;
    if (part_target > part) {
        struct ept_partition const part_buffer = *part;
        for (struct ept_partition *part_hot = part; part_hot < part_target; ++part_hot) {
            *(part_hot) = *(part_hot + 1);
        }
        *part_target = part_buffer;
    } else if (part_target < part) {
        struct ept_partition const part_buffer = *part;
        for (struct ept_partition *part_hot = part; part_hot > part_target; --part_hot) {
            *(part_hot) = *(part_hot - 1);
        }
        *part_target = part_buffer;
    }
    return 0;
}

int
ept_eedit_each(
    struct ept_table * const            table,
    size_t const                        capacity,
    struct parg_editor const * const    editor
){
    if (editor->modify) {
        struct parg_modifier const *const modifier = &editor->modifier;
        struct ept_partition *const part = ept_eedit_part_select(modifier, table);
        if (!part) {
            parg_report_failed_select(modifier);
            fputs("EPT eedit each: Failed selector\n", stderr);
            return 1;
        }
        switch (modifier->modify_part) {
            case PARG_MODIFY_PART_ADJUST:
                if (ept_eedit_adjust(modifier, part, capacity)) {
                    fputs("EPT eedit each: Failed to adjust partition detail\n", stderr);
                    return 2;
                }
                break;
            case PARG_MODIFY_PART_DELETE:
                if (table->partitions_count == 0) {
                    fputs("EPT eedit each: Cannot delete partition, there's already only 0 partitions left, which is impossible you could even see this\n", stderr);
                    return 3;
                }
                for (struct ept_partition *part_hot = part; part_hot - table->partitions < table->partitions_count - 1; ++part_hot) {
                    *part_hot = *(part_hot + 1);
                }
                --table->partitions_count;
                break;
            case PARG_MODIFY_PART_CLONE: {
                if (table->partitions_count >= MAX_PARTITIONS_COUNT) {
                    fputs("EPT eedit each: Trying to clone when partition count overflowed, refuse to continue\n", stderr);
                    return 4;
                }
                struct ept_partition *part_hot = table->partitions + table->partitions_count++;
                *part_hot = *part;
                strncpy(part_hot->name, modifier->name, MAX_PARTITION_NAME_LENGTH);
                break;
            }
            case PARG_MODIFY_PART_PLACE:
                if (ept_eedit_place(modifier, table, part)) {
                    fputs("EPT eedit each: Failed to place partition\n", stderr);
                    return 5;
                }
                break;
        }
    } else {
        if (table->partitions_count >= MAX_PARTITIONS_COUNT) {
            fputs("EPT eedit each: Trying to define partition when partition count overflowed, refuse to continue\n", stderr);
            return 1;
        }
        size_t const end = ept_get_capacity(table);
        struct parg_definer const *const definer= &editor->definer;
        struct ept_partition *const part = table->partitions + table->partitions_count++;
        strncpy(part->name, definer->name, MAX_PARTITION_NAME_LENGTH);
        part->size = definer->size;
        if (definer->set_offset) {
            if (definer->relative_offset) {
                part->offset = end + definer->offset;
            } else {
                part->offset = definer->offset;
            }
        } else {
            part->offset = end;
        }
        ept_sanitize_offset(part, capacity);
        if (definer->set_size) {
            part->size = definer->size;
        } else {
            part->size = capacity - part->offset;
        }
        ept_sanitize_size(part, capacity);
        if (definer->set_masks) {
            part->mask_flags = definer->masks;
        } else {
            part->mask_flags = 4;
        }
    }
    return 0;
}

int
ept_eedit_parse(
    struct ept_table * const    table,
    size_t const                capacity,
    int const                   argc,
    char const * const * const  argv
){
    if (!table || argc <=0 || !argv) {
        fputs("EPT eedit parse: Illegal arguments\n", stderr);
        return -1;
    }
    struct parg_editor_helper ehelper;
    if (parg_parse_eedit_mode(&ehelper, argc, argv)) {
        fputs("EPT eedit parse: Failed to parse arguments\n", stderr);
        return 1;
    }
    if (!ehelper.count) {
        free(ehelper.editors);
        fputs("EPT eedit parse: No editor\n", stderr);
        return 2;
    }
    fputs("EPT eedit parse: Table before editting:\n", stderr);
    ept_report(table);
    for (unsigned i = 0; i < ehelper.count; ++i) {
        if (ept_eedit_each(table, capacity, ehelper.editors + i)) {
            fputs("EPT eedit parse: Failed to edit table according to PARGs\n", stderr);
            return 3;
        }
    }
    free(ehelper.editors);
    fputs("EPT eedit parse: Table after editting:\n", stderr);
    ept_checksum_table(table);
    ept_report(table);
    return 0;
}

int
ept_ungap(
    struct ept_table *  table
){
    uint32_t const pcount = util_safe_partitions_count(table->partitions_count);
    struct ept_partition *part;
    struct ept_partition *bootloader = NULL;
    struct ept_partition *reserved = NULL;
    struct ept_partition *non_critical[MAX_PARTITIONS_COUNT - 2];
    uint32_t non_critical_count = 0;
    for (uint32_t i = 0; i < pcount; ++i) {
        part = table->partitions + i;
        part->offset = 0;
        if (strncmp(part->name, EPT_PARTITION_BOOTLOADER_NAME, MAX_PARTITION_NAME_LENGTH)) {
            if (strncmp(part->name, EPT_PARTITION_RESERVED_NAME, MAX_PARTITION_NAME_LENGTH)) {
                non_critical[non_critical_count++] = part;
            } else {
                reserved = part;
            }
        } else {
            bootloader = part;
        }
    }
    if (!bootloader || !reserved) {
        fputs("EPT ungap: Can not find both bootloader and reserved partition, refuse to continue\n", stderr);
        return 1;
    }
    if (bootloader != table->partitions) {
        fputs("EPT ungap: Bootloader is not the first partition, refuse to continue\n", stderr);
        return 2;
    }
    reserved->offset = cli_options.offset_reserved;
    size_t const gap = reserved->offset - bootloader->size;
    if (non_critical_count && gap) {
        struct ept_partition *buffer;
        for (uint32_t i = 0; i < non_critical_count - 1; ++i) { // Sort from large to small, decreasing sequence
            for (uint32_t j = i + 1; j < non_critical_count; ++j) {
                if (non_critical[i]->size < non_critical[j]->size) {
                    buffer = non_critical[i];
                    non_critical[i] = non_critical[j];
                    non_critical[j] = buffer;
                }
            }
        }
        bool strategy_best[MAX_PARTITIONS_COUNT - 2] = {0};
        size_t gap_minimum = gap;
        for (uint32_t i = 0; i < non_critical_count; ++i) { // Find best strategy, gready algorithm, go from biggest
            size_t gap_remaining = gap;
            bool strategy_current[MAX_PARTITIONS_COUNT - 2] = {0};
            for (uint32_t j = i; j < non_critical_count; ++j) {
                part = non_critical[j];
                if (part->size <= gap_remaining) {
                    strategy_current[j] = true;
                    if (!(gap_remaining -= part->size)) {
                        break;
                    }
                }
            }
            if (gap_remaining < gap_minimum) {
                memcpy(strategy_best, strategy_current, sizeof strategy_best);
                if (!(gap_minimum = gap_remaining)) {
                    break;
                }
            }
        }
        struct ept_partition parts_inserted[MAX_PARTITIONS_COUNT - 2] = {0};
        struct ept_partition parts_after[MAX_PARTITIONS_COUNT - 2] = {0};
        uint32_t parts_inserted_count = 0;
        uint32_t parts_after_count = 0;
        for (uint32_t i = 0; i < non_critical_count; ++i) {
            if (strategy_best[i] || !non_critical[i]->size) {
                part = parts_inserted + parts_inserted_count++;
            } else {
                part = parts_after + parts_after_count++;
            }
            *part = *non_critical[i];
        }
        struct ept_partition reserved_buffer = *reserved;
        struct ept_partition *part_target;
        struct ept_partition *part_before = table->partitions;
        for (uint32_t i = 0; i < parts_inserted_count; ++i) {
            part_target = part_before + 1;
            *part_target = parts_inserted[i];
            part_target->offset = part_before->offset + part_before->size;
            part_before = part_target;
        }
        ++part_before;
        *part_before = reserved_buffer;
        for (uint32_t i = 0; i <parts_after_count; ++i) {
            part_target = part_before + 1;
            *part_target = parts_after[i];
            part_target->offset = part_before->offset + part_before->size;
            part_before = part_target;
        }
    }
    return 0;
}

int
ept_ecreate_parse(
    struct ept_table * const        table_new,
    struct ept_table const * const  table_old,
    size_t const                    capacity,
    int const                       argc,
    char const * const * const      argv
){
    if (!table_new || !table_old || !capacity || argc > MAX_PARTITIONS_COUNT || !argv) {
        fputs("EPT ecreate parse: Illegal argumnets\n", stderr);
        return -1;
    }
    *table_new = ept_table_empty;
    uint32_t const pcount_old = util_safe_partitions_count(table_old->partitions_count);
    if (pcount_old) {
        struct ept_partition const *part_old;
        struct ept_partition *part_new;
        for (unsigned i = 0; i < pcount_old; ++i) {
            part_old = table_old->partitions + i;
            if (EPT_IS_PARTITION_ESSENTIAL(part_old)) { // Implement essential partitions
                bool replace = false;
                for (unsigned j = 0; j < table_new->partitions_count; ++j) {
                    part_new = table_new->partitions + j;
                    if (!strncmp(part_old->name, part_new->name, MAX_PARTITION_NAME_LENGTH)) {
                        *part_new = *part_old;
                        replace = true;
                        break;
                    }
                }
                if (!replace) {
                    if (table_new->partitions_count >= MAX_PARTITIONS_COUNT) {
                        fputs("EPT ecreate parse: Too many partitions\n", stderr);
                        return 1;
                    }
                    part_new = table_new->partitions + table_new->partitions_count++;
                    *part_new = *part_old;
                }
            }
        }
    }
    if (ept_ungap(table_new)) {
        fputs("EPT ecreate parse: Failed to remove gap\n", stderr);
        return 2;
    }
    if (argc > 0) {
        uint32_t const parg_max = MAX_PARTITIONS_COUNT - table_new->partitions_count;
        if ((unsigned)argc > parg_max) {
            pr_error("EPT ecreate parse: Too many PARGs, you've defined %d yet only %u is supported, since there's %u used by existing partitions:\n", argc, parg_max, table_new->partitions_count);
            return 3;
        }
        struct parg_definer_helper_static dhelper;
        if (parg_parse_ecreate_mode(&dhelper, argc, argv)) {
            fputs("EPT ecreate parse: Failed to parse arguments\n", stderr);
            return 3;
        }
        uint32_t const pcount_new = util_safe_partitions_count(dhelper.count);
        struct ept_partition *part_last = table_new->partitions + table_new->partitions_count - 1;
        struct ept_partition *part;
        struct parg_definer *definer;
        for (uint32_t i = 0; i < pcount_new; ++i) {
            definer = dhelper.definers + i;
            uint32_t const pcount_r = util_safe_partitions_count(table_new->partitions_count);
            for (uint32_t j = 0; j < pcount_r; ++j) {
                if (!strncmp(definer->name, table_new->partitions[j].name, MAX_PARTITION_NAME_LENGTH)) {
                    for (uint32_t k = j; k < pcount_r - 1; ++k) {
                        table_new->partitions[k] = table_new->partitions[k+1];
                    }
                    memset(part_last, 0, sizeof *part_last);
                    --part_last;
                    --table_new->partitions_count;
                    break;
                }
            }
            part = part_last + 1;
            strncpy(part->name, definer->name, MAX_PARTITION_NAME_LENGTH);
            if (definer->set_offset) {
                if (definer->relative_offset) {
                    part->offset = part_last->offset + part_last->size + definer->offset;
                } else {
                    part->offset = definer->offset;
                }
            } else {
                part->offset = part_last->offset + part_last->size;
            }
            ept_sanitize_offset(part, capacity);
            if (definer->set_size) {
                part->size = definer->size;
            } else {
                part->size = capacity - part->offset;
            }
            ept_sanitize_size(part, capacity);
            if (definer->set_masks) {
                part->mask_flags = definer->masks;
            } else {
                part->mask_flags = 4;
            }
            part_last = part;
            if (++table_new->partitions_count > MAX_PARTITIONS_COUNT) {
                fputs("EPT ecreate parse: Too many partitions!\n", stderr);
                return 4;
            }
        }
    } else {
        fputs("EPT ecreate pasrse: No PARGs defined, using only essential partitions got from old table\n", stderr);
    }
    fputs("EPT ecreate parse: New table:\n", stderr);
    ept_checksum_table(table_new);
    ept_report(table_new);
    return 0;
}

/* ept.c: eMMC Partition Table related functions */