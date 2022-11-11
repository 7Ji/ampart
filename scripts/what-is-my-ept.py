#!/usr/bin/env python3
# Official demo script for ampart by 7Ji. Same license as ampart itself (GPLv3)

# Usage:
#  - This script automatically finds the eMMC block device (e.g. /dev/mmcblk2), and gets EPT snapshot on it from ampart's stdout, and report it on the terminal

import dataclasses
import pathlib
import subprocess

@dataclasses.dataclass
class Partition:
    name: str
    offset: int
    size: int
    masks: int

    @classmethod
    def from_parg(cls, parg: str):
        parg_parts = parg.split(":")
        if len(parg_parts) != 4:
            raise ValueError("Splitted parg length is not 4")
        return cls(parg_parts[0], int(parg_parts[1]), int(parg_parts[2]), int(parg_parts[3]))

    @staticmethod
    def size_human_readable(size: int)->str:
        for suffix in "BKMGTPEZY":
            if size < 1024:
                return f"{size:7.2f}{suffix}"
            else:
                size /= 1024
        raise ValueError("Value too large to convert to human-readable")

    def show(self, id: int = 0, last_end: int = 0) -> int:
        current_end = self.offset + self.size
        if self.offset > last_end:
            diff = self.offset-last_end
            print(f"   (gap)                                      {diff:16x} {self.size_human_readable(diff)}")
        elif self.offset < last_end:
            diff = last_end-self.offset
            print(f"   (overlap)                                  {diff:16x} {self.size_human_readable(diff)}")
        print(f"{id:2d} {self.name:16s} {self.offset:16x} {self.size_human_readable(self.offset)} {self.size:16x} {self.size_human_readable(self.size)} {current_end:16x} {self.size_human_readable(current_end)} {self.masks:2d}")
        return current_end


@dataclasses.dataclass
class Snapshot:
    partitions: list[Partition]
    count: int

    @classmethod
    def from_line(cls, line: str):
        line_parts = line.split(" ")
        if len(line_parts) <= 0:
            raise ValueError("Parts of snapshot not greater than 0, impossible")
        partitions = [Partition.from_parg(line_part) for line_part in line_parts]
        return cls(partitions, len(partitions))

    def show(self):
        line:str = "=" * 102
        end:int = 0
        print(f"{self.count} partitions in the EPT\n{line}\nid|name           |            start| (human)|            size| (human)|            end| (human)|masks\n{line}")
        for i, partition in enumerate(self.partitions):
            end = partition.show(i, end)
        print(line)
        
# @dataclasses.dataclass
# class Snapshots:
#     decimal: Snapshot
#     hex: Snapshot
#     human: Snapshot

#     @classmethod
#     def from_log(cls, log: bytes):
#         snapshots = [Snapshot.from_line(line) for line in log.decode("utf-8").split("\n")[:-1]]
#         if len(snapshots) != 3:
#             raise ValueError(f"There is {len(snapshots)} instead of 3 snapshots, refuse to work")
#         return cls(snapshots[0], snapshots[1], snapshots[2])


if __name__ == "__main__":
    dev = pathlib.Path("/dev")
    rpmb = [block for block in dev.glob(r"mmcblk[0-9]rpmb")]
    if len(rpmb) != 1:
        raise ValueError(f"{len(rpmb)} instead of 1 rpmb devices, possibly caused by multiple eMMC which is impossible on Amlogic devices or eMMC does not exist, refuse to continue")
    emmc = rpmb[0].with_name(rpmb[0].name[:7])
    print(f"Automatically chosen {emmc} as eMMC")
    r = subprocess.run(
        (
            "ampart",
            emmc,
            "--mode",
            "esnapshot"
        ),
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )
    if r.returncode:
        raise RuntimeError(f"ampart returns with {r.returncode}, failed to read EPT")
    snapshot = Snapshot.from_line(r.stdout.decode("utf-8").split("\n")[0])
    snapshot.show()
