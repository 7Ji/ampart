# Terms
 - DT: Device Tree, a tree structure that describe the device so u-boot/kernel can behave accordingly
 - DTS: Device Tree Structure, the actual structure for DT, human-readable
 - DTB: Device Tree Blob, the file format to store DT, human-unreadble, it contains more metadata
 - FDT: Flattened Device Tree, actual format name, in the sense of ampart, this is an alias of DTB
 - plain DTB: A simple DTS stored in plain FDT format. One DTS = one file (although most of the time it's written as binary streams onto the eMMC)
 - multi DTB: Amlogic's proprietary format, zips multiple plain DTBs into one single file (although most of the time it's written as binary streams onto the eMMC)
 - gzipped DTB: On top of multi DTB, if the raw size of the multi DTB is too big (>256KiB-16B), then it's compressed as a gzip file (although most of the time it's written as binary streams onto the eMMC)
 - reserved partition: The partition to store partition table, DTB, keys, etc. It does not have a filesystem, all the data mentioned earlier are stored directly in byte format (binary streams). Usually 64M in size, and placed at 36M offset of the usable eMMC area, 32M offset relative to the bootloader partition.
 - DTB partition: The area in reserved partition, 512KiB in size, 4MiB offset relative to reserved partition's head, the partition contains two identical 256KiB copies of the same data streams, of which both the last 16B are used for metadata. So at most 256KiB-16B DTB datas (plain/multi/gzipped) can be stored.
 - EPT: eMMC Partition Table, the table stored at the beginning of the reserved partition (usually the second) on the eMMC. It has 32 partition slots, each of which can have a name of 16 characters (including terminating null byte), uses 64-bit unsigned integer as offset and size, and 32-bit unsigned integer as masks
 - pedantic EPT: An EPT created by Amlogic's u-boot, or with certain ampart modes. The first 4 partitions are forced to be bootloader(4M), reserved(64M), cache(0), env(8M). Bootlaoder starts at 0, reserved has 32M gap before it, all other partitions have 8M gap before them. All partitions must come in incremental order: the latter partition's offset can't be smaller than last partition's end point + 8M.

# Modes
All modes are in listed in a _**mode selector** (full name)_ style

## d2e (DTB to pedantic EPT)
Create pedantic EPT from the partitions node in DTB(s), behaves the same as Amlogic's u-boot

### Partition arguments:
 - No (all partition arguments will be ignored)

### Acceptable content
- DTB X (partial, the pedantic EPT can still be constructed, but the write will fail)
- Reserved √
- Disk √

### Failure points:
 - If the partitions node do not exist in the DTB, no pedantic EPT will be constructed. This can be the case if you've used the legacy version of ampart, or ceemmc to modify the EPT, as they would remove partitions node from DTBs to prevent Amlogic u-boot's EPT restoration function from working.
 - If multiple DTBs exist, yet they don't guarantee the same EPT, no construted EPT will be used

## e2d (pedantic EPT to DTB)
Restore/Recreate partitions node in DTB(s) according to a pedantic EPT

### Partition arguments:
 - No (all partition arguments will be ignored)

### Acceptable content
- DTB X (It's impossible to convert from EPT since there is not any)
- Reserved √
- Disk √

### Failture points:
 - If the EPT is not pedantic, no matter whatever the reason is, no DTB will be constructed

## pedantic (is EPT pedantic)
Check if the the EPT is a pedantic EPT, and gives advice on how to pedantize it.

### Partition arguments:
 - No (all partition arguments will be ignored)

### Acceptable content
- DTB X (It's impossible to check from EPT since there is not any)
- Reserved √
- Disk √


## eedit (EPT edit mode)
Partitions node in DTB will be removed


### Partition arguments:
 - Definer: {name}:{offset}:{size}:{masks}
 - Modifier: ^{selector}{operator}{extra}
   - Delete modifier (operator ?): ^{selector}?
   - Clone modifier (operator %): ^{selector}%{name}
   - Place modifier (operator @): ^{selector}@{placer}
     - Relative placer: ^{selector}@{placer}
       - Relative positive placer (move right): ^{selector}@+{placer}
       - Relative negative placer (move left): ^{selector}@-{placer}
     - Absolute placer: ^{selector}@(=){placer}  (= is only needed for negative)
       - Positive to place head to end (0 for first)
       - Negative to place end to head (-1 for last)
   - Adjust modifier (operator :): ^{selector}:{name}:{offset}:{size}:{masks}

### examples:
 - ``ampart --mode eedit /dev/mmcblk0 ^-1? ^cache@=-1 ^-2:my_second_lastp::-1G: lastp:::``
   - ``^-1?`` Delete the last partition
   - ``^cache@=-1`` Place cache as the last partition (Partition after the old cache are shifted left)
   - ``^-2:my_second_lastp::-1G:`` Rename the second last partition to my_second_lastp, shrinks its size by 1G
   - ``lastp:::`` Creat a partition lastp to take all the remaining space

## dedit (DTB edit mode)

### Partition arguments:
 - Definer: {name}::{size}:{masks}
 - Modifier: ^{selector}{operator}{extra}
   - Delete modifier (operator ?): ^{selector}?
   - Place modifier (operator @):



## yolo
Defining

### Partition arguments:
 - Definer: {name}:{offset}:{size}:{masks}