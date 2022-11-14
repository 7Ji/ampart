# Partition Argument Mini-Language / 分区参数迷你语言

Different from other common partition tools, **ampart** is completely non-interactive, this means you should feed all of what you want to do on the command-line arguments to it. (Check for why in [philosophy of ampart])  
和其他常见的分区工具不同，**ampart**是完全非互动式的，这意味着你应该把所有你想干的事都通过命令行参数提交给它（参考[ampart的哲学][philosophy of ampart]来了解原因）

For the special use case, I've defined a special mini-language so that each partition action (defining/editing) takes exact one argument, and a complex combination of actions can happen in one single command-line. This language, simply, is calle **(ampart) Partition Argument Mini-Language**, a shortened term **parg** in the following content will be used to refer to it.  
因为这一特别的的使用需求，我特别定义了一门迷你语言，以让每一个分区操作（定义，编辑）都能用一个参数来描述，并且复杂的操作可以在一次命令行操作中实现。这么语言很直白地叫做 **(ampart) 分区参数迷你语言**，下文中将使用**parg**作为简称来指代它

## Basic Syntax / 基本语法

A parg is a null-terminated string that can be represented by a single Linux command-line argument, that's in one of the following formats, with noticable **:** to seperate multiple parts of it:  
一个parg是能用单一的Linux命令行参数表示的由空字节作为结尾的字符串，由以下这些形式，由明显的 **:** 来分割其中多个部分
 - {name}:{offset}:{size}:{masks}  
 {名称}:{偏移}:{大小}:{掩码}
   - *This is called a **definer** parg, the same format can also be used for **adjustor**, although they have different requirement for their sub-arguments  
   叫做**定义器**parg，同一格式也可以用在**调整器**上，不过它们各自的自参数由不同要求*
 - ^{selector}{operator}({operator prefix})  
 ^{选择器}{操作符}({操作符后缀})
   - *This is called a **modifier** parg*  
   叫做**修改器**parg

Note: in the above context, the sign {} indicates a sub-argument that should be filled-in by user, without the {} in the result parg; the sign : and ^ should be typed as-is; the sign () indicates an optional sub-argument, that can be omitted if not needed  
注意：上述内容中，由符号{}框起来的是需要用户填写的子参数，结果parg中不应包含{}符号本身；符号:和^应该原模原样输入；符号()框起来的是可选的子参数，如果不需要的话可以省去

## Definer / 定义器
### Syntax / 语法
 - {name}:{offset}:{size}:{masks}  
 {名称}:{偏移}:{大小}:{掩码}


### Example / 例子
 - bootloader:0:4M:0
   - *A bootloader partition, placed at 0 offset, 4MiB in size, masks 0  
   一个bootloader分区，放置在0偏移处，大小4MiB，掩码0*
 - data:::
   - *A data partition, placed as the last partition, right after or 8MiB offset relative to the last partition depending on mode, taking all of the remaning space, auto masks  
   一个data分区，作为最后一个分区。根据具体模式，紧跟着上一个分区，或是隔着8MiB，占用所有空余空间，自动掩码*
 - system:+8M:2G:2
   - *A system partition, placed as the last partition, leave an 8MiB gap before it, size 2GiB, masks 2  
   一个system分区，作为最后一个分区。前面和上一个分区之间留8MiB空隙，大小2GiB，掩码2*

## Modifier / 修改器
### Syntax / 语法
 - ^{selector}{operator}({operator suffix})  
 ^{选择器}{操作符}({操作符后缀})

### Selector / 选择器
 - {name}  *Name Selector*  
 {名称} *名称选择器*
   - The exact name of a partition that exists  
   现存的具体分区名
 - {relative} *Relative Selector*  
 {相对值} *相对选择器*
   - A non-negative number to select a partition from start to end (0 for the first)  
   非负值从开头到结尾选择（0选择第一个）
   - A negative number to select a partition from end to start (-1 for the last)  
   负值从结尾到开头选择（-1选择最后一个）
#### Example / 例子
 - ``userdata``
   - Select the partition that's named userdata  
   选择一个名为userdata的分区
   - Example parg / 例子parg: ``^userdata?``
 - ``4``
   - Select the 5th partition  
   选择第五个分区
   - Example parg / 例子parg: ``^4:my::+1G:``
 - ``-2``
   - Select the second last partition  
   选择倒数第二个分区
   - Example parg / 例子parg: ``^-2%Cloned``
### Operator / 操作符
 - ? *Delete Operator / 删除操作符*
   - Deletes a partition, shift all partitions after it one to forward  
   删除分区，其后分区向前各移动一位
   - Suffix: *None*  
   无后缀
 - % *Clone Operator / 克隆操作符*
   - Clone a partition, create the new cloned partition as the last partition  
   克隆分区，把新的克隆出来的分区作为最后一个分区
   - Suffix: {name} *Cloner*  
   后缀：{名称} *克隆器*
     - The cloner should be a unique name that no existing partitions share  
     克隆器应该包含一个目前未使用的名字
   - Example / 例子:
     - ``%CE_STORAGE``
       - Clone the partition to CE_STORAGE  
       克隆分区为CE_STORAGE
       - Example parg / 例子parg: ``^-1%CE_STORAGE``
 - @ *Place Operator / 移位操作符*
   - Place a partition, move it around  
   把一个分区移位
   - Suffix: *{placer}*  
   后缀：*{移位器}*
     - {relatve}{non-negative number} *Relative placer*  
     {相对}{非负数} *相对移位器*
       - +{non-negative number}  
       +{非负数}
         - *for move to right  
         向右移动*
       - -{non-negative number}  
       -{非负数}
         - *for move to left  
         向左移动*
     - (=){number} *Absolute placer*  
     (=){数字} *绝对移位器*
       - ={negative number} *for place right to left, -1 for the last*  
       ={负数} *自右向左放置，-1为最后一位*
       - (=){non-negative number} *for place left to right, 0 for the first*  
       (=){非负数} *自左向右放置，0作为第一位*
    - Example / 例子:
      - @-2
        - Move the partition 2 to left, move the 2 partitions it crossed 1 to right  
        把分区向左移动两个位置，穿过的两个分区各向右移动一个位置
        - Example parg / 例子parg: ``^-1@-2``
      - @+3
        - Move the partition 3 to right, move the 3 partitions it crossed 1 to left  
        把分区向右移动三个位置，穿过的三个分区各向左移动一个位置
        - Example parg / 例子parg: ``^roms@+3``
      - @7 or @=+7  
      @7或@=+7
        - Place the partition as the 8th partition, if it moves to left, move the partitions it crossed 1 to right; if it moved to right, move the partitions it crosses 1 to left; if it does not move, nothing changes  
        把分区移动为第八个分区。如果向左移动，穿过的分区各向右移动一位；如果向右移动，穿过的分区各向左移动一位；如果没有移动，无事发生
        - Example parg / 例子parg: ``^system@7``, ``^system@=+7``
      - @=-4
        - Place the partition as the 4th last partition (should be 3 other partitions after it). Same logic as the last one.  
        把分区移动为倒数第四个分区（前面应该有3个其他分区），和上一个逻辑一样
        - Example parg / 例子parg: ``^logo@=-7``
 - : *Adjust Operator*  
 : *调整操作符*
   - Suffix: {adjustor}  
   后缀：{调整器}
     - {name}:{offset}:{size}:{masks}  
     {名称}:{偏移}:{大小}:{掩码}
       - The same parts as **definer**  
       和**定义器**的子部分相同
   - Example / 例子:
     - :bootleg::-1G:
       - Rename the partition to bootleg, shrink its size by 1G  
       把分区改名为bootleg，大小缩小1G
       - Example parg / 例子parg: ``^boot:bootleg::-1G:``
     - ::+2G:+1G:4
       - Increase the partition's offset by 2G, increase its size by 1G, change its mask to 4   
       把分区的偏移增加2G，大小增加1G，掩码改为4
       - Example parg / 例子parg: ``^factory::+2G:+1G:4``  

[philosophy of ampart]: empty