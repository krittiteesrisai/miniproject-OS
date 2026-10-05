#!/usr/bin/env bash
# Setup for Ubuntu on a real machine: install tools, inspect disks, build, test.
set -euo pipefail
cd "$(dirname "$0")/.."

echo "== 1. Install build tools =="
sudo apt update
sudo apt install -y build-essential

echo
echo "== 2. System =="
echo "Kernel : $(uname -r)"
echo "Distro : $(lsb_release -ds 2>/dev/null || grep PRETTY_NAME /etc/os-release | cut -d= -f2)"
echo "GCC    : $(gcc --version | head -1)"

echo
echo "== 3. Disks (ROTA 1 = HDD, 0 = SSD) =="
lsblk -d -e 7 -o NAME,ROTA,SIZE,MODEL
echo
for f in /sys/block/*/queue/scheduler; do
    dev=$(echo "$f" | cut -d/ -f4)
    case "$dev" in loop*|ram*|zram*) continue ;; esac
    printf "%-10s rotational=%s  scheduler: %s\n" "$dev" \
        "$(cat /sys/block/$dev/queue/rotational)" "$(cat "$f")"
done

echo
echo "== 4. POSIX shared memory (/dev/shm) =="
df -h /dev/shm

echo
echo "== 5. Build and test =="
make clean
make
make test

echo
echo "Done. Try:  ./disksim compare    or    ./disksim run -a scan -A 300"
