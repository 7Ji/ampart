#include "table_p.h"

struct table *table_read(const char *blockdev, const off_t offset) {
    struct table *table = malloc(sizeof(struct table));
    if (table == NULL) {
        fputs("Failed to allocate memory for partition table", stderr);
        return NULL;
    }
    const int fd = open(blockdev, O_RDONLY);
    if (fd < 0) {
        fputs("Failed to open underlying file/block device for partition table", stderr);
        free(table);
        return NULL;
    }
    if (lseek(fd, offset, SEEK_SET) != offset) {
        fputs("Failed to seek in underlying file/block device for partition table", stderr);
        free(table);
        close(fd);
        return NULL;
    }
    if (read(fd, table, sizeof(struct table)) != sizeof(struct table)) {
        fputs("Failed to read into buffer", stderr);
        free(table);
        close(fd);
        return NULL;
    }
    return table;
}

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
    if (header->partitions_count > 32 || header->partitions_count < 0) {
        fprintf(stderr, "Partitions count invalid, only integer 0~32 is acceppted, actual: %d\n", header->partitions_count);
        ++ret;
    }
    if (header->magic_uint32 != TABLE_HEADER_MAGIC_UINT32) {
        fprintf(stderr, "Magic invalid, expect: %"PRIx32", actual: %"PRIx32"\n", header->magic_uint32, TABLE_HEADER_MAGIC_UINT32);
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
        fprintf(stderr, "Version invalid, expect "TABLE_HEADER_VERSION_STRING", actual: %s\n", header->version_string);
        ret += 1;
    }
    uint32_t checksum = table_checksum(((struct table *)header)->partitions, header->partitions_count);
    if (header->checksum != checksum) {
        fprintf(stderr, "Checksum mismatch, calculated: %"PRIx32", actual: %"PRIx32"\n", checksum, header->checksum);
        ret += 1;
    }
    return ret;
}

unsigned int table_valid_partition_name(const char *name) {
    unsigned int ret = 0;
    char c;
    for (unsigned int i = 0; i < 16; ++i) {
        c = name[i];
        if (c >= '0') {
            if (c >= 'A') {
                if (c >= 'a' ) {
                    if (c <= 'z') {
                        //valid
                    } else {
                        ++ret;
                    }
                } else if (c <= 'Z' || c == '_') {
                    //valid
                } else {
                    ++ret;
                }
            } else if (c <= '9') {
                //valid
            } else {
                ++ret;
            }
        // } else if (c == '-') {
            
        // }
        }
    }
    if (ret) {
        fprintf(stderr, "%u illegal characters found in partition name '%s'\n", ret, name);
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
    for (int i=0; i<table->partitions_count; ++i) {
        ret += table_valid_partition(table->partitions+i);
    }
    return ret;
}

void table_report(struct table *table) {
    fprintf(stderr, "%d partitions in the table:\n===================================================================================\nID| name            |          offset|(   human)|            size|(   human)| masks\n-----------------------------------------------------------------------------------\n", table->partitions_count);
    struct table_partition *part;
    double num_offset, num_size;
    char suffix_offset, suffix_size;
    uint64_t last_end = 0, diff;
    for (int i=0; i<table->partitions_count; ++i) {
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
    return 0;
}