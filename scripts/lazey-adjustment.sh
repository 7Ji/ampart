#!/bin/bash
# Official demo script for ampart by 7Ji. Same license as ampart itself (GPLv3)

# Usage:
#  - This script gets the snapshot and report partitions to end-users, and it'll compare the existing snapshot to the expected snapshot, and only adjust the partitions if they are different

# Corresponding doc and result log: 
#  - ../doc/lazey-adjustment.md

# Argument: 
#  - $1: the path to either whole eMMC or the reserved partition or DTB block device, or a dumped image of the former two

SNAPSHOT='data::-1:4'
LOG=$(ampart "$1" --mode dsnapshot 2>/dev/null)
if (( $? )); then
  echo 'Failed to get DTB snapshot!'
  exit 1
fi
readarray -d $'\n' -t DTB_SNAPSHOTS <<< "${LOG}"
# 0 is decimal, 1 is hex, 2 is human-readable
echo "Please use the following snapshot to restore the DTB partitions if anything goes wrong:"
# Report the human-readable snapshot to user, which is easier to record
echo "${DTB_SNAPSHOTS[2]}"
if [ "${DTB_SNAPSHOTS[0]}" == "${SNAPSHOT}" ]; then
  echo 'DTB partitions layout has already been modified, no need to update it'
  exit 0
fi
readarray -d ' ' -t PARTITIONS < <(printf '%s' "${DTB_SNAPSHOTS[0]}")
if ! (( ${#PARTITIONS[@]} )); then
  echo 'No partitions in DTB, this is impossible, did you use ampart to modify the DTB yourself?'
  exit 2
fi
echo "Existing partitions in DTB:"
for i in $(seq 0 $(("${#PARTITIONS[@]}"-1)) ); do
  readarray -d ':' -t PARTINFO < <(printf '%s' "${PARTITIONS[$i]}")
  echo "Partition $i: ${PARTINFO[0]}, size ${PARTINFO[2]}, masks ${PARTINFO[3]}"
done
ampart "$1" --mode dclone ${SNAPSHOT} 2>/dev/null
if (( $? )); then
  echo 'Failed to adjust DTB partitions!'
  exit 3
fi
echo 'Successfully adjusted DTB partitions'