# Emulate eMMC with only DTB / 仅通过DTB来模拟eMMC

In some cases (e.g. where you need to reverse-engineer an Android box's partition layout, on which you can not boot a Linux distro and have to use the stock Android, and can't dump the whole eMMC, but you can still get its DTB from its firmware), to emulate an eMMC dump and generate EPT on it with the DTB is a good idea to get the EPT  
由的情况下（比如说你要逆向工程安卓盒子的分区布局，但你不能在上面启动Linux发行版，必须用原厂安卓，也不能搞到整个eMMC的转储，不过你能解包刷机包拿到DTB），模拟eMMC转储并在上面通过DTB生成EPT是一个拿到eMMC分区表的不错主义

## Getting the exact size of the actual eMMC / 获得实际eMMC的具体大小
If you can access the sysfs on the box, you can obtain the sectors of the corresponding block device:  
如果你能在盒子上浏览sysfs，你可以获得对应块设备的扇区数
```
cat /sys/block/your_eMMC_block_device/size
```
This can give you the sector account, with which you should multiply 512 to get the size in Byte.  
这条命令能拿到扇区数，乘以512就是具体的字节数

Or if you can take the box apart, you could identify the eMMC chip's model, and search its info online. The manufacturer's technical documentation would contain the byte-correct size for the user area (those except addtional ``boot0``, ``boot1``, ``rpmb``).  
或者如果你能把盒子拆开的话，你能识别eMMC芯片的型号，然后在网上查信息。制造商的官方技术文档里一般会包含精确到字节的用户区域（除了额外的`boot0`, `boot`, `rpmb`外的区域）大小

Or if you can't do both, then based on the eMMC capacity labeled by the vendor, you can guess its size from the existing data of devices in my hand:  
或者，都不行的话，基于盒子生产商标注的eMMC容量，你可以根据我受伤已有的设备的数据来猜它的大小

|size|name|make|hex|human|diff|
|-|-|-|-|-|-|
|8G|008G30|Toshiba|1d2000000|7456M|8G-736M|
|16G|AJTD4R|Samsung|3a3e00000|14910M|16G-1474M|
|32G|?|?|747c00000|29820M|32G-2948M|
|128G|DUTA32|Samsung|1d1ec0000|119276M|128G-11796M

If exact partition size for the last partition is not required (which is actually usually the case, the last partition ``data``/``userdata`` usually has a size ``-1`` in DTB, and will be mapped to a partition that occupies all the remaining spaces in eMMC. You don't neccessarrily need its exact size if all you want are the partition infos for those at the beginning), you can just YOLO it and use a guessed number (e.g. 30G for a 32G eMMC)  
如果不需要最后一个分区的具体大小的话（一般都不需要，最后一个分区`data`/`userdata`一般在DTB里大小是`-1`，会被转化为一个占用eMMC里所有剩余空间的分区。逆向最需要的是前面的定死的分区，这个分区的大小不是太重要）。你可以直接估算一个大体数字（比如32G eMMC估计一个30G）

## Create an empty file with the same size / 创建一个同样大小的空文件
If your fs supports ``truncate``, you can create an empty file like this:  
如果你的文件系统支持`truncate`，你可以这样创建一个空文件
```
truncate -s 119276M fake_emmc.img
```
You can also create the file with ``dd``:  
你也可以用`dd`来创建
```
dd if=/dev/null of=fake_emmc.img bs=1M count=119276M
```

## Inject DTB into fake eMMC / 把DTB注入到假eMMC里

The Android DTBs on Amlogic devices are stored at 4M offset in the reserved partition, which then is placed at 36M offset in the eMMC, so DTB is stored at 36M+4M=40M offset in the eMMC.  
晶晨设备上的DTB一般储存在保留分区的4M偏移处，而保留分区又放在eMMC的36M偏移处，所以DTB放在eMMC的40M偏移处

Writing your DTB to 40M offset so ampart can read it:  
把你的DTB写到40M偏移处，这样ampart就能读取了
```
dd if=your_dtb.dtb of=fake_emmc.img bs=1M seek=40 conv=notrunc
```
``notrunc`` is required, otherwise the part after the DTB will be removed  
这里`notrunc`是必要的，不然文件后面的部分会被砍掉

## Check if ampart recognizes the fake eMMC / 检查ampart是否识别假eMMC
If your operation is correct, ampart would now identify the file as a dump for a whole eMMC, with valid DTB and an empty EPT (eMMC partition table). Running ``ampart`` with only the file as argument:  
如果你的操作正确，ampart现在会把这个文件当成一个包含有效的DTB和空EPT的eMMC全盘的转储。只以这个文件作为参数运行`ampart`
```
ampart fake_emmc.img
```
Would report the partitions in DTB:  
会汇报DTB里的分区
```
DTS report partitions: 25 partitions in the DTB:
=======================================================
ID| name            |            size|(   human)| masks
-------------------------------------------------------
 0: frp                        200000 (   2.00M)      1
 1: factory                    800000 (   8.00M)     17
 2: vendor_boot_a             1800000 (  24.00M)      1
 3: vendor_boot_b             1800000 (  24.00M)      1
 4: tee                       2000000 (  32.00M)      1
 5: logo                       800000 (   8.00M)      1
 6: misc                       200000 (   2.00M)      1
 7: dtbo_a                     200000 (   2.00M)      1
 8: dtbo_b                     200000 (   2.00M)      1
 9: cri_data                   800000 (   8.00M)      2
10: param                     1000000 (  16.00M)      2
11: odm_ext_a                 1000000 (  16.00M)      1
12: odm_ext_b                 1000000 (  16.00M)      1
13: oem_a                     2000000 (  32.00M)      1
14: oem_b                     2000000 (  32.00M)      1
15: boot_a                    4000000 (  64.00M)      1
16: boot_b                    4000000 (  64.00M)      1
17: rsv                       1000000 (  16.00M)      1
18: metadata                  1000000 (  16.00M)      1
19: vbmeta_a                   200000 (   2.00M)      1
20: vbmeta_b                   200000 (   2.00M)      1
21: vbmeta_system_a            200000 (   2.00M)      1
22: vbmeta_system_b            200000 (   2.00M)      1
23: super                    90000000 (   2.25G)      1
24: userdata                          (AUTOFILL)      4
=======================================================
```
And an empty EPT:  
和空的EPT
```
EPT report: 0 partitions in the table:
===================================================================================
ID| name            |          offset|(   human)|            size|(   human)| masks
-----------------------------------------------------------------------------------
===================================================================================
```

## Generate EPT from DTB / 从DTB生成EPT
The ``dtoe`` mode in ampart can generate EPT (eMMC partition table) from the existing DTB in the same way Amlogic's u-boot would do it. With a command like this:  
`dtoe`模式可以用来从现存的DTB里以和晶晨的u-boot相同的方式生成EPT（eMMC分区表），通过下面这样的命令
```
ampart fake_emmc.img  --mode dtoe
```

The EPT will be genrated:  
就可以生成EPT
```
EPT report: 29 partitions in the table:
===================================================================================
ID| name            |          offset|(   human)|            size|(   human)| masks
-----------------------------------------------------------------------------------
 0: bootloader                      0 (   0.00B)           400000 (   4.00M)      0
    (GAP)                                                 2000000 (  32.00M)
 1: reserved                  2400000 (  36.00M)          4000000 (  64.00M)      0
    (GAP)                                                  800000 (   8.00M)
 2: cache                     6c00000 ( 108.00M)                0 (   0.00B)      0
    (GAP)                                                  800000 (   8.00M)
 3: env                       7400000 ( 116.00M)           800000 (   8.00M)      0
    (GAP)                                                  800000 (   8.00M)
 4: frp                       8400000 ( 132.00M)           200000 (   2.00M)      1
    (GAP)                                                  800000 (   8.00M)
 5: factory                   8e00000 ( 142.00M)           800000 (   8.00M)     17
    (GAP)                                                  800000 (   8.00M)
 6: vendor_boot_a             9e00000 ( 158.00M)          1800000 (  24.00M)      1
    (GAP)                                                  800000 (   8.00M)
 7: vendor_boot_b             be00000 ( 190.00M)          1800000 (  24.00M)      1
    (GAP)                                                  800000 (   8.00M)
 8: tee                       de00000 ( 222.00M)          2000000 (  32.00M)      1
    (GAP)                                                  800000 (   8.00M)
 9: logo                     10600000 ( 262.00M)           800000 (   8.00M)      1
    (GAP)                                                  800000 (   8.00M)
10: misc                     11600000 ( 278.00M)           200000 (   2.00M)      1
    (GAP)                                                  800000 (   8.00M)
11: dtbo_a                   12000000 ( 288.00M)           200000 (   2.00M)      1
    (GAP)                                                  800000 (   8.00M)
12: dtbo_b                   12a00000 ( 298.00M)           200000 (   2.00M)      1
    (GAP)                                                  800000 (   8.00M)
13: cri_data                 13400000 ( 308.00M)           800000 (   8.00M)      2
    (GAP)                                                  800000 (   8.00M)
14: param                    14400000 ( 324.00M)          1000000 (  16.00M)      2
    (GAP)                                                  800000 (   8.00M)
15: odm_ext_a                15c00000 ( 348.00M)          1000000 (  16.00M)      1
    (GAP)                                                  800000 (   8.00M)
16: odm_ext_b                17400000 ( 372.00M)          1000000 (  16.00M)      1
    (GAP)                                                  800000 (   8.00M)
17: oem_a                    18c00000 ( 396.00M)          2000000 (  32.00M)      1
    (GAP)                                                  800000 (   8.00M)
18: oem_b                    1b400000 ( 436.00M)          2000000 (  32.00M)      1
    (GAP)                                                  800000 (   8.00M)
19: boot_a                   1dc00000 ( 476.00M)          4000000 (  64.00M)      1
    (GAP)                                                  800000 (   8.00M)
20: boot_b                   22400000 ( 548.00M)          4000000 (  64.00M)      1
    (GAP)                                                  800000 (   8.00M)
21: rsv                      26c00000 ( 620.00M)          1000000 (  16.00M)      1
    (GAP)                                                  800000 (   8.00M)
22: metadata                 28400000 ( 644.00M)          1000000 (  16.00M)      1
    (GAP)                                                  800000 (   8.00M)
23: vbmeta_a                 29c00000 ( 668.00M)           200000 (   2.00M)      1
    (GAP)                                                  800000 (   8.00M)
24: vbmeta_b                 2a600000 ( 678.00M)           200000 (   2.00M)      1
    (GAP)                                                  800000 (   8.00M)
25: vbmeta_system_a          2b000000 ( 688.00M)           200000 (   2.00M)      1
    (GAP)                                                  800000 (   8.00M)
26: vbmeta_system_b          2ba00000 ( 698.00M)           200000 (   2.00M)      1
    (GAP)                                                  800000 (   8.00M)
27: super                    2c400000 ( 708.00M)         90000000 (   2.25G)      1
    (GAP)                                                  800000 (   8.00M)
28: userdata                 bcc00000 (   2.95G)       1c62000000 ( 113.53G)      4
===================================================================================
```
which will be 100% the same as the EPT on the actual device.  
这个EPT和实际设备上的EPT完全相同

## Get snapshots of the new EPT / 获得新EPT的快照
If you have a script that'll do some logic depending on the layout, or want to create a nicer table with the data, the above table might be hard to parse. You can use ampart's ``esnapshot`` mode to get the snapshot of the table. ampart will always print the logs to stderr and snapshots to stdout.  
如果你有一个根据分区布局来运行的脚本，或者想用上面的数据创建一个更好看的表格，上面的表可能很难处理。你可以用ampart的`esnapshot`模式来获得分区表的快照。ampart总是把日志打印到标准错误，快照打印到标准输出

Please refer to [Scripting: Create CSV table from snapshot][script-csv] for further documentation  
请参考[脚本：从快照创建CSV表格][script-csv]来了解更多

[script-csv]: scripting-csv-table-from-snapshot.md