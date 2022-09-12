#include "table_p.h"

// struct table *table_read(const char *blockdev, const off_t offset) {
//     struct table *table = malloc(sizeof(struct table));
//     if (table == NULL) {
//         fputs("table read: Failed to allocate memory for partition table", stderr);
//         return NULL;
//     }
//     const int fd = open(blockdev, O_RDONLY);
//     if (fd < 0) {
//         fputs("table read: Failed to open underlying file/block device for partition table", stderr);
//         free(table);
//         return NULL;
//     }
//     if (lseek(fd, offset, SEEK_SET) != offset) {
//         fputs("table read: Failed to seek in underlying file/block device for partition table", stderr);
//         free(table);
//         close(fd);
//         return NULL;
//     }
//     if (read(fd, table, sizeof(struct table)) != sizeof(struct table)) {
//         fputs("table read: Failed to read into buffer", stderr);
//         free(table);
//         close(fd);
//         return NULL;
//     }
//     return table;
// }

uint32_t table_checksum(const struct table_partition *partitions, const int partitions_count) {
    int i, j;
    uint32_t checksum = 0, *p;
    for (i = 0; i < partitions_count; i++) { // This is utterly wrong, but it's how amlogic does. So we have to stick with the glitch algorithm that only calculates 1 partition if we want ampart to work
        p = (uint32_t *)partitions;
        for (j = sizeof(struct table_partition)/4; j > 0; --j) {
            checksum += *p;
            p++;
        }
    }
    return checksum;
}

int table_valid_header(const struct table_header *header) {
    int ret = 0;
    if (header->partitions_count > 32) {
        fprintf(stderr, "table valid header: Partitions count invalid, only integer 0~32 is acceppted, actual: %d\n", header->partitions_count);
        ++ret;
    }
    if (header->magic_uint32 != TABLE_HEADER_MAGIC_UINT32) {
        fprintf(stderr, "table valid header: Magic invalid, expect: %"PRIx32", actual: %"PRIx32"\n", header->magic_uint32, TABLE_HEADER_MAGIC_UINT32);
        ++ret;
    }
    bool version_invalid = false;
    for (unsigned short i=0; i<3; ++i) {
        if (header->version_uint32[i] != table_header_version_uint32[i]) {
            version_invalid = true;
            break;
        }
    }
    if (version_invalid) {
        fprintf(stderr, "table valid header: Version invalid, expect (little endian) 0x%08x%08x%08x ("TABLE_HEADER_VERSION_STRING"), actual: 0x%08"PRIx32"%08"PRIx32"%08"PRIx32"\n", TABLE_HEADER_VERSION_UINT32_2, TABLE_HEADER_VERSION_UINT32_1, TABLE_HEADER_VERSION_UINT32_0, header->version_uint32[2], header->version_uint32[1], header->version_uint32[0]);
        ret += 1;
    }
    if (header->partitions_count < 32) { // If it's corrupted it may be too large
        uint32_t checksum = table_checksum(((struct table *)header)->partitions, header->partitions_count);
        if (header->checksum != checksum) {
            fprintf(stderr, "table valid header: Checksum mismatch, calculated: %"PRIx32", actual: %"PRIx32"\n", checksum, header->checksum);
            ret += 1;
        }
    }
    return ret;
}

unsigned int table_valid_partition_name(const char *name) {
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
        fprintf(stderr, "table valid partition name: %u illegal characters found in partition name '%s'", ret, name);
        if (term) {
            fputc('\n', stderr);
        } else {
            fputs(", and it does not end properly\n", stderr);
        }
    }
    return ret;
}


int table_valid_partition(const struct table_partition *part) {
    if (table_valid_partition_name(part->name)) {
        return 1;
    }
    return 0;
}

int table_valid(struct table *table) {
    int ret = table_valid_header((struct table_header *)table);
    uint32_t valid_count = table->partitions_count < 32 ? table->partitions_count : 32;
    for (uint32_t i=0; i<valid_count; ++i) {
        ret += table_valid_partition(table->partitions+i);
    }
    return ret;
}

void table_report(struct table *table) {
    fprintf(stderr, "table report: %d partitions in the table:\n===================================================================================\nID| name            |          offset|(   human)|            size|(   human)| masks\n-----------------------------------------------------------------------------------\n", table->partitions_count);
    struct table_partition *part;
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

static inline int table_pedantic_offsets(struct table *table, uint64_t capacity) {
    if (table->partitions_count < 4) {
        fputs("table pedantic offsets: refuse to fill-in offsets for heavily modified table (at least 4 partitions should exist)\n", stderr);
        return 1;
    }
    if (strncmp(table->partitions[0].name, TABLE_PARTITION_BOOTLOADER_NAME, MAX_PARTITION_NAME_LENGTH) || strncmp(table->partitions[1].name, TABLE_PARTITION_RESERVED_NAME, MAX_PARTITION_NAME_LENGTH) || strncmp(table->partitions[2].name, TABLE_PARTITION_CACHE_NAME, MAX_PARTITION_NAME_LENGTH) || strncmp(table->partitions[3].name, TABLE_PARTITION_ENV_NAME, MAX_PARTITION_NAME_LENGTH)) {
        fprintf(stderr, "table pedantic offsets: refuse to fill-in offsets for a table where the first 4 partitions are not bootloader, reserved, cache, env (%s, %s, %s, %s instead)\n", table->partitions[0].name, table->partitions[1].name, table->partitions[2].name, table->partitions[3].name);
        return 2;
    }
    table->partitions[0].offset = 0;
    table->partitions[1].offset = table->partitions[0].size + TABLE_PARTITION_GAP_RESERVED;
    struct table_partition *part_current;
    struct table_partition *part_last = table->partitions + 1;
    for (uint32_t i=2; i<table->partitions_count; ++i) {
        part_current= table->partitions + i;
        part_current->offset = part_last->offset + part_last->size + TABLE_PARTITION_GAP_GENERIC;
        if (part_current->offset > capacity) {
            fprintf(stderr, "table pedantic offsets: partition %"PRIu32" (%s) overflows, its offset (%"PRIu64") is larger than eMMC capacity (%zu)\n", i, part_current->name, part_current->offset, capacity);
            return 3;
        }
        if (part_current->size == (uint64_t)-1 || part_current->offset + part_current->size > capacity) {
            part_current->size = capacity - part_current->offset;
        }
        part_last = part_current;
    }
    fputs("table pedantic offsets: layout now compatible with Amlogic's u-boot\n", stderr);
    return 0;
}

struct table *table_complete_dtb(struct dts_partitions_helper *dhelper, uint64_t capacity) {
    if (!dhelper) {
        return NULL;
    }
    struct table *table = malloc(sizeof(struct table));
    if (!table) {
        return NULL;
    }
    memcpy(table, &table_empty, sizeof(struct table));
    struct dts_partition_entry *part_dtb;
    struct table_partition *part_table;
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
    if (table_pedantic_offsets(table, capacity)) {
        free(table);
        return NULL;
    }
    return table;
}

struct table *table_from_dtb(uint8_t *dtb, size_t dtb_size, uint64_t capacity) {
    struct dts_partitions_helper *dhelper = dtb_get_partitions(dtb, dtb_size);
    if (!dhelper) {
        return NULL;
    }
    // dtb_report_partitions(dhelper);
    struct table *table = table_complete_dtb(dhelper, capacity);
    free(dhelper);
    if (!table) {
        return NULL;
    }
    table->checksum = table_checksum(table->partitions, table->partitions_count);
    // table_report(table);
    // printf("%d\n", table_valid(table));
    return table;
}

int table_compare(struct table *table_a, struct table *table_b) {
    if (!(table_a && table_b)) {
        fputs("table compare: not both tables are valid\n", stderr);
        return -1;
    }
    return (memcmp(table_a, table_b, sizeof(struct table)));
}

uint64_t table_get_capacity(struct table *table) {
    if (!table || !table->partitions_count) {
        return 0;
    }
    uint64_t capacity = 0;
    uint64_t end;
    struct table_partition *part;
    for (uint32_t i = 0; i < table->partitions_count; ++i) {
        part = table->partitions + i;
        end = part->offset + part->size;
        if (end > capacity) {
            capacity = end;
        }
    }
    return capacity;
}

#if 0
int main(int argc, char **argv) {
    if (argc <= 1) {
        return 1;
    }
    struct table *table = table_read(argv[1], 0);
    if (table) {
        puts("read");
        if (!table_valid(table)) {
            table_report(table);
            puts("All pass");
        }
        free(table);
    }
    table_report(&table_empty);
    return 0;
}
#endif