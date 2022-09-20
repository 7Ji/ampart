# Amlogic emmc partition tool
**ampart** is a partition tool written for **HybridELEC**(a project that brings side-by-side dual-booting support to **CoreELEC**+**EmuELEC**) for easy re-partitioning of internal emmc device for **almost all Amlogic devices** to liberate me from editing the device tree for every device just to achive a custom partition layout. It is written totally in **C** for portability and therefore **simple, fast yet reliable**, and provides a simple yet powerful argument-driven CLI for easy implementation in scripts.  

Everything is done in a **single session**, without any **repeated execution** or **reboot**  


# Documentation
Please refer to the documentation under [doc](doc) for the usage and details about the arguments

# License
**ampart**(Amlogic emmc partition tool) is licensed under [**GPL3**](https://gnu.org/licenses/gpl.html)
 * Copyright (C) 2022 7Ji (pugokushin@gmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.