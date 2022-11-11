# Scripting: Quick report of EPT

Documentation for demo script [what-is-my-ept.py](../scripts/what-is-my-ept.py)

This scripts automatically identified the eMMC block device under ``/dev``, and report to the user about the EPT layout

Root permission is required as always.

It is written in Python3, and uses ``dataclass``, ``pathlib`` and ``subprocess``, these modules usually come by default on most distros when installing Python3, but you need to take care if they does not exist

A run with this script:
```
[root@bestv7Ji ~]# /tmp/what-is-my-ept.py 
Automatically chosen /dev/mmcblk2 as eMMC
5 partitions in the EPT
======================================================================================================
id|name           |            start| (human)|            size| (human)|            end| (human)|masks
======================================================================================================
 0 bootloader                      0    0.00B           400000    4.00M           400000    4.00M  0
   (gap)                                               2000000   32.00M
 1 reserved                  2400000   36.00M          4000000   64.00M          6400000  100.00M  0
   (gap)                                                800000    8.00M
 2 cache                     6c00000  108.00M                0    0.00B          6c00000  108.00M  0
   (gap)                                                800000    8.00M
 3 env                       7400000  116.00M           800000    8.00M          7c00000  124.00M  0
   (gap)                                                800000    8.00M
 4 data                      8400000  132.00M        1c9c00000    7.15G        1d2000000    7.28G  4
======================================================================================================
```