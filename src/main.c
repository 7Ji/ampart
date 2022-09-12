#include "common.h"
#include "table.h"
#include "dtb.h"
#include "io.h"
#include "gzip.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc, char **argv) {
    if (argc <= 1) {
        return 1;
    }
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        return 2;
    }
    struct stat st;
    if (fstat(fd, &st)) {
        close(fd);
        return 3;
    }
    uint8_t *inbuffer = malloc(st.st_size);
    if (!inbuffer) {
        return 4;
    }
    if (io_read_till_finish(fd, inbuffer, st.st_size)) {
        close(fd);
        return 5;
    }
    close(fd);
    struct table *table_emmc = (struct table *)inbuffer;
    if (table_valid(table_emmc)) {
        free(inbuffer);
        return 6;
    }
    table_report(table_emmc);
    uint64_t capacity = table_get_capacity(table_emmc);
    uint8_t *dtb = inbuffer + DTB_PARTITION_OFFSET;
    enum dtb_type dtb_type = dtb_identify_type(dtb);
    struct table *table_dtb;
    if (dtb_type == DTB_TYPE_PLAIN) {
        table_dtb = table_from_dtb(dtb, DTB_PARTITION_SIZE, capacity);
        table_report(table_dtb);
        if (table_compare(table_emmc, table_dtb)) {
            puts("Different");
        } else {
            puts("same");
        }
        free(table_dtb);
    } else if (dtb_type == DTB_TYPE_MULTI) {
        struct dtb_multi_entries_helper *mhelper = dtb_parse_multi_entries(dtb);
        if (mhelper) {
            for (uint32_t i = 0; i<mhelper->entry_count; ++i) {
                table_dtb = table_from_dtb(mhelper->entries[i].dtb, mhelper->entries[i].size, capacity);
                table_report(table_dtb);
                if (table_compare(table_emmc, table_dtb)) {
                    puts("Different");
                } else {
                    puts("same");
                }
                free(table_dtb);
            }
        }
    } else if (dtb_type == DTB_TYPE_GZIPPED) {
        uint8_t *dtb_raw;
        size_t dtb_raw_size = gzip_unzip(dtb, DTB_PARTITION_DATA_SIZE, &dtb_raw);
        if (dtb_raw_size) {
            enum dtb_type dtb_raw_type = dtb_identify_type(dtb_raw);
            switch (dtb_raw_type) {
                case DTB_TYPE_PLAIN:
                    puts("r: plain");
                    break;
                case DTB_TYPE_MULTI:
                    // struct dtb_multi_entries_helper *mhelper = dtb_parse_multi_entries(dtb_raw);
                    // if (mhelper) {
                    //     for (uint32_t i = 0; i<mhelper->entry_count; ++i) {
                    //         printf("%x\n", *(uint32_t *)(mhelper->entries[i]));
                    //         table_dtb = table_from_dtb(mhelper->entries[i], DTB_PARTITION_SIZE, capacity);
                    //     }
                    // }
                    puts("r: multi");
                    break;
                default:
                    puts("r: fuckit");
                    break;
            }
        } else {
            puts("Failed to decompress");
        }
        
    }
    // free(table_emmc);
    free(inbuffer);

    

    // struct table *table = table_from_dtb(inbuffer, st.st_size);
    // if (table) {
    //     table_report(table);
    //     free(table);

    // }
    
    return 0;
}