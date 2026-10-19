#!/bin/sh
# record_env.sh - task 1.2: save the machine setup so every result can be traced back to it.
#   sh record_env.sh /path/to/testfile.bin [LIBURING_PREFIX]  > env_babbage.txt
F=${1:-}
LU=${2:-$HOME/liburing-inst}
echo "date        : $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo "host        : $(hostname)"
echo "kernel      : $(uname -r)"
echo "os          : $(grep PRETTY_NAME /etc/os-release 2>/dev/null | cut -d= -f2)"
echo "compiler    : $(${CXX:-g++} --version | head -1)"
if [ -f "$LU/lib/pkgconfig/liburing.pc" ]; then
  echo "liburing    : $(grep '^Version' "$LU/lib/pkgconfig/liburing.pc" | cut -d' ' -f2) ($LU)"
else
  echo "liburing    : $(pkg-config --modversion liburing 2>/dev/null || echo unknown) (system)"
fi
echo "cpu model   : $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | sed 's/^ *//')"
echo "cpus online : $(nproc --all)  allowed for this shell: $(taskset -pc $$ 2>/dev/null | cut -d: -f2 | sed 's/^ *//')"
echo "governor    : $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo n/a)"
echo "io_uring    : $(cat /proc/sys/kernel/io_uring_disabled 2>/dev/null | sed 's/^0$/enabled (0)/' || echo 'sysctl not present (enabled)')"
if [ -n "$F" ]; then
  echo "test file   : $F ($(stat -c %s "$F" 2>/dev/null) bytes)"
  echo "filesystem  : $(df -T "$F" | tail -1 | awk '{print $1, $2, $7}')"
  DEV=$(df "$F" | tail -1 | awk '{print $1}' | sed 's#/dev/##; s/p\?[0-9]*$//')
  echo "device      : $DEV  model: $(cat /sys/block/$DEV/device/model 2>/dev/null || echo n/a)  rotational: $(cat /sys/block/$DEV/queue/rotational 2>/dev/null || echo n/a)  scheduler: $(cat /sys/block/$DEV/queue/scheduler 2>/dev/null || echo n/a)"
fi
