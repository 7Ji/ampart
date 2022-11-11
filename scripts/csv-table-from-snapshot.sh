#!/bin/bash
# Official demo script for ampart by 7Ji. Same license as ampart itself (GPLv3)

# Usage:
#  - This script gets EPT snapshots from ampart's stdout, and convert them to a csv table for visualization

# Corresponding doc and result image: 
#  - ../doc/scripting-csv-table-from-snapshot.md

# Argument: 
#  - $1: the path to either whole eMMC or the reserved partition, or a dumped image of the former two
#  - $2: the output


log="$(ampart --mode esnapshot "$1" 2>/dev/null)"
readarray -d $'\n' -t snapshots <<< "${log}"
partitions_decimal=(${snapshots[0]})
partitions_hex=(${snapshots[1]})
partitions_human=(${snapshots[2]})
echo 'name, offset(decimal), offset(hex), offset(human), size(decimal), size(hex), size(human), masks' > "$2"
for i in $(seq 0 $((${#partitions_decimal[@]} - 1))); do
    readarray -d ':' -t partinfo_decimal < <(printf '%s' "${partitions_decimal[i]}")
    readarray -d ':' -t partinfo_hex < <(printf '%s' "${partitions_hex[i]}")
    readarray -d ':' -t partinfo_human < <(printf '%s' "${partitions_human[i]}")
    echo "${partinfo_decimal[0]}, ${partinfo_decimal[1]}, ${partinfo_hex[1]}, ${partinfo_human[1]}, ${partinfo_decimal[2]}, ${partinfo_hex[2]}, ${partinfo_human[2]}, ${partinfo_decimal[3]}" >> "$2"
done