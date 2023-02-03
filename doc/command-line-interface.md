# Command-Line Interface / 命令行界面
## Basic syntax / 基本语法
````
ampart ([nop 1] ([nop 1 arg]) [nop 2] ([nop 2 arg])...) [target] [parg 1] [parg 2]...
````
 - **nop**
    - means non-positional argument, nops can be written anywhere, if a nop has its required argument, that required argument must be right after that very nop.    
    - 意为非位置参数，非位置参数可以被写在任何位置，如果非位置参数有其对应的形参，此形参必须紧跟着这个非位置参数
 - **target** 
    - means the target file/block device to operate on, it can either store DTB (plain DTB, Amlogic multi-DTB, or their gzipped versions), or the reserved partition, or the whole disk. By default ampart auto-recognize the target content, but you can hard-define it (see below)  
    - 意为需要操作的目标文件/块设备，可以储存DTB（普通DTB，Amlogic多DTB，或者gzip压缩过的两者），或者是保留分区，或者是全盘。默认情况下ampart会自动识别目标内容类型，但你也可以硬定义（看下面）

 - **parg**
    - means partition argument, each of the mode has its own requirement of parg. Check [Partition Argument Mini-Language][parg] and [Available modes][modes] for more details  
    - 意为分区参数，每一个模式都有各自的对分区参数的要求。参考[位置参数迷你语言][parg]和[可用模式][modes]来了解更多细节


e.g. 
The following two commands are inter-changable  
比如，下面这两条命令是可以互换的
````
ampart --dry-run --mode dclone /dev/mmcblk0 data::-1:4
         ^nop1    ^nop2 ^nop2-arg ^target      ^parg1
````
    
````
ampart /dev/mmcblk0 data::-1:4 --mode dclone --dry-run 
         ^target      ^parg1    ^nop1 ^nop1-arg  ^nop2
````
But the following two commands are not  
但下面这两条不可以
```
ampart --mode dclone /dev/mmcblk0 system::2G:2 data::-1:4
        ^nop1 ^nop1-arg ^target    ^parg1       ^parg2
```
```
ampart --mode dclone /dev/mmcblk0 data::-1:4 system::2G:2
        ^nop1 ^nop1-arg ^target    ^parg1     ^parg2
```

## Non-positional arguments / 非位置参数
All of the following arguments are listed in the --long-arg/-short-arg ([required argument]) style, you can use either form  
所有以下的形参都以 --长参数/-短参数（[对应形参]）的样式写出，你可以使用任意一者
 - --version/-v
   - Prints the version and early quit  
   打印版本，直接退出
 - --help/-h
   - Prints the help message and early quit  
   打印帮助信息，直接退出
 - --mode/-m [mode name / 模式名]
   - Set ampart to run in one of the following modes:  
   设置ampart来运行在以下一个模式之中
     - dtoe (DTB to EPT / DTB转换到EPT)
     - etod (EPT to DTB / EPT转换到DTB)
     - epedantic (is EPT pedantic / EPT迂腐吗)
     - dedit (DTB edit / DTB编辑)
     - eedit (EPT edit / EPT编辑)
     - dsnapshot (DTB snapshot / DTB快照)
     - esnapshot (EPT snapshot / EPT快照)
     - webreport (WEB report / 网页汇报)
     - dclone (DTB clone / DTB克隆)
     - eclone (EPT clone / EPT克隆)
     - ecreate (EPT create / EPT创建)
   - Default: none, if no mode is set, ampart will not process the target  
   默认：无，如果不设置任何模式，ampart不会处理目标
 - --content/-c [content type / 内容类型]
   - Set the content of the target  
   设置目标的内容类型
     - **auto** auto-identify target's content / 自动识别目标内容
     - **dtb** force ampart to treat target as DTB / 强制ampart将目标视作DTB
     - **reserved** force ampart to treat target as reserved partition / 强制ampart将目标视作保留分区
     - **disk** force ampart to treat target as whole disk / 强制ampart将目标视作全盘
   - Default: auto  
   默认：自动
 - --migrate/-M [migrate strategy / 迁移策略]
   - Set the migrate strategy about partitions  
   设置分区的迁移策略
     - **none** don't migrate any partition / 不迁移任何分区
     - **essential** only migrate essential partitions (reserved, env, misc, logo, etc) / 只迁移必要分区（保留，环境，杂项，图标等）
     - **all** migrate all partitions / 迁移所有分区
   - Default: essential  
   默认：仅必要
 - --strict-device/-s
   - If target is a block device, and --content is set, stick with that, and don't try to find the corresponding whole disk (If not set, and target is, e.g. /dev/reserved, ampart will find its underlying disk /dev/mmcblk0 and operate on that instead)  
   如果目标是块设备，且--content已设置目标类型，保持这一设置的目标类型和目标本身，不要尝试寻找对应的全盘（如果不设置，在目标是比如说`/dev/reserved`保留分区的情况下，ampart会搜寻其对应的全盘`/dev/mmcblk0`并转而在其上面操作）
 - --dry-run/-d
   - Don't do any write  
   不要作任何写入
 - --offset-reserved/-R [offset of reserved partition in disk / 保留分区在盘内的偏移]
 - --offset-dtb/-D [offset of dtb in reserved partition / DTB在保留分区内的迁移]
 - --gap-partition/-p [gap between partitions / 分区间的间隔]
 - --gap-reserved/-r [gap before reserved partition / 保留分区前的间隔]

## Standard Input/Output / 标准输入输出
### stdin / 标准输入
ampart does **not** accept any input nor read anything from stdin (for now). This will potentially be used in the future for reading instructions piped in, but will **never** be used to read user input, as ampart is **only** intended to be called by scripts (and probabaly by some power-users)  
ampart**不会**自标准输入读取任何的用户输入或者其他东西（仅目前而言）。在未来这可能被用来读取传入的命令，但**绝不会**用来读取用户输入，因为ampart**仅**应被脚本调用（或者可能被一些高级用户使用）
### stdout / 标准输出
ampart **only** generates snapshots on standard output in **dsnapshot** and **esnapshot** mode, otherwise nothing is written to the standard output. 3 different snapshots will be guaranteed to be fed on standard output in the decimal>hex>human(decimal) order, one snapshot per line, scripts calling ampart can split the snapshots on \n character to get each snapshot, and split each snapshot on ' '(space, \x20)  character to get each partition, then split each partition on : character to get name, offset, size and masks  
ampart**只**会在**dsnapshot**和**esnapshot**模式下于标准输出上生成快照，其他情况下不会向标准输出写入任何东西。三个不同的快照会保证以十进制->十六进制->人类可读的顺序发送到标准输出，每个快照各占一行，调用ampart的脚本可以根据`\n`（换行符）来拆分得到每个快照，再根据` `/`\x20`（空格）来拆分得到每个分区，然后再根据`:`拆分分区来得到名称，偏移，大小和掩码

The following script calls ampart to get snapshot and prints the partition info  
下面这个脚本调用ampart来获得快照并打印分区信息
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
上面脚本的输出：
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
如你所间，前两个输出对脚本来说更容易处理，最后一个最适合汇报给用户
### stderr / 标准错误
Since standard output is used for snapshots only, all logs are printed to standard error  
因为标准输出被包留给快照使用，所有的日志都打印在标准错误上

[parg]:partition-argument-mini-language.md
[modes]:available-modes.md