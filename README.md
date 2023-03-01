# Amlogic eMMC Partition Tool
[查看中文文档](README_cn.md)

**ampart** is a partition tool that grants you freedom on adjusting Amlogic's proprietary eMMC partition table in any way you like it.

The following SoCs are proven to be compatible with ampart:  
 - gxbb (s905)
 - gxl (s905l, s905x, s905d)
 - g12a (s905x2)
 - g12b (s922x)
 - sm1 (s905x3)
 - sc2 (s905x4)

It supports building and running under following platforms:
 - x86_64
   - mainly on dumped images
 - aarch64
   - either on eMMC directly, or
   - on dumped image

# Documentation
Please refer to the documentation under [doc] for the usage and details about the arguments and official demo scripts. The ampart program itself **does not have built-in CLI help message**, as it is intended to be used by power users or in scripts, for which the officially maintained doc in the repo should always be refered to anyway.


# Usage
A few easy use cases are listed here, keep in mind these are not the only things ampart could do.

## Get partition info
```
ampart /path/to/emmc/or/reserved/block/device/or/dump --mode webreport
```
This will give your a URL like the following:
```
https://7ji.github.io/ampart-web-reporter/?dsnapshot=logo::8388608:1%20recovery::25165824:1%20misc::8388608:1%20dtbo::8388608:1%20cri_data::8388608:2%20param::16777216:2%20boot::16777216:1%20rsv::16777216:1%20metadata::16777216:1%20vbmeta::2097152:1%20tee::33554432:1%20vendor::335544320:1%20odm::134217728:1%20system::1946157056:1%20product::134217728:1%20cache::1174405120:2%20data::18446744073709551615:4&esnapshot=bootloader:0:4194304:0%20reserved:37748736:67108864:0%20cache:113246208:1174405120:2%20env:1296039936:8388608:0%20logo:1312817152:8388608:1%20recovery:1329594368:25165824:1%20misc:1363148800:8388608:1%20dtbo:1379926016:8388608:1%20cri_data:1396703232:8388608:2%20param:1413480448:16777216:2%20boot:1438646272:16777216:1%20rsv:1463812096:16777216:1%20metadata:1488977920:16777216:1%20vbmeta:1514143744:2097152:1%20tee:1524629504:33554432:1%20vendor:1566572544:335544320:1%20odm:1910505472:134217728:1%20system:2053111808:1946157056:1%20product:4007657472:134217728:1%20data:4150263808:120919687168:4
```
Copy it to your browser and you can get well-formatted DTB and eMMC partition info, with notes are whether or not some areas on your eMMC could be written to

## Re-partition eMMC
```
ampart /path/to/your/eMMC/drive/or/dumped/image --mode dclone data::-1:4
```
This can turn a eMMC partition table that co-exists with MBR/GPT where you dare not create partitions freely on to like this where you can freely create MBR/GPT partitions on (4M-36M, 100M-116M, 117M-end):
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

## Extreme eMMC utilization
```
ampart /path/to/your/eMMC/drive/or/dumped/image --mode ecreate data:::
```
You can turn EPT further to like this where you can freely create MBR partitions on more areas(5M-36M, 100M-end):
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
# Download
Release versions can be downloaded from the [release page]. The static versions are recommended if you're not using ArchLinux.

The Github actions in this repo is also configured to compile ampart to x86_64 and aarch64 static binaries on each push, you can download these builds by clicking the green `√` the top middle of this page

# Building

## Officially maintained package
If you're using ArchLinux, you can build ampart with the AUR package [ampart-git] maintained by myself:
```
git clone https://aur.archlinux.org/ampart-git.git
cd ampart-git
makepkg -si
```

## Manually build
You can clone the repo and build it from source with ``make``. If you're using a Debian-derived distro like Ubuntu, for example, you can build it like this (ampart only depends on ``zlib``, the FDT support is implemented from ground up without `libfdt` at all):
 - Install the build dependencies, You could ignore these apt commands if you have the build tools and dependencies installed
    ```
    sudo apt update
    sudo apt install build-essential zlib1g-dev git
    ```
 - Clone and build
    ```
    git clone https://github.com/7Ji/ampart.git
    cd ampart
    make
    ```

# Inclusion in other projects
You're free to include ampart in your project as long as it meets [the license][license]. You're recommended to build it from source rather than downloading the binary release.

# License
**ampart**(Amlogic emmc partition tool) is licensed under [**GPL3**](https://gnu.org/licenses/gpl.html)
 * Copyright (C) 2022-2023 7Ji (pugokushin@gmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.

[license]: #license
[doc]: doc
[release page]: https://github.com/7Ji/ampart/releases
[ampart-git]:https://aur.archlinux.org/packages/ampart-git
