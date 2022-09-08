#include "dtb_p.h"
#include <string.h>
#include "util.h"
#include <sys/stat.h>
#include <stringblock.h>
unsigned int dtb_checksum(struct dtb_partition *dtb) {
    // All content of the dtb partition (256K) except the last 4 byte (checksum) is sumed for checksum (thus summing 256K-4)
    unsigned int checksum = 0;
    unsigned int *dtb_as_uint = (unsigned int *)dtb;
    for (int i=0; i<DTB_PARTITION_CHECKSUM_COUNT; ++i) {
        checksum += dtb_as_uint[i];
    }
    return checksum;
}

off_t dtb_search_node(const unsigned char *dtb, const char *name, size_t namelen) {
    if (!namelen) {
        namelen = strlen(name);
    }
    size_t namelen_actual = util_nearest_upper_bound_ulong(namelen + 1, 4, 1); // Suppose a name is "partitions", there's 10 ascii characters in there, and it needs to be padded to nearest multiply of 4: 12, it's actually stored as "partitions\0\0"
    char name_actual[namelen_actual];
    strcpy(name_actual, name);
    for (size_t i = namelen+1; i<namelen_actual; ++i) {
        name_actual[i] = '\0'; // Padded 0, [namelen] can be skipped since it's automatically added by strcpy
    }
    struct dtb_header *dh = (struct dtb_header *)dtb;
    uint32_t count = bswap_32(dh->size_dt_struct)/4;
    uint32_t *start = (uint32_t *)(dtb + bswap_32(dh->off_dt_struct));
    for (uint32_t i=0; i<count; ++i) {
        if (start[i] == DTB_FDT_BEGIN_NODE_ACTUAL) {
            if (!memcmp(start + i + 1, name_actual, namelen_actual)) {
                return 4*i;
            }
        }
    }
    return -1;
}

// unsigned int find_phandle(char *buffer, unsigned int buffersz) {
//     for (unsigned int i=0; i<buffersz; ++i) {
//         if (!strncmp(buffer + i, "phandle", 7)) {
//             return i;
//         }
//     }
//     return 0;
// };

struct dtb_header dtb_header_swapbytes(struct dtb_header *dh) {
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

void report_dtb_header(struct dtb_header *dh) {
    fprintf(stderr, "Magic: %"PRIx32"\nTotal size: %"PRIu32"\nOffset of dt struct: %"PRIu32"\nOffset of dt string: %"PRIu32"\nOffset of memory reserve map: %"PRIu32"\nVersion: %"PRIu32"\nLast Compatible version: %"PRIu32"\nBoot CPUID PHYs: %"PRIu32"\nSize of DT strings: %"PRIu32"\nSize of DT struct: %"PRIu32"\n", bswap_32(dh->magic), bswap_32(dh->totalsize), bswap_32(dh->off_dt_struct), bswap_32(dh->off_dt_strings), bswap_32(dh->off_mem_rsvmap), bswap_32(dh->version), bswap_32(dh->last_comp_version), bswap_32(dh->boot_cpuid_phys), bswap_32(dh->size_dt_struct), bswap_32(dh->size_dt_struct));
}

void report_dtb_header_no_swap(struct dtb_header *dh) {
    fprintf(stderr, "Magic: %"PRIx32"\nTotal size: %"PRIu32"\nOffset of dt struct: %"PRIu32"\nOffset of dt string: %"PRIu32"\nOffset of memory reserve map: %"PRIu32"\nVersion: %"PRIu32"\nLast Compatible version: %"PRIu32"\nBoot CPUID PHYs: %"PRIu32"\nSize of DT strings: %"PRIu32"\nSize of DT struct: %"PRIu32"\n", dh->magic, dh->totalsize, dh->off_dt_struct, dh->off_dt_strings, dh->off_mem_rsvmap, dh->version, dh->last_comp_version, dh->boot_cpuid_phys, dh->size_dt_struct, dh->size_dt_struct);
}

// void prepare_layer(uint32_t layer) {
//     for (uint32_t i=0; i<layer; ++i) {
//         putc('.', stdout);
//     }
// }

uint32_t dtb_travel_node(const unsigned char *node, const uint32_t max_offset) {
    uint32_t count = max_offset / 4;
    if (!count) {
        return 0;
    }
    size_t len_name = strlen((const char *)node) + 1;
    uint32_t *start = (uint32_t *)node + len_name / 4 + (bool)(len_name % 4);
    uint32_t *current;
    uint32_t len_prop;
    uint32_t offset_child;
    for (uint32_t i = 0; i < count; ++i) {
        current = start + i;
        switch (*current) {
            case DTB_FDT_BEGIN_NODE_ACTUAL:
                offset_child = dtb_travel_node((unsigned char *)(current + 1), max_offset - 4*i);
                if (offset_child) {
                    i += offset_child + 1;
                } else {
                    fputs("Failed to travel through child node\n", stderr);
                    return 0;
                }
                break;
            case DTB_FDT_END_NODE_ACTUAL:
                return i + start - (uint32_t *)node;
            case DTB_FDT_PROP_ACTUAL:
                len_prop = bswap_32(*(current+1));
                i += 2 + len_prop / 4;
                if (len_prop % 4) {
                    ++i;
                }
                break;
            case DTB_FDT_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "Invalid token %"PRIu32"\n", bswap_32(*current));
                return 0;
        }
    }
    fputs("Node not properly ended\n", stderr);
    return 0;
}

uint32_t dtb_get_node(const unsigned char *node, const uint32_t max_offset, const char *name, const unsigned layers, const unsigned char **target) {
    uint32_t count = max_offset / 4;
    if (!count) {
        return 0;
    }
    if (strcmp((const char *)node, name)) {
        return dtb_travel_node(node, max_offset);
    }
    if (!layers) {
        *target = node;
        return dtb_travel_node(node, max_offset);
    }
    size_t len_name = strlen((const char *)node) + 1;
    uint32_t *start = (uint32_t *)node + len_name / 4 + (bool)(len_name % 4);
    uint32_t *current;
    uint32_t len_prop;
    uint32_t offset_child;
    for (uint32_t i = 0; i < count; ++i) {
        current = start + i;
        switch (*current) {
            case DTB_FDT_BEGIN_NODE_ACTUAL:
                offset_child = dtb_get_node((unsigned char *)(current + 1), max_offset - 4*i, name + len_name, layers - 1, target);
                if (offset_child) {
                    i += offset_child + 1;
                } else {
                    fputs("Failed to travel through child node\n", stderr);
                    return 0;
                }
                break;
            case DTB_FDT_END_NODE_ACTUAL:
                return i + start - (uint32_t *)node;
            case DTB_FDT_PROP_ACTUAL:
                len_prop = bswap_32(*(current+1));
                i += 2 + len_prop / 4;
                if (len_prop % 4) {
                    ++i;
                }
                break;
            case DTB_FDT_NOP_ACTUAL:
                break;
            default:
                fprintf(stderr, "Invalid token %"PRIu32"\n", bswap_32(*current));
                return 0;
        }
    }
    fputs("Node not properly ended\n", stderr);
    return 0;
}

unsigned char *dtb_get_node_from_path(const unsigned char *dtb, const uint32_t max_offset, const char *path, const size_t len_path) {
    if (!dtb) {
        fputs("No dtb to lookup\n", stderr);
        return NULL;
    }
    if (!max_offset) {
        fputs("Max offset invalid\n", stderr);
        return NULL;
    }
    if (max_offset % 4) {
        fputs("Offset is not multiply of 4\n", stderr);
        return NULL;
    }
    if (!path) {
        fputs("No path to lookup\n", stderr);
        return NULL;
    }
    if (!path[0]) {
        fputs("Empty path to lookup\n", stderr);
        return NULL;
    }
    if (path[0] != '/') {
        fputs("Path does not start with /\n", stderr);
        return NULL;
    }
    size_t len_path_actual;
    if (len_path) {
        len_path_actual = len_path;
    } else {
        if (!(len_path_actual = strlen(path))) { // This should not happen
            fputs("Empty path to lookup\n", stderr);
            return NULL;
        }
    }
    uint32_t *current = (uint32_t *)dtb;
    while (*current == DTB_FDT_NOP_ACTUAL) {
        ++current;
    }
    if (*current != DTB_FDT_BEGIN_NODE_ACTUAL) {
        fputs("Node does not start properly", stderr);
        return NULL;
    }
    if (len_path_actual == 1) {
        puts("Early quit");
        return (unsigned char *)(current + 1);
    }
    char *path_actual = strdup(path);
    if (!path_actual) {
        fputs("Failed to dup path\n", stderr);
        return NULL;
    }
    unsigned layers = 0;
    for (size_t i = 0; i<len_path_actual; ++i) {
        switch (path_actual[i]) {
            case '/':
                path_actual[i] = '\0';
                ++layers;
                break;
            case '\0': // This should not happend
                fputs("Path ends prematurely", stderr);
                free(path_actual);
                return NULL;
        }
    }
    unsigned char *target = NULL;
    uint32_t r = dtb_get_node((unsigned char *)(current + 1), max_offset - 4, path_actual, layers, (const unsigned char **)&target);
    free(path_actual);
    if (r) {
        return target;
    } else {
        fputs("Error occured, can not find", stderr);
        return NULL;
    }
}

// uint32_t enter_node(uint32_t layer, uint32_t offset_phandle, unsigned char *dtb, uint32_t offset, uint32_t end_offset, struct dtb_header *dh) {
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
//             case DTB_FDT_BEGIN_NODE_ACTUAL:
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
    unsigned char buffer[sz_dtb];
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
    // struct stringblock_helper shelper;
    // shelper.length = dh.size_dt_strings;
    // shelper.allocated_length = util_nearest_upper_bound_long(dh.size_dt_strings, 0x1000, 1);
    // shelper.stringblock = malloc(shelper.allocated_length);
    // if (!shelper.stringblock) {
    //     puts("Failed to allocate memory for stringblock");
    //     return 7;
    // }
    dtb_travel_node(buffer + dh.off_dt_struct + 4, dh.size_dt_struct - 4);
    unsigned char * r = dtb_get_node_from_path(buffer + dh.off_dt_struct, dh.size_dt_struct, "/partitions", 0);
    if (r){
        puts((const char *)r);
        printf("%lx\n", r - buffer);
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
    // unsigned char *partitions_p = dtb_get_node_from_path(buffer + dh.off_dt_struct, dh.size_dt_struct, "/partitions", 0, &offset);
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
    //     diff = (unsigned char *)dt_hot - buffer;
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