# Terminology / 术语
 - DT
   - Device Tree, a tree-like data structure that describe the device so u-boot/kernel can behave accordingly  
    设备树，一种可以描述设备，从而让u-boot/内核能恰当工作的树状数据结构
 - DTS
   - Device Tree Structure, the actual data structure for DT, human-readable  
   设备树结构，用于DT的实际数据结构，人类可读
 - DTB
   - Device Tree Blob (Big Large Object Binary), the file format to store DT, human-unreadble, it contains more metadata  
   设备树二进制，用于储存设备树的文件格式，人类不可读，内含有元信息
 - FDT
   - Flattened Device Tree, actual format name, in the sense of ampart, this is an alias of DTB  
   扁平设备树，实际格式名，在ampart里，和DTB同义
 - plain DTB / 普通TDB
   - A simple DTS stored in plain FDT format. One DTS = one file (although most of the time it's written as binary streams onto the eMMC)  
   一个以FDT格式储存的简单DTS。一个DTS等于一个文件（不过大多数时候都是以二进制流的形式存在eMMC上）
 - multi DTB / 多DTB
   - Amlogic's proprietary format, zips multiple plain DTBs into one single file (although most of the time it's written as binary streams onto the eMMC)  
   Amlogic专有的格式，把多个普通DTB整合成单一的文件（不过大多数时候都是作为二进制流的形式存在eMMC上）
 - gzipped DTB / 压缩DTB
   - On top of multi DTB, if the raw size of the multi DTB is too big (>256KiB-16B), then it's compressed as a gzip file (although most of the time it's written as binary streams onto the eMMC)  
   在多DTB的基础上，如果体积太大（大于256KiB-16B），那么就会被压缩位一个gzip文件（不过大多数时候都是作为二进制流的形式存在eMMC上）
 - reserved partition / 保留分区
   - The partition in EPT (usually the second entry) to store partition table, DTB, keys, etc. It does not have a filesystem, all the data mentioned earlier are stored directly in byte format (binary streams). Usually 64MiB in size, and placed at 36MiB offset of the eMMC user area, 32M offset relative to the bootloader partition's end.  
   EPT里用来储存分区表，DTB，密钥等的分区（一般是第二个分区）。并没有文件系统，所有前述数据都直接以二进制的形式（二进制流）存在里面。通常大小是64MiB，并且放在eMMC用户区域的36MiB偏移位置，相对于引导程序分区偏差32M
 - DTB partition / DTB分区
   - The area in reserved partition, 512KiB in size, 4MiB offset relative to reserved partition's head, the partition contains two identical 256KiB copies of the same data streams, of which both the last 16B are used for metadata. So at most 256KiB-16B DTB datas (plain/multi/gzipped) can be stored.
   - 保留分区中的区域，大小512KiB，相对于保留分区的开头偏差4MiB，这个分区里包含两份一样的256KiB的同一个数据流的副本，两者都是最后16B用来储存元数据。所以至多可以储存256KiB-16B的DTB数据（普通，多DTB，或者gzip压缩过的）。
 - EPT
   - eMMC Partition Table, the table stored at the beginning of the reserved partition on the eMMC. It has 32 partition slots, each of which can have a name of 15 characters / 16 bytes (including terminating null byte), uses 64-bit unsigned integer as offset and size, and 32-bit unsigned integer as masks  
   eMMC分区表，储存在eMMC上保留分区的开头。有32个可用分区，每个都能有最长15个字符/16个比特（包含终止位空比特）长的名字，用64位无符号整型作为偏移和大小，32位无符号整型作为遮罩
 - pedantic EPT / 迂腐EPT
   - An EPT created by Amlogic's u-boot, or with certain ampart modes. The first 4 partitions are forced to be bootloader(4MiB), reserved(64MiB), cache(0), env(8MiB). Bootlaoder starts at 0, reserved has 32MiB gap before it, all other partitions have 8MiB gap before them. Partitions in the `partitions` node in DTB are used to populate the EPT, latter replacing former. All partitions must come in incremental order: the latter partition's offset can't be smaller than last partition's end point + 8MiB.
   - Amlogic的u-boot或者是特定ampart模式创建的EPT。首四个分区必需是引导程序（4MiB），保留（64MiB），缓存（0），环境（8MiB）。引导程序分区自0开始，保留分区前有32MiB空隙，所有其他的分区前都有8MiB空隙。DTB里`partitions(分区)`节点的分区用来补全EPT，后者替换前者。所有分区必须以升序排列：后面的分区的偏移不嫩比前一个分区的结尾+8MiB小