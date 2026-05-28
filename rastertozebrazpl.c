/*
 * rastertozebrazpl.c
 * CUPS filter: converts CUPS raster (v1/v2/v3, big or little endian) to
 * Zebra ZPL II ^GFA graphic commands for the ZD200t / ZD230.
 *
 * Build:  cc -O2 -o rastertozebrazpl rastertozebrazpl.c
 * Install: cp rastertozebrazpl /usr/libexec/cups/filter/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Page-header offsets (same for v1 @ 420 bytes and v2/v3 @ 1796 bytes) */
#define OFF_WIDTH       372
#define OFF_HEIGHT      376
#define OFF_BITS_PIXEL  388
#define OFF_BYTES_LINE  392
#define OFF_COLORSPACE  400
#define OFF_COMPRESSION 404

#define HDR_V1 420
#define HDR_V2 1796

/* CUPS color spaces where 0 = black, max = white (sRGB convention) */
#define WHITE_HIGH(cs) ((cs)==0||(cs)==18||(cs)==19)

static void log_msg(const char *msg) {
    fprintf(stderr, "rastertozebrazpl: %s\n", msg);
    fflush(stderr);
}

static uint32_t u32le(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1]<<8) |
           ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
}

static uint32_t u32be(const uint8_t *b) {
    return ((uint32_t)b[0]<<24) | ((uint32_t)b[1]<<16) |
           ((uint32_t)b[2]<<8) | (uint32_t)b[3];
}

static int read_exact(FILE *f, uint8_t *buf, size_t n) {
    return fread(buf, 1, n, f) == n ? 0 : -1;
}

/* Emit one ZPL label from a raster page */
static void emit_label(FILE *out,
                        uint32_t width, uint32_t height,
                        uint32_t bpp, uint32_t bpl, uint32_t colorspace,
                        const uint8_t *pixels,
                        int darkness, int has_darkness,
                        int speed,    int has_speed)
{
    static const char HEX[] = "0123456789ABCDEF";
    uint32_t zpl_bpr = (width + 7) / 8;
    uint32_t total   = zpl_bpr * height;
    int wh = WHITE_HIGH(colorspace);

    uint8_t *mono = malloc(total);
    if (!mono) { log_msg("out of memory"); return; }

    for (uint32_t row = 0; row < height; row++) {
        const uint8_t *src = pixels + (size_t)row * bpl;
        uint8_t       *dst = mono   + (size_t)row * zpl_bpr;

        if (bpp == 1) {
            uint32_t copy = zpl_bpr < bpl ? zpl_bpr : bpl;
            memcpy(dst, src, copy);
            if (copy < zpl_bpr) memset(dst + copy, 0, zpl_bpr - copy);
            if (wh) { for (uint32_t i = 0; i < zpl_bpr; i++) dst[i] ^= 0xFF; }
        } else if (bpp == 8) {
            for (uint32_t bi = 0; bi < zpl_bpr; bi++) {
                uint8_t byte_val = 0;
                for (int bit = 0; bit < 8; bit++) {
                    uint32_t px = bi * 8 + bit;
                    if (px >= width) break;
                    uint8_t v = (px < bpl) ? src[px] : (wh ? 255 : 0);
                    int black = wh ? (v < 128) : (v >= 128);
                    if (black) byte_val |= (uint8_t)(0x80 >> bit);
                }
                dst[bi] = byte_val;
            }
        } else if (bpp == 24) {
            for (uint32_t bi = 0; bi < zpl_bpr; bi++) {
                uint8_t byte_val = 0;
                for (int bit = 0; bit < 8; bit++) {
                    uint32_t px = bi * 8 + bit;
                    if (px >= width) break;
                    uint32_t i3 = px * 3;
                    uint8_t gray;
                    if (i3 + 2 < bpl) {
                        gray = (uint8_t)(0.299*src[i3] + 0.587*src[i3+1] + 0.114*src[i3+2]);
                    } else {
                        gray = wh ? 255 : 0;
                    }
                    int black = wh ? (gray < 128) : (gray >= 128);
                    if (black) byte_val |= (uint8_t)(0x80 >> bit);
                }
                dst[bi] = byte_val;
            }
        }
    }

    fprintf(out, "^XA\n^LH0,0\n^PW%u\n^LL%u\n", width, height);
    if (has_speed && speed > 0)         fprintf(out, "^PR%d,%d\n", speed, speed);
    if (has_darkness && darkness != 0)  fprintf(out, "^MD%+d\n", darkness);
    fprintf(out, "^FO0,0\n^GFA,%u,%u,%u,", total, total, zpl_bpr);

    for (uint32_t i = 0; i < total; i++) {
        fputc(HEX[mono[i] >> 4], out);
        fputc(HEX[mono[i] & 0xF], out);
    }
    fputs("\n^XZ\n", out);
    fflush(out);
    free(mono);
}

static int parse_int_opt(const char *opts, const char *key, int *out) {
    const char *p = strstr(opts, key);
    if (!p) return 0;
    p += strlen(key);
    *out = atoi(p);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 6 && argc != 7) {
        log_msg("usage: rastertozebrazpl job-id user title copies options [file]");
        return 1;
    }

    const char *opts = argv[5];
    int darkness = 0, has_darkness = parse_int_opt(opts, "ZebraDarkness=", &darkness);
    int speed    = 0, has_speed    = parse_int_opt(opts, "ZebraSpeed=",    &speed);

    FILE *fin = (argc == 7) ? fopen(argv[6], "rb") : stdin;
    if (!fin) { log_msg("cannot open input file"); return 1; }

    /* Read and identify magic */
    uint8_t magic[4];
    if (read_exact(fin, magic, 4)) { log_msg("cannot read magic"); goto fail; }

    int is_le = 0, hdr_size = HDR_V2;
    if      (!memcmp(magic, "3SaR", 4)) { is_le=1; hdr_size=HDR_V2; }
    else if (!memcmp(magic, "RaS3", 4)) { is_le=0; hdr_size=HDR_V2; }
    else if (!memcmp(magic, "2SaR", 4)) { is_le=1; hdr_size=HDR_V2; }
    else if (!memcmp(magic, "RaS2", 4)) { is_le=0; hdr_size=HDR_V2; }
    else if (!memcmp(magic, "tSaR", 4)) { is_le=1; hdr_size=HDR_V1; }
    else if (!memcmp(magic, "RaSt", 4)) { is_le=0; hdr_size=HDR_V1; }
    else {
        fprintf(stderr, "rastertozebrazpl: unknown magic %02x%02x%02x%02x\n",
                magic[0], magic[1], magic[2], magic[3]);
        goto fail;
    }

    uint8_t *hdr = malloc(hdr_size);
    if (!hdr) { log_msg("out of memory"); goto fail; }

    int page_count = 0;

    while (read_exact(fin, hdr, hdr_size) == 0) {
        uint32_t (*r32)(const uint8_t *) = is_le ? u32le : u32be;
        uint32_t width       = r32(hdr + OFF_WIDTH);
        uint32_t height      = r32(hdr + OFF_HEIGHT);
        uint32_t bpp         = r32(hdr + OFF_BITS_PIXEL);
        uint32_t bpl         = r32(hdr + OFF_BYTES_LINE);
        uint32_t colorspace  = r32(hdr + OFF_COLORSPACE);
        uint32_t compression = r32(hdr + OFF_COMPRESSION);

        if (width == 0 || height == 0) break;

        fprintf(stderr, "rastertozebrazpl: page %d: %ux%u bpp=%u cs=%u\n",
                page_count + 1, width, height, bpp, colorspace);
        fflush(stderr);

        size_t raw = (size_t)height * bpl;
        uint8_t *pixels = malloc(raw);
        if (!pixels) { log_msg("out of memory"); free(hdr); goto fail; }

        if (compression == 0) {
            if (read_exact(fin, pixels, raw)) {
                log_msg("short pixel read"); free(pixels); break;
            }
        } else if (compression == 1) {
            /* PackBits */
            size_t pos = 0;
            uint8_t ctrl, val;
            while (pos < raw && fread(&ctrl, 1, 1, fin) == 1) {
                if (ctrl < 128) {
                    size_t n = ctrl + 1;
                    if (pos + n > raw) n = raw - pos;
                    fread(pixels + pos, 1, n, fin); pos += n;
                } else if (ctrl > 128) {
                    if (!fread(&val, 1, 1, fin)) break;
                    size_t n = 257 - ctrl;
                    if (pos + n > raw) n = raw - pos;
                    memset(pixels + pos, val, n); pos += n;
                }
            }
        } else {
            fprintf(stderr, "rastertozebrazpl: unsupported compression %u\n", compression);
            free(pixels); break;
        }

        emit_label(stdout, width, height, bpp, bpl, colorspace, pixels,
                   darkness, has_darkness, speed, has_speed);
        free(pixels);
        page_count++;
    }

    free(hdr);
    if (fin != stdin) fclose(fin);

    if (page_count == 0) { log_msg("no pages"); return 1; }
    return 0;

fail:
    if (fin && fin != stdin) fclose(fin);
    return 1;
}
