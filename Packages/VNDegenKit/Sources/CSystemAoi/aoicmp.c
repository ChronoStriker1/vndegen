#include "sa_private.h"

enum {
    HEADER = 16,
    BLOCK_HEADER = 5,
    SYMBOLS = 257,
    END_SYMBOL = 256,
    TOP = 1 << 24,
    BOTTOM = 1 << 16,
    METHOD_RLE = 1,
    METHOD_LZ = 2,
    METHOD_RANGE = 0x80,
    LZ_MIN = 5,
    RUN_MAX = 255,
    LZ_WINDOW = 4096,
};

typedef struct Model {
    u32 freq[SYMBOLS], cum[SYMBOLS + 1], total;
} Model;

static void model_sums(Model *m) {
    m->freq[END_SYMBOL] = 1;
    m->cum[0] = 0;
    for (int i = 0; i < SYMBOLS; i++) m->cum[i + 1] = m->cum[i] + m->freq[i];
    m->total = m->cum[SYMBOLS];
}

static bool range_decode(const u8 *src, size_t n, u8 *out, size_t cap, size_t *outLen) {
    if (n < 256 + 4) return false;
    Model m;
    for (int k = 0; k < 256; k++) m.freq[255 - k] = src[k];
    model_sums(&m);
    size_t p = 256;
    u32 code = 0;
    for (int i = 0; i < 4; i++) code = code << 8 | src[p++];
    u32 low = 0, range = 0xffffffffu;
    u32 r = range / m.total;
    u32 value = code / r;
    size_t o = 0;
    for (;;) {
        if (value >= m.total) return false;
        int lo = 0, hi = END_SYMBOL;
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            if (m.cum[mid + 1] > value) hi = mid;
            else lo = mid + 1;
        }
        if (lo == END_SYMBOL) break;
        if (o >= cap) return false;
        out[o++] = (u8)lo;
        low += m.cum[lo] * r;
        range = m.freq[lo] * r;
        while ((low ^ (low + range)) < TOP) {
            code = code << 8 | (p < n ? src[p] : 0);
            p++;
            low <<= 8;
            range <<= 8;
        }
        while (range < BOTTOM) {
            code = code << 8 | (p < n ? src[p] : 0);
            p++;
            range = (BOTTOM - (low & 0xffff)) << 8;
            low <<= 8;
        }
        r = range / m.total;
        value = (code - low) / r;
    }
    *outLen = o;
    return true;
}

static bool range_encode(const u8 *src, size_t n, u8 *out, size_t *outLen) {
    if (n < 512) return false;
    Model m = {0};
    for (size_t i = 0; i < n; i++) m.freq[src[i]]++;
    u32 most = 0;
    for (int i = 0; i < 256; i++)
        if (m.freq[i] > most) most = m.freq[i];
    if (most > 255)
        for (int i = 0; i < 256; i++) m.freq[i] = (m.freq[i] * 255 + most - 1) / most;
    for (int k = 0; k < 256; k++) out[k] = (u8)m.freq[255 - k];
    model_sums(&m);
    size_t k = 256;
    u32 low = 0, range = 0xffffffffu;
    for (size_t i = 0; i <= n; i++) {
        int s = i < n ? src[i] : END_SYMBOL;
        u32 r = range / m.total;
        low += m.cum[s] * r;
        range = m.freq[s] * r;
        while ((low ^ (low + range)) < TOP) {
            if (k + 1 >= n) return false;
            out[k++] = (u8)(low >> 24);
            low <<= 8;
            range <<= 8;
        }
        while (range < BOTTOM) {
            if (k + 1 >= n) return false;
            out[k++] = (u8)(low >> 24);
            range = (BOTTOM - (low & 0xffff)) << 8;
            low <<= 8;
        }
    }
    for (int i = 0; i < 4; i++) {
        if (k + 1 >= n) return false;
        out[k++] = (u8)(low >> 24);
        low <<= 8;
    }
    *outLen = k;
    return true;
}

static bool lz_decode(const u8 *src, size_t n, u8 *out, size_t cap, size_t *outLen) {
    size_t i = 0, o = 0;
    while (i < n) {
        u8 c = src[i++];
        if (c) {
            if (i + c > n || o + c > cap) return false;
            memcpy(out + o, src + i, c);
            i += c;
            o += c;
            continue;
        }
        if (i + 3 > n) return false;
        size_t dist = (size_t)src[i] | (size_t)src[i + 1] << 8, len = src[i + 2];
        i += 3;
        if (len && (dist > o || dist == 0 || o + len > cap)) return false;
        for (size_t k = 0; k < len; k++, o++) out[o] = out[o - dist];
    }
    *outLen = o;
    return true;
}

static bool lz_encode(const u8 *src, size_t n, u8 *out, size_t *outLen) {
    if (n < 6) return false;
    long head[256];
    for (int i = 0; i < 256; i++) head[i] = -1;
    long *prev = sa_alloc(sizeof *prev * n);
    size_t pos = 0, o = 0, lit = 0;
    bool ok = false;
    while (pos < n) {
        size_t max = n - pos > RUN_MAX ? RUN_MAX : n - pos;
        size_t best = 0, bestAt = 0;
        for (long q = head[src[pos]]; q >= 0 && (size_t)q + LZ_WINDOW >= pos; q = prev[q]) {
            size_t len = 1;
            while (len < max && src[q + len] == src[pos + len]) len++;
            if (len > best) {
                best = len;
                bestAt = (size_t)q;
                if (len == max) break;
            }
        }
        size_t step = 1;
        if (best >= LZ_MIN) {
            if (lit) {
                if (lit + o + 1 > n) goto done;
                out[o++] = (u8)lit;
                memcpy(out + o, src + pos - lit, lit);
                o += lit;
                lit = 0;
            }
            if (o + 4 > n) goto done;
            size_t dist = pos - bestAt;
            out[o] = 0;
            out[o + 1] = (u8)dist;
            out[o + 2] = (u8)(dist >> 8);
            out[o + 3] = (u8)best;
            o += 4;
            step = best;
        } else if (++lit > RUN_MAX) {
            if (o + 256 > n) goto done;
            out[o++] = RUN_MAX;
            memcpy(out + o, src + pos - RUN_MAX, RUN_MAX);
            o += RUN_MAX;
            lit -= RUN_MAX;
        }
        for (size_t k = 0; k < step; k++, pos++) {
            prev[pos] = head[src[pos]];
            head[src[pos]] = (long)pos;
        }
    }
    if (lit) {
        if (lit + o + 1 > n) goto done;
        out[o++] = (u8)lit;
        memcpy(out + o, src + pos - lit, lit);
        o += lit;
    }
    *outLen = o;
    ok = true;
done:
    free(prev);
    return ok;
}

static bool rle_decode(const u8 *src, size_t n, u8 *out, size_t cap, size_t *outLen) {
    size_t i = 0, o = 0;
    while (i < n) {
        if (i + 2 > n) return false;
        int type = src[i], count = src[i + 1];
        i += 2;
        if (type > 4) return false;
        size_t width = type ? (size_t)type : (size_t)count, copies = type ? (size_t)count : 1;
        if (i + width > n || o + width * copies > cap) return false;
        for (size_t c = 0; c < copies; c++, o += width) memcpy(out + o, src + i, width);
        i += width;
    }
    *outLen = o;
    return true;
}

static int repeats(const u8 *p, size_t rem, size_t width) {
    static const int least[5] = {0, 4, 3, 2, 2};
    if (rem <= width) return 0;
    int count = 0;
    size_t off = 0;
    while (count < RUN_MAX && !memcmp(p + off, p, width)) {
        count++;
        off += width;
        if (off + width >= rem) break;
    }
    return count >= least[width] ? count : 0;
}

static bool rle_encode(const u8 *src, size_t n, u8 *out, size_t *outLen) {
    if (n < 4) return false;
    size_t pos = 0, o = 0, lit = 0;
    while (pos < n) {
        size_t rem = n - pos, width = 0;
        int count = 0;
        for (size_t w = 1; rem >= 4 && w <= 4 && !count; w++)
            if ((count = repeats(src + pos, rem, w))) width = w;
        if (!count) {
            if (++lit > RUN_MAX) {
                if (o + 2 + RUN_MAX > n) return false;
                out[o] = 0;
                out[o + 1] = RUN_MAX;
                memcpy(out + o + 2, src + pos + 1 - lit, RUN_MAX);
                o += 2 + RUN_MAX;
                lit -= RUN_MAX;
            }
            pos++;
            continue;
        }
        if (lit) {
            if (o + lit + 2 > n) return false;
            out[o] = 0;
            out[o + 1] = (u8)lit;
            memcpy(out + o + 2, src + pos - lit, lit);
            o += lit + 2;
            lit = 0;
        }
        if (o + width + 2 > n) return false;
        out[o] = (u8)width;
        out[o + 1] = (u8)count;
        memcpy(out + o + 2, src + pos, width);
        o += width + 2;
        pos += (size_t)count * width;
    }
    if (lit) {
        if (o + lit + 2 > n) return false;
        out[o] = 0;
        out[o + 1] = (u8)lit;
        memcpy(out + o + 2, src + pos - lit, lit);
        o += lit + 2;
    }
    *outLen = o;
    return true;
}

uint8_t *vnsystemaoi_unpack(const uint8_t *data, size_t size, size_t *outSize) {
    if (size < HEADER || rd32(data + 4) != size) return NULL;
    size_t total = rd32(data);
    u8 *out = sa_alloc(total ? total : 1);
    if (rd32(data + 12) & 1) {
        if (size - HEADER != total) goto fail;
        memcpy(out, data + HEADER, total);
        *outSize = total;
        return out;
    }
    u8 *tmp = sa_alloc(total ? total : 1);
    size_t o = 0, p = HEADER;
    while (p < size) {
        if (p + BLOCK_HEADER > size) goto fail_tmp;
        size_t n = rd32(data + p);
        u8 method = data[p + 4];
        p += BLOCK_HEADER;
        if (n > size - p || method & 0x70 || (method & 0xf) > METHOD_LZ) goto fail_tmp;
        const u8 *src = data + p;
        size_t len = n;
        if (method & METHOD_RANGE) {
            if (!range_decode(src, n, tmp, total, &len)) goto fail_tmp;
            src = tmp;
        }
        size_t made = len;
        bool ok = true;
        if ((method & 0xf) == METHOD_LZ) ok = lz_decode(src, len, out + o, total - o, &made);
        else if ((method & 0xf) == METHOD_RLE) ok = rle_decode(src, len, out + o, total - o, &made);
        else if (len <= total - o) memcpy(out + o, src, len);
        else ok = false;
        if (!ok) goto fail_tmp;
        o += made;
        p += n;
    }
    free(tmp);
    if (o != total) goto fail;
    *outSize = total;
    return out;
fail_tmp:
    free(tmp);
fail:
    free(out);
    return NULL;
}

uint8_t *vnsystemaoi_pack(const uint8_t *data, size_t size, size_t *outSize) {
    if (!size) return NULL;
    u8 *stage = sa_alloc(size), *coded = sa_alloc(size), *best = sa_alloc(size);
    size_t bestLen = size;
    u8 bestMethod = 0;
    memcpy(best, data, size);
    for (int method = 0; method <= METHOD_LZ; method++) {
        size_t len = size;
        const u8 *src = data;
        if (method == METHOD_RLE) {
            if (!rle_encode(data, size, stage, &len)) continue;
            src = stage;
        } else if (method == METHOD_LZ) {
            if (!lz_encode(data, size, stage, &len)) continue;
            src = stage;
        }
        if (len < bestLen) {
            bestLen = len;
            bestMethod = (u8)method;
            memcpy(best, src, len);
        }
        size_t codedLen;
        if (range_encode(src, len, coded, &codedLen) && codedLen < bestLen) {
            bestLen = codedLen;
            bestMethod = (u8)(method | METHOD_RANGE);
            memcpy(best, coded, codedLen);
        }
    }
    size_t total = HEADER + BLOCK_HEADER + bestLen;
    u8 *out = sa_alloc(total);
    wr32(out, (u32)size);
    wr32(out + 4, (u32)total);
    wr32(out + 8, (u32)size);
    wr32(out + 12, 0);
    wr32(out + HEADER, (u32)bestLen);
    out[HEADER + 4] = bestMethod;
    memcpy(out + HEADER + BLOCK_HEADER, best, bestLen);
    free(stage);
    free(coded);
    free(best);
    *outSize = total;
    return out;
}
