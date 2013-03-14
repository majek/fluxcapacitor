#!/bin/bash
echo "Unsorted: $*"
function f() {
    sleep "$1"
    echo -n "$1 "
}
while [ -n "$1" ]
do
    f "$1" &
    shift
done
echo -n "Sorted:   "
wait
echo
