# Amlogic emmc partition tool
**ampart** is a partition tool written for **HybridELEC** for easier re-partitioning of internal emmc device for **Amlogic devices**. It is written totally in **C** for portability and therefore **simple, fast yet reliable**, and provides a simple yet powerful argument-driven CLI for easy implementation in scripts.  

Everything is done in a **single session**, without any **repeated execution** or **reboot**  

The main reason I wrote this is that **CoreELEC**'s proprietary **ceemmc** can not be easily modified for intalling EmuELEC and HybridELEC to internal emmc, as its partition sizes are **hard-coded**

In theory **ampart** should work perfectly fine for any system that have built-in meson-gx-mmc driver, e.g. **CoreELEC**, **EmuELEC**, **HybridELEC**, etc.

Even if you are using a system that does not have built-in meson-gx-mmc driver (e.g. **Armbian**, **OpenWrt**), you can still use **ampart** to get the partition info of your emmc and partition it. You can then use the partitions by losetup or [blkdevparts=](https://www.kernel.org/doc/html/latest/block/cmdline-partition.html) kernel command line. In this way you won't mess up with the reserved partitions and utilize more space than the tricky 700M offset partitioning way.

***
## Usage
**ampart** does not contain any interactive CLI. This is perfect for implementation in scripts but does not mean the user won't be able to understand what is happening.

The command-line usage of **ampart** is very simple:

````
ampart [reserved/emmc] ([partition] ...) ([option [optarg]]...) 
````
In which the positional arguments are:
*Arguments with \* are required*
* **reserved/emmc\*** the path to ``/dev/reserved`` or ``/dev/mmcblkN`` or any image file you dumped from them (e.g. via ``dd if=/dev/mmcblk0 of=emmc.img``, or ``dd if=/dev/reserved of=reserved.img``).  
Whether it's a whole emmc disk or a reserved partition will be identified by its name and header, but you can also force its type by options ``--reserved`` / ``-r`` or ``--disk`` / ``-d``  
* **partition**(s) partition(s) you would like to create, when omitted **ampart** will only print the old partition table. Should be in format **name**:**offset**:**size**:**mask**. In which:  
    * **name\*** the partition name, supports **a**-**z**, **A**-**Z** and **_**(underscore), 15 characters at max.  
    e.g. boot, system, data  
    ***A**-**Z** and **_** are supported but not **suggested**, as it **may confuse some kernel and bootloader***    
    * **offset** the partition's **absolute**(*without + prefix*) or  **relative**(*with + prefix*) offset. interger with optional *B/K/M/G* suffix *(for byte, kibibyte=1024B, mebibyte=1024K, gibibytere=1024M spectively)*.   
        1. when omitted, equals to **last partition's end**
        2. without prefix **+** for offset to **the disk's start**. e.g. 2G, will place partition at 2G
        3. with prefix **+** for offset to **last partition's end**. e.g. +8M, will place partiton at **last partition's end + 8M**, meaning there will be an **8M gap** between them  
        4. will be **rounded up** to **multiplies of 4096 (4KiB)** for **4K alignment**. e.g. **2047** will be rounded up to **4096**=**4K**, **1023K** will be rounded up to **1024K**  
    * **size** the partition's size. interger with optional *B/K/M/G* suffix (like offset).
        1. when omitted, size will equal to **all of free space after the last partition**
        2. will be **rounded up** to **multiplies of 4096 (4KiB)** for **4K alignment** just like offset
    * **mask** the partition's mask. either 2 or 4,   
    *it is **only useful for u-boot** to figure out which partition should be cleaned *(0-filled)* when you flash a USB Burning Tool and choose **normal erase**. It really doesn't make much sense as you've modified the partition table and Android images won't work anymore unless you choose **total erase**.* Default is 4.
        * 2 for system partitions, these partitions will always be cleaned whether **normal erase** is chosen or not when you flash an **Android image** via **USB Burning Tool**.
        * 4 for data partitions, these partitions will only be cleaned if you choose **total erase** when you flash an **Android image** via **USB Burning Tool**

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

    **Warning**: some partitions are **reserved** and **can't be defined by user**, they will be **generated** according to the old partition table:  
    1. **bootloader** This is where **u-boot** is stored, and it should always be the **first 4M** of the emmc device. It **won't** be touched.
    2. **reserved** This is where **partition table**, **dtb** and many other stuffs are stored, it usually starts at 36M and leaves an 32M gap between it and **bootloader** partition. It **won't** be touched.
    3. **env** This is where u-boot envs are stored, it is usually 8M, and placed after a cache partition. It **will be moved** as the cache partition takes 512M and does not make much sense for a pure ELEC installation.  
      1. Partitions defined by users will start at **end of reserved partition** + **size of env partition** as a result of this, to maximize the usable disk space by avoiding those precious ~512M taken by cache partition.   
      2. If Users don't want envs be reset to default, they should **backup their env partition** (e.g. via ``dd if=/dev/env of=env.img``) then restore it after the partitioning (e.g. via ``dd if=env.img of=/dev/env``) if **they use ampart to partition a disk for the first time**, as the env partiiton will most likely be moved at the first time.

And options are:
* **--version**/**-v** will print the version info, 
  * ampart will early quit
* **--help**/**-h** will print a help message, early quit
  * ampart will early quit
* **--disk**/**-d** will force ampart to treat input as a whole emmc disk
  * **conflicts** with --reserved/-r
* **--reserved**/**-r** will force ampart to treat input as a reserved partition
  *  **conflicts** with --disk/-d
* **--offset**/**-O** **[offset]** will overwrite the default offset of the reserved partition
  * **only valid** when input is a whole emmc disk
  * only make sense if your OEM has modified it (e.g. Xiaomi set it to 4M, right after the bootloader)
  * default: 36M
* **--dry-run**/**-D** will only generate the new partition table but not actually write it, this will help you to make sure the new partition table is desired.
  * ampart will quit before commiting changes
* **--output**/**-o** **[path]** will write the ouput table to somewhere else rather than the path you set in [reserved/mmc]
  * **currently is not implemented**

**Warning:** Changes will be written to disk without further asking when:
  * you don't specify ``--dry-run``/``-D`` 
  * you have provided valid partition arguments
  * the new partition table is valid

So you'd better make sure the new partition table is desired by ``--dry-run``/``-D``

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
````
(*In all of the commands above, those which create new partition tables will clear all partitions, copy the old partition info of bootloader, reserved and env as partition 0-2, then change the offset of env to the end of reserved. The env partition will most likely be moved. **If you don't want envs to be reset to default, you should backup the env partition first then restore it***)

## Examples
Running ``ampart /dev/mmcblk0`` to print the table when an Android storage is on an 8G emmc:
````
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
````
Running ``ampart /dev/mmcblk0`` to print the table when ``ceemmc`` has altered the partition table:
````
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
````
Running the following commands to format a single big chunky data partition on emmc:
````
dd if=/dev/env of=env.img
ampart /dev/mmcblk0 data:::
dd if=env.img of=/dev/env
````
The partition table would be:
````
==============================================================
NAME                          OFFSET                SIZE  MARK
==============================================================
bootloader               0(   0.00B)    400000(   4.00M)     0
  (GAP)                                2000000(  32.00M)
reserved           2400000(  36.00M)   4000000(  64.00M)     0
env                6400000( 100.00M)    800000(   8.00M)     0
data               6c00000( 108.00M) 1cb400000(   7.18G)     4
==============================================================
````
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