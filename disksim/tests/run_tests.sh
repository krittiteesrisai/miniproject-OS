#!/usr/bin/env bash
# Checks results against the textbook example (head 53, disk 0..199)
cd "$(dirname "$0")/.."
pass=0; fail=0
check() {  # expected, args...
    local exp=$1; shift
    local got
    got=$(./disksim run -q "$@")
    if [ "$got" = "$exp" ]; then pass=$((pass+1)); printf "  ok    %-40s %s\n" "$*" "$got"
    else fail=$((fail+1)); printf "  FAIL  %-40s got %s expected %s\n" "$*" "$got" "$exp"; fi
}
echo "Textbook checks:"
check 640 -a fcfs
check 236 -a sstf
check 236 -a scan  -d down
check 331 -a scan  -d up
check 382 -a cscan -d up
check 183 -a cscan -d up -J
check 299 -a look  -d up
check 208 -a look  -d down
check 322 -a clook -d up
check 153 -a clook -d up -J
check 640 -a fcfs  -f traces/textbook.txt
echo "passed $pass, failed $fail"
[ "$fail" -eq 0 ]
