# Migration / 迁移策略

## Modes / 模式
Planning of partitions migration will occur in the following mode, at the end of the run (before writing EPT), if the requirements are met, regardless of whether or not a migration is requested by the user (thus, even if the user does not want migration done by ampart, they can still migrate the partitions by themselves with the instrucstion given by an ampart dry-run session)  
以下模式中会在运行的结尾（写入EPT前）计划分区迁移，只要对应要求都满足，不管用户是否请求迁移（因此，即使用户不想ampart来完成迁移，它们也可以根据ampart在dry-run模式下给出的指示来自己完成迁移）

 - dtoe *(DTB to EPT / DTB转化为EPT)*
   - If the target is not DTB  
   目标非DTB
   - If a valid EPT exists in the target before the run
   运行前，目标中存在有效EPT
   - If the new EPT is different from the old one  
   新旧EPT不同
 - dedit *(DTB edit / DTB编辑)*
   - If the target is not DTB  
   目标非DTB
   - If a valid EPT exists in the target before the run  
   运行前，目标中存在有效EPT
   - A new EPT can be constructed from the editted DTB  
   编辑过的DTB可以构建新的EPT
   - If the new EPT is different from the old one  
   新旧EPT不同
 - eedit *(EPT edit / EPT编辑)*
   - If the new EPT is different from the old one  
   新旧EPT不同
 - dclone *(DTB clone / DTB编辑)*
   - If the target is not DTB  
   目标非DTB
   - If a valid EPT exists in the target before the run  
   运行前，目标中存在有效EPT
   - A new EPT can be constructed from the cloned-in DTB  
   克隆来的DTB可以构建新的EPT
   - If the new EPT is different from the old one  
   新旧EPT不同
 - eclone *(EPT clone / EPT克隆)*
   - If a valid EPT exists in the target before the run  
   运行前，目标中存在有效EPT
   - If the cloned-in EPT is different from the old one  
   克隆来的EPT与旧的不同
 - ecreate *(EPT create / EPT创建)*
   - If a valid EPT exists in the target before the run  
   运行前，目标中存在有效EPT
   - If the created EPT is different from the old one  
   创建的EPT与旧的不同

## Technical Detail / 技术细节
Migration includes planning and actual migration  
迁移包括计划和实际迁移

### Prerequisite / 前提
Two EPT (new and old) need to be both valid and contain at least 1 partition. Validation of partitions themselves are not managed here, as the corresponding modes should take care of it before calling migration.  
新旧两个EPT必须都有效，且包含至少一个分区。对于分区本身的验证工作不在迁移模块中实现，由各模式拉起迁移模块之前的流程负责

### Block / 块
The least common multiple <=4M for all partitions in both EPTs' offsets and sizes will be chosen as the migration block. Usually this will be 2M or 4M. If this can't be gotten, a migration plan will not be constructed   
两个EPT中所有分区的偏移和大小的不大于4M的最大公约数会被选择作为迁移块。一般是2M或者4M。如果不能获得迁移块，不会构建迁移计划  
Each block will has a boolean migration flag set to True if it needs to be migrated, and records its target block ID. The migration flag defaults to false, and the target block ID defaults to 0 before planning. (Target block ID = 0 does not neccessarily mean the block does not have migration plan, it could actually be block 0)  
每个块都有一个布尔值的迁移标志，如果需要迁移的话设置为真，并且记录目标块ID。迁移标志默认为假，并且计划前目标块ID默认为0（模块块ID=0并不意味着块没有迁移计划，可能实际上目标块就是0）

## Routine / 流程
For each block that needs to be migrated (marked as pending), the following steps are executed:  
对于每个需要被迁移的块（标记为待办），执行以下步骤
 - Check if the migration is circle (If its target's target's ....'s target, is itself)  
 检查是否为环形迁移（如果目标的目标的...目标是它本身呢）
   - If circle  
   是环形
     - Read into buffer  
     读入缓存
     - If the target block is in pending status, and its buffer is not read yet, call its routine  
     如果目标块为待办状态，且其缓存未读取，拉起其迁移流程
       - If failed, halt the migration  
       如果失败，放弃整个迁移计划
     - Write to target block  
     写入目标块
     - Unmark as pending  
     取消待办状态标记
     - Free buffer and set it to NULL  
     释放缓存，设置为空指针
   - Plain  
   普通非环形
     - If the target block is in pending status, call its routine  
     如果目标块为待办状态，拉起其迁移流程
     - Read into buffer  
     读入缓存
     - Write to target block  
     写入目标块
     - Unmark as pending  
     取消待办状态标记
     - Free buffer and set it to NULL  
     释放缓存，设置为空指针
   
Two important status marks (pending, buffer) are checked here. The pending mark is set to true for all blocks that need to be migrated during planning. The buffer is checked during circle migration so that a circle migration chain won't get into dead-lock  
这里检查了两个重要重要的状态标记（待办，缓存）。对于在计划期间所有需要迁移的分区，待办标记均设置为真。缓存在环形迁移中检查，所以环形迁移不会进入死锁
