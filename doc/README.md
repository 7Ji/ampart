# Documentation for ampart (Amlogic eMMC Partition Tool) / ampart (Amlogic eMMC Partition Tool)的文档

The current available documentation:  
目前可用的文档：

[Command line interface / 命令行界面](command-line-interface.md)
 - Basic command-line interface documentation, how you should call ampart on command-line, how ampart handles its stdin, stdout and stderr, ampart's options, etc  
 基本命令行界面的文档，如何在命令行调用ampart，ampart怎么处理标准输入，标准输出和标准错误，ampart的选项，等等

[Available modes / 可用模式](available-modes.md)
 - All available modes in ampart that can be selected with command-line option ``--mode [mode]`` and their usage and requirements.   
 所有可以通过命令行选项``--mode [模式]``来选择的模式和它们的用法以及要求

[PARG](partition-argument-mini-language.md)
 - Official documentation for the domain-specific Partition ARGument mini-language specially designed for ampart.   
 特别设计的用于ampart的领域特定的分区参数迷你语言的官方文档

[terminology / 术语](terminology.md)
 - Some terminology used in the documentation here, e.g. EPT (eMMC Partition Table), DTB, DTS, etc  
 这里的文档中用到的术语，比如EPT，DTB，DTS等


[migration / 迁移](migration.md)
 - How partition migration is planned and executed.  
分区迁移是怎样计划并执行的

[emulate eMMC / 模拟eMMC](emulate-emmc-with-only-dtb.md)
 - A special use case for reverse-engineering the partition layout on devices where the eMMC dump could not be gotten  
 适用于逆向工程eMMC的分区布局而搞不到eMMC的转储时的特别应用情景

scripting-... / 脚本-
 - Documentation for official demo scripts under [scripts](../scripts)  
 [scripts](../scripts)下的官方演示脚本的文档
