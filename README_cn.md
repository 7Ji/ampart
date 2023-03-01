# Amlogic eMMC分区工具

**ampart**是能让你随心所欲地调整晶晨专有的eMMC分区表的分区工具

下列SoC已证明与ampart相兼容
 - gxbb (s905)
 - gxl (s905l, s905x, s905d)
 - g12a (s905x2)
 - g12b (s922x)
 - sm1 (s905x3)
 - sc2 (s905x4)

ampart支持在以下平台上构建和运行：
 - x86_64
   - 主要是在dump的镜像上
 - aarch64
   - 可以直接在eMMC上操作
   - 也可以在dump的镜像上 

# 文档
请参考[doc]下的文档来了解参数的细节和用法，以及官方演示脚本。ampart程序本身**不提供内置的CLI帮助信息**，因为它应该被高级用户或者是在脚本里使用，而对于这些用途应该始终参照官方维护的文档

# 用途
这里列出一些ampart的用途，但ampart可不只是能做这些事

## 获取分区信息
```
ampart /path/to/emmc/or/reserved/block/device/or/dump --mode webreport
```
就能获得像下面这样的链接
```
https://7ji.github.io/ampart-web-reporter/?dsnapshot=logo::8388608:1%20recovery::25165824:1%20misc::8388608:1%20dtbo::8388608:1%20cri_data::8388608:2%20param::16777216:2%20boot::16777216:1%20rsv::16777216:1%20metadata::16777216:1%20vbmeta::2097152:1%20tee::33554432:1%20vendor::335544320:1%20odm::134217728:1%20system::1946157056:1%20product::134217728:1%20cache::1174405120:2%20data::18446744073709551615:4&esnapshot=bootloader:0:4194304:0%20reserved:37748736:67108864:0%20cache:113246208:1174405120:2%20env:1296039936:8388608:0%20logo:1312817152:8388608:1%20recovery:1329594368:25165824:1%20misc:1363148800:8388608:1%20dtbo:1379926016:8388608:1%20cri_data:1396703232:8388608:2%20param:1413480448:16777216:2%20boot:1438646272:16777216:1%20rsv:1463812096:16777216:1%20metadata:1488977920:16777216:1%20vbmeta:1514143744:2097152:1%20tee:1524629504:33554432:1%20vendor:1566572544:335544320:1%20odm:1910505472:134217728:1%20system:2053111808:1946157056:1%20product:4007657472:134217728:1%20data:4150263808:120919687168:4
```
复制到浏览器打开，你就能得到格式整齐的DTB和eMMC分区信息，并且也有注释哪些区域能不能写入

## 给eMMC重分区
```
ampart /path/to/your/eMMC/drive/or/dumped/image --mode dclone data::-1:4
```
就能把你不敢随意分区的和MBR/GPT共存eMMC分区表转换成下面这样，你就可以自由创建MBR/GPT分区表的样子（4M-36M，100M-116M，117M到结尾）
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

## 极限eMMC利用率
```
ampart /path/to/your/eMMC/drive/or/dumped/image --mode ecreate data:::
```
你能把EPT进一步转变成像这样可以在更多区域创建MBR分区（5M-36M，100M到结尾）
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
# 下载
发布版本可以从[发布页][release page]下载。如果你使用的不是ArchLinux，建议下载静态编译版本

本仓库的Github actions也配置为每次推送更新时自动构建x86_64和aarch64的静态二进制文件，你可以点击本页面中间上面的绿色`√`来下载

# 构建

## 官方维护的软件包
如果你用的是ArchLinux，你可以通过我自己维护的AUR包[ampart-git]来构建ampart：
```
git clone https://aur.archlinux.org/ampart-git.git
cd ampart-git
makepkg -si
```

## 手动构建
你可以克隆仓库然后用`make`从源码构建。如果你用的是Debian衍生的发行版比如说Ubuntu，你可以像这样构建（ampart只依赖`zlib`，设备树的支持是从底层不经`libfdt`实现的）
 - 安装构建依赖，如果你已经安装了这些工具，你可以略过这一步
    ```
    sudo apt update
    sudo apt install build-essential zlib1g-dev git
    ```
 - 克隆并构建
    ```
    git clone https://github.com/7Ji/ampart.git
    cd ampart
    make
    ```

# 包含在其他项目中
你可以自由地把ampart引入到你的项目中，只要它符合[授权许可][license]。建议你从源码构建，而不是下载二进制发布

# 授权许可
**ampart**(Amlogic emmc partition tool) is licensed under [**GPL3**](https://gnu.org/licenses/gpl.html)
 * Copyright (C) 2022-2023 7Ji (pugokushin@gmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.

[license]: #授权许可
[doc]: doc
[release page]: https://github.com/7Ji/ampart/releases
[ampart-git]:https://aur.archlinux.org/packages/ampart-git
