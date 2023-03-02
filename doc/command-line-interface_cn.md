# 命令行界面
## 基本语法
````
ampart ([nop 1] ([nop 1 arg]) [nop 2] ([nop 2 arg])...) [target] [parg 1] [parg 2]...
````
 - **nop**
    - 意为非位置参数，非位置参数可以被写在任何位置，如果非位置参数有其对应的形参，此形参必须紧跟着这个非位置参数
 - **target** 
    - 意为需要操作的目标文件/块设备，可以储存DTB（普通DTB，Amlogic多DTB，或者gzip压缩过的两者），或者是保留分区，或者是全盘。默认情况下ampart会自动识别目标内容类型，但你也可以硬定义（看下面）

 - **parg**
    - 意为分区参数，每一个模式都有各自的对分区参数的要求。参考[位置参数迷你语言][parg]和[可用模式][modes]来了解更多细节


比如，下面这两条命令是可以互换的
````
ampart --dry-run --mode dclone /dev/mmcblk0 data::-1:4
         ^nop1    ^nop2 ^nop2-arg ^target      ^parg1
````
    
````
ampart /dev/mmcblk0 data::-1:4 --mode dclone --dry-run 
         ^target      ^parg1    ^nop1 ^nop1-arg  ^nop2
````
但下面这两条不可以
```
ampart --mode dclone /dev/mmcblk0 system::2G:2 data::-1:4
        ^nop1 ^nop1-arg ^target    ^parg1       ^parg2
```
```
ampart --mode dclone /dev/mmcblk0 data::-1:4 system::2G:2
        ^nop1 ^nop1-arg ^target    ^parg1     ^parg2
```

## 非位置参数
所有以下的形参都以 --长参数/-短参数（[对应形参]）的样式写出，你可以使用任意一者
 - --version/-v
   - 打印版本，直接退出
 - --help/-h
   - 打印帮助信息，直接退出
 - --mode/-m [模式名]
   - 设置ampart来运行在以下一个模式之中
     - dtoe (DTB转换到EPT)
     - etod (EPT转换到DTB)
     - epedantic (EPT迂腐吗)
     - dedit (DTB编辑)
     - eedit (EPT编辑)
     - dsnapshot (DTB快照)
     - esnapshot (EPT快照)
     - webreport (网页汇报)
     - dclone (DTB克隆)
     - eclone (EPT克隆)
     - ecreate (EPT创建)
   - 默认：无，如果不设置任何模式，ampart不会处理目标
 - --content/-c [内容类型]
   - 设置目标的内容类型
     - **auto** 自动识别目标内容
     - **dtb** 强制ampart将目标视作DTB
     - **reserved** 强制ampart将目标视作保留分区
     - **disk** 强制ampart将目标视作全盘
   - 默认：自动
 - --migrate/-M [迁移策略]
   - 设置分区的迁移策略
     - **none** 不迁移任何分区
     - **essential** 只迁移必要分区（保留，环境，杂项，图标等）
     - **all** 迁移所有分区
   - 默认：仅必要
 - --strict-device/-s
   - 如果目标是块设备，且--content已设置目标类型，保持这一设置的目标类型和目标本身，不要尝试寻找对应的全盘（如果不设置，在目标是比如说`/dev/reserved`保留分区的情况下，ampart会搜寻其对应的全盘`/dev/mmcblk0`并转而在其上面操作）
 - --dry-run/-d
   - 不要作任何写入
 - --offset-reserved/-R [保留分区在盘内的偏移]
 - --offset-dtb/-D [DTB在保留分区内的迁移]
 - --gap-partition/-p [分区间的间隔]
 - --gap-reserved/-r [保留分区前的间隔]

## 标准输入输出
### 标准输入
ampart**不会**自标准输入读取任何的用户输入或者其他东西（仅目前而言）。在未来这可能被用来读取传入的命令，但**绝不会**用来读取用户输入，因为ampart**仅**应被脚本调用（或者可能被一些高级用户使用）
### 标准输出
ampart**只**会在**dsnapshot**和**esnapshot**模式下于标准输出上生成快照，或者在**webreport**模式下生成url，其他情况下不会向标准输出写入任何东西。三个不同的快照会保证以十进制->十六进制->人类可读的顺序发送到标准输出，每个快照各占一行，调用ampart的脚本可以根据`\n`（换行符）来拆分得到每个快照，再根据` `/`\x20`（空格）来拆分得到每个分区，然后再根据`:`拆分分区来得到名称，偏移，大小和掩码

### 标准错误
因为标准输出被保留给快照使用，所有的日志都打印在标准错误上

[parg]:partition-argument-mini-language.md
[modes]:available-modes.md