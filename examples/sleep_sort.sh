#!/bin/bash
function f() {
    sleep "$1"
    echo -n "$1 "
}
while [ -n "$1" ]
do
    f "$1" &
    shift
done
wait
echo
