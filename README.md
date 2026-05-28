# Zebra ZD200t / ZD230 macOS CUPS Driver

A custom macOS printer driver for the Zebra ZD200t and ZD230 label printers.
Zebra does not publish an official macOS driver for these models, so this driver
handles the PDF-to-ZPL conversion that macOS needs to print to them.

---

## How It Works

macOS uses CUPS (the Unix printing system) under the hood. This driver adds:

- A **PPD file** — describes the printer's capabilities (label sizes, speed, darkness) to macOS
- A **CUPS filter** — a compiled C program that converts macOS's raster image output into ZPL II commands the Zebra understands

When you print, the chain is:
```
Your app  ->  PDF  ->  macOS CUPS  ->  raster image  ->  our filter  ->  ZPL  ->  printer
```

---

## Requirements

- macOS (tested on Ventura / Sonoma / Sequoia, Apple Silicon and Intel)
- Xcode Command Line Tools (for the one-time compile during install)
- Zebra ZD200t or ZD230 connected via USB

---

## Installation

### Step 1 — Install Xcode Command Line Tools (if not already installed)

Open Terminal and run:

```bash
xcode-select --install
```

A dialog will appear. Click Install and wait for it to finish. If it says tools are already installed, skip this step.

### Step 2 — Run the install script

In Terminal:

```bash
sudo bash CHANGE_ME_TO_THE_LOCATION_OF_THIS_REPO_ON_YOUR_MACHINE/install.sh
```

The script will:
1. Compile the filter from C source (no Python or other runtime needed)
2. Copy the filter to `/usr/libexec/cups/filter/`
3. Copy the PPD to `/Library/Printers/PPDs/Contents/Resources/`
4. Auto-detect your Zebra USB printer and add it as **Zebra_ZD200t**

### Step 3 — Verify in System Settings

Open **System Settings > Printers & Scanners**. You should see **Zebra ZD200t/ZD230 Label Printer** listed and ready.

---

## Printing

### From any macOS app (such as your browser, Pages, Keynote, Number, etc)

1. File > Print (Cmd+P)
2. Select **Zebra ZD200t/ZD230 Label Printer** from the printer list
3. Click the **Paper Size** dropdown and choose your label size
4. Optionally open **Printer Options** to set Speed and Darkness
5. Print

### From the command line

```bash
# Basic print — 4x6 label
lp -d Zebra_ZD200t -o media=w288h432 yourfile.pdf

# With custom darkness and speed
lp -d Zebra_ZD200t -o media=w288h432 -o ZebraSpeed=3 -o ZebraDarkness=+5 yourfile.pdf

# Check job status
lpstat -p Zebra_ZD200t
```

---

## Label Sizes

Select these in the **Paper Size** dropdown in any print dialog, or use the
`media=` option on the command line.

| Name in dialog         | media= value | Actual size       |
|------------------------|--------------|-------------------|
| 4" x 6" - Shipping     | w288h432     | 4" x 6"           |
| 4" x 5"                | w288h360     | 4" x 5"           |
| 4" x 4"                | w288h288     | 4" x 4"           |
| 4" x 3.5"              | w288h252     | 4" x 3.5"         |
| 4" x 3"                | w288h216     | 4" x 3"           |
| 4" x 2.5"              | w288h180     | 4" x 2.5"         |
| 4" x 2"                | w288h144     | 4" x 2"           |
| 4" x 1.5"              | w288h108     | 4" x 1.5"         |
| 4" x 1"                | w288h72      | 4" x 1"           |
| 4" x 0.75"             | w288h54      | 4" x 0.75"        |
| 4" x 8"                | w288h576     | 4" x 8"           |
| 3.625" x 2.5" - Receipt| w261h180     | 3.625" x 2.5"     |
| 3.625" x 1.75"         | w261h126     | 3.625" x 1.75"    |
| 3" x 6"                | w216h432     | 3" x 6"           |
| 3" x 4"                | w216h288     | 3" x 4"           |
| 3" x 3"                | w216h216     | 3" x 3"           |
| 3" x 2"                | w216h144     | 3" x 2"           |
| 3" x 1"                | w216h72      | 3" x 1"           |
| 3" x 0.75"             | w216h54      | 3" x 0.75"        |
| 2.5" x 1"              | w180h72      | 2.5" x 1"         |
| 2.5" x 0.75"           | w180h54      | 2.5" x 0.75"      |
| 2.25" x 1.75"          | w162h126     | 2.25" x 1.75"     |
| 2.25" x 1.25"          | w162h90      | 2.25" x 1.25"     |
| 2.25" x 1"             | w162h72      | 2.25" x 1"        |
| 2.25" x 0.75" - Price Tag | w162h54   | 2.25" x 0.75"     |
| 2" x 2"                | w144h144     | 2" x 2"           |
| 2" x 1.5"              | w144h108     | 2" x 1.5"         |
| 2" x 1"                | w144h72      | 2" x 1"           |
| 2" x 0.75"             | w144h54      | 2" x 0.75"        |
| 1.5" x 1"              | w108h72      | 1.5" x 1"         |
| 1" x 1"                | w72h72       | 1" x 1"           |

> **Tip:** The `media=` value encodes the size in points (1 point = 1/72 inch).
> `w288h432` means width=288pt (4") and height=432pt (6").

---

## Printer Features

These appear in the **Printer Options** section of the print dialog.

### Print Speed (`ZebraSpeed`)

Controls how fast the label feeds through the printer. Slower speeds give better
print quality on fine detail or small text.

| Dialog label      | Command line value | Speed      |
|-------------------|--------------------|------------|
| 2 ips - Slowest   | ZebraSpeed=2       | 2 in/sec   |
| 3 ips - Slow      | ZebraSpeed=3       | 3 in/sec   |
| 4 ips - Normal    | ZebraSpeed=4       | 4 in/sec (default) |
| 5 ips - Fast      | ZebraSpeed=5       | 5 in/sec   |
| 6 ips - Fastest   | ZebraSpeed=6       | 6 in/sec   |

### Print Darkness (`ZebraDarkness`)

Adjusts how much heat the print head applies. Increase if barcodes are faint or
not scanning. Decrease if labels are smearing or burning.

| Dialog label    | Command line value | Effect          |
|-----------------|--------------------|-----------------|
| -10 - Much lighter | ZebraDarkness=-10 | Very light     |
| -5 - Lighter    | ZebraDarkness=-5   | Light           |
| 0 - Normal      | ZebraDarkness=0    | Printer default |
| +5 - Darker     | ZebraDarkness=+5   | Slightly dark   |
| +10 - Much darker | ZebraDarkness=+10 | Dark           |
| +15 - Very dark | ZebraDarkness=+15  | Very dark       |
| +20 - Maximum   | ZebraDarkness=+20  | Maximum         |

> These values are relative adjustments on top of whatever darkness level is
> saved in the printer's own settings.

---

## Troubleshooting

### Print job fails with "Data failed" or "Filter failed"

Check the CUPS error log:

```bash
tail -50 /var/log/cups/error_log | grep "Job\|filter\|error\|status"
```

Common causes:
- **"no pages in raster stream"** — the raster data format wasn't recognized. Run `sudo bash install.sh` again to ensure the latest filter is installed.
- **"cannot open input file"** — permissions issue in the spool directory. Try restarting CUPS: `sudo launchctl kickstart -k system/org.cups.cupsd`
- **Filter compiled for wrong architecture** — if you switched between Intel and Apple Silicon, re-run `sudo bash install.sh` to recompile.

### Labels print blank or all black

- **All black**: the color space inversion is wrong. Try printing a simple black-on-white document from Preview or TextEdit to confirm.
- **All white / blank**: check that the label roll is loaded correctly (thermal side faces the print head).

### Label size doesn't match physical label

Make sure the size selected in the print dialog matches the label roll loaded in
the printer. The printer itself does not auto-detect label size from the driver.

### Barcodes don't scan after printing

Increase darkness by 1-2 steps. Also try reducing speed to 3 ips or slower.

### Printer disappears from the list after reconnecting USB

The printer should reappear automatically. If it shows as offline, unplug and
replug the USB cable. If the queue disappears entirely, re-run the install script.

### "Zebra Technologies ZTC ZD230-203dpi ZPL" also appears in my printer list

That is the raw ZPL queue macOS auto-creates. It only works if you send ZPL
code directly — regular apps like Pages or Word cannot use it. Use the
**Zebra ZD200t/ZD230 Label Printer** queue installed by this driver instead.

---

## Uninstalling

```bash
sudo bash CHANGE_ME_TO_THE_LOCATION_OF_THIS_REPO_ON_YOUR_MACHINE/uninstall.sh
```

This removes the printer queue, the filter binary, and the PPD file.

---

## Files in This Folder

| File                   | Purpose                                              |
|------------------------|------------------------------------------------------|
| `ZebraZD200t.ppd`      | Printer description file — label sizes and options   |
| `rastertozebrazpl.c`   | CUPS filter source code (C)                          |
| `install.sh`           | Compiles the filter and registers the printer        |
| `uninstall.sh`         | Removes everything installed by install.sh           |
| `rastertozebrazpl`     | Old shell wrapper (unused, kept for reference)       |
| `rastertozebrazpl.py`  | Old Python filter (unused, kept for reference)       |
