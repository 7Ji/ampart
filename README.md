# Amlogic emmc partition tool
**ampart** is a partition tool written for **HybridELEC**(a project that brings side-by-side dual-booting support to **CoreELEC**+**EmuELEC**) for easy re-partitioning of internal emmc device for **almost all Amlogic devices** to liberate me from editing the device tree for every device just to achive a custom partition layout. It is written totally in **C** for portability and therefore **simple, fast yet reliable**, and provides a simple yet powerful argument-driven CLI for easy implementation in scripts.  

Everything is done in a **single session**, without any **repeated execution** or **reboot**  

***
## Usage
**ampart** does not contain any interactive CLI. This is perfect for implementation in scripts but does not mean the user won't be able to understand what is happening.

**ampart** works in 3 modes: 
* **normal mode**(default): New partitions other than those reserved should be defined, i.e. a partition table without bootloader, reserved, env, logo and misc should be described by users. Most of the stuff are auto-generated, users do not need to know the detail of the underlying disk.
  * Minor details will differ depending on the old partition table, ampart would try its best to preserve the reserved partitions, and then create user-defined partitions
  * A straight-forward pure CoreELEC/EmuELEC installation can be achived in this way.
* **clone mode**: All partitions should be defined, i.e. a partition table with bootloader, reserved, env, logo and misc should be described by users, in an explicit way, almost all details of the part table can be modified as long as they are legal. This mode can be used to restore a snapshot or utilized by scripts that want to achive their specific partition layout.   
  * The output of clone mode will be a 1:1 binary replica of what you would get from a ampart snapshot taken via ``ampart --snapshot``
  * Scripts can operate in this mode for specific devices, if the emmc capacity is believed to be always the same, i.e. ``ampart /dev/mmcblk0 --clone bootloader:0B:4M:0 reserved:36M:64M:0 cache:108M:512M:2 env:628M:8M:0 logo:644M:32M:1 recovery:684M:32M:1 rsv:724M:8M:1 tee:740M:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32M:1 system:876M:2G:1 data:2932M:4524M:4`` would always update the partition layout to be 100% the same
  * Android updates would be possible if you don't mess up with all of the parts before data
* **update mode**: Work on the old partition layout and add minor tweaks here and there. 
  * All partitons not defined in the update array will not be affected
  * Scripts can operate in this mode if they just want some minor tweaks on the old table. i.e. ``ampart /dev/mmcblk --update ^data:::-2G: hybrid:::`` would shrink the data partition by 2G, and create a ``hybrid`` partition after it to take all of the new free space. 
  * Android updates will always be possible as long as you keep your new parts after data
  * Android+CoreELEC/EmuELEC dual boot on emmc can be achived in this mode
  * Updates happend on the **partition level**, not **filesystem level**, corresponding data backup and recovery, fs-resize and fs-createion needs to be done with other tools

All of the above modes will both update the partition table itself and block devices under /dev, i.e. you would expect a /dev/whatever to be present immediately after you create it with ``ampart /dev/mmcblk0 whatever:::``.

The command-line usage of **ampart** is very simple:

````
ampart [reserved/emmc] ([partition] ...) ([option [optarg]]...) 
````
In which the positional arguments are:
*Arguments with \* are required*
* **reserved/emmc\*** the path to ``/dev/reserved`` or ``/dev/mmcblkN`` or any image file you dumped from them (e.g. via ``dd if=/dev/mmcblk0 of=emmc.img``, or ``dd if=/dev/reserved of=reserved.img``).  
Whether it's a whole emmc disk or a reserved partition will be identified by its name and header, but you can also force its type by options ``--reserved`` / ``-r`` or ``--disk`` / ``-d``  
* (**normal/clone** mode) **partition**(s) partition(s) you would like to create, when omitted **ampart** will only print the old partition table. Should be in format **name**:**offset**:**size**:**mask**. In which:  
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
    3. **env** This is where u-boot envs are stored, it is usually 8M, and placed after a cache partition. It **will most likely be moved** as the cache partition takes 512M and does not make much sense for a pure ELEC installation.  
    4. **logo** This is where u-boot logo is stored. It **will most likely be moved**
    5. **misc** This is where shared args between kernel and u-boot are stored. It **will most likely be moved**
    * In normal mode, ampart will try to insert env, logo and misc to the gap between bootloader and reserved to avoid disk space being wasted
    * In all modes, when positions(offset) of env, logo and misc are changed, they will be migrated automatically by ampart (read all, write all, so don't worry about overwriting things we haven't read)
    * Even though ampart will migrate the content of the env, logo and misc partition from their old locations to their new locations, and the content at the new locations will be the same on the **binary level**, you can always backup images of these partitions for double insurance e.g. ``dd if=/dev/env of=env.img``
  

* (**update mode**) **partition**(s) partition(s) you would like to update, when omitted **ampart** will only print the old partition table. Should be in either format **selector**(:**name**:**offset**:**size**:**mask**) or **name**:**offset**:**size**:**mask**. In which:  
  * **selector*** starts with ^, a partition name existing in the old table, or positive interger to select a part from the beginning, or negative interger to select a part from the end.   
  If omitted, the **name**:**offset**:**size**:**mask** argument is parsed just like in **normal mode** (*Notice that, however, you can't use update mode as normal mode, even if you manually remove all parts except the reserved ones, as reserved parts will not be inserted into the gap to save disk space in this mode*)  
  e.g.   
    * ^-1 selects the last partition
    * ^0 selects the first partition (in most cases this would be bootloader partition)
    * ^data selects the data partition
    * *(empty)* selects nothing, create a part according to the remaining args
  * **(operator)** optional suffix added after selector can define special behaviour:
    * ? deletes the selected partition, no other args after : are needed. think it as *oh, where's the part now? I don't remember it anymore.*
    * % clone the selected partition, give it a name after : ignore other args. think it as *get that part, and clone it! I want them to be identical just that those two o in the % symbol*
  * **name** unless operator is defined and is ?, is the same as normal/clone mode, would update parts name if set, or do nothing if omitted
  * **offset** integer number with optional +/- prefix and optional B/K/M/G suffix. Without prefix for replacing offset, with prefix for increasing/decreasing offset
  * **size** integer number with optional +/- prefix and optional B/K/M/G suffix. Without prefix for replacing size, with prefix for increasing/decreasing size
  * **mask** either 0, 1, 2 or 4, same as clone mode

  e.g.
  * ``^-1?`` deletes the last partition
  * ``^-1:::-512M:`` shrinks the last part by 512M
  * ``^-1%:CE_STORAGE`` clones the last part (usually data) to a new part ``CE_STORAGE``
  * ``^data:CE_STORAGE`` clones the data partition like the last one, except we select by name this time
  * ``CE_FLASH::512M:`` creates a new partition ``CE_FLASH`` with size 512M, just like you would in **normal mode**

  The following one-line command achives a same partition layout as what ceemmc in dual-boot mode would do:
  ````
  ampart --update /dev/mmcblk0 ^-1:::-512M: ^-1%:CE_STORAGE CE_FLASH:::
  ````
  and the following would shrink the data more so EmuELEC or HybridELEC can be stored:
  ````
  ampart --update /dev/mmcblk0 ^-1:::2G: ^-1%:CE_STORAGE CE_FLASH:::
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
  * scripts can parse the last two lines to get the part table, the offset/size of the first one is always human-readable for users to record; the last one is always in byte, without suffix, script can get this with a simple ``ampart --snapshot /dev/mmcblk0 | tail -n 1``  
  e.g. (the human-readable output)
    ````
    bootloader:0B:4M:0 reserved:36M:64M:0 cache:108M:512M:2 env:628M:8M:0 logo:644M:32M:1 recovery:684M:32M:1 rsv:724M:8M:1 tee:740M:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32M:1 system:876M:2G:1 data:2932M:4524M:4
    ````
    (the machine-friendly output)
    ````
    bootloader:0:4194304:0 reserved:37748736:67108864:0 cache:113246208:536870912:2 env:658505728:8388608:0 logo:675282944:33554432:1 recovery:717225984:33554432:1 rsv:759169024:8388608:1 tee:775946240:8388608:1 crypt:792723456:33554432:1 misc:834666496:33554432:1 boot:876609536:33554432:1 system:918552576:2147483648:1 data:3074424832:4743757824:4
    ````
    later if you regret about the new part table, you can call ampart with --clone and pass it one of the lines above:
    ````
    ampart --clone /dev/mmcblk0 bootloader:0B:4M:0 reserved:36M:64M:0 cache:108M:512M:2 env:628M:8M:0 logo:644M:32M:1 recovery:684M:32M:1 rsv:724M:8M:1 tee:740M:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32M:1 system:876M:2G:1 data:2932M:4524M:4
    ````
   
* **--clone**/**-c** enable clone mode, only verify some args of the parts, partition arguments must be set **explicitly**
  * reserved partitions can be defined and should be defined (bootloader, reserved, env)
  * name, offset, size, mask must be **explicit set**
  * users can pass the partition arguments generated by a previous **snapshot** session to restore the part to exactly what it was like back then, **byte-to-byte correct**
  * scripts can call ampart with modified args previously acquired by a **snapshot** call, and apply an updated part table, they can either keep almost all of the table not affected, or do a complete overhaul: you want the freedom, ampart gives you.
* **--update**/**-u** enable update mode, [partitions] describe how you would like to modify some parts' info
* **--no-node**/**-N** don't try to remove partitions node from dtb
* **--dry-run**/**-D** will only generate the new partition table but not actually write it, this will help you to make sure the new partition table is desired.
  * ampart will quit before commiting changes *(return 0 for success)*
* **--partprobe**/**-p** inform kernel about partition layout changes of the emmc (so you can use new partitions immediately). ampart auto does this when you update the part table so there is no nuch need to call this unless you run it with *--no-reload*/*-n*
* **--no-reload**/**-n** do not notify kernel about the partition layout changes, remember to end your session with a --partprobe call if you are calling ampart for multiple times and don't want it to force the kernel to reload the partitions table every time (e.g. in a script, where you want to do stuffs in the GNU/parted -s way)
* **--output**/**-o** **[path]** will write the ouput table to somewhere else rather than the path you set in [reserved/mmc]
  * **currently is not implemented**

**Warning:** Changes will be written to disk without further asking when:
  * you don't specify ``--dry-run``/``-D`` 
  * you have provided valid partition arguments
  * the new partition table is valid

So you'd better make sure the new partition table is desired by option ``--dry-run``/``-D`` first

New paritions should be available immediately under /dev after partitioning, **without reboot**

~~**Warning**: ampart won't check if the partitions are in used or not, users should umount the old parts by themselvies if they don't want any data loss. **this also mean you can partition the emmc being used by system even if your system is running from it.** This is a **feature**, not a **bug**, you told ampart to do so and **you yourself should take the responsibility**.~~  
This is no longer the case, ampart will refuse to partition a disk if it's a device and any of its part is mounted

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

# Shrink, clone data, then create CE_FLASH, just like ceemmc in dual boot mode
ampart --update /dev/mmcblk0 ^-1:::-512M: ^-1%:CE_STORAGE CE_FLASH:::
````
~~(*In all of the commands above, those which create new partition tables will clear all partitions, copy the old partition info of bootloader, reserved and env as partition 0-2, then change the offset of env to the end of reserved, then clone the content of the env partition if it's moved.*)~~ *ampart now has an insertion optimizer algorithm that'll try its best to insert env, logo, misc into the gap between bootloader and reserved to save disk space (normal mode only)*


## Examples
Print the table when an Android installation is on an 8G emmc:    

    EmuELEC:~ # ampart /dev/mmcblk0
    Path '/dev/mmcblk0' seems a device file
    Path '/dev/mmcblk0' detected as whole emmc disk
    Path '/dev/mmcblk0' is a device, getting its size via ioctl
    Disk size is 7818182656 (7.28G)
    Reading old partition table...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Validating partition table...
    Partitions count: 13, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated 5e11f97, recorded 5e11f97, GOOD √
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
    DTB is stored in plain Amlogic multi-dtb format

Take a snapshot of the partition table in case anything goes wrong:  

    EmuELEC:~ # ampart --snapshot /dev/mmcblk0
    Notice: running in snapshot mode, new partition table won't be parsed
    Path '/dev/mmcblk0' seems a device file
    Path '/dev/mmcblk0' detected as whole emmc disk
    Path '/dev/mmcblk0' is a device, getting its size via ioctl
    Disk size is 7818182656 (7.28G)
    Reading old partition table...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Validating partition table...
    Partitions count: 13, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated 5e11f97, recorded 5e11f97, GOOD √
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
    DTB is stored in plain Amlogic multi-dtb format
    Give one of the following partition arguments with options --clone/-c to ampart to reset the partition table to what it looks like now:
    bootloader:0B:4M:0 reserved:36M:64M:0 cache:108M:512M:2 env:628M:8M:0 logo:644M:32M:1 recovery:684M:32M:1 rsv:724M:8M:1 tee:740M:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32M:1 system:876M:2G:1 data:2932M:4524M:4
    bootloader:0:4194304:0 reserved:37748736:67108864:0 cache:113246208:536870912:2 env:658505728:8388608:0 logo:675282944:33554432:1 recovery:717225984:33554432:1 rsv:759169024:8388608:1 tee:775946240:8388608:1 crypt:792723456:33554432:1 misc:834666496:33554432:1 boot:876609536:33554432:1 system:918552576:2147483648:1 data:3074424832:4743757824:4
(For actual users instead of scripts, you should record the last line as it's shorter and easier to write/remember)  

Use the new aminstall script I've written for EmuELEC that utilizes ampart to re-partition the emmc to achive a new CE_FLASH + CE_STORAGE + EEROMS partition layout, in single-boot mode, and check the new partition table after installation using ampart:

    EmuELEC:~ # aminstall --iknowwhatimdoing
    /usr/bin/dtname: line 70: warning: command substitution: ignored null byte in input
    Checking if mmcblk0 is properly driven
    Warning: working in single boot mode
    This script will erase the old partition table on your emmc
    and create a new part table that ONLY contains CE_FLASH, CE_STORAGE and EEROMS (if emmc size big enough) partiitons
    (reserved partitions like bootloader, reserved, env, logo and misc will be kept)
    You Android installation will be COMPLETELY erased and can not be recovered
    Unless you use Amlogic USB Burning Tool to flash a stock image
    This script will install EmuELEC that you booted from SD card/USB drive.

    WARNING: The script does not have any safeguards, you will not receive any
    support for problems that you may encounter if you proceed!

    Type "yes" if you know what you are doing or anything else to exit: yes
    Oh sweet! You emmc is larger than 4G (8G), so you can create a dedicated EEROMS partition to save your ROMs, savestates, etc
    Do you want to create a dedicated EEROMS partition? (if not, CE_STORAGE partition will fill the rest of the disk) [Y/n]
    Warning: from this point onward, all of the changes are IRREVERSIBLE since new data will be written to emmc. Make sure you keep the power pluged in.
    If anything breaks apart, you can revert your partition table with the following command, but changes to the data are IRREVERSIBLE

    ampart --clone /dev/mmcblk0 bootloader:0B:4M:0 reserved:36M:64M:0 cache:108M:512M:2 env:628M:8M:0 logo:644M:32M:1 recovery:684M:32M:1 rsv:724M:8M:1 tee:740M:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32M:1 system:876M:2G:1 data:2932M:4524M:4

    Ready to actually populate internal EmuELEC installation in 10 seconds...
    You can ctrl+c now to stop the installation if you regret
    Populating internal CE_FLASH partition...
    Formatting internal CE_FLASH partition...
    Copying all system files (kernel, SYSTEM, dtb, etc) under /flash to internal CE_FLASH partition...
    Populated internal CE_FLASH partition...
    Populating internal CE_STORAGE partition...
    Formatting internal CE_STORAGE partition...
    Do you want to copy your user data under /storage to internal CE_STORAGE partition? (This will not include all of the stuffs under /storage/roms, they will be copied to EEROMS partition later) [Y/n]
    Stopping EmulationStation so we can make sure configs are flushed onto disk... You can run 'systemctl start emustation.service' later to bring EmulationStation back up
    Copying user data...
    Populated internal CE_STORAGE partition...
    Populating internal EEROMS partition...
    Formatting internal EEROMS partition...
    Note: EEROMS on the emmc will always be formatted as EXT4, Since you can not plug the emmc to a Windows PC just like you would for a SD card/USB drive
    Do you want to copy all of your ROMs, savestates, etc under /storage/roms to internal EEROMS partition? [Y/n]
    Copying Roms, savestates, etc...
    Populated internal EEROMS partition...
    All done!
    EmuELEC has been installed to your internal emmc.

    Would you like to reboot to the fresh-installed internal installation of EmuELEC? (y/N)
    EmuELEC:~ # ampart /dev/mmcblk0
    Path '/dev/mmcblk0' seems a device file
    Path '/dev/mmcblk0' detected as whole emmc disk
    Path '/dev/mmcblk0' is a device, getting its size via ioctl
    Disk size is 7818182656 (7.28G)
    Reading old partition table...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Validating partition table...
    Partitions count: 8, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated c88a8998, recorded c88a8998, GOOD √
    Warning: Misbehaviour found in partition name 'CE_FLASH':
    - Uppercase letter (A-Z) is used, this is not recommended as it may confuse some kernel and bootloader
    - Underscore (_) is used, this is not recommended as it may confuse some kernel and bootloader
    Warning: Misbehaviour found in partition name 'CE_STORAGE':
    - Uppercase letter (A-Z) is used, this is not recommended as it may confuse some kernel and bootloader
    - Underscore (_) is used, this is not recommended as it may confuse some kernel and bootloader
    Warning: Misbehaviour found in partition name 'EEROMS':
    - Uppercase letter (A-Z) is used, this is not recommended as it may confuse some kernel and bootloader
    Partition table read from '/dev/mmcblk0':
    ==============================================================
    NAME                          OFFSET                SIZE  MARK
    ==============================================================
    bootloader               0(   0.00B)    400000(   4.00M)     0
    logo                400000(   4.00M)   2000000(  32.00M)     1
    reserved           2400000(  36.00M)   4000000(  64.00M)     0
    env                6400000( 100.00M)    800000(   8.00M)     0
    misc               6c00000( 108.00M)   2000000(  32.00M)     1
    CE_FLASH           8c00000( 140.00M)  80000000(   2.00G)     2
    CE_STORAGE        88c00000(   2.14G)  80000000(   2.00G)     4
    EEROMS           108c00000(   4.14G)  c9400000(   3.14G)     4
    ==============================================================
    Disk size totalling 7818182656 (7.28G) according to partition table
    DTB is stored in plain Amlogic multi-dtb format




If you want to go back to your stock layout after you've installed CE/EE/HE to emmc:

    EmuELEC:~ #  ampart --clone /dev/mmcblk0 bootloader:0B:4M:0 reserved:36M:64M:0 cache:108M:512M:2 env:628M:8M:0 logo:644M:32M:1 recovery:684M:32M:1 rsv:724M:8M:1 tee:740M:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32M:1 system:876M:2G:1 data:2932M:4524M:4:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32MNotice: running in clone mode, partition arguments won't be filtered, reserved partitions can be set
    Path '/dev/mmcblk0' seems a device file
    Path '/dev/mmcblk0' detected as whole emmc disk
    Path '/dev/mmcblk0' is a device, getting its size via ioctl
    Disk size is 7818182656 (7.28G)
    Reading old partition table...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Validating partition table...
    Partitions count: 8, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated c88a8998, recorded c88a8998, GOOD √
    Warning: Misbehaviour found in partition name 'CE_FLASH':
    - Uppercase letter (A-Z) is used, this is not recommended as it may confuse some kernel and bootloader
    - Underscore (_) is used, this is not recommended as it may confuse some kernel and bootloader
    Warning: Misbehaviour found in partition name 'CE_STORAGE':
    - Uppercase letter (A-Z) is used, this is not recommended as it may confuse some kernel and bootloader
    - Underscore (_) is used, this is not recommended as it may confuse some kernel and bootloader
    Warning: Misbehaviour found in partition name 'EEROMS':
    - Uppercase letter (A-Z) is used, this is not recommended as it may confuse some kernel and bootloader
    Partition table read from '/dev/mmcblk0':
    ==============================================================
    NAME                          OFFSET                SIZE  MARK
    ==============================================================
    bootloader               0(   0.00B)    400000(   4.00M)     0
    logo                400000(   4.00M)   2000000(  32.00M)     1
    reserved           2400000(  36.00M)   4000000(  64.00M)     0
    env                6400000( 100.00M)    800000(   8.00M)     0
    misc               6c00000( 108.00M)   2000000(  32.00M)     1
    CE_FLASH           8c00000( 140.00M)  80000000(   2.00G)     2
    CE_STORAGE        88c00000(   2.14G)  80000000(   2.00G)     4
    EEROMS           108c00000(   4.14G)  c9400000(   3.14G)     4
    ==============================================================
    Disk size totalling 7818182656 (7.28G) according to partition table
    DTB is stored in plain Amlogic multi-dtb format
    Warning: input is a device, check if any partitions under its corresponding emmc disk '/dev/mmcblk0' is mounted...
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
    Oopening '/dev/mmcblk0' as read/append to write new patition table...
    Modifying Amlogic multi-dtb...
    Warning: input is a whole emmc disk, checking if we should migrate env, logo and misc partitions as their position may be moved
    Warning: offset of reserved partition env changed, it should be migrated
    Warning: offset of reserved partition logo changed, it should be migrated
    Warning: offset of reserved partition misc changed, it should be migrated
    Old reserved partitions all read into memory
    Writing reserved partition env to its new location...
    Writing reserved partition logo to its new location...
    Writing reserved partition misc to its new location...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Notifying kernel about partition table change...
    We need to reload the driver for emmc as the meson-mmc driver does not like partition table being hot-updated
    Opening '/sys/bus/mmc/drivers/mmcblk/unbind' so we can unbind driver for 'emmc:0001'
    Successfully unbinded the driver, all partitions and the disk itself are not present under /dev as a result of this
    Opening '/sys/bus/mmc/drivers/mmcblk/bind' so we can bind driver for 'emmc:0001'
    Successfully binded the driver, you can use the new partition table now!
    Everything done! Enjoy your fresh-new partition table!

Put a big single data parititon on emmc and use it as storage, if you don't care about Android:

    EmuELEC:~ # ampart /dev/mmcblk0 data:::
    Path '/dev/mmcblk0' seems a device file
    Path '/dev/mmcblk0' detected as whole emmc disk
    Path '/dev/mmcblk0' is a device, getting its size via ioctl
    Disk size is 7818182656 (7.28G)
    Reading old partition table...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Validating partition table...
    Partitions count: 13, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated 5e11f97, recorded 5e11f97, GOOD √
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
    DTB is stored in plain Amlogic multi-dtb format
    Warning: input is a device, check if any partitions under its corresponding emmc disk '/dev/mmcblk0' is mounted...
    Using 7818182656 (7.28G) as the disk size
    [Insertion optimizer] the following partitions will be inserted into the gap between bootloader and reserved: logo
    Usable space of the disk is 7671382016 (7.14G)
    Make sure you've backed up the env, logo and misc partitions as their offset will mostly change,
    - even ampart will auto-migrate them for you
    - This is because all user-defined partitions will be created after the reserved partition
    - Yet most likely these old partitions have gap before them
    - Which wastes a ton of space if we start at there
    Parsing user input for partition: data:::
    - Name: data
    - Offset: 146800640 (140.00M)
    - Size: 7671382016 (7.14G)
    - Mask: 4
    New partition table is generated successfully in memory
    Validating partition table...
    Partitions count: 6, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated 1667e732, recorded 1667e732, GOOD √
    ==============================================================
    NAME                          OFFSET                SIZE  MARK
    ==============================================================
    bootloader               0(   0.00B)    400000(   4.00M)     0
    logo                400000(   4.00M)   2000000(  32.00M)     1
    reserved           2400000(  36.00M)   4000000(  64.00M)     0
    env                6400000( 100.00M)    800000(   8.00M)     0
    misc               6c00000( 108.00M)   2000000(  32.00M)     1
    data               8c00000( 140.00M) 1c9400000(   7.14G)     4
    ==============================================================
    Disk size totalling 7818182656 (7.28G) according to partition table
    Oopening '/dev/mmcblk0' as read/append to write new patition table...
    Modifying Amlogic multi-dtb...
    Warning: input is a whole emmc disk, checking if we should migrate env, logo and misc partitions as their position may be moved
    Warning: offset of reserved partition env changed, it should be migrated
    Warning: offset of reserved partition logo changed, it should be migrated
    Warning: offset of reserved partition misc changed, it should be migrated
    Old reserved partitions all read into memory
    Writing reserved partition env to its new location...
    Writing reserved partition logo to its new location...
    Writing reserved partition misc to its new location...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Notifying kernel about partition table change...
    We need to reload the driver for emmc as the meson-mmc driver does not like partition table being hot-updated
    Opening '/sys/bus/mmc/drivers/mmcblk/unbind' so we can unbind driver for 'emmc:0001'
    Successfully unbinded the driver, all partitions and the disk itself are not present under /dev as a result of this
    Opening '/sys/bus/mmc/drivers/mmcblk/bind' so we can bind driver for 'emmc:0001'
    Successfully binded the driver, you can use the new partition table now!
    Everything done! Enjoy your fresh-new partition table!
Now I have a 7.18 G data partition **located immediately at /dev/data** and I can format/use it just in the way I like! There's no tricky loop mount or BS rebooting over and over again.

Put only a system partition and a data partition on emmc, if you want to install CoreELEC/EmuELEC in the good-old installtointernal way and don't want to waste any single byte on Andorid BS:

    EmuELEC:~ # ampart /dev/mmcblk0 system::2G: data:::
    Path '/dev/mmcblk0' seems a device file
    Path '/dev/mmcblk0' detected as whole emmc disk
    Path '/dev/mmcblk0' is a device, getting its size via ioctl
    Disk size is 7818182656 (7.28G)
    Reading old partition table...
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Validating partition table...
    Partitions count: 6, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated 1667e732, recorded 1667e732, GOOD √
    Partition table read from '/dev/mmcblk0':
    ==============================================================
    NAME                          OFFSET                SIZE  MARK
    ==============================================================
    bootloader               0(   0.00B)    400000(   4.00M)     0
    logo                400000(   4.00M)   2000000(  32.00M)     1
    reserved           2400000(  36.00M)   4000000(  64.00M)     0
    env                6400000( 100.00M)    800000(   8.00M)     0
    misc               6c00000( 108.00M)   2000000(  32.00M)     1
    data               8c00000( 140.00M) 1c9400000(   7.14G)     4
    ==============================================================
    Disk size totalling 7818182656 (7.28G) according to partition table
    DTB is stored in plain Amlogic multi-dtb format
    Warning: input is a device, check if any partitions under its corresponding emmc disk '/dev/mmcblk0' is mounted...
    Using 7818182656 (7.28G) as the disk size
    [Insertion optimizer] the following partitions will be inserted into the gap between bootloader and reserved: logo
    Usable space of the disk is 7671382016 (7.14G)
    Make sure you've backed up the env, logo and misc partitions as their offset will mostly change,
    - even ampart will auto-migrate them for you
    - This is because all user-defined partitions will be created after the reserved partition
    - Yet most likely these old partitions have gap before them
    - Which wastes a ton of space if we start at there
    Parsing user input for partition: system::2G:
    - Name: system
    - Offset: 146800640 (140.00M)
    - Size: 2147483648 (2.00G)
    - Mask: 4
    Parsing user input for partition: data:::
    - Name: data
    - Offset: 2294284288 (2.14G)
    - Size: 5523898368 (5.14G)
    - Mask: 4
    New partition table is generated successfully in memory
    Validating partition table...
    Partitions count: 7, GOOD √
    Magic: MPT, GOOD √
    Version: 01.00.00, GOOD √
    Checksum: calculated ef793865, recorded ef793865, GOOD √
    ==============================================================
    NAME                          OFFSET                SIZE  MARK
    ==============================================================
    bootloader               0(   0.00B)    400000(   4.00M)     0
    logo                400000(   4.00M)   2000000(  32.00M)     1
    reserved           2400000(  36.00M)   4000000(  64.00M)     0
    env                6400000( 100.00M)    800000(   8.00M)     0
    misc               6c00000( 108.00M)   2000000(  32.00M)     1
    system             8c00000( 140.00M)  80000000(   2.00G)     4
    data              88c00000(   2.14G) 149400000(   5.14G)     4
    ==============================================================
    Disk size totalling 7818182656 (7.28G) according to partition table
    Oopening '/dev/mmcblk0' as read/append to write new patition table...
    Modifying Amlogic multi-dtb...
    Warning: input is a whole emmc disk, checking if we should migrate env, logo and misc partitions as their position may be moved
    Notice: Seeking 37748736 (36.00M) (offset of reserved partition) into disk
    Notifying kernel about partition table change...
    We need to reload the driver for emmc as the meson-mmc driver does not like partition table being hot-updated
    Opening '/sys/bus/mmc/drivers/mmcblk/unbind' so we can unbind driver for 'emmc:0001'
    Successfully unbinded the driver, all partitions and the disk itself are not present under /dev as a result of this
    Opening '/sys/bus/mmc/drivers/mmcblk/bind' so we can bind driver for 'emmc:0001'
    Successfully binded the driver, you can use the new partition table now!
    Everything done! Enjoy your fresh-new partition table!

Wait, I think I don't want to install or format anything, I want to take a regret pill and get my old partition table back. Oh, I can just go back with the snapshot I've taken before!
````
CoreELEC:~ # ./ampart --clone /dev/mmcblk0 bootloader:0B:4M:0 reserved:36M:64M:0 cache:108M:512M:2 env:628M:8M:0 logo:644M:32M:1 recovery:684M:32M:1 rsv:724M:8M:1 tee:740M:8M:1 crypt:756M:32M:1 misc:796M:32M:1 boot:836M:32M:1 system:876M:2G:1 data:2932M:4524M:4
````

## About
The main reason I started to write this is that **CoreELEC**'s proprietary **ceemmc** can not be modified for intalling EmuELEC and HybridELEC to internal emmc, as its partition sizes are **hard-coded** 

The partition tool is more or less a final step of the long journey of my past almost 2 months into achiving side-by-side dual-booting for CoreELEC+EmuELEC for all Amlogic devices, it is new, but the thoughts behind it is **mature** and **experienced**. You can expect an Amlogic-ng HybridELEC release utilizing ampart that can achive side-by-side dual-booting of CE+EE on the internal storage very soon. 

In theory **ampart** should work perfectly fine for any Amlogic device, mainly those with linux-amlogic kernel, e.g. **CoreELEC**, **EmuELEC**, etc. But I only have 2 used non-mainstream Amlogic TVboxs lying around, one Xiaomi mibox3 (MDZ-16-AA, gxbb_p200) with a locked bootloader that forced me into developing [HybridELEC](https://github.com/7Ji/HybridELEC) (it starts as a USB Burning Tool burnable image for CoreELEC), and one BesTV R3300L (gxl_p212). I can only test on these two boxes and have no willing of buying other development boards/TV boxes, or free money to do so. 

If you want me to bring official support to other devices, you can donate some funds/device to me for development. Note that devices violating EULA of CoreELEC or EmuELEC (especially those come with pre-installed image and Roms) are never supported. I'm living in China so devices shipped from China are more appreciated. Email me at pugokushin@gmail.com if you want to do so, and I'm considering opening Paypal/Patreon for public donation in the future.

Even if you are using a system that does not run a linux-amlogic kernel (e.g. **Armbian**, **OpenWrt**), you can still use **ampart** to get the partition info of your emmc and partition it. You can then use the partitions by losetup or [blkdevparts=](https://www.kernel.org/doc/html/latest/block/cmdline-partition.html) kernel command line. In this way you won't mess up with the reserved partitions and utilize more space than the tricky 700M offset partitioning way.

I'm into a big exam at the end of the year which I failed in the past two years and that's my last try, which would decide my fate greatly. A slang 'World War 3' is used for this in China. So every line I commit to ampart is precious and do me a favor, don't ask for much.


## License
**ampart**(Amlogic emmc partition tool) is licensed under [**GPL3**](https://gnu.org/licenses/gpl.html)
 * Copyright (C) 2022 7Ji (pugokushin@gmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.