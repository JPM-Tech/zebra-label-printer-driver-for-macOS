#!/bin/bash
# uninstall.sh — Remove the Zebra ZD200t/ZD230 CUPS driver from macOS

set -euo pipefail

PRINTER_NAME="Zebra_ZD200t"

if [[ $EUID -ne 0 ]]; then
    echo "Run with sudo:  sudo bash uninstall.sh"
    exit 1
fi

lpstat -p "$PRINTER_NAME" &>/dev/null && {
    echo "Removing printer queue '$PRINTER_NAME'..."
    lpadmin -x "$PRINTER_NAME"
}

for f in \
    /usr/libexec/cups/filter/rastertozebrazpl \
    /usr/libexec/cups/filter/rastertozebrazpl.py \
    /Library/Printers/PPDs/Contents/Resources/ZebraZD200t.ppd; do
    [[ -f "$f" ]] && { echo "Removing $f"; rm "$f"; }
done

echo "Uninstall complete."
