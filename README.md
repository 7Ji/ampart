# Amlogic emmc partition tool
**ampart** is a partition tool initially written for **HybridELEC** (a project that brings side-by-side dual-booting support to **CoreELEC**+**EmuELEC**) for easy re-partitioning of internal emmc device for **almost all Amlogic devices** to liberate me from editing the device tree for every device just to achive a custom partition layout. 

Yet now it has become **the one and only partition tool** for Amlogic's proprietary eMMC partition table format. It is written totally in **C** for portability and therefore **simple, fast yet reliable**, and provides a simple yet powerful argument-driven CLI for easy implementation in scripts.  

The following SoCs are proven to be compatible with ampart:
 - gxbb (s905)
 - gxl (s905l, s905x, s905d)
 - g12a (s905x2)
 - g12b (s922x)
 - sm1 (s905x3)
 - sc2 (s905x4) 
 
*DTB modes ``dclone`` and ``dedit`` is more suggested for post-sc2 SoC2, as they will break if the partitions node in DTB is broken*

Everything is done in a **single session**, without any **repeated execution** or **reboot**  

# Usage
With ampart, you can safely turn an eMMC partition table that co-exists with MBR/GPT partition tables on Amlogic devices from like this where you dare not create MBR/GPT partitions on:
```
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
28: userdata                 bcc00000 (   2.95G)        68b000000 (  26.17G)      4
===================================================================================
```
with an easy command
```
ampart /path/to/your/eMMC/drive/or/dumped/image --mode dclone data::-1:4
```
to like this where you can freely create MBR/GPT partitions on (4M-36M, 100M-116M, 117M-end):
```
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
 4: data                      8400000 ( 132.00M)        73f800000 (  28.99G)      4
===================================================================================
```
without breaking the partitions node in DTB so post-SC2 devices won't brick after partitioning (since they rely on partitions node in DTB to work)

And if you're willing to break partitions node in DTB, (should be OK for everything before SC2), then a command line like this:
```
ampart /path/to/your/eMMC/drive/or/dumped/image --mode ecreate data:::
```
to like this where you can freely create MBR partitions on (5M-36M, 100M-end):
```
===================================================================================
ID| name            |          offset|(   human)|            size|(   human)| masks
-----------------------------------------------------------------------------------
 0: bootloader                      0 (   0.00B)           400000 (   4.00M)      0
 1: env                        400000 (   4.00M)           800000 (   8.00M)      0
 2: cache                      c00000 (  12.00M)                0 (   0.00B)      0
    (GAP)                                                 1800000 (  24.00M)
 3: reserved                  2400000 (  36.00M)          4000000 (  64.00M)      0
 4: data                      6400000 ( 100.00M)        741800000 (  29.02G)      4
===================================================================================
```
So you can have more freedom when creating MBR partitions on eMMC, without worrying overwriting existing important partitions like ``reserved``, ``env``, etc

There're many more different modes available in ampart, please refer to the documentation for their corresponding usages.


# Documentation
Please refer to the documentation under [doc](doc) for the usage and details about the arguments. The ampart program itself **does not have built-in CLI help message**, as it is intended to be used in power users and scripts, for which the officially maintained doc in the repo should always be refered to anyway.

# Building
If you're using an ArchLinux-derived distro (ArchLinux itself, ArchLinux ARM, Manjaro, etc), you can build ampart with the AUR package [ampart-git](https://aur.archlinux.org/packages/ampart-git) maintained by myself:
```
git clone https://aur.archlinux.org/ampart-git.git
cd ampart-git
makepkg -si
```
Otherwise, you'll have to clone the repo and build it from source with ``make``. If you're using A Debian-derivced distro like Ubuntu, for example, you can build it like this (ampart only depends on ``zlib``, the FDT support is implemented from ground up without libfdt at all):
```
# You could ignore these apt commands if you have the build tools and dependencies installed
sudo apt update
sudo apt install build-essential zlib1g-dev
# The actual clone and build
git clone https://github.com/7Ji/ampart.git
cd ampart
make
# Installing is optional, you can call ampart with its path directly without installing
sudo make install
```

# License
**ampart**(Amlogic emmc partition tool) is licensed under [**GPL3**](https://gnu.org/licenses/gpl.html)
 * Copyright (C) 2022 7Ji (pugokushin@gmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.