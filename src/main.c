// #include "common.h"
// #include "table.h"
// #include "dtb.h"
// #include "io.h"
// #include "gzip.h"
// #include "util.h"
#include "cli.h"

// #include <getopt.h>
// #include <fcntl.h>
// #include <sys/stat.h>
// #include <unistd.h>
// void teller(enum cli_partition_modify_detail_method method) {
//     switch (method) {
//         case CLI_PARTITION_MODIFY_DETAIL_PRESERVE:
//             puts("Should be preserved");
//             break;
//         case CLI_PARTITION_MODIFY_DETAIL_ADD:
//             puts("Should be add");
//             break;
//         case CLI_PARTITION_MODIFY_DETAIL_SUBSTRACT:
//             puts("Should be substarct");
//             break;
//         case CLI_PARTITION_MODIFY_DETAIL_SET:
//             puts("Should be set");
//             break;        
//     }
// }
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc, char *argv[]) {
    // if (argc <= 1) {
    //     return 1;
    // }
    // int fd = open(argv[1], O_RDONLY);
    // if (fd < 0) {
    //     return 2;
    // }
    // struct stat st;
    // if (fstat(fd, &st)) {
    //     return 3;
    // }
    // uint8_t *buffer = malloc(st.st_size);
    // if (!buffer) {
    //     return 4;
    // }
    // read(fd, buffer, st.st_size);
    // close(fd);
    
    // // printf("%x\n", *(uint32_t *)buffer);
    // struct dts_phandle_list *plist = dtb_get_phandles(buffer, st.st_size);
    // if (!plist) {
    //     return 5;
    // }
    // uint32_t min = 0, max = 0;
    // bool init = false;
    // for (uint32_t i = 0; i < plist->allocated_count; ++i) {
    //     if (plist->phandles[i]) {
    //         if (init)  {
    //             if (i<min) {
    //                 min = i;
    //             }
    //             if (i>max) {
    //                 max = i;
    //             }
    //         } else {
    //             init = true;
    //             min = i;
    //             max = i;
    //         }
    //     }
    //     printf("Handle: %u, %x; Count: %u\n", i, i, plist->phandles[i]);
    // }
    // if (!init) {
    //     return 6;
    // }
    // for (uint32_t i = plist->min_phandle; i <= plist->max_phandle; ++i) {
    //      printf("San Handle: %u, %x; Count: %u\n", i, i, plist->phandles[i]);
    // }
    // printf("Min: %u, Max: %u\n", plist->min_phandle, plist->max_phandle);
    return cli_interface(argc, argv);
    // struct cli_partition_definer *part_d = cli_parse_partition_raw("bootloader::32G:", CLI_ARGUMENT_REQUIRED, CLI_ARGUMENT_ALLOW_ABSOLUTE | CLI_ARGUMENT_ALLOW_RELATIVE | CLI_ARGUMENT_DISALLOW, CLI_ARGUMENT_ALLOW_ABSOLUTE | CLI_ARGUMENT_ALLOW_RELATIVE, CLI_ARGUMENT_ANY, 13);
    // if (part_d) {
    //     puts("parsed");
    //     printf("Name: %s, Offset: %lu, %lx (rel: %d), Size: %lu, %lx (rel: %d), Masks: %d\n", part_d->name, part_d->offset, part_d->offset, part_d->relative_offset, part_d->size, part_d->size, part_d->relative_size, part_d->masks);
    // }
#if 0
    if (argc <= 1) {
        return 1;
    }
    struct cli_partition_updater *updater = cli_parse_partition_update_mode(argv[1]);
    if (updater) {
        if (updater->modify) {
            puts("modify mode");
            if (updater->modifier.select == CLI_PARTITION_SELECT_RELATIVE) {
                puts("Relative select");
                printf("%d\n", updater->modifier.select_relative);
            } else {
                puts("name select");
                printf("%s\n", updater->modifier.select_name);
            }
            switch (updater->modifier.modify_part) {
                case CLI_PARTITION_MODIFY_PART_ADJUST:
                    puts("Adjust mode");
                    printf("Name: ");
                    teller(updater->modifier.modify_name);
                    printf("Offset: ");
                    teller(updater->modifier.modify_offset);
                    printf("Size: ");
                    teller(updater->modifier.modify_size);
                    printf("Masks: ");
                    teller(updater->modifier.modify_masks);
                    printf("%s, %lu, %lu, %u\n", updater->modifier.name, updater->modifier.offset, updater->modifier.size, updater->modifier.masks);
                    break;
                case CLI_PARTITION_MODIFY_PART_CLONE:
                    printf("Clone mode, target: %s\n", updater->modifier.name);
                    break;
                case CLI_PARTITION_MODIFY_PART_DELETE:
                    puts("Delete mode");
                    break;
                case CLI_PARTITION_MODIFY_PART_PLACE:
                    puts("Place mode");
                    switch (updater->modifier.modify_place) {
                        case CLI_PARTITION_MODIFY_PLACE_PRESERVE:
                            puts("Preserve");
                            break;
                        case CLI_PARTITION_MODIFY_PLACE_ABSOLUTE:
                            puts("Absolute");
                            break;
                        case CLI_PARTITION_MODIFY_PLACE_RELATIVE:
                            puts("Relative");
                            break;
                    }
                    printf("Placer: %d\n", updater->modifier.place);
                    break;
            }
            
            
        } else {
            puts("plain mode");
            struct cli_partition_definer *part_d = &updater->definer;
            printf("Name: %s, Offset: %lu, %lx (rel: %d), Size: %lu, %lx (rel: %d), Masks: %d\n", part_d->name, part_d->offset, part_d->offset, part_d->relative_offset, part_d->size, part_d->size, part_d->relative_size, part_d->masks);
        }
        puts("Parsed");
    }
    
    return 0;
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
    struct dtb_partition *dtb_a = (struct dtb_partition *)(inbuffer + DTB_PARTITION_OFFSET);
    struct dtb_partition *dtb_b = dtb_a + 1;
    uint8_t *dtb = NULL;
    if (dtb_checksum(dtb_a) == dtb_a->checksum) {
        puts("Using first copy");
        dtb = (uint8_t *)dtb_a;
    } else if (dtb_checksum(dtb_b) == dtb_b->checksum) {
        puts("Using second copy");
        dtb = (uint8_t *)dtb_b;
    } else {
        puts("Both invalid, defaulting to first");
        dtb = (uint8_t *)dtb_a;
    }
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
                    struct dtb_multi_entries_helper *mhelper = dtb_parse_multi_entries(dtb_raw);
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
#endif
}