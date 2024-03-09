#!/bin/bash

input_file="$1"
exit_file="$2"
collapse_count_file="$3"

cp "$input_file" "$exit_file"

sed -n 's/#pragma trahrhe collapse(\([0-9]\+\))/\1/p' "$input_file" > "$collapse_count_file"

sed -i 's/#pragma trahrhe collapse([0-9]\+)/#pragma scop/g' "$exit_file"

sed -i 's/#pragma endtrahrhe/#pragma endscop/g' "$exit_file"

