#include "dtb_p.h"

unsigned int dtb_checksum(struct dtb_partition *dtb) {
    // All content of the dtb partition (256K) except the last 4 byte (checksum) is sumed for checksum (thus summing 256K-4)
    unsigned int checksum = 0;
    unsigned int *dtb_as_uint = (unsigned int *)dtb;
    for (int i=0; i<DTB_PARTITION_CHECKSUM_COUNT; ++i) {
        checksum += dtb_as_uint[i];
    }
    return checksum;
}

unsigned int find_phandle(char *buffer, unsigned int buffersz) {
    for (unsigned int i=0; i<buffersz; ++i) {
        if (!strncmp(buffer + i, "phandle", 7)) {
            return i;
        }
    }
    return 0;
};

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

void prepare_layer(uint32_t layer) {
    for (uint32_t i=0; i<layer; ++i) {
        putc('.', stdout);
    }
}

uint32_t enter_node(uint32_t layer, uint32_t offset_phandle, unsigned char *dtb, uint32_t offset, uint32_t end_offset, struct dtb_header *dh) {
    if (offset >= end_offset) {
        puts("ERROR: Trying to enter a node exceeding end point");
        return 0;
    }
    // printf("Entering node at %"PRIu32" (%"PRIx32")\n", offset, offset);
    uint32_t *start = (uint32_t *)(dtb + offset);
    uint32_t *current;
    uint32_t i;
    uint32_t count = (end_offset - offset) / 4;
    // printf("Will do scan for %"PRIu32" times\n", count);
    uint32_t sub_length = 0;
    uint32_t nameoff;
    uint32_t len_prop;
    size_t len_node_name;
    char *node_name;
    bool in_node = false;
    for (i=0; i<count; ++i) { // Skip the first begin node 01
        current = start + i;
        switch (*current) {
            case DTB_FDT_BEGIN_NODE_ACTUAL:
                if (in_node) {
                    // puts("Encountered sub node");
                    sub_length = enter_node(layer + 1,offset_phandle, dtb, offset+4*i, end_offset, dh);
                    if (!sub_length) {
                        puts("ERROR: sub node no length");
                        return 0;
                    }
                    i += sub_length;
                } else {
                    prepare_layer(layer);
                    // puts("Node actual start");
                    node_name = (char *)(current+1);
                    printf("/%s\n", node_name);
                    len_node_name = strlen(node_name);
                    i += len_node_name / 4;
                    if (len_node_name % 4) {
                        i += 1;
                    }
                    in_node = true;
                }
                break;
            case DTB_FDT_PROP_ACTUAL:
                len_prop = bswap_32(*(current+1));
                i += 2 + len_prop / 4;
                if (len_prop % 4) {
                    i += 1;
                }
                prepare_layer(layer);
                nameoff = bswap_32(*(current+2));
                printf("->%s\n", dtb+(dh->off_dt_strings)+nameoff);
                if (nameoff == offset_phandle) {
                    // prepare_layer(layer);
                    printf("==>Phandle: %"PRIu32"\n", bswap_32(*(current+3)));
                }
                break;
            case DTB_FDT_NOP_ACTUAL: // Nothing to do
                break;
            case DTB_FDT_END_NODE_ACTUAL:
                if (in_node) {
                    // printf("Exiting node at %"PRIx32"\n", offset+4*i);
                    return i;
                } else {
                    puts("Error: node ends without starting");
                    return 0;
                }
        }
    }
    return 0;
}

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
    // long diff;
    unsigned int offset_phandle = find_phandle((char *)buffer + dh.off_dt_strings, dh.size_dt_strings);
    printf("Offset of phandle is %u\n", offset_phandle);
    if (!offset_phandle) {
        printf("Failed to find string offset for phandle");
        return 1;
    }
    enter_node(0, offset_phandle, buffer, dh.off_dt_struct, dh.off_dt_struct + dh.size_dt_struct, &dh);
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