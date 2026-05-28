#!/bin/bash
# install.sh -- Install the Zebra ZD200t/ZD230 CUPS driver on macOS
# Run with: sudo bash install.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PPD_SRC="$SCRIPT_DIR/ZebraZD200t.ppd"
C_SRC="$SCRIPT_DIR/rastertozebrazpl.c"

FILTER_DIR="/usr/libexec/cups/filter"
PPD_DST_DIR="/Library/Printers/PPDs/Contents/Resources"
FILTER_DST="$FILTER_DIR/rastertozebrazpl"
PPD_DST="$PPD_DST_DIR/ZebraZD200t.ppd"
PRINTER_NAME="Zebra_ZD200t"

# ── Sanity checks ────────────────────────────────────────────────────────────
if [[ $EUID -ne 0 ]]; then
    echo "Run with sudo:  sudo bash install.sh"
    exit 1
fi

for f in "$PPD_SRC" "$C_SRC"; do
    [[ -f "$f" ]] || { echo "ERROR: missing $f"; exit 1; }
done

# ── Compile the C filter ─────────────────────────────────────────────────────
echo "Compiling CUPS filter from source..."
TMP_BIN="$(mktemp /tmp/rastertozebrazpl.XXXXXX)"

# Find a C compiler (cc, clang, or gcc)
CC_CMD=""
for try_cc in /usr/bin/cc /usr/bin/clang /usr/local/bin/gcc; do
    if [[ -x "$try_cc" ]]; then CC_CMD="$try_cc"; break; fi
done

if [[ -z "$CC_CMD" ]]; then
    echo "ERROR: No C compiler found."
    echo "Install Xcode or Command Line Tools:  xcode-select --install"
    exit 1
fi

"$CC_CMD" -O2 -o "$TMP_BIN" "$C_SRC" 2>&1
echo "Compiled OK with $CC_CMD"

# ── Install filter binary ────────────────────────────────────────────────────
echo "Installing filter -> $FILTER_DST"
cp "$TMP_BIN" "$FILTER_DST"
chmod 755 "$FILTER_DST"
chown root:wheel "$FILTER_DST"
rm -f "$TMP_BIN"

# Remove old Python helper if present from a previous install
rm -f "$FILTER_DIR/rastertozebrazpl.py"

# ── Install PPD ──────────────────────────────────────────────────────────────
echo "Installing PPD -> $PPD_DST"
mkdir -p "$PPD_DST_DIR"
cp "$PPD_SRC" "$PPD_DST"
chmod 644 "$PPD_DST"
chown root:wheel "$PPD_DST"

# ── Detect and register Zebra USB printer ────────────────────────────────────
echo ""
echo "Detecting Zebra printer..."
ZEBRA_URI=$(lpinfo -v 2>/dev/null | grep -i 'zebra' | awk '{print $2}' | head -1)

if [[ -z "$ZEBRA_URI" ]]; then
    echo "WARNING: No Zebra USB printer detected right now."
    echo "Connect and power on the printer, then run:"
    echo "  sudo lpadmin -p $PRINTER_NAME -E -v <uri> -P '$PPD_DST'"
    echo "  lpinfo -v | grep -i zebra   # to find the URI"
else
    echo "Found: $ZEBRA_URI"

    if lpstat -p "$PRINTER_NAME" &>/dev/null; then
        echo "Removing old queue '$PRINTER_NAME'..."
        lpadmin -x "$PRINTER_NAME" || true
    fi

    echo "Adding printer queue '$PRINTER_NAME'..."
    lpadmin \
        -p "$PRINTER_NAME" \
        -E \
        -v "$ZEBRA_URI" \
        -P "$PPD_DST" \
        -D "Zebra ZD200t/ZD230 Label Printer" \
        -L "USB Label Printer"

    echo ""
    echo "Done! Printer '$PRINTER_NAME' is ready."
fi

echo ""
echo "Label size quick reference (use with -o media=...):"
echo "  4\"x6\"  -> w288h432    2.25\"x1.25\" -> w162h90"
echo "  4\"x4\"  -> w288h288    2.25\"x0.75\" -> w162h54  (price tag)"
echo "  4\"x3\"  -> w288h216    2\"x1\"       -> w144h72"
echo "  4\"x2\"  -> w288h144    1.5\"x1\"     -> w108h72"
echo "  4\"x1\"  -> w288h72     1\"x1\"       -> w72h72"
echo "  3\"x2\"  -> w216h144    3.625\"x2.5\" -> w261h180 (receipt)"
echo ""
echo "Speed and darkness example:"
echo "  lp -d $PRINTER_NAME -o media=w288h432 -o ZebraSpeed=3 -o ZebraDarkness=+5 file.pdf"
