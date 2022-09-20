# Command-Line Interface
## Basic syntax
````
ampart ([nop 1] ([nop 1 arg]) [nop 2] ([nop 2 arg])...) [target] [parg 1] [parg 2]...
````
 - **nop** means non-positional argument, nops can be written anywhere, if a nop has its required argument, that required argument must be right after that very nop.
 - **target** means the target file/block device to operate on, it can either store DTB (plain DTB, Amlogic multi-DTB, or their gzipped versions), or the reserved partition, or the whole disk. By default ampart auto-recognize the target content, but you can hard-define it (see below)
 - **parg** means partition argument, each of the mode has its own requirement of parg. Check [Partition Argument Mini-Language][parg] and [Available modes][modes] for more details


e.g. 
The following two commands are inter-changable
````
ampart --dry-run --mode dclone /dev/mmcblk0 data::-1:4
         ^nop1    ^nop2 ^nop2-arg ^target      ^parg1
````
    
````
ampart /dev/mmcblk0 data::-1:4 --mode dclone --dry-run 
         ^target      ^parg1    ^nop1 ^nop1-arg  ^nop2
````
But the following two commands are not
```
ampart --mode dclone /dev/mmcblk0 system::2G:2 data::-1:4
        ^nop1 ^nop1-arg ^target    ^parg1       ^parg2
```
```
ampart --mode dclone /dev/mmcblk0 data::-1:4 system::2G:2
        ^nop1 ^nop1-arg ^target    ^parg1     ^parg2
```

## Standard Input/Output
### stdin
ampart does **not** accept any input non read anything from stdin (for now). This will potentially be used in the future for reading instructions piped in, but will **never** be used to read user input, as ampart is **only** intended to be called by scripts
### stdout
ampart **only** generates snapshots on standard output in **dsnapshot** and **esnapshot** mode, otherwise nothing is written to the standard output. 3 different snapshots will be guaranteed to be fed on standard output in the decimal>hex>human(decimal) order, one snapshot per line, scripts calling ampart can split the snapshots on \n character to get each snapshot, and split each snapshot on ' '(space, \x20)  character to get each partition, then split each partition on : character to get name, offset, size and masks

The following script calls ampart to get snapshot and prints it
```
IFS=$'\n'
snapshots=($(./ampart --mode dsnapshot dtbs/gxl_stock.dtb))
unset IFS
for i in {0..2}; do
    echo "Partitions in snapshot $i"
    j=0
    for parg in ${snapshots[$i]}; do
        IFS=':'
        read -r -a part <<< "$parg"
        unset IFS
        echo "$j: Name '${part[0]}', Offset ${part[1]}, Size ${part[2]}, Masks ${part[3]}"
        let j++
    done
done
```
output of the above script:
```
Partitions in snapshot 0
0: Name 'logo', Offset , Size 33554432, Masks 1
1: Name 'recovery', Offset , Size 33554432, Masks 1
2: Name 'rsv', Offset , Size 8388608, Masks 1
3: Name 'tee', Offset , Size 8388608, Masks 1
4: Name 'crypt', Offset , Size 33554432, Masks 1
5: Name 'misc', Offset , Size 33554432, Masks 1
6: Name 'boot', Offset , Size 33554432, Masks 1
7: Name 'system', Offset , Size 2147483648, Masks 1
8: Name 'cache', Offset , Size 536870912, Masks 2
9: Name 'data', Offset , Size -1, Masks 4
Partitions in snapshot 1
0: Name 'logo', Offset , Size 0x2000000, Masks 1
1: Name 'recovery', Offset , Size 0x2000000, Masks 1
2: Name 'rsv', Offset , Size 0x800000, Masks 1
3: Name 'tee', Offset , Size 0x800000, Masks 1
4: Name 'crypt', Offset , Size 0x2000000, Masks 1
5: Name 'misc', Offset , Size 0x2000000, Masks 1
6: Name 'boot', Offset , Size 0x2000000, Masks 1
7: Name 'system', Offset , Size 0x80000000, Masks 1
8: Name 'cache', Offset , Size 0x20000000, Masks 2
9: Name 'data', Offset , Size -1, Masks 4
Partitions in snapshot 2
0: Name 'logo', Offset , Size 32M, Masks 1
1: Name 'recovery', Offset , Size 32M, Masks 1
2: Name 'rsv', Offset , Size 8M, Masks 1
3: Name 'tee', Offset , Size 8M, Masks 1
4: Name 'crypt', Offset , Size 32M, Masks 1
5: Name 'misc', Offset , Size 32M, Masks 1
6: Name 'boot', Offset , Size 32M, Masks 1
7: Name 'system', Offset , Size 2G, Masks 1
8: Name 'cache', Offset , Size 512M, Masks 2
9: Name 'data', Offset , Size -1, Masks 4
```
As you can see the first and second output is more friendly for script to parse, the last is best for reporting to user
### stderr
Since standard output is used for snapshots only, all logs are printed to standard error

## Non-positional arguments
All of the following arguments are listed in the --long-arg/-short-arg ([required argument]) style, you can use either form
 - --version/-v
   - Prints the version and early quit
 - --help/-h
   - Prints the help message and early quit
 - --mode/-m [mode name]
   - Set ampart to run in one of the following modes:
     - dtoe (DTB to EPT)
     - etod (EPT to DTB)
     - epedantic (is EPT pedantic)
     - dedit (DTB edit)
     - eedit (EPT edit)
     - dsnapshot (DTB snapshot)
     - esnapshot (EPT snapshot)
     - dclone (DTB clone)
     - eclone (EPT clone)
     - ecreate (EPT create)
   - Default: none, if no mode is set, ampart will not process the target
 - --content/-c [content type]
   - Set the content of the target
     - **auto** auto-identify target's content
     - **dtb** force ampart to treat target as DTB
     - **reserved** force ampart to treat target as reserved partition
     - **disk** force ampart to treat target as whole disk
   - Default: auto
 - --migrate/-M [migrate strategy]
   - Set the migrate strategy about partitions
     - **none** don't migrate any partition
     - **essential** only migrate essential partitions (reserved, env, misc, logo, etc)
     - **all** migrate all partitions
 - --strict-device/-s
   - If target is a block device, and --content is set, stick with that, and don't try to find the corresponding whole disk (If not set, and target is, e.g. /dev/reserved, ampart will find its underlying disk /dev/mmcblk0 and operate on that instead)
 - --dry-run/-d
   - Don't do any write
 - --offset-reserved/-R [offset of reserved partition in disk]
 - --offset-dtb/-D [offset of dtb in reserved partition]
 - --gap-partition/-p [gap between partitions]
 - --gap-reserved/-r [gap before reserved partition]

[parg]:partition-argument-mini-language.md
[modes]:available-modes.md