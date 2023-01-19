#!/usr/bin/env python3
# Official demo script for ampart by 7Ji. Same license as ampart itself (GPLv3)

# Usage:
#  - This script splits an emmc dump (got with e.g. dd) into multiple partition dump files

import dataclasses
import subprocess
import sys
import datetime
import io
import os

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

    def split(self, id: int, ifile: io.BufferedReader, odir: str):
        with open(f"{odir}/{id:02d}_{self.name}.img", "wb") as ofile:
            count, remain = divmod(self.size, 0x10000)
            ifile.seek(self.offset, os.SEEK_SET)
            for _ in range(count):
                ofile.write(ifile.read(0x10000))
            if remain:
                ofile.write(ifile.read(remain))

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

    def split(self, ifile: io.BufferedReader, odir: str):
        for id, partition in enumerate(self.partitions):
            partition.split(id, ifile, odir)

if __name__ == "__main__":
    if len(sys.argv) <= 1:
        sys.exit(1)
    ifile = sys.argv[1]
    r = subprocess.run(
        (
            "ampart",
            ifile,
            "--mode",
            "esnapshot"
        ),
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )
    if r.returncode:
        raise RuntimeError(f"ampart returns with {r.returncode}, failed to read EPT")
    snapshot = Snapshot.from_line(r.stdout.decode("utf-8").split("\n")[0])
    stamp = datetime.datetime.today().strftime(r"%Y%m%d-%H%M%S")
    odir = f"{ifile}_split_{stamp}"
    print(f"Splitting into {odir}")
    os.mkdir(odir, 0o744)
    with open(ifile, "rb") as f:
        snapshot.split(f, odir)