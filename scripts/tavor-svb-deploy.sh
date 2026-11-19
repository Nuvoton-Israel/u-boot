#!/usr/bin/env bash
# Deploy U-Boot to the Tavor SVB boot flash (CS1, offset 0x200000) over FTDI channel B.
# Usage: scripts/tavor-svb-deploy.sh [-n] [-y] [-F] [-b backup.bin] [-c] [u-boot.bin]
#   -n  dry run: only back up/verify the flash read path, do not program
#   -y  do not ask for confirmation
#   -F  back up the whole CS1 flash (slow: 256 MiB takes minutes) instead of only the U-Boot window
#   -b  backup file (default: ./cs1-backup-<timestamp>.bin; holds the 1 MiB window at OFFSET unless -F)
#   -c  skip the console check
# Only the 4 KiB sectors that differ from the flash are erased/programmed.
# Env: FLASH_PROG (default /mnt/storage/work/flash-prog), CONSOLE (default /dev/ttyUSB2), BAUD (default auto: 115200 then 750000),
#      MENU = BootBlock menu option sent to start U-Boot (default 2 = XIP U-Boot at CS1+2MiB, skip DDR; 4 = train DDR first)
set -euo pipefail

FLASH_PROG=${FLASH_PROG:-/mnt/storage/work/flash-prog}
CONSOLE=${CONSOLE:-/dev/ttyUSB2}
BAUD=${BAUD:-auto}
MENU=${MENU:-2}
OFFSET=0x200000          # U-Boot location in CS1 (XIP at 0x30200000)
MAX_SIZE=$((0x100000))   # keep well inside the space after U-Boot
HOLD_MS=300000

SECTOR=4096

dry=0 yes=0 check=1 full=0 backup=""
while getopts "nyFb:c" o; do
	case $o in
	n) dry=1 ;; y) yes=1 ;; F) full=1 ;; b) backup=$OPTARG ;; c) check=0 ;;
	*) sed -n '2,11p' "$0"; exit 2 ;;
	esac
done
shift $((OPTIND - 1))

SRC_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMG=$(realpath "${1:-$SRC_ROOT/u-boot.bin}")
[[ -f $IMG ]] || { echo "image not found: $IMG" >&2; exit 1; }
size=$(stat -c %s "$IMG")
((size > 0 && size <= MAX_SIZE)) || { echo "bad image size $size (max $MAX_SIZE)" >&2; exit 1; }
backup=${backup:-$PWD/cs1-backup-$(date +%Y%m%d-%H%M%S).bin}

PROG="$FLASH_PROG/build/FTDI_SPI_PROG"
[[ -x $PROG ]] || { echo "missing $PROG (build flash-prog first)" >&2; exit 1; }
spi=(-open-desc "Tavor_SVB_Rev_A B" -pin-set 6 1 1 -pin-set 7 1 0) # CS1, SPI3 buffer enabled

# Console owner check: ports are exclusive
if ((check)) && fuser "$CONSOLE" >/dev/null 2>&1; then
	echo "$CONSOLE is in use (close minicom/serial sessions) or pass -c" >&2
	exit 1
fi

echo "Image : $IMG ($size bytes)"
strings -a "$IMG" | grep -m1 '^U-Boot 20' || echo "warning: no U-Boot version string in image"
echo "Target: CS1 @ $OFFSET   Backup: $backup"
if ((!yes && !dry)); then
	read -r -p "Program flash? [y/N] " a
	[[ $a == y || $a == Y ]] || { echo aborted; exit 1; }
fi

holder=""
release() {
	if [[ -n $holder ]]; then
		kill "$holder" 2>/dev/null || true
		wait "$holder" 2>/dev/null || true
		holder=""
	fi
}
flog() { tr '\r@' '\n\n' | grep -vE "bytes left" || true; }
window=$(mktemp)
probe=$(mktemp)
trap 'release; rm -f "$window" "$probe"' EXIT

echo "== Hold BMC in reset"
"$FLASH_PROG/scripts/tavor-svb-porst.sh" "$HOLD_MS" >/dev/null &
holder=$!
sleep 2
kill -0 "$holder" 2>/dev/null || { echo "failed to hold PORST" >&2; holder=""; exit 1; }

if ((full)); then
	echo "== Backup whole CS1 -> $backup"
	"$PROG" "${spi[@]}" -flash-info -dump-file "$backup" 0 -1 2>&1 | flog
	# CS1 flash size varies per board (32 MiB and 256 MiB seen)
	bsize=$(stat -c %s "$backup")
	((bsize >= 33554432 && (bsize & (bsize - 1)) == 0)) || { echo "backup incomplete ($bsize bytes)" >&2; exit 1; }
	dd if="$backup" of="$window" bs=$SECTOR skip=$((OFFSET / SECTOR)) count=$((MAX_SIZE / SECTOR)) status=none
	head -c 4194304 "$backup" >"$probe"
	restore="-prog-file $backup 0"
else
	echo "== Backup CS1 [$OFFSET, +$(printf 0x%x $MAX_SIZE)) -> $backup"
	"$PROG" "${spi[@]}" -flash-info -dump-file "$probe" 0 0x10000 -dump-file "$backup" "$OFFSET" "$MAX_SIZE" 2>&1 | flog
	[[ $(stat -c %s "$backup") -eq $MAX_SIZE ]] || { echo "backup incomplete" >&2; exit 1; }
	cp "$backup" "$window"
	restore="-prog-file $backup $OFFSET"
fi
# CS1 starts with the MCU firmware; all 0xFF means the flash is not really readable
if [[ $(tr -d '\377' <"$probe" | wc -c) -eq 0 ]]; then
	echo "flash start reads blank (0xFF); is the BMC really in reset on CS1?" >&2
	exit 1
fi

# Changed 4 KiB sector runs of the image vs. the current flash: "<file-offset> <bytes>" per line
mapfile -t runs < <(python3 - "$IMG" "$window" "$SECTOR" <<'EOF'
import sys
img = open(sys.argv[1], "rb").read()
cur = open(sys.argv[2], "rb").read()
sec = int(sys.argv[3])
start = None
for off in range(0, len(img) + sec, sec):
    diff = off < len(img) and img[off:off + sec] != cur[off:off + min(sec, len(img) - off)]
    if diff and start is None:
        start = off
    elif not diff and start is not None:
        print(start, min(off, len(img)) - start)
        start = None
EOF
)
nbytes=0
for r in "${runs[@]}"; do nbytes=$((nbytes + ${r#* })); done
echo "== $nbytes of $size bytes differ (${#runs[@]} run(s) of ${SECTOR}-byte sectors)"

if ((dry)); then
	echo "dry run: not programming"
elif ((${#runs[@]} == 0)); then
	echo "flash already matches image, nothing to program"
else
	echo "== Program + verify changed sectors at $OFFSET"
	cmds=()
	for r in "${runs[@]}"; do
		read -r foff n <<<"$r"
		cmds+=(-prog-file "$IMG" $((OFFSET + foff)) "$foff" "$n")
	done
	set +e
	"$PROG" "${spi[@]}" -flash-info -verify-on "${cmds[@]}" 2>&1 | flog
	rc=${PIPESTATUS[0]}
	set -e
	if ((rc != 0)); then
		echo "program/verify failed (rc=$rc); restore with: $PROG ${spi[*]} -flash-info -verify-on $restore" >&2
		exit 1
	fi
	echo "== Independent read-back compare"
	cmp_file=$(mktemp)
	"$PROG" "${spi[@]}" -flash-info -dump-file "$cmp_file" $((OFFSET)) "$size" >/dev/null 2>&1
	cmp -n "$size" "$IMG" "$cmp_file" && echo "flash matches image"
	rm -f "$cmp_file"
fi

console_check() {
	python3 - "$CONSOLE" "$1" "$MENU" <<'EOF'
import sys, time, serial
s = serial.Serial(sys.argv[1], int(sys.argv[2]), timeout=0.2)
buf = b""
sent = False
end = time.time() + 40
while time.time() < end:
    buf += s.read(4096)
    if not sent and b"Select option:" in buf:
        s.write(sys.argv[3].encode())
        sent = True
    if b"U-Boot 20" in buf and b"U-Boot>" in buf:
        break
    if not sent and time.time() > end - 30 and b"Nuvoton" not in buf:
        break  # nothing readable at this baud
text = buf.decode(errors="replace")
ok = "U-Boot 20" in text
if ok or "Nuvoton" in text:
    print(text[-1800:])
print("RESULT:", "U-Boot started" if ok else "U-Boot did NOT start", "@", sys.argv[2])
sys.exit(0 if ok else 1)
EOF
}

release
if ((check)); then
	# BootBlock/U-Boot console baud depends on the BootBlock UART clock setup
	[[ $BAUD == auto ]] && bauds=(115200 750000) || bauds=("$BAUD")
	for b in "${bauds[@]}"; do
		echo "== Release reset (PORST pulse), console check ($CONSOLE @ $b)"
		"$FLASH_PROG/scripts/tavor-svb-porst.sh" 300 >/dev/null &
		console_check "$b" && exit 0
		wait || true
	done
	exit 1
else
	echo "== Release reset (PORST pulse)"
	"$FLASH_PROG/scripts/tavor-svb-porst.sh" 300
fi
