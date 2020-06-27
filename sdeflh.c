#include "sdeflh.h"

#include <assert.h> /* assert */
#include <string.h> /* memcpy */
#include <limits.h> /* CHAR_BIT */

#define SDEFL_ZLIB_HDR          (0x01)

#define SDEFL_NIL               (-1)
#define SDEFL_MIN_MATCH         4
#define SDEFL_MAX_MATCH         258
#define SDEFL_MAX_CODE_LEN      (15)
#define SDEFL_SYM_BITS          (10u)
#define SDEFL_SYM_MSK           ((1u << SDEFL_SYM_BITS)-1u)
#define SDEFL_LIT_LEN_CODES     (14)
#define SDEFL_OFF_CODES         (15)
#define SDEFL_PRE_CODES         (7)
#define SDEFL_CNT_NUM(n)        ((((n)+3u/4u)+3u)&~3u)
#define SDEFL_EOB               (256)
#define SDEFL_BLK_MAX           (300*1024)

struct sdefl_match {int off, len;};
#define sdefl_npow2(n) (1<<(sdefl_ilog2((n)-1)+1))
#define sdefl_blk_end(dst, s) sdefl_put(dst, s, (int)s->cod.word.lit[SDEFL_EOB], s->cod.len.lit[SDEFL_EOB])

static unsigned
sdefl_adler32(unsigned adler32, const unsigned char *in, int in_len)
{
    #define SDEFL_ADLER_INIT  (1)
    const unsigned ADLER_MOD = 65521;
    unsigned s1 = adler32 & 0xffff;
    unsigned s2 = adler32 >> 16;
    unsigned blk_len, i;

    blk_len = in_len % 5552;
    while (in_len) {
        for (i=0; i + 7 < blk_len; i += 8) {
            s1 += in[0]; s2 += s1;
            s1 += in[1]; s2 += s1;
            s1 += in[2]; s2 += s1;
            s1 += in[3]; s2 += s1;
            s1 += in[4]; s2 += s1;
            s1 += in[5]; s2 += s1;
            s1 += in[6]; s2 += s1;
            s1 += in[7]; s2 += s1;
            in += 8;
        }
        for (; i < blk_len; ++i)
            s1 += *in++, s2 += s1;
        s1 %= ADLER_MOD; s2 %= ADLER_MOD;
        in_len -= blk_len;
        blk_len = 5552;
    } return (unsigned)(s2 << 16) + (unsigned)s1;
}
static int
sdefl_ilog2(int n)
{
#ifdef _MSC_VER
    unsigned long msbp;
    _BitScanReverse(&msbp, (unsignd long)n);
    return (int)msbp;
#elif defined(__GNUC__) || defined(__clang__)
    return (int)sizeof(unsigned long)*CHAR_BIT-1-__builtin_clzl((unsigned long)n);
#else
    #define lt(n) n,n,n,n, n,n,n,n, n,n,n,n ,n,n,n,n
    static const char tbl[256] = {0,0,1,1,2,2,2,2,3,3,3,3,
        3,3,3,3,lt(4),lt(5),lt(5),lt(6),lt(6),lt(6),lt(6),
        lt(7),lt(7),lt(7),lt(7),lt(7),lt(7),lt(7),lt(7)
    }; int tt, t;
    if ((tt = (n >> 16)))
        return (t = (tt >> 8)) ? 24+tbl[t]: 16+tbl[tt];
    else return (t = (n >> 8)) ? 8+tbl[t]: tbl[n];
    #undef lt
#endif
}
static unsigned
sdefl_uload32(const void *p)
{
    /* hopefully will be optimized to an unaligned read */
    unsigned int n = 0;
    memcpy(&n, p, sizeof(n));
    return n;
}
static unsigned
sdefl_hash32(const void *p)
{
    unsigned n = sdefl_uload32(p);
    return (n*0x9E377989)>>(32-SDEFL_HASH_BITS);
}
static void
sdefl_put(unsigned char **dst, struct sdefl *s, int code, int bitcnt)
{
    s->bits |= (code << s->cnt);
    s->cnt += bitcnt;
    while (s->cnt >= 8) {
        unsigned char *tar = *dst;
        *tar = (unsigned char)(s->bits & 0xFF);
        s->bits >>= 8;
        s->cnt -= 8;
        *dst = *dst + 1;
    }
}
static void
sdefl_heap_sub(unsigned A[], unsigned len, unsigned sub)
{
    unsigned c, p = sub;
    unsigned v = A[sub];
    while ((c = p << 1) <= len) {
        if (c < len && A[c + 1] > A[c]) c++;
        if (v >= A[c]) break;
        A[p] = A[c], p = c;
    } A[p] = v;
}
static void
sdefl_heap_array(unsigned *A, unsigned len)
{
    unsigned sub;
    for (sub = len >> 1; sub >= 1; sub--)
        sdefl_heap_sub(A, len, sub);
}
static void
sdefl_heap_sort(unsigned *A, unsigned len)
{
    A--;
    sdefl_heap_array(A, len);
    while (len >= 2) {
        unsigned tmp = A[len];
        A[len--] = A[1];
        A[1] = tmp;
        sdefl_heap_sub(A, len, 1);
    }
}
static unsigned
sdefl_sort_sym(unsigned sym_cnt, unsigned *freqs,
    unsigned char *lens, unsigned *sym_out)
{
    unsigned cnts[SDEFL_CNT_NUM(SDEFL_SYM_MAX)] = {0};
    unsigned cnt_num = SDEFL_CNT_NUM(sym_cnt);
    unsigned used_sym = 0;

    unsigned sym, i;
    for (sym = 0; sym < sym_cnt; sym++) {
        freqs[sym] = (!freqs[sym]) ? 1 : freqs[sym];
        cnts[freqs[sym] < cnt_num-1 ? freqs[sym]: cnt_num-1]++;
    }
    for (i = 1; i < cnt_num; i++) {
        unsigned cnt = cnts[i];
        cnts[i] = used_sym;
        used_sym += cnt;
    }
    for (sym = 0; sym < sym_cnt; sym++) {
        unsigned freq = freqs[sym];
        if (freq) {
            unsigned idx = freq < cnt_num-1 ? freq : cnt_num-1;
            sym_out[cnts[idx]++] = sym | (freq << SDEFL_SYM_BITS);
        } else lens[sym] = 0;
    }
    sdefl_heap_sort(sym_out + cnts[cnt_num-2], cnts[cnt_num-1] - cnts[cnt_num-2]);
    return used_sym;
}
static void
sdefl_build_tree(unsigned *A, unsigned sym_cnt)
{
    unsigned i = 0, b = 0, e = 0;
    do {unsigned m, n, freq_shift;
        if (i != sym_cnt && (b == e || (A[i] >> SDEFL_SYM_BITS) <= (A[b] >> SDEFL_SYM_BITS)))
            m = i++;
        else m = b++;
        if (i != sym_cnt && (b == e || (A[i] >> SDEFL_SYM_BITS) <= (A[b] >> SDEFL_SYM_BITS)))
            n = i++;
        else n = b++;

        freq_shift = (A[m] & ~SDEFL_SYM_MSK) + (A[n] & ~SDEFL_SYM_MSK);
        A[m] = (A[m] & SDEFL_SYM_MSK) | (e << SDEFL_SYM_BITS);
        A[n] = (A[n] & SDEFL_SYM_MSK) | (e << SDEFL_SYM_BITS);
        A[e] = (A[e] & SDEFL_SYM_MSK) | freq_shift;
    } while (sym_cnt - ++e > 1);
}
static void
sdefl_gen_len_cnt(unsigned *A, unsigned root,
    unsigned *len_cnt, unsigned max_code_len)
{
    int n;
    unsigned i;
    for (i = 0; i <= max_code_len; i++)
        len_cnt[i] = 0;
    len_cnt[1] = 2;

    A[root] &= SDEFL_SYM_MSK;
    for (n = (int)root - 1; n >= 0; n--) {
        unsigned p = A[n] >> SDEFL_SYM_BITS;
        unsigned pdepth = A[p] >> SDEFL_SYM_BITS;
        unsigned depth = pdepth + 1;
        unsigned len = depth;

        A[n] = (A[n] & SDEFL_SYM_MSK) | (depth << SDEFL_SYM_BITS);
        if (len >= max_code_len) {
            len = max_code_len;
            do len--; while (!len_cnt[len]);
        }
        len_cnt[len]--;
        len_cnt[len+1] += 2;
    }
}
static void
sdefl_gen_codes(unsigned *A, unsigned char *lens,
    const unsigned *len_cnt, unsigned max_code_word_len, unsigned sym_cnt)
{
    unsigned i, sym, len, nxt[SDEFL_MAX_CODE_LEN + 1];
    for (i = 0, len = max_code_word_len; len >= 1; len--) {
        unsigned cnt = len_cnt[len];
        while (cnt--) lens[A[i++] & SDEFL_SYM_MSK] = (unsigned char)len;
    }
    nxt[0] = nxt[1] = 0;
    for (len = 2; len <= max_code_word_len; len++)
        nxt[len] = (nxt[len-1] + len_cnt[len-1]) << 1;
    for (sym = 0; sym < sym_cnt; sym++)
        A[sym] = nxt[lens[sym]]++;
}
static unsigned
sdefl_rev(unsigned c, unsigned char n)
{
    c = ((c & 0x5555) << 1) | ((c & 0xAAAA) >> 1);
    c = ((c & 0x3333) << 2) | ((c & 0xCCCC) >> 2);
    c = ((c & 0x0F0F) << 4) | ((c & 0xF0F0) >> 4);
    c = ((c & 0x00FF) << 8) | ((c & 0xFF00) >> 8);
    return c >> (16-n);
}
static void
sdefl_huff(unsigned char *lens, unsigned *codes, unsigned *freqs,
    unsigned num_syms, unsigned max_code_len)
{
    unsigned c, *A = codes;
    unsigned len_cnt[SDEFL_MAX_CODE_LEN + 1];
    unsigned used_syms = sdefl_sort_sym(num_syms, freqs, lens, A);
    assert(used_syms > 1);

    sdefl_build_tree(A, used_syms);
    sdefl_gen_len_cnt(A, used_syms-2, len_cnt, max_code_len);
    sdefl_gen_codes(A, lens, len_cnt, max_code_len, num_syms);
    for (c = 0; c < num_syms; c++)
        codes[c] = sdefl_rev(codes[c], lens[c]);
}
static void
sdefl_precode(unsigned *freqs, unsigned *items, unsigned *item_cnt,
    const unsigned char *lens, const unsigned cnt)
{
    unsigned run_end;
    unsigned run_start = 0;

    unsigned *at = items;
    do {unsigned len = lens[run_start];
        run_end = run_start;
        do run_end++; while (run_end != cnt && len == lens[run_end]);
        if ((run_end - run_start) >= 4) {
            freqs[len]++;
            *at++ = len;
            run_start++;

            do {unsigned xbits = (run_end - run_start) - 3;
                xbits = xbits < 0x03 ? xbits : 0x03;
                *at++ = 16 | (xbits << 5);
                run_start += 3 + xbits;
                freqs[16]++;
            } while ((run_end - run_start) >= 3);
        }
        while (run_start != run_end) {
            freqs[len]++;
            *at++ = len;
            run_start++;
        }
    } while (run_start != cnt);
    *item_cnt = (unsigned)(at - items);
}
static void
sdefl_blk_begin(unsigned char **dst, struct sdefl *s)
{
    unsigned i, item_cnt = 0;
    unsigned char lens[SDEFL_PRE_MAX];
    unsigned freqs[SDEFL_PRE_MAX] = {0};
    unsigned codes[SDEFL_PRE_MAX];
    unsigned items[SDEFL_SYM_MAX + SDEFL_OFF_MAX];
    static const unsigned char perm[SDEFL_PRE_MAX] = {16,17,18,0,8,7,9,6,10,5,11,
        4,12,3,13,2,14,1,15};

    sdefl_huff(s->cod.len.lit, s->cod.word.lit, s->freq.lit, SDEFL_SYM_MAX, SDEFL_LIT_LEN_CODES);
    sdefl_huff(s->cod.len.off, s->cod.word.off, s->freq.off, SDEFL_OFF_MAX, SDEFL_OFF_CODES);
    sdefl_precode(freqs, items, &item_cnt, s->cod.len.lit, SDEFL_SYM_MAX+SDEFL_OFF_MAX);
    sdefl_huff(lens, codes, freqs, SDEFL_PRE_MAX, SDEFL_PRE_CODES);

    sdefl_put(dst, s, 0x00, 1); /* normal block */
    sdefl_put(dst, s, 0x02, 2); /* dynamic huffman */
    sdefl_put(dst, s, SDEFL_SYM_MAX - 257, 5);
    sdefl_put(dst, s, SDEFL_OFF_MAX - 1, 5);
    sdefl_put(dst, s, SDEFL_PRE_MAX - 4, 4);

    for (i = 0; i < SDEFL_PRE_MAX; ++i)
        sdefl_put(dst, s, lens[perm[i]], 3);
    for (i = 0; i < item_cnt; ++i) {
        unsigned sym = items[i] & 0x1F;
        sdefl_put(dst, s, (int)codes[sym], lens[sym]);
        if (sym == 16) sdefl_put(dst, s, items[i] >> 5, 2);
    }
    memset(&s->freq, 0, sizeof(s->freq));
}
static void
sdefl_lit(unsigned char **dst, struct sdefl *s, int c)
{
    sdefl_put(dst, s, (int)s->cod.word.lit[c], s->cod.len.lit[c]);
    s->freq.lit[c]++;
}
static void
sdefl_match(unsigned char **dst, struct sdefl *s, int dist, int len)
{
    static const char lxn[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const short lmin[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,
        51,59,67,83,99,115,131,163,195,227,258};
    static const short dmin[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,
        385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const short dxmax[] = {0,6,12,24,48,96,192,384,768,1536,3072,6144,12288,24576};
    static const unsigned char lslot[258+1] = {
        0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 12,
        12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 16,
        16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18,
        18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
        21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
        22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
        23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25,
        25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
        25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26,
        26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
        26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
        27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
        27, 27, 28
    };
    int ls = lslot[len];
    int lc = 257 + ls;
    int dx = sdefl_ilog2(sdefl_npow2(dist) >> 2);
    int dc = dx ? ((dx + 1) << 1) + (dist > dxmax[dx]) : dist-1;

    sdefl_put(dst, s, (int)s->cod.word.lit[lc], s->cod.len.lit[lc]);
    sdefl_put(dst, s, len - lmin[ls], lxn[ls]);
    sdefl_put(dst, s, (int)s->cod.word.off[dc], s->cod.len.off[dc]);
    sdefl_put(dst, s, dist - dmin[dc], dx);

    s->freq.lit[lc]++;
    s->freq.off[dc]++;
}
static void
sdefl_fnd(struct sdefl_match *m, const struct sdefl *s,
    int chain_len, int max_match, const unsigned char *in, int p)
{
    int i = s->tbl[sdefl_hash32(&in[p])];
    int limit = ((p-SDEFL_WIN_SIZ)<SDEFL_NIL)?SDEFL_NIL:(p-SDEFL_WIN_SIZ);
    while (i > limit) {
        if (in[i+m->len] == in[p+m->len] &&
            (sdefl_uload32(&in[i]) == sdefl_uload32(&in[p]))){
            int n = SDEFL_MIN_MATCH;
            while (n < max_match && in[i+n] == in[p+n]) n++;
            if (n > m->len) {
                m->len = n, m->off = p - i;
                if (n == max_match) break;
            }
        }
        if (!(--chain_len)) break;
        i = s->prv[i&SDEFL_WIN_MSK];
    }
}
static int
sdefl_compr(struct sdefl *s, unsigned char *out,
    const unsigned char *in, int in_len, int lvl, unsigned flags)
{
    unsigned char *q = out;
    static const unsigned char pref[] = {8,10,14,24,30,48,65,96,130};
    int max_chain = (lvl < 8) ? (1<<(lvl+1)): (1<<13);
    int i = 0;

    s->bits = s->cnt = 0;
    for (i = 0; i < SDEFL_HASH_SIZ; ++i)
        s->tbl[i] = SDEFL_NIL;

    /* static huffman tables */
    for (i = 0; i <= 143; i++) s->freq.lit[i] = 1 << (9 - 8);
    for (i = 144; i <= 255; i++) s->freq.lit[i] = 1 << (9 - 9);
    for (i = 256; i <= 279; i++) s->freq.lit[i] = 1 << (9 - 7);
    for (i = 280; i <= 287; i++) s->freq.lit[i] = 1 << (9 - 8);
    for (i = 0; i < 32; i++) s->freq.off[i] = 1 << (5-5);
    i = 0;

    if (flags & SDEFL_ZLIB_HDR) {
        sdefl_put(&q, s, 0x78, 8); /* deflate, 32k window */
        sdefl_put(&q, s, 0x01, 8); /* fast compression */
    }
    do {int blk_end = i + SDEFL_BLK_MAX < in_len ? i + SDEFL_BLK_MAX : in_len;
        sdefl_blk_begin(&q, s);
        while (i < blk_end) {
            struct sdefl_match m = {0};
            int max_match = ((in_len-i)>SDEFL_MAX_MATCH) ? SDEFL_MAX_MATCH:(in_len-i);
            int nice_match = pref[lvl] < max_match ? pref[lvl] : max_match;
            int run = 1, inc = 1;

            if (max_match > SDEFL_MIN_MATCH)
                sdefl_fnd(&m, s, max_chain, max_match, in, i);
            if (lvl >= 5 && m.len >= SDEFL_MIN_MATCH && m.len < nice_match){
                struct sdefl_match m2 = {0};
                sdefl_fnd(&m2, s, max_chain, m.len+1, in, i+1);
                m.len = (m2.len > m.len) ? 0 : m.len;
            }
            if (m.len >= SDEFL_MIN_MATCH) {
                sdefl_match(&q, s, m.off, m.len);
                if (lvl < 2 && m.len >= nice_match)
                    inc = m.len;
                else run = m.len;
            } else sdefl_lit(&q, s, in[i]);

            while (run-- != 0) {
                unsigned h = sdefl_hash32(&in[i]);
                s->prv[i&SDEFL_WIN_MSK] = s->tbl[h];
                s->tbl[h] = i, i += inc;
            }
        }
        sdefl_blk_end(&q, s);
    } while (i < in_len);

    sdefl_put(&q, s, 0x01, 1); /* last block */
    sdefl_put(&q, s, 0x01, 2); /* static huffman */
    sdefl_put(&q, s, 0x00, 7); /* end of stream */
    if (s->cnt) sdefl_put(&q, s, 0, 8 - s->cnt);

    if (flags & SDEFL_ZLIB_HDR) {
        /* optionally append adler checksum */
        unsigned a = sdefl_adler32(SDEFL_ADLER_INIT, in, in_len);
        for (i = 0; i < 4; ++i) {
            sdefl_put(&q, s, (a>>24)&0xFF, 8);
            a <<= 8;
        }
    } return (int)(q - out);
}
extern int
sdeflate(struct sdefl *s, unsigned char *out,
    const unsigned char *in, int in_len, int lvl)
{
    return sdefl_compr(s, out, in, in_len, lvl, 0u);
}
extern int
zsdeflate(struct sdefl *s, unsigned char *out,
    const unsigned char *in, int in_len, int lvl)
{
    return sdefl_compr(s, out, in, in_len, lvl, SDEFL_ZLIB_HDR);
}
extern int
sdefl_bound(int len)
{
    int a = 128 + (len * 110) / 100;
    int b = 128 + len + ((len / (31 * 1024)) + 1) * 5;
    return (a > b) ? a : b;
}

