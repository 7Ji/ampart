# Amlogic emmc partition tool
**ampart** is a partition tool written for **HybridELEC**(a project that brings side-by-side dual-booting support to **CoreELEC**+**EmuELEC**) for easy re-partitioning of internal emmc device for **almost all Amlogic devices** to liberate me from editing the device tree for every device just to achive a custom partition layout. It is written totally in **C** for portability and therefore **simple, fast yet reliable**, and provides a simple yet powerful argument-driven CLI for easy implementation in scripts.  

Everything is done in a **single session**, without any **repeated execution** or **reboot**  

***
## Usage
**ampart** does not contain any interactive CLI. This is perfect for implementation in scripts but does not mean the user won't be able to understand what is happening.

**ampart** works in 3 modes: 
* **normal mode**(default): New partitions other than those reserved should be defined, i.e. a partition table without bootloader, reserved, env should be described by users. Most of the stuff are auto-generated, users do not need to know the detail of the underlying disk.
  * Minor details will differ depend on the old partition table, ampart would try its best to preserve the reserved partitions, and then create user-defined partitions
  * A straight-forward pure CoreELEC/EmuELEC installation can be achived in this way.
* **clone mode**: All partitions should be defined, i.e. a partition table with bootloader, reserved, env should be described by users, in an explicit way, almost all details of the part table can be modified as long as they are legal. This mode can be used to restore a snapshot or utilized by scripts that want to achive their specific partition layout.   
  * The output of clone mode will be a 1:1 binary replica of what you would get from a ampart snapshot taken via ``ampart --snapshot``
  * Scripts can operate in this mode for specific devices, if the emmc capacity is believed to be always the same, i.e. ``ampart /dev/mmcblk0 --clone bootloader:0B:4M:0 reserved:36M:64M:0 cache:108M:512M:2 env:628M:8M:0 logo:644M:32M:1 recovery:684M:32M:1 rsv:724M:8M:1 tee:740M:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32M:1 system:876M:2G:1 data:2932M:4524M:4`` would always update the partition layout to be 100% the same
  * Android updates would be possible if you don't mess up with all of the parts before data
* **update mode**: Work on the old partition layout and add minor tweaks here and there. 
  * All partitons not defined in the update array will not be affected
  * Scripts can operate in this mode if they just want some shrink and addition. i.e. ``ampart /dev/mmcblk --update data::-2G: hybrid:::`` would shrink the data partition by 2G, and create a ``hybrid`` partition after it to take all of the new free space. 
  * Android updates will always be possible as long as you keep your new parts after data
  * Updates happend on the **partition level**, not **filesystem level**, corresponding data backup and recovery and fs-createion needs to be done with other tools

All of the above modes will both update the partition table itself and block devices under /dev, i.e. you would expect a /dev/whatever to be present immediately after you create it.

The command-line usage of **ampart** is very simple:

````
ampart [reserved/emmc] ([partition] ...) ([option [optarg]]...) 
````
In which the positional arguments are:
*Arguments with \* are required*
* **reserved/emmc\*** the path to ``/dev/reserved`` or ``/dev/mmcblkN`` or any image file you dumped from them (e.g. via ``dd if=/dev/mmcblk0 of=emmc.img``, or ``dd if=/dev/reserved of=reserved.img``).  
Whether it's a whole emmc disk or a reserved partition will be identified by its name and header, but you can also force its type by options ``--reserved`` / ``-r`` or ``--disk`` / ``-d``  
* **partition**(s) (normal/clone mode) partition(s) you would like to create, when omitted **ampart** will only print the old partition table. Should be in format **name**:**offset**:**size**:**mask**. In which:  
    * **name\*** the partition name, supports **a**-**z**, **A**-**Z** and **_**(underscore), 15 characters at max.  
    e.g. boot, system, data  
    ***A**-**Z** and **_** are supported but not **suggested**, as it **may confuse some kernel and bootloader***    
    reserved names (bootloader, reserved, env) can only be set in **clone mode**
    * **offset** the partition's **absolute**(*without + prefix*) or  **relative**(*with + prefix*) offset (not supported in clone mode). interger with optional *B/K/M/G* suffix *(for byte, kibibyte=1024B, mebibyte=1024K, gibibytere=1024M spectively)*.   
        1. when omitted in **normal mode**, equals to **last partition's end**
        2. without prefix **+** for offset to **the disk's start**. e.g. 2G, will place partition at 2G
        3. with prefix **+** for offset to **last partition's end**. e.g. +8M, will place partiton at **last partition's end + 8M**, meaning there will be an **8M gap** between them  
        4. will be **rounded up** to **multiplies of 4096 (4KiB)** for **4K alignment**. e.g. **2047** will be rounded up to **4096**=**4K**, **1023K** will be rounded up to **1024K**  
        5. can not be omitted in **clone mode**
    * **size** the partition's size. interger with optional *B/K/M/G* suffix (like offset).
        1. when omitted in **normal mode**, size will equal to **all of free space after the last partition**
        2. will be **rounded up** to **multiplies of 4096 (4KiB)** for **4K alignment** just like offset
        3. can not be omitted in **clone mode**
    * **mask** the partition's mask. either 0, 1, 2 or 4,   
    *it is **only useful for u-boot** to figure out which partition should be cleaned *(0-filled)* when you flash a USB Burning Tool and choose **normal erase**. It really doesn't make much sense if you are not running in clone mode as you've modified the partition table and Android images won't work anymore unless you choose **total erase**.* Default is 4.
        * 0 for reserved partitions, **only** valid in **clone mode**
        * 1 for u-boot partitions, e.g. logo
        * 2 for system partitions, these partitions will always be cleaned whether **normal erase** is chosen or not when you flash an **Android image** via **USB Burning Tool**.
        * 4 for data partitions, these partitions will only be cleaned if you choose **total erase** when you flash an **Android image** via **USB Burning Tool**
        * can not be omitted in **clone mode**

    e.g.

    1. ``system:+8M:2G:2`` A partition named system, leave an 8M gap between it and the last partition, size is 2G, mask is 2  
    2. ``data:::``  A partition named data, no gap before it, uses all the remaining space, mask is 4

    **Note**: the **order** of partitions matters, e.g. 
    1. ``system:+8M:2G:2 data::::`` will work if disk's size is greater than 2G+8M+size of reserved partitions
    2.  ``data::: system:+8M:2G:2`` won't work because data has already taken all of the remaining space yet system wants 2G, and it wants to start at the disk's endpoint + 8M.
    3. ``data::: data2::: data3::: ...`` works, data2, data3, ... will all be 0-sized partitions as data has taken all of the remaining space

    **Note2**: partitions must be created **incrementally**, a new partition's *start point* **can not** be **smaller** than the previous one's *end point*. e.g. 
    1. ``system::2G: data:0:2G:`` won't work as data's offset is smaller than system's end point (2G + reserved partitions)
    2. ``system::2G: data:+0:2G:``will work and it's the same as ``system::2G: data::2G:``

    **Warning**: unless running in clone mode, some partitions are **reserved** and **can't be defined by user**, they will be **generated** according to the old partition table:  
    1. **bootloader** This is where **u-boot** is stored, and it should always be the **first 4M** of the emmc device. It **won't** be touched.
    2. **reserved** This is where **partition table**, **dtb** and many other stuffs are stored, it usually starts at 36M and leaves an 32M gap between it and **bootloader** partition. It **won't** be touched.
    3. **env** This is where u-boot envs are stored, it is usually 8M, and placed after a cache partition. It **will be moved** as the cache partition takes 512M and does not make much sense for a pure ELEC installation.  
      1. Partitions defined by users will start at **end of reserved partition** + **size of env partition** as a result of this, to maximize the usable disk space by avoiding those precious ~512M taken by cache partition.   
      2. **ampart** will clone the content of the env partition from its old location to the new location, and the content at the new location will be the same on the **binary level**, but you can always ``dd if=/dev/env of=env.img`` for double insurance.
  

* **partition**(s) (update mode) partition(s) you would like to update, when omitted **ampart** will only print the old partition table. Should be in format **selector(operator)**(:**name**:**offset**:**size**:**mask**). In which:  
  * **selector** partition name existing in the old table, or positive interger to select a part from the beginning, or negative interger to select a part from the end.   
  If omitted, creates a partition according to the remaining args just like in normal mode  
  e.g.   
    * -1 selects the last partition
    * 2 selects the second partition (in most cases this would be reserved partition)
    * data selects the data partition
    * *(empty)* selects nothing, create a part according to the remaining args
  * **(operator)** optional suffix of selector that defines special behaviour:
    * ! deletes the selected partition, ignore other args
    * ^ clone the selected partition, give it a name defined in **name**, ignore other args
  * **name** unless operator is ^, is the same as normal/clone mode, would update parts name if set, or do nothing if omitted
  * **offset** integer number with optional +/- prefix and optional B/K/M/G suffix. Without prefix for replacing offset, with prefix for increasing/decreasing offset
  * **size** integer number with optional +/- prefix and optional B/K/M/G suffix. Without prefix for replacing size, with prefix for increasing/decreasing size
  * **mask** either 0, 1, 2 or 4, same as clone mode

  e.g.
  * ``-1!`` deletes the last partition
  * ``-1:::-512M:`` shrinks the last part by 512M
  * ``-1^:CE_STORAGE`` clones the last part, create a partition after it with the same offset, size, and mask and name it ``CE_STORAGE``
  * ``data^CE_STORAGE`` clones the data partition like the last one, except we select by name this time
  * ``:CE_FLASH::512M:`` creates a new partition ``CE_FLASH`` with size 512M

  The following one-line command achives a same partition layout as what ceemmc in dual-boot mode would do:
  ````
  ampart --update /dev/mmcblk0 -1:::-512M: -1^:CE_STORAGE :CE_FLASH::512M:
  ````
  And partition /dev/CE_STORAGE and /dev/CE_FLASH are available right after you've executed the command, you can then immediately mount and edit their content as you like.

And options are:
* **--version**/**-v** will print the version info
  * ampart will early quit *(return 0 for success)*
* **--help**/**-h** will print a help message
  * ampart will early quit *(return 0 for success)*
* **--disk**/**-d** will force ampart to treat input as a whole emmc disk
  * **conflicts** with --reserved/-r
* **--reserved**/**-r** will force ampart to treat input as a reserved partition
  *  **conflicts** with --disk/-d
* **--offset**/**-O** **[offset]** will overwrite the default offset of the reserved partition
  * **only valid** when input is a whole emmc disk
  * only make sense if your OEM has modified it (e.g. Xiaomi set it to 4M, right after the bootloader)
  * default: 36M
* **--snapshot**/**-s** outputs partition arguments that can be used to restore the partition table to what it looks like now, or as a start point for scripts that want to modify the part table in their way, early quit
  * ampart will quit after the part args are printed *(return 0 for success)*
  * scripts can parse the last two lines to get the part table, the offset/size of the first one is always in byte, without suffix; the last one is human-readable for users to record  
  e.g. (the human-readable output)
    ````
    bootloader:0:4194304:0 reserved:37748736:67108864:0 cache:113246208:536870912:2 env:658505728:8388608:0 logo:675282944:33554432:1 recovery:717225984:33554432:1 rsv:759169024:8388608:1 tee:775946240:8388608:1 crypt:792723456:33554432:1 misc:834666496:33554432:1 boot:876609536:33554432:1 system:918552576:2147483648:1 data:3074424832:4743757824:4
    ````
    (the machine-friendly output)
    ````
    bootloader:0B:4M:0 reserved:36M:64M:0 cache:108M:512M:2 env:628M:8M:0 logo:644M:32M:1 recovery:684M:32M:1 rsv:724M:8M:1 tee:740M:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32M:1 system:876M:2G:1 data:2932M:4524M:4
    ````
   
* **--clone**/**-c** enable clone mode, only verify some args of the parts, partition arguments must be set **explicitly**
  * reserved partitions can be defined and should be defined (bootloader, reserved, env)
  * name, offset, size, mask must be **explicit set**
  * users can pass the partition arguments generated by a previous **snapshot** session to restore the part to exactly what it was like back then, **byte-to-byte correct**
  * scripts can call ampart with modified args previously acquired by a **snapshot** call, and apply an updated part table, they can either keep almost all of the table not affected, or do a complete overhaul: you want the freedom, ampart gives you.
* **--update**/**-u** enable update mode, [partitions] describe how you would like to modify some parts' info
* **--dry-run**/**-D** will only generate the new partition table but not actually write it, this will help you to make sure the new partition table is desired.
  * ampart will quit before commiting changes *(return 0 for success)*
* **--output**/**-o** **[path]** will write the ouput table to somewhere else rather than the path you set in [reserved/mmc]
  * **currently is not implemented**

**Warning:** Changes will be written to disk without further asking when:
  * you don't specify ``--dry-run``/``-D`` 
  * you have provided valid partition arguments
  * the new partition table is valid

So you'd better make sure the new partition table is desired by option ``--dry-run``/``-D`` first

New paritions should be available immediately under /dev after partitioning, **without reboot**

**Warning**: ampart won't check if the partitions are in used or not, users should umount the old parts by themselvies if they don't want any data loss. **this also mean you can partition the emmc being used by system even if your system is running from it.** This is a **feature**, not a **bug**, you told ampart to do so and **you yourself should take the responsibility**.

All of the above would seem complicated but the CLI is really **clean and easy**, e.g.   
````
# Print the partition table of /dev/mmcblk0
ampart /dev/mmcblk0
# Print the partition table of underlying disk of /dev/reserved
ampart /dev/reserved
# Create a single data partition to utilize all the space
ampart /dev/mmcblk0 data:::
# Create a 2G system partition, mask2, and a data partition to utilize all the remaining space
ampart /dev/mmcblk0 system::2G:2 data:::
# Same as the last one, except we leave a 8M gap before the 2 partitions, like some OEMs would do
ampart /dev/mmcblk0 system:+8M:2G:2 data:+8M::
# A layout for EmuELEC
ampart /dev/mmcblk0 system::2G:2 data::2G: eeroms:::
# Recreate a previous taken snapshot
ampart /dev/mmcblk0 --clone bootloader:0B:4M:0 reserved:36M:64M:0 cache:108M:512M:2 env:628M:8M:0 logo:644M:32M:1 recovery:684M:32M:1 rsv:724M:8M:1 tee:740M:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32M:1 system:876M:2G:1 data:2932M:4524M:
````
(*In all of the commands above, those which create new partition tables will clear all partitions, copy the old partition info of bootloader, reserved and env as partition 0-2, then change the offset of env to the end of reserved, then clone the content of the env partition if it's moved.*)

## Examples
Print the table when an Android installation is on an 8G emmc:    

    CoreELEC:~ # ampart /dev/mmcblk0
    Path '/dev/mmcblk0' is a device, getting its size via ioctl
    Disk size is 7818182656 (7.28G)
    Reading old partition table...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Validating partition table...
    Partitions count: 13, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated 3d5695ff, recorded 3d5695ff, GOOD √
    Partition table read from '/dev/mmcblk0':
    ==============================================================
    NAME                          OFFSET                SIZE  MARK
    ==============================================================
    bootloader               0(   0.00B)    400000(   4.00M)     0
      (GAP)                                2000000(  32.00M)
    reserved           2400000(  36.00M)   4000000(  64.00M)     0
      (GAP)                                 800000(   8.00M)
    cache              6c00000( 108.00M)  20000000( 512.00M)     2
      (GAP)                                 800000(   8.00M)
    env               27400000( 628.00M)    800000(   8.00M)     0
      (GAP)                                 800000(   8.00M)
    logo              28400000( 644.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    recovery          2ac00000( 684.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    rsv               2d400000( 724.00M)    800000(   8.00M)     1
      (GAP)                                 800000(   8.00M)
    tee               2e400000( 740.00M)    800000(   8.00M)     1
      (GAP)                                 800000(   8.00M)
    crypt             2f400000( 756.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    misc              31c00000( 796.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    boot              34400000( 836.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    system            36c00000( 876.00M)  80000000(   2.00G)     1
      (GAP)                                 800000(   8.00M)
    data              b7400000(   2.86G) 11ac00000(   4.42G)     4
    ==============================================================
    Disk size totalling 7818182656 (7.28G) according to partition table
    Using 7818182656 (7.28G) as the disk size
Print the table when ``ceemmc`` has altered the partition table and **bricked the box**:

    CoreELEC:~ # ampart /dev/mmcblk0
    Path '/dev/mmcblk0' is a device, getting its size via ioctl
    Disk size is 7818182656 (7.28G)
    Reading old partition table...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Validating partition table...
    Partitions count: 13, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated 3d5695ff, recorded 3d5695ff, GOOD √
    Partition table read from '/dev/mmcblk0':
    ==============================================================
    NAME                          OFFSET                SIZE  MARK
    ==============================================================
    bootloader               0(   0.00B)    400000(   4.00M)     0
      (GAP)                                2000000(  32.00M)
    reserved           2400000(  36.00M)   4000000(  64.00M)     0
      (GAP)                                 800000(   8.00M)
    cache              6c00000( 108.00M)  20000000( 512.00M)     2
      (GAP)                                 800000(   8.00M)
    env               27400000( 628.00M)    800000(   8.00M)     0
      (GAP)                                 800000(   8.00M)
    logo              28400000( 644.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    recovery          2ac00000( 684.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    rsv               2d400000( 724.00M)    800000(   8.00M)     1
      (GAP)                                 800000(   8.00M)
    tee               2e400000( 740.00M)    800000(   8.00M)     1
      (GAP)                                 800000(   8.00M)
    crypt             2f400000( 756.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    misc              31c00000( 796.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    boot              34400000( 836.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    system            36c00000( 876.00M)  80000000(   2.00G)     1
      (GAP)                                 800000(   8.00M)
    data              b7400000(   2.86G)  fac00000(   3.92G)     4
      (OVERLAP)                           fac00000(   3.92G)
    CE_STORAGE        b7400000(   2.86G)  fac00000(   3.92G)     4
    CE_FLASH         1b2000000(   6.78G)  20000000( 512.00M)     4
    ==============================================================
    Disk size totalling 7818182656 (7.28G) according to partition table
    Using 7818182656 (7.28G) as the disk size `1
Format a single big chunky data partition on emmc:

    CoreELEC:~ # ampart /dev/mmcblk0 data:::
    Path '/dev/mmcblk0' is a device, getting its size via ioctl
    Disk size is 7818182656 (7.28G)
    Reading old partition table...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Validating partition table...
    Partitions count: 5, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated 3d5695ff, recorded 3d5695ff, GOOD √
    Partition table read from '/dev/mmcblk0':
    ==============================================================
    NAME                          OFFSET                SIZE  MARK
    ==============================================================
    bootloader               0(   0.00B)    400000(   4.00M)     0
      (GAP)                                2000000(  32.00M)
    reserved           2400000(  36.00M)   4000000(  64.00M)     0
      (GAP)                                 800000(   8.00M)
    cache              6c00000( 108.00M)  20000000( 512.00M)     2
      (GAP)                                 800000(   8.00M)
    env               27400000( 628.00M)    800000(   8.00M)     0
      (GAP)                                 800000(   8.00M)
    logo              28400000( 644.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    recovery          2ac00000( 684.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    rsv               2d400000( 724.00M)    800000(   8.00M)     1
      (GAP)                                 800000(   8.00M)
    tee               2e400000( 740.00M)    800000(   8.00M)     1
      (GAP)                                 800000(   8.00M)
    crypt             2f400000( 756.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    misc              31c00000( 796.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    boot              34400000( 836.00M)   2000000(  32.00M)     1
      (GAP)                                 800000(   8.00M)
    system            36c00000( 876.00M)  80000000(   2.00G)     1
      (GAP)                                 800000(   8.00M)
    data              b7400000(   2.86G)  fac00000(   3.92G)     4
      (OVERLAP)                           fac00000(   3.92G)
    CE_STORAGE        b7400000(   2.86G)  fac00000(   3.92G)     4
    CE_FLASH         1b2000000(   6.78G)  20000000( 512.00M)     4
    ==============================================================
    Disk size totalling 7818182656 (7.28G) according to partition table
    Using 7818182656 (7.28G) as the disk size
    Usable space of the disk is 7704936448 (7.18G)
    - This is due to reserved partition ends at 104857600 (100.00M)
    - And env partition takes 8388608 (8.00M)
    Make sure you've backed up the env partition as its offset will mostly change
    - This is because all user-defined partitions will be created after the env partition
    - Yet most likely the old env partition was created after a cache partition
    - Which wastes a ton of space if we start at there
    Parsing user input for partition: data:::
    - Name: data
    - Offset: 113246208 (108.00M)
    - Size: 7704936448 (7.18G)
    - Mask: 4
    New partition table is generated successfully in memory
    Validating partition table...
    Partitions count: 4, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated 644544cc, recorded 644544cc, GOOD √
    ==============================================================
    NAME                          OFFSET                SIZE  MARK
    ==============================================================
    bootloader               0(   0.00B)    400000(   4.00M)     0
      (GAP)                                2000000(  32.00M)
    reserved           2400000(  36.00M)   4000000(  64.00M)     0
    env                6400000( 100.00M)    800000(   8.00M)     0
    data               6c00000( 108.00M) 1cb400000(   7.18G)     4
    ==============================================================
    Disk size totalling 7818182656 (7.28G) according to partition table
    Re-opening input path '/dev/mmcblk0' to write new patition table...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Warning: input is a whole emmc disk, checking if we should copy the env partition as the env partition may be moved
    Offset of the env partition has changed, copying content of it...
    Copied content env partiton
    Notifying kernel about partition table change...
    We need to reload the driver for emmc as the meson-mmc driver does not like partition table being hot-updated
    Opening '/sys/bus/mmc/drivers/mmcblk/unbind' so we can unbind driver for 'emmc:0001'
    Successfully unbinded the driver, all partitions and the disk itself are not present under /dev as a result of this
    Opening '/sys/bus/mmc/drivers/mmcblk/bind' so we can bind driver for 'emmc:0001'
    Successfully binded the driver, you can use the new partition table now!
    Everything done! Enjoy your fresh-new partition table!


Running the new installtointernal script in EmuELEC I've written for EmuELEC and HybridELEC, if you choose to create EEROMS, the partition table would be:
````
==============================================================
NAME                          OFFSET                SIZE  MARK
==============================================================
bootloader               0(   0.00B)    400000(   4.00M)     0
  (GAP)                                2000000(  32.00M)
reserved           2400000(  36.00M)   4000000(  64.00M)     0
env                6400000( 100.00M)    800000(   8.00M)     0
system             6c00000( 108.00M)  80000000(   2.00G)     2
data              86c00000(   2.11G)  80000000(   2.00G)     4
eeroms           106c00000(   4.11G)  cb400000(   3.18G)     4
==============================================================
````
Or if you don't choose to create EEROMS:
````
==============================================================
NAME                          OFFSET                SIZE  MARK
==============================================================
bootloader               0(   0.00B)    400000(   4.00M)     0
  (GAP)                                2000000(  32.00M)
reserved           2400000(  36.00M)   4000000(  64.00M)     0
env                6400000( 100.00M)    800000(   8.00M)     0
system             6c00000( 108.00M)  80000000(   2.00G)     2
data              86c00000(   2.11G) 14b400000(   5.18G)     4
==============================================================
````
Apply a previous taken snapshot from stock Android installation:
````
EmuELEC:~ # ./ampart /dev/mmcblk0 --clone bootloader:0B:4M:0 reserved:36M:64M:0 cache:108M:512M:2 env:628M:8M:0 logo:644M:32M:1 recovery:684M:32M:1 rsv:724M:8M:1 tee:740M:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32M:1 system:876M:2G:1 data:2932M:4524M:4
Notice: running in clone mode, partition arguments won't be filtered, reserved partitions can be set
Path '/dev/mmcblk0' seems a device file
Path '/dev/mmcblk0' detected as whole emmc disk
Path '/dev/mmcblk0' is a device, getting its size via ioctl
Disk size is 7818182656 (7.28G)
Reading old partition table...
Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
Validating partition table...
Partitions count: 5, GOOD √
Magic: MPT, GOOD √
Version: 01.00.00, GOOD √
Checksum: calculated 3d5695ff, recorded 3d5695ff, GOOD √
Partition table read from '/dev/mmcblk0':
==============================================================
NAME                          OFFSET                SIZE  MARK
==============================================================
bootloader               0(   0.00B)    400000(   4.00M)     0
  (GAP)                                2000000(  32.00M)
reserved           2400000(  36.00M)   4000000(  64.00M)     0
env                6400000( 100.00M)    800000(   8.00M)     0
system             6c00000( 108.00M)  80000000(   2.00G)     2
data              86c00000(   2.11G) 14b400000(   5.18G)     4
==============================================================
Disk size totalling 7818182656 (7.28G) according to partition table
Using 7818182656 (7.28G) as the disk size
Parsing user input for partition (clone mode): bootloader:0B:4M:0
 - Name: bootloader
 - Offset: 0 (0.00B)
 - Size: 4194304 (4.00M)
 - Mask: 0
Parsing user input for partition (clone mode): reserved:36M:64M:0
 - Name: reserved
 - Offset: 37748736 (36.00M)
 - Size: 67108864 (64.00M)
 - Mask: 0
Parsing user input for partition (clone mode): cache:108M:512M:2
 - Name: cache
 - Offset: 113246208 (108.00M)
 - Size: 536870912 (512.00M)
 - Mask: 2
Parsing user input for partition (clone mode): env:628M:8M:0
 - Name: env
 - Offset: 658505728 (628.00M)
 - Size: 8388608 (8.00M)
 - Mask: 0
Parsing user input for partition (clone mode): logo:644M:32M:1
 - Name: logo
 - Offset: 675282944 (644.00M)
 - Size: 33554432 (32.00M)
 - Mask: 1
Parsing user input for partition (clone mode): recovery:684M:32M:1
 - Name: recovery
 - Offset: 717225984 (684.00M)
 - Size: 33554432 (32.00M)
 - Mask: 1
Parsing user input for partition (clone mode): rsv:724M:8M:1
 - Name: rsv
 - Offset: 759169024 (724.00M)
 - Size: 8388608 (8.00M)
 - Mask: 1
Parsing user input for partition (clone mode): tee:740M:8M:1
 - Name: tee
 - Offset: 775946240 (740.00M)
 - Size: 8388608 (8.00M)
 - Mask: 1
Parsing user input for partition (clone mode): crypt:756M:32M:1
 - Name: crypt
 - Offset: 792723456 (756.00M)
 - Size: 33554432 (32.00M)
 - Mask: 1
Parsing user input for partition (clone mode): misc:796M:32M:1
 - Name: misc
 - Offset: 834666496 (796.00M)
 - Size: 33554432 (32.00M)
 - Mask: 1
Parsing user input for partition (clone mode): boot:836M:32M:1
 - Name: boot
 - Offset: 876609536 (836.00M)
 - Size: 33554432 (32.00M)
 - Mask: 1
Parsing user input for partition (clone mode): system:876M:2G:1
 - Name: system
 - Offset: 918552576 (876.00M)
 - Size: 2147483648 (2.00G)
 - Mask: 1
Parsing user input for partition (clone mode): data:2932M:4524M:4
 - Name: data
 - Offset: 3074424832 (2.86G)
 - Size: 4743757824 (4.42G)
 - Mask: 4
New partition table is generated successfully in memory
Validating partition table...
Partitions count: 13, GOOD √
Magic: MPT, GOOD √
Version: 01.00.00, GOOD √
Checksum: calculated 5e11f97, recorded 5e11f97, GOOD √
==============================================================
NAME                          OFFSET                SIZE  MARK
==============================================================
bootloader               0(   0.00B)    400000(   4.00M)     0
  (GAP)                                2000000(  32.00M)
reserved           2400000(  36.00M)   4000000(  64.00M)     0
  (GAP)                                 800000(   8.00M)
cache              6c00000( 108.00M)  20000000( 512.00M)     2
  (GAP)                                 800000(   8.00M)
env               27400000( 628.00M)    800000(   8.00M)     0
  (GAP)                                 800000(   8.00M)
logo              28400000( 644.00M)   2000000(  32.00M)     1
  (GAP)                                 800000(   8.00M)
recovery          2ac00000( 684.00M)   2000000(  32.00M)     1
  (GAP)                                 800000(   8.00M)
rsv               2d400000( 724.00M)    800000(   8.00M)     1
  (GAP)                                 800000(   8.00M)
tee               2e400000( 740.00M)    800000(   8.00M)     1
  (GAP)                                 800000(   8.00M)
crypt             2f400000( 756.00M)   2000000(  32.00M)     1
  (GAP)                                 800000(   8.00M)
misc              31c00000( 796.00M)   2000000(  32.00M)     1
  (GAP)                                 800000(   8.00M)
boot              34400000( 836.00M)   2000000(  32.00M)     1
  (GAP)                                 800000(   8.00M)
system            36c00000( 876.00M)  80000000(   2.00G)     1
  (GAP)                                 800000(   8.00M)
data              b7400000(   2.86G) 11ac00000(   4.42G)     4
==============================================================
Disk size totalling 7818182656 (7.28G) according to partition table
Re-opening input path '/dev/mmcblk0' to write new patition table...
Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
Warning: input is a whole emmc disk, checking if we should copy the env partition as the env partition may be moved
Offset of the env partition has changed, copying content of it...
Copied content env partiton
Notifying kernel about partition table change...
We need to reload the driver for emmc as the meson-mmc driver does not like partition table being hot-updated
Opening '/sys/bus/mmc/drivers/mmcblk/unbind' so we can unbind driver for 'emmc:0001'
Successfully unbinded the driver, all partitions and the disk itself are not present under /dev as a result of this
Opening '/sys/bus/mmc/drivers/mmcblk/bind' so we can bind driver for 'emmc:0001'
Successfully binded the driver, you can use the new partition table now!
Everything done! Enjoy your fresh-new partition table!
````
## About
The main reason I started to write this is that **CoreELEC**'s proprietary **ceemmc** which bricked **my only S905X** box can not be modified for reverting what it has done, and intalling EmuELEC and HybridELEC to internal emmc, as its partition sizes are **hard-coded** and refuse to change the way it works because the writer decides they should decide what users want, and this triggers me deeply for my KISS (Keep It Simple, Stupid) principles.

The partition tool is more or less a final step of the long journey of my past almost 2 months into achiving side-by-side dual-booting for CoreELEC+EmuELEC for all Amlogic devices, it is new, but the thoughts behind it is **mature** and **experienced**. You can expect an Amlogic-ng HybridELEC release utilizing ampart that can achive side-by-side dual-booting of CE+EE on the internal storage very soon.

In theory **ampart** should work perfectly fine for any Amlogic device, mainly those with linux-amlogic kernel, e.g. **CoreELEC**, **EmuELEC**, etc. But your poor little developer 7Ji just has 2 used Amlogic TVboxs lying around, one Xiaomi mibox3 (MDZ-16-AA, gxbb_p200) with a locked bootloader that forced me into developing [HybridELEC](https://github.com/7Ji/HybridELEC) (it starts as a USB Burning Tool burnable image for CoreELEC), and one BesTV R3300L (gxl_p212) which **ceemmc bricks** that forced me into developing **ampart**. I can only test on these two boxes and have no willing of buying other development boards/TV boxes, or free money to do so. 

Even if you are using a system that does not run a linux-amlogic kernel (e.g. **Armbian**, **OpenWrt**), you can still use **ampart** to get the partition info of your emmc and partition it. You can then use the partitions by losetup or [blkdevparts=](https://www.kernel.org/doc/html/latest/block/cmdline-partition.html) kernel command line. In this way you won't mess up with the reserved partitions and utilize more space than the tricky 700M offset partitioning way.

I'm into a big exam at the end of the year which I failed in the past two years and that's my last try, which would decide my fate greatly. A slang 'World War 3' is used for this in China .So every line I commit to ampart is precious and do me a favor, don't ask for much.


## License
**ampart**(Amlogic emmc partition tool) is licensed under [**GPL3**](https://gnu.org/licenses/gpl.html)
* This is free software: you are free to change and redistribute it.
* There is NO WARRANTY, to the extent permitted by law.
* Modification of **ampart** should be open-source under the same license (**GPL3**)
* Inclusion of **ampart** in close-source projects are **not allowed**.   
  * Inclusion of **ampart**'s source code in your closed-source program is not allowed
  * Calling binary executable of **ampart** from your closed-source program via any method is not allowed as it is not a system library