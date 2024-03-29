# Migration
[中文文档](migration_cn.md)

## Modes
Planning of partitions migration will occur in the following mode, at the end of the run (before writing EPT), if the requirements are met, regardless of whether or not a migration is requested by the user (thus, even if the user does not want migration done by ampart, they can still migrate the partitions by themselves with the instrucstion given by an ampart dry-run session)

 - dtoe *(DTB to EPT)*
   - If the target is not DTB
   - If a valid EPT exists in the target before the run
   - If the new EPT is different from the old one
 - dedit *(DTB edit)*
   - If the target is not DTB
   - If a valid EPT exists in the target before the run
   - A new EPT can be constructed from the editted DTB
   - If the new EPT is different from the old one
 - eedit *(EPT edit)*
   - If the new EPT is different from the old one
 - dclone *(DTB clone)*
   - If the target is not DTB
   - If a valid EPT exists in the target before the run
   - A new EPT can be constructed from the cloned-in DTB
   - If the new EPT is different from the old one
 - eclone *(EPT clone)*
   - If a valid EPT exists in the target before the run
   - If the cloned-in EPT is different from the old one
 - ecreate *(EPT create)*
   - If a valid EPT exists in the target before the run
   - If the created EPT is different from the old one

## Technical Detail
Migration includes planning and actual migration

### Prerequisite
Two EPT (new and old) need to be both valid and contain at least 1 partition. Validation of partitions themselves are not managed here, as the corresponding modes should take care of it before calling migration.

### Block
The least common multiple <=4M for all partitions in both EPTs' offsets and sizes will be chosen as the migration block. Usually this will be 2M or 4M. If this can't be gotten, a migration plan will not be constructed

Each block will has a boolean migration flag set to True if it needs to be migrated, and records its target block ID. The migration flag defaults to false, and the target block ID defaults to 0 before planning. (Target block ID = 0 does not neccessarily mean the block does not have migration plan, it could actually be block 0)

## Routine
For each block that needs to be migrated (marked as pending), the following steps are executed:以下步骤
 - Check if the migration is circle (If its target's target's ....'s target, is itself)
   - If circle
     - Read into buffer
     - If the target block is in pending status, and its buffer is not read yet, call its routine
       - If failed, halt the migration
     - Write to target block
     - Unmark as pending
     - Free buffer and set it to NULL
   - Plain
     - If the target block is in pending status, call its routine
     - Read into buffer
     - Write to target block
     - Unmark as pending
     - Free buffer and set it to NULL
   
Two important status marks (pending, buffer) are checked here. The pending mark is set to true for all blocks that need to be migrated during planning. The buffer is checked during circle migration so that a circle migration chain won't get into dead-lock
