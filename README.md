# Amlogic emmc partition tool
**ampart** is a partition tool written for **HybridELEC** for easier re-partitioning of internal emmc device for **Amlogic devices**. It is written totally in **C** for portability and therefore **simple, fast yet reliable**, and provides a simple yet powerful argument-driven CLI for easy implementation in scripts.  

The main reason I wrote this is that **CoreELEC**'s proprietary **ceemmc** can not be easily modified for intalling EmuELEC and HybridELEC to internal emmc, as its partition sizes are **hard-coded**

***
## Usage
**ampart** does not contain any interactive CLI. This is perfect for implementation in scripts but does not mean the user won't be able to understand what is happening.

The arguments of **ampart** is very simple:

````
./ampart [reserved/emmc] [partition] ... [option [optarg]]... 
````
In which the positional arguments are:
* **reserved/emmc** is the path to ``/dev/reserved`` or ``/dev/mmcblkN`` or any image file you dumped from them (e.g. via ``dd if=/dev/mmcblk0 of=emmc.img``).  
**ampart** will auto-recognize its type by name and header, but you can also force it to be recognized as reserved or whole emmc disk by options ``--reserved`` / ``-r`` or ``--disk`` / ``-d``  
* **partition**(s) are partitions you would like to create, when leaved empty **ampart** will only print the old partition table. Should be written in format **name**:**offset**:**size**:**mask**. In which:  
    * **name** is the partition name, it supports **a**-**z**, **A**-**Z** and **_**, **must be set**, 15 characters at max.  
        ****A**-**Z** and **_** are supported but not **suggested**, as it **may confuse some kernel and bootloader***    
    e.g. boot, system, data
    * **offset** is the partition's **absolute** offset, this should be interger number with optional *B/K/M/G* suffix *(for each step, multiplies 1024 instead of 1000, i.e. these are KiB, MiB and GiB respectively)*.   
        1. When leaved empty, offset will be set to last partition's end
        2. You can add an optional **+** for **relative** offset, e.g. +8M, this will leave a 8M gap between this partiton and the last one.  
        3. Any offset will be **rounded up** to **multiplies of 4096 (4KiB)** for **4K alignment**  
        e.g. 32M (32M offset in the whole disk), +1M (leave a 1M gap after the last partition), +2047 (leave a 4K gap after the last partition as it's rounded up to 4096)  
    * **size** is the partition's size, this should be interger number with optional *B/K/M/G* suffix.
        1. When leaved empty, size will be equal to all free space after the last partition
        2. Any offset will be **rounded up** to **multiplies of 4096 (4KiB)** for **4K alignment**  
        e.g. 2G (a 2GiB partition)
    * **mask** is the partition's mask, this should be either 2 or 4, it is **only useful for u-boot** to figure out which partition should be cleaned *(0-filled)* when you flash a USB Burning Tool and choose **normal erase**. It really doesn't make much sense as you've modified the partition table anyway, and Android images won't work anymore unless you choose **total erase**. Default is 4.
        * 2 is for system partitions, these partitions will be cleaned whether **normal erase** is chosen or not when you flash an **Android image** via **USB Burning Tool**.
        * 4 is for data partitions, these partitions will only be cleaned if you choose **total erase** whn you flash an **Android image** via **USB Burning Tool**

    e.g.

    * **``system:+8M:2G:2``** A partition named system, leave a gap between it and the last partition, size is 2G, mask is 2  
    * **``data:::``**  A partition named data, no gap before it, size=free space(use all remaining space after the last partition)

    Note that **the order of partitions** matter, e.g. ``system:+8M:2G:2 data::::`` will work, but ``data::: system:+8M:2G:2`` won't because data has already taken all of the remaining space.

    Partitions also must be **incremental**, which means you can't create a partition whose position is **before** a one already existing. e.g. ``system::2G: data:0:2G:`` won't work as data's offset is greater than system's end point (2G + reserved partitions)

    The following partitions are **reserved** and **can't be defined by user**, they will be **generated** according to the old partition table:

    1. **bootloader** This is where **u-boot** is stored, and it should always be the **first 4M** of the emmc device. It **won't** be touched.
    2. **reserved** This is where **partition table**, **dtb** and many other stuffs are stored, it usually starts at 36M and leaves an 32M gap between it and **bootloader** partition. It **won't** be touched.
    3. **env** This is where u-boot envs are stored, it is usually 8M, and placed after a cache partition. It **will be moved** as the cache partition takes 512M and does not make much sense for a pure ELEC installation.

    So partitions defined by users will start at **end of reserved partition** + **size of env partition**, to maximize the usable disk space by avoiding those precious ~512M taken by cache partition.   
    As a result, if they don't want envs be reset to default, users should **backup their env partition** (e.g. via ``dd if=/dev/env of=env.img``) then rewrite it after the partitioning (e.g. via ``dd if=env.img of=/dev/env``) latter.

And options are:
* **--help**/**-h** will print a help message
* **--disk**/**-d** will force ampart to treat input as a whole emmc disk, **conflicts** with --reserved/-r
* **--reserved**/**-r** will force ampart to treat input as a reserved partition, **conflicts** with --disk/-d
* **--offset**/**-O** **[offset]** will overwrite the default offset of the reserved partition (i.e. where partition table is), **only valid** when input is a whole disk, useful when OEMs have modified it (e.g. Xiaomi set it to 4M, right after the bootloader). default: 36M
* **--dry-run**/**-D** will only generate the new partition table but not actually write it, this will help you to make sure the output table is desired.
* **--output**/**-o** **[path]** (**currently is not implemented**) will write the ouput table to somewhere else rather than the path you set in [reserved/mmc]

All of the above would seem complicated but the CLI is really **clean and easy**, e.g.   
(*All of the following will clear all partitions, copy the old partition info of partition bootloader, reserved and env as partition 0-2, change the offset of env to the end of*)
````
# Create a single data partition to utilize all the space
./ampart /dev/mmcblk0 data:::
# Create a 2G system partition, mask2, and a data partition to utilize all the remaining space
./ampart /dev/mmcblk0 system::2G:2 data:::
# Same as the last one, except we leave a 8M gap before the 2 partitions, like some OEMs would do
./ampart /dev/mmcblk0 system:+8M:2G:2 data:+8M::
````


