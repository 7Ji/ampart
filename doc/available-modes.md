# Available Modes / 可用模式
All modes are in listed in a _**mode selector** (full name)_ style  
所有模式都以 _**模式选择名** (全名)_ 的风格列出

## d2e (DTB to pedantic EPT / DTB转化为迂腐EPT)
Create pedantic EPT from the partitions node in DTB(s), behaves the same as Amlogic's u-boot  
根据DTB里面的分区节点构建迂腐EPT，和Amlogic的u-boot行为一致

### Partition arguments / 分区参数:
 - No (all partition arguments will be ignored)  
 无（所有的分区参数都会被忽略）

### Acceptable content / 可接受内容
 - DTB / 设备树 X
 - Reserved / 保留分区 √
 - Disk / 全盘 √

### Possible failure points / 可能的故障点:
 - If the partitions node do not exist in the DTB, no pedantic EPT will be constructed. This can be the case if you've used the legacy version of ampart, or ceemmc to modify the EPT, as they would remove partitions node from DTBs to prevent Amlogic u-boot's EPT restoration function from working.  
 如果DTB里不存在分区节点，迂腐EPT不会被创建。可能因为你用过旧版本的ampart，或者是ceemmc来修改过EPT而造成。这些方案会把DTB里面的分区节点移除，来避免Amlogic的u-boot来通过DTB还原EPT
 - If multiple DTBs exist, yet they don't guarantee the same EPT, no construted EPT will be used  
 如果存在多个DTB，但不能保证构建相同的EPT，任何构建的EPT都不会被使用

## e2d (pedantic EPT to DTB / 迂腐EPT转化为DTB)
Restore/Recreate partitions node in DTB(s) according to a pedantic EPT  
根据一个迂腐EPT来还原/重建DTB里的分区节点

### Partition arguments / 分区参数:
 - No (all partition arguments will be ignored)  
 无（所有的分区参数都会被忽略）

### Acceptable content / 可接受内容
- DTB / 设备树 X
- Reserved / 保留分区 √
- Disk / 全盘 √

### Possible failure points / 可能的失败点:
 - If the EPT is not pedantic, no matter whatever the reason is, no DTB will be constructed  
 如果EPT不迂腐，不论原因为何，不会构建任何DTB

## epedantic (is EPT pedantic / EPT迂腐吗)
Check if the the EPT is a pedantic EPT, and gives advice on how to pedantize it.  
检查EPT是否是一个迂腐EPT，并且给出建议怎么让它变得迂腐

### Partition arguments / 分区参数:
 - No (all partition arguments will be ignored)  
 无（所有的分区参数都会被忽略）

### Acceptable content / 可接受内容
 - DTB / 设备树 X
 - Reserved / 保留分区 √
 - Disk / 全盘 √

## dedit (DTB edit mode / 设备树编辑模式)
Edit partitions node in DTB, and update the DTB. The EPT will also be updated if the target is either Reserved or Disk and the corresponding pedantic EPT is different from the existing one.  
编辑DTB里的分区节点，并更新DTB。如果目标是保留分区或者全盘，且对应的迂腐EPT和现存的不同，也更新EPT。

### Partition arguments / 分区参数:
 - Definer: {name}::{size}:{masks}  
 定义器： {名称}::{大小}:{掩码}  
   - {name} must be set explicitly  
   {名称}必须明确设置
   - ~~{offset}~~ must **not** be set  
   ~~{偏移}~~ 必须**不能**被设置
   - {size} must be set explicitly as absolute value  
   {大小}必须明确以绝对值设置
   - {masks} must be set explicitly  
   {掩码}必须明确设置
 - Modifier: ^{selector}{operator}{suffix}  
 修改器: ^{选择器}{操作符}{后缀}
   - Selector {selector} must be set explicitly  
   选择器 {选择器}必需被明确设置
     - Name selector {name} should be an existing partition name  
     名称选择器 {名称} 必需是现存的分区名
     - Relative selector {relative} 
     相对选择器 {相对} 
       - Non-negative to select from start (0 as first)  
       非负相对选择器从开头选取（0选择第一个）
       - Negative to select from end (-1 as last)  
       负相对选择器从结尾选取（-1选择最后一个）
   - Operator {operator}  
   操作符 {操作符}
     - Delete operator ?  
     删除操作符 ?
     - Clone operator %  
     克隆操作符%
       - Suffix: Cloner {name}  
       后缀：克隆器 {名称}
         - {name} must be set explicitly  
         {名称} 必需被明确设定
     - Place operator @  
     移位操作符 @
       - Suffix: Placer {place}  
       后缀：移位器 {移位}
         - Absolute placer (=){non-negative} / ={negative}  
         绝对移位器 (=){非负数} / ={负数}
           - Forward absolute placer (=){non-negative}, 0 as first  
           正向绝对移位器 (=){非负数}，0移位到第一个
           - Backwards absolute placer ={negative}, -1 as last  
           逆向移位器 ={负数}，-1移位到最后一个
         - Relative placer +{non-negative} / -{non-negative}  
         相对移位器 +{非负数} / -{非负数}
           - Move-right relative placer +{non-negative}
           向右相对移位器 +{非负数}
           - Move-left relative placer -{non-negative}
           向左相对移位器 -{非负数}
     - Adjust operator :  
     调整操作符 :
       - Suffix: Adjustor {name}::{size}:{masks}
       后缀：调整器 {名称}::{大小}:{掩码}
         - {name} is optional  
         {名称} 是可选的
         - ~~{offset}~~ must **not** be set
         ~~{偏移}~~必须**不能**被设置
         - {size} is optional  
         {大小}是可选的
           - -{non-negative} substract  
           -{非负数}减小
           - +{non-negative} add  
           +{非负数}增加
           - {non-negative} set  
           {非负数}设置
         - {masks} is optional  
         {掩码}是可选的

### Acceptable content / 可接受内容
- DTB / 设备树 √
- Reserved / 保留分区 √
- Disk / 全盘 √

## eedit (EPT edit mode / EPT编辑模式)
Edit EPT. If the result EPT is pedantic, also update partitions node in DTB; otherwise, remove partitions node in DTB.  
编辑EPT。如果结果的EPT是迂腐的话，也更新DTB里面的分区节点；否则，移除DTB里的分区节点

### Partition arguments / 分区参数:
 - Definer: {name}:{offset}:{size}:{masks}  
 定义器： {名称}:{偏移}:{大小}:{掩码} 
   - {name} must be set explicitly  
   {名称}必须明确设置
   - {offset} is optional  
   {偏移}是可选的
     - +{non-negative} relative offset to the last partition's end  
     +{非负数}对于上一分区结尾而言的相对偏移
     - {non-negative} absolute offset to the whole disk  
     {非负数}对于全盘而言的绝对偏移
   - {size} is optional, and should only be set as absolute  
   {大小}是可选择，并且只应以绝对值的形式设置
     - If not set, autofill the remaining space  
     如果没有设置，自动填充剩余空间
   - {masks} is optional  
   {掩码}是可选的
     - If not set, defaults to 4  
     如果不设置，默认为4
 - Modifier: ^{selector}{operator}{suffix}  
 修改器: ^{选择器}{操作符}{后缀}
   - Selector {selector} must be set explicitly  
   选择器 {选择器}必需被明确设置
     - Name selector {name} should be an existing partition name  
     名称选择器 {名称} 必需是现存的分区名
     - Relative selector {relative}  
     相对选择器 {相对} 
       - Non-negative to select from start (0 as first)  
       非负相对选择器从开头选取（0选择第一个）
       - Negative to select from end (-1 as last)  
       负相对选择器从结尾选取（-1选择最后一个）
   - Operator {operator}  
   操作符 {操作符}
     - Delete operator ?  
     删除操作符 ?
     - Clone operator %  
     克隆操作符%
       - Suffix: Cloner {name}  
       后缀：克隆器 {名称}
         - {name} must be set explicitly  
         {名称} 必需被明确设定
     - Place operator @  
     移位操作符 @
       - Suffix: Placer {place}  
       后缀：移位器 {移位}
         - Absolute placer (=){non-negative} / ={negative}  
         绝对移位器 (=){非负数} / ={负数}
           - Forward absolute placer (=){non-negative}, 0 as first  
           正向绝对移位器 (=){非负数}，0移位到第一个
           - Backwards absolute placer ={negative}, -1 as last  
           逆向移位器 ={负数}，-1移位到最后一个
         - Relative placer +{non-negative} / -{non-negative}  
         相对移位器 +{非负数} / -{非负数}
           - Move-right relative placer +{non-negative}
           向右相对移位器 +{非负数}
           - Move-left relative placer -{non-negative}
           向左相对移位器 -{非负数}
     - Adjust operator :  
     调整操作符 :
       - Suffix: Adjustor {name}:{offset}:{size}:{masks}
       后缀：调整器 {名称}:{偏移}:{大小}:{掩码}
         - {name} is optional  
         {名称} 是可选的
         - {offset} is optional  
         {偏移} 是可选的
           - -{non-negative} substract  
           -{非负数}减小
           - +{non-negative} add  
           +{非负数}增加
           - {non-negative} set  
           {非负数}设置
         - {size} is optional
           - -{non-negative} substract  
           -{非负数}减小
           - +{non-negative} add  
           +{非负数}增加
           - {non-negative} set  
           {非负数}设置
         - {masks} is optional  
         {掩码}是可选的

### Acceptable content / 可接受内容
- DTB / 设备树 X
- Reserved / 保留分区 √
- Disk / 全盘 √

### examples / 例子:
 - ``ampart --mode eedit /dev/mmcblk0 ^-1? ^cache@=-1 ^-2:my_second_lastp::-1G: lastp:::``
   - ``^-1?``
     - Delete the last partition  
     删除最后一个分区
   - ``^cache@=-1``
     - Place cache as the last partition (Partition after the old cache are shifted left)  
     把cache位移作为最后一个分区（旧位置后的分区左移）
   - ``^-2:my_second_lastp::-1G:`` 
     - Rename the second last partition to `my_second_lastp`, shrinks its size by 1G  
     把倒数第二个分区改名位`my_second_lastp`，大小缩小1G
   - ``lastp:::``
     - Creat a partition lastp to take all the remaining space  
     创建一个lastp分区，占用所有剩余空间

## dsnapshot (DTB snapshot mode / DTB快照模式)
Take a snapshot of the partitions node in DTB and print it to standard output, 3 inter-changable snapshots seperated by `\n`(new-line character) in the order of decimal, hex, human-readable (decimal). The snapshots can later be used in `dclone` mode to get identical partitions   
获取DTB里分区节点的快照，并打印到标准输出，三个可以互换的快照以十进制，十六进制，人类可读（十进制）的顺序被`\n`分割（换行符）。快照之后可以在`dclone`模式里用来获得一致的分区

### Partition arguments / 分区参数:
 - No (all partition arguments will be ignored)  
 无（所有的分区参数都会被忽略）

### Acceptable content / 可接受内容
- DTB / 设备树 √
- Reserved / 保留分区 √
- Disk / 全盘 √

## esnapshot (EPT snapshot mode / EPT快照模式)
Take a snapshot of the EPT and output it to standard output, 3 inter-changable snapshots in the order of decimal, hex, human-readable (decimal). The snapshots can later be used in `eclone` mode to get identical partitions
获取EPT的快照，并打印到标准输出，三个可以互换的快照以十进制，十六进制，人类可读（十进制）的顺序被`\n`分割（换行符）。快照之后可以在`eclone`模式里用来获得一致的分区

### Partition arguments / 分区参数:
 - No (all partition arguments will be ignored)  
 无（所有的分区参数都会被忽略）

### Acceptable content / 可接受内容
- DTB / 设备树 X
- Reserved / 保留分区 √
- Disk / 全盘 √

## webreport (WEB report mode / 网页汇报模式)
Generate a URL on standard output that can be opened on browser to get a well-formatted webpage of partition info, powered by my another project [ampart-web-reporter]. The device running this mode does not need to be online.  
在标准输出上生成一个URL，可以在浏览器打开网页查看格式整齐的分区信息，由我的另一个项目[ampart-web-reporter]驱动。运行这一模式的设备本身不需要联网。

[ampart-web-reporter]: https://github.com/7Ji/ampart-web-reporter

### Partition arguments / 分区参数:
 - No (all partition arguments will be ignored)  
 无（所有的分区参数都会被忽略）

### Acceptable content / 可接受内容
- DTB / 设备树 X
- Reserved / 保留分区 √
- Disk / 全盘 √

## dclone (DTB clone mode / DTB克隆模式)
Restore a snapshot taken in `dsnapshot` mode, or also define the partitions if you like. If the target is Reserved or Disk, and the corresponding pedantic EPT is different from the existing EPT, the EPT will also be updated  
恢复一个`dsnapshot`模式里获得快照，或者如果你想的话也能定义分区。如果目标为保留分区或者全盘，并且对应的迂腐EPT和现存的EPT不同的话，EPT也会一并更新

### Partition arguments / 分区参数
 - Definer: {name}::{size}:{masks}  
 定义器： {名称}::{大小}:{掩码}  
   - {name} must be set explicitly  
   {名称}必须明确设置
   - ~~{offset}~~ must **not** be set  
   ~~{偏移}~~ 必须**不能**被设置
   - {size} must be set explicitly as absolute value  
   {大小}必须明确以绝对值设置
     - A special value `-1` means auto-fill  
     特别值`-1`意为自动填充
   - {masks} must be set explicitly  
   {掩码}必须明确设置

### Acceptable content / 可接受内容
- DTB / 设备树 √
- Reserved / 保留分区 √
- Disk / 全盘 √

### Note / 笔记
On HK1 Rbox X4, the u-boot reads some important partition info from DTB, this is the main reason I re-wrote ampart to implement this mode. The three non-data partitions listed below (``boot_a::64M:1 vbmeta_a::2M:1 vbmeta_system_a::2M:1``) are the essential partitions  
在HK1 Rbox X4上，u-boot会从DTB里读取一些重要的分区信息，这也是我重写ampart来实现这个模式的主要原因。下面列出的三个data外的分区（``boot_a::64M:1 vbmeta_a::2M:1 vbmeta_system_a::2M:1``）是必要的分区
```
./ampart --mode dclone /dev/mmcblk0 --migrate all boot_a::64M:1 vbmeta_a::2M:1 vbmeta_system_a::2M:1 data::-1:4
```

## eclone (EPT clone mode / EPT克隆模式)
Restore a snapshot taken in `esnapshot` mode, or also define the partitions if you like. If the result EPT is pedantic, also update partitions node in DTB if differnet; otherwise, remove the partitions node in DTB  
恢复`esnapshot`模式中获得的快照，或者如果你想的话也能定义分区。如果结果的EPT是迂腐的，也在对应的DTB里分区节点不同的情况下更新它；不然，移除DTB里的分区节点

### Partition arguments / 分区参数:
 - Definer: {name}:{offset}:{size}:{masks}  
 定义器： {名称}:{偏移}:{大小}:{掩码} 
   - {name} must be set explicitly  
   {名称}必须明确设置
   - {offset} must be set explicitly as absolute  
   {偏移}必须以绝对值的形式明确设置  
   - {size} must be set explicitly as absolute  
   {大小}必须以绝对值的形式明确设置
   - {masks} must be set explicitly  
   {掩码}必须明确设置


### Acceptable content / 可接受内容
- DTB / 设备树 X
- Reserved / 保留分区 √
- Disk / 全盘 √

## ecreate (EPT create mode / EPT创建模式)
Defining a table in a yolo way, you don't need to care about essential partitions, their info will be retrieved from old table and optimized. Partitions node in DTB **will be 100% removed**  
以一种梭哈的方式定义分区表，你不需要在乎重要分区，它们的信息会自动从旧分区表里获取并优化。DTB里的分区节点**一定会被移除**

### Partition arguments / 分区参数:
 - Definer: {name}:{offset}:{size}:{masks}  
 定义器： {名称}:{偏移}:{大小}:{掩码} 
   - {name} must be set explicitly  
   {名称}必须明确设置
   - {offset} is optional  
   {偏移}是可选的
     - +{non-negative} relative offset to last partition'end  
     +{非负数}相对于上一分区结尾的相对偏移
     - {non-negative} absolute offset to the whole drive  
     {非负数}相对于全盘的绝对偏移
   - {size} is optional, and it should be set as absolute  
   {大小}是可选的，且只应以绝对值的形式设定
     - If not set, auto-fill the remaning space  
     如果不设置，自动填充剩余空间
   - {masks} is optional  
   {掩码}是可选的
     - If not set, defaults to 4  
     如果不设置，默认为4

### Acceptable content / 可接受内容
- DTB / 设备树 X
- Reserved / 保留分区 √
- Disk / 全盘 √