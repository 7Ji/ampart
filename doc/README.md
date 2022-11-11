# Documentation for ampart (Amlogic PARtition Tool)

The current available documentation:

[Command line interface](command-line-interface.md)
 - Basic command-line interface documentation, how you should call ampart on command-line, how ampart handles its stdin, stdout and stderr, ampart's options, etc

[Available modes](available-modes.md)
 - All available modes in ampart that can be selected with command-line option ``--mode [mode]`` and their usage and requirements. 

[PARG](partition-argument-mini-language.md)
 - Official documentation for the domain-specific Partition ARGument mini-language specially designed for ampart. 

[terminology](terminology.md)
 - Some terminology used in the documentation here, e.g. EPT (eMMC Partition Table), DTB, DTS, etc

[migration](migration.md)
 - How partition migration is planned and executed. Command-line options ``--migrate none/essential/all`` can affect the migration strategy.

[emulate eMMC](emulate-emmc-with-only-dtb.md)
 - A special use case for reverse-engineering the partition layout on devices where the eMMC dump could not be gotten

scripting-...
 - Documentation for official demo scripts under [scripts](../scripts)
