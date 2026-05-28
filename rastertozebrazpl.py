#!/usr/bin/env python3
"""
CUPS filter implementation: rastertozebrazpl
Converts CUPS raster (v1/v2/v3) to Zebra ZPL II for ZD200t / ZD230.

Installed to: /usr/libexec/cups/filter/rastertozebrazpl.py
Invoked via:  /usr/libexec/cups/filter/rastertozebrazpl (shell wrapper)
"""

import sys
import struct

SYNC_MAGIC = {
    b'RaSt': (1, '>'),
    b'tSaR': (1, '<'),
    b'RaS2': (2, '>'),
    b'2SaR': (2, '<'),
    b'RaS3': (3, '>'),
    b'3SaR': (3, '<'),
}

HEADER_SIZE = {1: 420, 2: 1796, 3: 1796}

OFF_WIDTH       = 372
OFF_HEIGHT      = 376
OFF_BITS_COLOR  = 384
OFF_BITS_PIXEL  = 388
OFF_BYTES_LINE  = 392
OFF_COLORSPACE  = 400
OFF_COMPRESSION = 404

# Color spaces where 0 = black, max = white (sRGB / standard white convention)
WHITE_HIGH = {0, 18, 19}   # CUPS_CSPACE_W, SW, SK


def log(msg):
    print(f"rastertozebrazpl: {msg}", file=sys.stderr, flush=True)


def parse_options(opt_str):
    opts = {}
    for token in opt_str.split():
        if '=' in token:
            k, v = token.split('=', 1)
            opts[k] = v
    return opts


def read_raster(data):
    """Yield (header_dict, pixel_bytes) per page in a CUPS raster byte string."""
    if len(data) < 4:
        return

    magic = data[:4]
    if magic not in SYNC_MAGIC:
        log(f"unknown raster magic {magic!r}")
        return

    version, endian = SYNC_MAGIC[magic]
    hdr_size = HEADER_SIZE[version]
    fmt = endian + 'I'
    pos = 4

    while pos + hdr_size <= len(data):
        h = data[pos:pos + hdr_size]
        pos += hdr_size

        width       = struct.unpack_from(fmt, h, OFF_WIDTH)[0]
        height      = struct.unpack_from(fmt, h, OFF_HEIGHT)[0]
        bpp         = struct.unpack_from(fmt, h, OFF_BITS_PIXEL)[0]
        bpl         = struct.unpack_from(fmt, h, OFF_BYTES_LINE)[0]
        colorspace  = struct.unpack_from(fmt, h, OFF_COLORSPACE)[0]
        compression = struct.unpack_from(fmt, h, OFF_COMPRESSION)[0]

        if width == 0 or height == 0:
            break

        raw_size = height * bpl

        if compression == 0:
            pixels = data[pos:pos + raw_size]
            pos += raw_size
        elif compression == 1:
            pixels = bytearray()
            while len(pixels) < raw_size and pos < len(data):
                ctrl = data[pos]; pos += 1
                if ctrl < 128:
                    n = ctrl + 1
                    pixels.extend(data[pos:pos + n]); pos += n
                elif ctrl > 128:
                    n = 257 - ctrl
                    pixels.extend(bytes([data[pos]]) * n); pos += 1
            pixels = bytes(pixels)
        else:
            log(f"unsupported compression {compression}")
            break

        yield {
            'width': width, 'height': height,
            'bpp': bpp, 'bpl': bpl,
            'colorspace': colorspace,
        }, pixels


def raster_to_zpl(width, height, bpp, bpl, colorspace, pixels, darkness, speed):
    """Return a complete ZPL II label string for one raster page."""
    zpl_bpr = (width + 7) // 8
    white_is_high = colorspace in WHITE_HIGH
    mono = bytearray()

    for row_idx in range(height):
        row = pixels[row_idx * bpl:(row_idx + 1) * bpl]
        out = bytearray(zpl_bpr)

        if bpp == 1:
            chunk = row[:zpl_bpr]
            for i, b in enumerate(chunk):
                out[i] = (b ^ 0xFF) if white_is_high else b

        elif bpp == 8:
            for byte_idx in range(zpl_bpr):
                byte_val = 0
                for bit in range(8):
                    px = byte_idx * 8 + bit
                    if px >= width:
                        break
                    v = row[px] if px < len(row) else (255 if white_is_high else 0)
                    is_black = (v < 128) if white_is_high else (v >= 128)
                    if is_black:
                        byte_val |= (0x80 >> bit)
                out[byte_idx] = byte_val

        elif bpp == 24:
            for byte_idx in range(zpl_bpr):
                byte_val = 0
                for bit in range(8):
                    px = byte_idx * 8 + bit
                    if px >= width:
                        break
                    i3 = px * 3
                    if i3 + 2 < len(row):
                        r, g, b = row[i3], row[i3 + 1], row[i3 + 2]
                        gray = int(0.299 * r + 0.587 * g + 0.114 * b)
                    else:
                        gray = 255 if white_is_high else 0
                    is_black = (gray < 128) if white_is_high else (gray >= 128)
                    if is_black:
                        byte_val |= (0x80 >> bit)
                out[byte_idx] = byte_val

        mono.extend(out)

    total = len(mono)
    hex_data = mono.hex().upper()

    # Build ZPL label with optional speed/darkness settings
    lines = [
        "^XA",
        "^LH0,0",
        f"^PW{width}",
        f"^LL{height}",
    ]
    if speed is not None:
        lines.append(f"^PR{speed},{speed}")
    if darkness is not None:
        lines.append(f"^MD{darkness}")
    lines += [
        "^FO0,0",
        f"^GFA,{total},{total},{zpl_bpr},{hex_data}",
        "^XZ",
        "",
    ]
    return "\n".join(lines)


def main():
    if len(sys.argv) not in (6, 7):
        log("usage: rastertozebrazpl.py job-id user title copies options [file]")
        sys.exit(1)

    opts = parse_options(sys.argv[5])

    # ZPL ^PR: print speed in ips (2–6)
    speed = opts.get("ZebraSpeed") or opts.get("zebra-speed")

    # ZPL ^MD: media darkness -30..+30
    darkness = opts.get("ZebraDarkness") or opts.get("zebra-darkness")
    if darkness == "0" or darkness == "+0":
        darkness = None  # let printer use its saved setting

    if len(sys.argv) == 7:
        try:
            with open(sys.argv[6], 'rb') as fh:
                data = fh.read()
        except OSError as e:
            log(f"cannot open file: {e}")
            sys.exit(1)
    else:
        data = sys.stdin.buffer.read()

    page_count = 0
    try:
        for hdr, pix in read_raster(data):
            page_count += 1
            log(f"page {page_count}: {hdr['width']}x{hdr['height']} bpp={hdr['bpp']}")
            zpl = raster_to_zpl(
                hdr['width'], hdr['height'],
                hdr['bpp'], hdr['bpl'],
                hdr['colorspace'], pix,
                darkness, speed,
            )
            sys.stdout.write(zpl)
            sys.stdout.flush()
    except BrokenPipeError:
        pass  # USB backend closed; ZPL was already sent
    except Exception as e:
        log(f"error: {e}")
        sys.exit(1)

    if page_count == 0:
        log("no pages in raster stream")
        sys.exit(1)


if __name__ == '__main__':
    main()
