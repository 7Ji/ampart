# Partition Argument Mini-Language

Different from other common partition tools, **ampart** is completely non-interactive, this means you should feed all of what you want to do on the command-line arguments to it. (Check for why in [philosophy of ampart][philosophy of ampart])

For the special use case, I've defined a special mini-language so that each partition action (defining/editing) takes exact one argument, and a complex combination of actions can happen in one single command-line. This language, simply, is calle **(ampart) Partition Argument Mini-Language**, a shortened term **parg** in the following content will be used to refer to it.

## Basic Syntax

A parg is a null-terminated string that can be represented by a single Linux command-line argument, that's in one of the following formats, with noticable **:** to seperate multiple parts of it:
 - {name}:{offset}:{size}:{masks}
   - *This is called a **definer** parg, the same format can also be used for **adjustor**, although they have different requirement for their sub-arguments*
 - ^{selector}{operator}({operator prefix})
   - *This is called a **modifier** parg*

Note: in the above context, the sign {} indicates a sub-argument that should be filled-in by user, without the {} in the result parg; the sign : and ^ should be typed as-is; the sign () indicates an optional sub-argument, that can be omitted if neccessary

## Definer
### Syntax
 - {name}:{offset}:{size}:{masks}


### Example
 - bootloader:0:4M:0
   - *A bootloader partition, placed at 0 offset, 4MiB in size, masks 0*
 - data:::
   - *A data partition, placed as the last partition, right after or 8MiB offset relative to the last partition depending on mode, taking all of the remaning space, auto masks*
 - system:+8M:2G:2
   - *A system partition, placed as the last partition, leave an 8MiB gap before it, size 2GiB, masks 2*

## Modifier
### Syntax
 - ^{selector}{operator}({operator suffix})

### Selector
 - {name}  *Name Selector*
   - The exact name of a partition that exists
 - {relative} *Relative Selector*
   - A non-negative number to select a partition from start to end (0 for the first)
   - A negative number to select a partition from end to start (-1 for the last)
#### Example
 - userdata
   - Select the partition that's named userdata
 - 4
   - Select the 5th partition
 - -2
   - Select the second last partition
### Operator
 - ? *Delete Operator*
   - Deletes a partition, shift all partitions after it one to left.
   - Suffix: *None*
 - % *Clone Operator*
   - Clone a partition, create the new cloned partition as the last partition
   - Suffix: {name} *Cloner*
     - The cloner should be a unique name that no existing partitions share
   - Example:
     - %CE_STORAGE
       - Clone the partition to CE_STORAGE
 - @ *Place Operator*
   - Place a partition, move it around
   - Suffix: {placer}
     - {relatve}{non-negative number} *Relative placer*
       - +{non-negative number}
         - *for move to right*
       - -{non-negative number}
         - *for move to left*
     - (=){number} *Absolute placer*
       - ={negative number} *for place right to left, -1 for the last*
       - (=){non-negative number} *for place left to right, 0 for the first*
    - Example:
      - @-2
        - Move the partition 2 to left, move the 2 partitions it crossed 1 to right
      - @+3
        - Move the partition 3 to right, move the 3 partitions it crossed 1 to left
      - @7 or @=+7
        - Place the partition as the 8th partition, if it moves to left, move the partitions it crossed 1 to right; if it moved to right, move the partitions it crosses 1 to left; if it does not move, nothing changes
      - @=-4
        - Place the partition as the 4th last partition (should be 3 other partitions after it). Same logic as the last one.
 - : *Adjust Operator*
   - Suffix: {adjustor}
     - {name}:{offset}:{size}:{masks}
       - The same parts as **definer**

   - Example:
     - :bootleg::-1G:
       - Rename the partition to bootleg, shrink its size by 1G
     - ::+2G:+1G:4
       - Increase the partition's offset by 2G, increase its size by 1G, change its mask to 4 

[philosophy of ampart]: empty