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