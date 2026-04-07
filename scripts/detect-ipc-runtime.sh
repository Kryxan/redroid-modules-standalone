#!/usr/bin/env bash
set -euo pipefail

FORMAT="json"
STRICT=0
MOUNT_DIR="${TMPDIR:-/tmp}/binderfs-test"

while [ $# -gt 0 ]; do
  case "$1" in
    --format)
      FORMAT="${2:-}"
      shift 2
      ;;
    --strict)
      STRICT=1
      shift
      ;;
    --mount-dir)
      MOUNT_DIR="${2:-}"
      shift 2
      ;;
    --help|-h)
      cat <<'EOF'
Usage: scripts/detect-ipc-runtime.sh [--strict] [--format json|keyvalue] [--mount-dir PATH]

Checks binderfs, binder devices/ioctl, and ashmem device/mmap availability.
Returns structured output for CI consumption.
EOF
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

json_escape() {
  printf '%s' "$1" | python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))'
}

as_bool() {
  if [ "$1" = "1" ]; then
    printf 'true'
  else
    printf 'false'
  fi
}

binderfs_supported=0
binderfs_mount_ok=0
binderfs_mount_skipped=0
binderfs_mount_error=""
binderfs_auto_mount=0
binderfs_auto_mount_paths=""
binder_devices_present=""
binder_device_available=0
binder_ioctl_ok=0
binder_ioctl_protocol_version=""
binder_ioctl_error=""
ashmem_device_present=0
ashmem_mmap_ok=0
ashmem_mmap_error=""

if grep -qw binder /proc/filesystems 2>/dev/null; then
  binderfs_supported=1
fi

mapfile -t binder_mount_points < <(awk '$3 == "binder" { print $2 }' /proc/mounts 2>/dev/null || true)
if [ ${#binder_mount_points[@]} -gt 0 ]; then
  binderfs_auto_mount=1
  binderfs_auto_mount_paths="$(IFS=','; echo "${binder_mount_points[*]}")"
fi

record_binder_device() {
  local dev="$1"
  [ -c "$dev" ] || return 0

  case ";$binder_devices_present;" in
    *";$dev;"*)
      ;;
    *)
      if [ -n "$binder_devices_present" ]; then
        binder_devices_present+=";"
      fi
      binder_devices_present+="$dev"
      binder_device_available=1
      ;;
  esac
}

for dev in /dev/binder /dev/hwbinder /dev/vndbinder; do
  record_binder_device "$dev"
done

for mount_point in "${binder_mount_points[@]}"; do
  for name in binder hwbinder vndbinder; do
    record_binder_device "$mount_point/$name"
  done
done

if [ "$binderfs_supported" = "1" ]; then
  mkdir -p "$MOUNT_DIR"
  if mountpoint -q "$MOUNT_DIR"; then
    binderfs_mount_ok=1
  else
    if mount -t binder binder "$MOUNT_DIR" 2>"$MOUNT_DIR.mount.err"; then
      binderfs_mount_ok=1
      umount "$MOUNT_DIR" || true
    else
      binderfs_mount_error="$(tr '\n' ' ' < "$MOUNT_DIR.mount.err" | sed 's/[[:space:]]\+/ /g' | sed 's/^ //;s/ $//')"
      if grep -qiE 'operation not permitted|permission denied' "$MOUNT_DIR.mount.err"; then
        binderfs_mount_skipped=1
      fi
    fi
    rm -f "$MOUNT_DIR.mount.err"
  fi
else
  binderfs_mount_error="binderfs not listed in /proc/filesystems"
fi

selected_binder_dev=""
for dev in /dev/binder /dev/hwbinder /dev/vndbinder; do
  if [ -c "$dev" ]; then
    selected_binder_dev="$dev"
    break
  fi
done

if [ -z "$selected_binder_dev" ]; then
  for mount_point in "${binder_mount_points[@]}"; do
    for dev in "$mount_point/binder" "$mount_point/hwbinder" "$mount_point/vndbinder"; do
      if [ -c "$dev" ]; then
        selected_binder_dev="$dev"
        break 2
      fi
    done
  done
fi

if [ -n "$selected_binder_dev" ]; then
  ioctl_result="$(python3 - "$selected_binder_dev" <<'PY'
import fcntl
import os
import struct
import sys

path = sys.argv[1]
BINDER_VERSION = 0xC0046209

try:
    fd = os.open(path, os.O_RDONLY | os.O_CLOEXEC)
except OSError as e:
    print(f"ERR:{e.errno}:{e.strerror}")
    raise SystemExit(0)

buf = bytearray(4)
try:
    fcntl.ioctl(fd, BINDER_VERSION, buf, True)
    version = struct.unpack('i', bytes(buf))[0]
    print(f"OK:{version}")
except OSError as e:
    print(f"ERR:{e.errno}:{e.strerror}")
finally:
    os.close(fd)
PY
)"
  if [[ "$ioctl_result" == OK:* ]]; then
    binder_ioctl_ok=1
    binder_ioctl_protocol_version="${ioctl_result#OK:}"
  else
    binder_ioctl_error="${ioctl_result#ERR:}"
  fi
fi

if [ -c /dev/ashmem ]; then
  ashmem_device_present=1
  ashmem_result="$(python3 - <<'PY'
import fcntl
import mmap
import os

ASHMEM_SET_SIZE = 0x40087703

try:
    fd = os.open('/dev/ashmem', os.O_RDWR | os.O_CLOEXEC)
except OSError as e:
    print(f"ERR:{e.errno}:{e.strerror}")
    raise SystemExit(0)

try:
    fcntl.ioctl(fd, ASHMEM_SET_SIZE, 4096)
    mm = mmap.mmap(fd, 4096, flags=mmap.MAP_SHARED, prot=mmap.PROT_READ | mmap.PROT_WRITE)
    mm[0:11] = b'ashmem-test'
    if mm[0:11] != b'ashmem-test':
      print('ERR:0:mmap write/read mismatch')
    else:
      print('OK')
    mm.close()
except OSError as e:
    print(f"ERR:{e.errno}:{e.strerror}")
finally:
    os.close(fd)
PY
)"
  if [ "$ashmem_result" = "OK" ]; then
    ashmem_mmap_ok=1
  else
    ashmem_mmap_error="${ashmem_result#ERR:}"
  fi
fi

overall_ok=1
fail_reasons=()

if [ "$binderfs_supported" != "1" ]; then
  overall_ok=0
  fail_reasons+=("binderfs unsupported")
fi

if [ "$binderfs_mount_ok" != "1" ] && [ "$binderfs_mount_skipped" != "1" ]; then
  overall_ok=0
  fail_reasons+=("binderfs mount test failed")
fi

if [ "$binder_device_available" != "1" ]; then
  overall_ok=0
  fail_reasons+=("no binder character device")
fi

if [ "$binder_ioctl_ok" != "1" ] && [ "$binder_device_available" = "1" ]; then
  overall_ok=0
  fail_reasons+=("binder ioctl failed")
fi

if [ "$ashmem_device_present" != "1" ]; then
  overall_ok=0
  fail_reasons+=("/dev/ashmem missing")
fi

if [ "$ashmem_mmap_ok" != "1" ] && [ "$ashmem_device_present" = "1" ]; then
  overall_ok=0
  fail_reasons+=("ashmem mmap test failed")
fi

if [ "$FORMAT" = "keyvalue" ]; then
  echo "binderfs_supported=$(as_bool "$binderfs_supported")"
  echo "binderfs_mount_ok=$(as_bool "$binderfs_mount_ok")"
  echo "binderfs_mount_skipped=$(as_bool "$binderfs_mount_skipped")"
  echo "binderfs_mount_error=$binderfs_mount_error"
  echo "binderfs_auto_mount=$(as_bool "$binderfs_auto_mount")"
  echo "binderfs_auto_mount_paths=$binderfs_auto_mount_paths"
  echo "binder_devices_present=$binder_devices_present"
  echo "binder_device_available=$(as_bool "$binder_device_available")"
  echo "binder_ioctl_ok=$(as_bool "$binder_ioctl_ok")"
  echo "binder_ioctl_protocol_version=$binder_ioctl_protocol_version"
  echo "binder_ioctl_error=$binder_ioctl_error"
  echo "ashmem_device_present=$(as_bool "$ashmem_device_present")"
  echo "ashmem_mmap_ok=$(as_bool "$ashmem_mmap_ok")"
  echo "ashmem_mmap_error=$ashmem_mmap_error"
  echo "overall_ok=$(as_bool "$overall_ok")"
  if [ ${#fail_reasons[@]} -gt 0 ]; then
    echo "fail_reasons=$(IFS=';'; echo "${fail_reasons[*]}")"
  else
    echo "fail_reasons="
  fi
else
  fail_json="[]"
  if [ ${#fail_reasons[@]} -gt 0 ]; then
    fail_json="$(printf '%s\n' "${fail_reasons[@]}" | python3 -c 'import json,sys; print(json.dumps([x.strip() for x in sys.stdin if x.strip()]))')"
  fi

  cat <<EOF
{
  "binderfs_supported": $(as_bool "$binderfs_supported"),
  "binderfs_mount_ok": $(as_bool "$binderfs_mount_ok"),
  "binderfs_mount_skipped": $(as_bool "$binderfs_mount_skipped"),
  "binderfs_mount_error": $(json_escape "$binderfs_mount_error"),
  "binderfs_auto_mount": $(as_bool "$binderfs_auto_mount"),
  "binderfs_auto_mount_paths": $(json_escape "$binderfs_auto_mount_paths"),
  "binder_devices_present": $(json_escape "$binder_devices_present"),
  "binder_device_available": $(as_bool "$binder_device_available"),
  "binder_ioctl_ok": $(as_bool "$binder_ioctl_ok"),
  "binder_ioctl_protocol_version": $(json_escape "$binder_ioctl_protocol_version"),
  "binder_ioctl_error": $(json_escape "$binder_ioctl_error"),
  "ashmem_device_present": $(as_bool "$ashmem_device_present"),
  "ashmem_mmap_ok": $(as_bool "$ashmem_mmap_ok"),
  "ashmem_mmap_error": $(json_escape "$ashmem_mmap_error"),
  "overall_ok": $(as_bool "$overall_ok"),
  "fail_reasons": $fail_json
}
EOF
fi

if [ "$STRICT" = "1" ] && [ "$overall_ok" != "1" ]; then
  exit 1
fi
