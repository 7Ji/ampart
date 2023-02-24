The rules here is useful if you want to create symbol links under `/dev/block` on a mainline kernel with ept names as link names

The kernel would need the following patchset and `CONFIG_AMLOGIC_PARTITION=y` to natively support such partitions (choose one convinient for your use case):
 - [compare](https://github.com/7Ji/linux-6.1.y/compare/54968e71b3d75d34117442db5fd1fc3f0761439e%5E...e14cc34758dc48ac7d1a9b4015ddfe4f4eabf080)
 - [patch](https://github.com/7Ji/linux-6.1.y/compare/54968e71b3d75d34117442db5fd1fc3f0761439e%5E...e14cc34758dc48ac7d1a9b4015ddfe4f4eabf080.patch)
 - [diff](https://github.com/7Ji/linux-6.1.y/compare/54968e71b3d75d34117442db5fd1fc3f0761439e%5E...e14cc34758dc48ac7d1a9b4015ddfe4f4eabf080.diff)

**Do not use the patchset or the udev rules if you're on CoreELEC or EmuELEC, the vendor kernel they use already supports such partitions and will create block devices under `/dev` with correct names**

example dmesg output:
```
[    1.392239] mmc2: new DDR MMC card at address 0001
[    1.395820] mmcblk2: mmc2:0001 008G30 7.28 GiB 
[    1.402019]  mmcblk2: p0(bootloader) p1(reserved) p2(cache) p3(env) p4(logo) p5(recovery) p6(rsv) p7(tee) p8(crypt) p9(misc) p10(instaboot) p11(boot) p12(system) p13(params) p14(data)
[    1.418000] mmcblk2boot0: mmc2:0001 008G30 4.00 MiB 
[    1.422305] mmcblk2boot1: mmc2:0001 008G30 4.00 MiB 
[    1.426812] mmcblk2rpmb: mmc2:0001 008G30 4.00 MiB, chardev (244:0)
```
example lsblk output:
```
total 0
lrwxrwxrwx  1 root root   13 Feb 18 08:41 boot -> ../mmcblk2p11
lrwxrwxrwx  1 root root   12 Feb 18 08:41 cache -> ../mmcblk2p2
lrwxrwxrwx  1 root root   12 Feb 18 08:41 crypt -> ../mmcblk2p8
lrwxrwxrwx  1 root root   13 Feb 18 08:41 data -> ../mmcblk2p14
lrwxrwxrwx  1 root root   12 Feb 18 08:41 env -> ../mmcblk2p3
lrwxrwxrwx  1 root root   13 Feb 18 08:41 instaboot -> ../mmcblk2p10
lrwxrwxrwx  1 root root   12 Feb 18 08:41 logo -> ../mmcblk2p4
lrwxrwxrwx  1 root root   12 Feb 18 08:41 misc -> ../mmcblk2p9
lrwxrwxrwx  1 root root   13 Feb 18 08:41 params -> ../mmcblk2p13
lrwxrwxrwx  1 root root   12 Feb 18 08:41 recovery -> ../mmcblk2p5
lrwxrwxrwx  1 root root   12 Feb 18 08:41 reserved -> ../mmcblk2p1
lrwxrwxrwx  1 root root   12 Feb 18 08:41 rsv -> ../mmcblk2p6
lrwxrwxrwx  1 root root   13 Feb 18 08:41 system -> ../mmcblk2p12
lrwxrwxrwx  1 root root   12 Feb 18 08:41 tee -> ../mmcblk2p7
```
