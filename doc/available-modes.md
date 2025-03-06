# Available Modes
[中文文档](available-modes_cn.md)
## Summary
|mode|usage|dtb|reserved|disk|
|-|-|-|-|-|
|dtoe|generate pedantic EPT from DTB|X|√|√|
|etod|generate DTB partitions node from EPT|X|√|√|
|epedantic|check if EPT is pedantic|X|√|√|
|dedit|edit partitions in DTB, update EPT when necessary|√|√|√|
|eedit|edit EPT, update DTB when necessary|X|√|√|
|dsnapshot|take snapshot of DTB partitions|√|√|√|
|esnapshot|take snapshot of EPT|X|√|√|
|webreport|report partitions using [ampart-web-reporter]|X|√|√|
|dclone|restore a snapshot taken in dsnapshot mode|√|√|√|
|eclone|restore a snapshot taken in esnapshot mode|X|√|√|
|ecreate|create a EPT in a YOLO way|X|√|√|

_dtb, reserved, disk columns stand for whether the mode accept the content with that type_

All modes are in listed in a _**mode selector** (full name)_ style

## dtoe (DTB to pedantic EPT)
Create pedantic EPT from the partitions node in DTB(s), behaves the same as Amlogic's u-boot

### Partition arguments
 - No (all partition arguments will be ignored)

### Acceptable content
 - DTB X
 - Reserved √
 - Disk √

### Possible failure points:
 - If the partitions node do not exist in the DTB, no pedantic EPT will be constructed. This can be the case if you've used the legacy version of ampart, or ceemmc to modify the EPT, as they would remove partitions node from DTBs to prevent Amlogic u-boot's EPT restoration function from working.
 - If multiple DTBs exist, yet they don't guarantee the same EPT, no construted EPT will be used

## etod (pedantic EPT to DTB)
Restore/Recreate partitions node in DTB(s) according to a pedantic EPT

### Partition arguments:
 - No (all partition arguments will be ignored)

### Acceptable content
- DTB X
- Reserved √
- Disk √

### Possible failure points:
 - If the EPT is not pedantic, no matter whatever the reason is, no DTB will be constructed

## epedantic (is EPT pedantic)
Check if the the EPT is a pedantic EPT, and gives advice on how to pedantize it.

### Partition arguments:
 - No (all partition arguments will be ignored)

### Acceptable content
 - DTB X
 - Reserved √
 - Disk √

## dedit (DTB edit mode)
Edit partitions node in DTB, and update the DTB. The EPT will also be updated if the target is either Reserved or Disk and the corresponding pedantic EPT is different from the existing one.

### Partition arguments:
 - Definer: {name}::{size}:{masks}
   - {name} must be set explicitly
   - ~~{offset}~~ must **not** be set
   - {size} must be set explicitly as absolute value
   - {masks} must be set explicitly
 - Modifier: ^{selector}{operator}{suffix}
   - Selector {selector} must be set explicitly
     - Name selector {name} should be an existing partition name
     - Relative selector {relative}
       - Non-negative to select from start (0 as first)
       - Negative to select from end (-1 as last)
   - Operator {operator}
     - Delete operator ?
     - Clone operator %
       - Suffix: Cloner {name}
         - {name} must be set explicitly
     - Place operator @
       - Suffix: Placer {place}
         - Absolute placer (=){non-negative} / ={negative}
           - Forward absolute placer (=){non-negative}, 0 as first
           - Backwards absolute placer ={negative}, -1 as last
         - Relative placer +{non-negative} / -{non-negative}
           - Move-right relative placer +{non-negative}
           - Move-left relative placer -{non-negative}
     - Adjust operator :
       - Suffix: Adjustor {name}::{size}:{masks}
         - {name} is optional
         - ~~{offset}~~ must **not** be set
         - {size} is optional
           - -{non-negative} substract
           - +{non-negative} add
           - {non-negative} set
         - {masks} is optional

### Acceptable content
- DTB √
- Reserved √
- Disk √

## eedit (EPT edit mode)
Edit EPT. If the result EPT is pedantic, also update partitions node in DTB; otherwise, remove partitions node in DTB.

### Partition arguments:
 - Definer: {name}:{offset}:{size}:{masks}
   - {name} must be set explicitly
   - {offset} is optional
     - +{non-negative} relative offset to the last partition's end
     - {non-negative} absolute offset to the whole disk
   - {size} is optional, and should only be set as absolute
     - If not set, autofill the remaining space
   - {masks} is optional
     - If not set, defaults to 4
 - Modifier: ^{selector}{operator}{suffix}
   - Selector {selector} must be set explicitly
     - Name selector {name} should be an existing partition name
     - Relative selector {relative}
       - Non-negative to select from start (0 as first)
       - Negative to select from end (-1 as last)
   - Operator {operator}
     - Delete operator ?
     - Clone operator %
       - Suffix: Cloner {name}
         - {name} must be set explicitly
     - Place operator @
       - Suffix: Placer {place}
         - Absolute placer (=){non-negative} / ={negative}
           - Forward absolute placer (=){non-negative}, 0 as first
           - Backwards absolute placer ={negative}, -1 as last
         - Relative placer +{non-negative} / -{non-negative}
           - Move-right relative placer +{non-negative}
           - Move-left relative placer -{non-negative}
     - Adjust operator :
       - Suffix: Adjustor {name}:{offset}:{size}:{masks}
         - {name} is optional
         - {offset} is optional
           - -{non-negative} substract
           - +{non-negative} add
           - {non-negative} set
         - {size} is optional
           - -{non-negative} substract
           - +{non-negative} add
           - {non-negative} set
         - {masks} is optional

### Acceptable content
- DTB X
- Reserved √
- Disk √

### examples:
 - ``ampart --mode eedit /dev/mmcblk0 ^-1? ^cache@=-1 ^-2:my_second_lastp::-1G: lastp:::``
   - ``^-1?``
     - Delete the last partition
   - ``^cache@=-1``
     - Place cache as the last partition (Partition after the old cache are shifted left)
   - ``^-2:my_second_lastp::-1G:`` 
     - Rename the second last partition to `my_second_lastp`, shrinks its size by 1G
   - ``lastp:::``
     - Creat a partition lastp to take all the remaining space

## dsnapshot (DTB snapshot mode)
Take a snapshot of the partitions node in DTB and print it to standard output, 3 inter-changable snapshots seperated by `\n`(new-line character) in the order of decimal, hex, human-readable (decimal). The snapshots can later be used in `dclone` mode to get identical partitions

### Partition arguments:
 - No (all partition arguments will be ignored)

### Acceptable content
- DTB √
- Reserved √
- Disk √

## esnapshot (EPT snapshot mode)
Take a snapshot of the EPT and output it to standard output, 3 inter-changable snapshots in the order of decimal, hex, human-readable (decimal). The snapshots can later be used in `eclone` mode to get identical partitions

### Partition arguments:
 - No (all partition arguments will be ignored)

### Acceptable content
- DTB X
- Reserved √
- Disk √

## webreport (WEB report mode)
Generate a URL on standard output that can be opened on browser to get a well-formatted webpage of partition info, powered by my another project [ampart-web-reporter]. The device running this mode does not need to be online.

[ampart-web-reporter]: https://github.com/7Ji/ampart-web-reporter

### Partition arguments:
 - No (all partition arguments will be ignored)

### Acceptable content
- DTB X
- Reserved √
- Disk √

## dclone (DTB clone mode)
Restore a snapshot taken in `dsnapshot` mode, or also define the partitions if you like. If the target is Reserved or Disk, and the corresponding pedantic EPT is different from the existing EPT, the EPT will also be updated

### Partition arguments
 - Definer: {name}::{size}:{masks}
   - {name} must be set explicitly
   - ~~{offset}~~ must **not** be set
   - {size} must be set explicitly as absolute value
     - A special value `-1` means auto-fill
   - {masks} must be set explicitly

### Acceptable content
- DTB √
- Reserved √
- Disk √

### Note
On HK1 Rbox X4, the u-boot reads some important partition info from DTB, this is the main reason I re-wrote ampart to implement this mode. The three non-data partitions listed below (``boot_a::64M:1 vbmeta_a::2M:1 vbmeta_system_a::2M:1``) are the essential partitions
```
./ampart --mode dclone /dev/mmcblk0 --migrate all boot_a::64M:1 vbmeta_a::2M:1 vbmeta_system_a::2M:1 data::-1:4
```

## eclone (EPT clone mode)
Restore a snapshot taken in `esnapshot` mode, or also define the partitions if you like. If the result EPT is pedantic, also update partitions node in DTB if differnet; otherwise, remove the partitions node in DTB

### Partition arguments:
 - Definer: {name}:{offset}:{size}:{masks}
   - {name} must be set explicitly
   - {offset} must be set explicitly as absolute
   - {size} must be set explicitly as absolute
   - {masks} must be set explicitly

### Acceptable content
- DTB X
- Reserved √
- Disk √

## ecreate (EPT create mode)
Defining a table in a yolo way, you don't need to care about essential partitions, their info will be retrieved from old table and optimized. Partitions node in DTB **will be 100% removed**

### Partition arguments:
 - Definer: {name}:{offset}:{size}:{masks}
   - {name} must be set explicitly
   - {offset} is optional
     - +{non-negative} relative offset to last partition'end
     - {non-negative} absolute offset to the whole drive
   - {size} is optional, and it should be set as absolute
     - If not set, auto-fill the remaning space
   - {masks} is optional
     - If not set, defaults to 4

### Acceptable content
- DTB X
- Reserved √
- Disk √