# Scripting: Lazy adjustment of partitions


Documentation for demo script [lazey-adjustment.sh](../scripts/lazey-adjustment.sh)

This script gets the snapshot and report partitions to end-users, and it'll compare the existing snapshot to the expected snapshot, and only adjust the partitions if they are different

*First run to adjust the layout*
```
[nomad7ji@laptop7ji images]$ sh ../scripts/dclone_once.sh x3_emmc.img 
Please use the following snapshot to restore the DTB partitions if anything goes wrong:
logo::8M:1 recovery::24M:1 misc::8M:1 dtbo::8M:1 cri_data::8M:2 param::16M:2 boot::16M:1 rsv::16M:1 metadata::16M:1 vbmeta::2M:1 tee::32M:1 vendor::320M:1 odm::128M:1 system::1856M:1 product::128M:1 cache::1120M:2 data::-1:4 
Existing partitions in DTB:
Partition 0: logo, size 8388608, masks 1
Partition 1: recovery, size 25165824, masks 1
Partition 2: misc, size 8388608, masks 1
Partition 3: dtbo, size 8388608, masks 1
Partition 4: cri_data, size 8388608, masks 2
Partition 5: param, size 16777216, masks 2
Partition 6: boot, size 16777216, masks 1
Partition 7: rsv, size 16777216, masks 1
Partition 8: metadata, size 16777216, masks 1
Partition 9: vbmeta, size 2097152, masks 1
Partition 10: tee, size 33554432, masks 1
Partition 11: vendor, size 335544320, masks 1
Partition 12: odm, size 134217728, masks 1
Partition 13: system, size 1946157056, masks 1
Partition 14: product, size 134217728, masks 1
Partition 15: cache, size 1174405120, masks 2
Partition 16: data, size -1, masks 4
Successfully adjusted DTB partitions
```

*Second run to ignore it*
```
[nomad7ji@laptop7ji images]$ sh ../scripts/dclone_once.sh x3_emmc.img 
Please use the following snapshot to restore the DTB partitions if anything goes wrong:
data::-1:4 
DTB partitions layout has already been modified, no need to update it
```