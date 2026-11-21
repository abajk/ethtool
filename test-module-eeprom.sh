#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only

export LC_ALL=C
tmpdir=$(mktemp -d) || exit 1
trap 'rm -rf "$tmpdir"' EXIT
trap 'exit 1' HUP INT TERM

decode_hex()
{
	awk '
		BEGIN { digits = "0123456789abcdef" }
		NR == 1 && NF == 2 && $1 == "Offset" && $2 == "Values" { next }
		NR == 2 && NF == 2 && $1 == "------" && $2 == "------" { next }
		NR <= 2 || NF < 2 || NF > 17 ||
		    tolower($1) != sprintf("0x%04x:", offset) { exit 1 }
		{
			for (i = 2; i <= NF; i++) {
				byte = tolower($i)
				if (byte !~ /^[0-9a-f][0-9a-f]$/)
					exit 1
				high = index(digits, substr(byte, 1, 1)) - 1
				low = index(digits, substr(byte, 2, 1)) - 1
				printf "%c", 16 * high + low
				offset++
			}
		}
		END { if (offset == 0) exit 1 }
	' "$1"
}

found=0
status=0
for dump in "${srcdir:-.}"/test/module-eeprom/*/*.raw; do
	[ -f "$dump" ] || continue
	found=1
	base=${dump%.raw}
	dir=${dump%/*}
	type=${dir##*/}
	echo "Testing ${type}/${base##*/}"

	if ! decode_hex "$dump" > "$tmpdir/input"; then
		echo "Invalid ethtool hex dump: $dump" >&2
		status=1
		continue
	fi

	./test-module-eeprom "$type" raw < "$tmpdir/input" > "$tmpdir/raw" &&
		cmp "$tmpdir/input" "$tmpdir/raw" || status=1
	./test-module-eeprom "$type" text < "$tmpdir/input" > "$tmpdir/text" &&
		diff -u "$base.txt" "$tmpdir/text" || status=1
	./test-module-eeprom "$type" json < "$tmpdir/input" > "$tmpdir/json" &&
		diff -u "$base.json" "$tmpdir/json" || status=1
done

if [ "$found" -eq 0 ]; then
	echo "No module EEPROM test vectors found" >&2
	exit 1
fi
exit "$status"
