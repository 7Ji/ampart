#include "common.h"
#include "table.h"
#include "dtb.h"
#include "io.h"
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
    uint8_t inbuffer[st.st_size];
    if (io_read_till_finish(fd, inbuffer, st.st_size)) {
        close(fd);
        return 4;
    }
    struct dts_partitions_helper *dhelper = dtb_get_partitions(inbuffer, st.st_size);
    if (!dhelper) {
        return 5;
    }
    dtb_report_partitions(dhelper);
    struct table *table = table_complete_dtb(dhelper, 125069950976UL);
    free(dhelper);
    if (!table) {
        return 6;
    }
    table_report(table);
    
    
    



    close(fd);
}