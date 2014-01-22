#!/bin/sh

: "${top_srcdir:=$(dirname "$0")/../../..}"
. "${top_srcdir}/tests/testenv.sh"
srcdir="${top_srcdir}/tests/glade/markup"

translate_set=0
podir_set=0
for arg in "$@" ; do
    if [ "$arg" = "--translate" -o "$arg" = "-t" ]; then
        translate_set=1
    elif [ "$arg" = "--podir" -o "$arg" = "-p" ]; then
        podir_set=1
    fi
done

if [ "$translate_set" -eq 1 -a "$podir_set" -eq 0 ]; then
    set -- "$@" --podir "${top_srcdir}/po"
fi

find "${top_srcdir}/pyanaconda" -name '*.glade' -exec "${srcdir}/check_markup.py" "$@" {} +
