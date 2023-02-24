The rules here is useful if you want to create symbol links under `/dev/block` on a mainline kernel with ept names as link names

The kernel would need the following patchset and `CONFIG_AMLOGIC_PARTITION=y` to natively support such partitions (choose one convinient for your use case):
 - [compare](https://github.com/7Ji/linux-6.1.y/compare/54968e71b3d75d34117442db5fd1fc3f0761439e%5E...f51821c5dc09619495172afeeab2fd19d9d38787)
 - [patch](https://github.com/7Ji/linux-6.1.y/compare/54968e71b3d75d34117442db5fd1fc3f0761439e%5E...f51821c5dc09619495172afeeab2fd19d9d38787.patch)
 - [diff](https://github.com/7Ji/linux-6.1.y/compare/54968e71b3d75d34117442db5fd1fc3f0761439e%5E...f51821c5dc09619495172afeeab2fd19d9d38787.diff)

**Do not use the patchset or the udev rules if you're on CoreELEC or EmuELEC, the vendor kernel they use already supports such partitions and will create block devices under `/dev` with correct names**

example dmesg output:
```
[    1.392239] mmc2: new DDR MMC card at address 0001
[    1.395820] mmcblk2: mmc2:0001 008G30 7.28 GiB 
[    1.402019]  mmcblk2: p1(bootloader) p2(reserved) p3(cache) p4(env) p5(logo) p6(recovery) p7(rsv) p8(tee) p9(crypt) p10(misc) p11(instaboot) p12(boot) p13(system) p14(params) p15(data)
[    1.418000] mmcblk2boot0: mmc2:0001 008G30 4.00 MiB 
[    1.422305] mmcblk2boot1: mmc2:0001 008G30 4.00 MiB 
[    1.426812] mmcblk2rpmb: mmc2:0001 008G30 4.00 MiB, chardev (244:0)
```
example lsblk output:
```
mmcblk2      179:32   0   7.3G  0 disk
├─mmcblk2p1  179:33   0     4M  0 part 
├─mmcblk2p2  179:34   0    64M  0 part 
├─mmcblk2p3  179:35   0   512M  0 part 
├─mmcblk2p4  179:36   0     8M  0 part 
├─mmcblk2p5  179:37   0    32M  0 part 
├─mmcblk2p6  179:38   0    32M  0 part 
├─mmcblk2p7  179:39   0     8M  0 part 
├─mmcblk2p8  179:40   0     8M  0 part 
├─mmcblk2p9  179:41   0    32M  0 part 
├─mmcblk2p10 179:42   0    32M  0 part 
├─mmcblk2p11 179:43   0   512M  0 part 
├─mmcblk2p12 179:44   0    32M  0 part 
├─mmcblk2p13 179:45   0     1G  0 part 
├─mmcblk2p14 179:46   0    64M  0 part 
└─mmcblk2p15 179:47   0   4.8G  0 part
```

example symbol links under `/dev/block`:
```
total 0
lrwxrwxrwx  1 root root   13 Feb 18 08:41 boot -> ../mmcblk2p12
lrwxrwxrwx  1 root root   13 Feb 18 08:41 bootloader -> ../mmcblk2p1
lrwxrwxrwx  1 root root   12 Feb 18 08:41 cache -> ../mmcblk2p3
lrwxrwxrwx  1 root root   12 Feb 18 08:41 crypt -> ../mmcblk2p9
lrwxrwxrwx  1 root root   13 Feb 18 08:41 data -> ../mmcblk2p15
lrwxrwxrwx  1 root root   12 Feb 18 08:41 env -> ../mmcblk2p4
lrwxrwxrwx  1 root root   13 Feb 18 08:41 instaboot -> ../mmcblk2p11
lrwxrwxrwx  1 root root   12 Feb 18 08:41 logo -> ../mmcblk2p5
lrwxrwxrwx  1 root root   12 Feb 18 08:41 misc -> ../mmcblk2p10
lrwxrwxrwx  1 root root   13 Feb 18 08:41 params -> ../mmcblk2p14
lrwxrwxrwx  1 root root   12 Feb 18 08:41 recovery -> ../mmcblk2p6
lrwxrwxrwx  1 root root   12 Feb 18 08:41 reserved -> ../mmcblk2p2
lrwxrwxrwx  1 root root   12 Feb 18 08:41 rsv -> ../mmcblk2p7
lrwxrwxrwx  1 root root   13 Feb 18 08:41 system -> ../mmcblk2p13
lrwxrwxrwx  1 root root   12 Feb 18 08:41 tee -> ../mmcblk2p8
```
