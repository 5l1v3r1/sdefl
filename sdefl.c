#include "sdefl.h"

#include <string.h> /* memcpy */
#include <limits.h> /* CHAR_BIT */

#define SDEFL_ZLIB_HDR      (0x01)

struct sdefl_match {int off, len;};
#define sdefl_npow2(n) (1<<(sdefl_ilog2(n-1)+1))
static const unsigned char sdefl_mirror[256] = {
    #define R2(n) n, n + 128, n + 64, n + 192
    #define R4(n) R2(n), R2(n + 32), R2(n + 16), R2(n + 48)
    #define R6(n) R4(n), R4(n +  8), R4(n +  4), R4(n + 12)
    R6(0), R6(2), R6(1), R6(3),
};
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

    if (lc < 280)
        sdefl_put(dst, s, sdefl_mirror[(lc-256)<<1], 7);
    else sdefl_put(dst, s, sdefl_mirror[(0xc0-280+lc)], 8);
    sdefl_put(dst, s, len - lmin[ls], lxn[ls]);
    sdefl_put(dst, s, sdefl_mirror[dc << 3], 5);
    sdefl_put(dst, s, dist - dmin[dc], dx);
}
static void
sdefl_lit(unsigned char **dst, struct sdefl *s, int c)
{
    if (c <= 143)
        sdefl_put(dst, s, sdefl_mirror[0x30+c], 8);
    else sdefl_put(dst, s, 1 + 2 * sdefl_mirror[0x90 - 144 + c], 9);
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
    int p = 0;
    unsigned char *q = out;
    static const unsigned char pref[] = {8,10,14,24,30,48,65,96,130};
    int max_chain = (lvl < 8) ? (1<<(lvl+1)): (1<<13);

    s->bits = s->cnt = 0;
    for (p = 0; p < SDEFL_HASH_SIZ; ++p)
        s->tbl[p] = SDEFL_NIL;

    p = 0;
    if (flags & SDEFL_ZLIB_HDR) {
        sdefl_put(&q, s, 0x78, 8); /* deflate, 32k window */
        sdefl_put(&q, s, 0x01, 8); /* fast compression */
    }
    sdefl_put(&q, s, 0x01, 1); /* block */
    sdefl_put(&q, s, 0x01, 2); /* static huffman */
    while (p < in_len) {
        struct sdefl_match m = {0};
        int max_match = ((in_len-p)>SDEFL_MAX_MATCH) ? SDEFL_MAX_MATCH:(in_len-p);
        int nice_match = pref[lvl] < max_match ? pref[lvl] : max_match;
        int run = 1;

        if (max_match > SDEFL_MIN_MATCH)
            sdefl_fnd(&m, s, max_chain, max_match, in, p);
        if (lvl >= 5 && m.len >= SDEFL_MIN_MATCH && m.len < nice_match){
            struct sdefl_match m2 = {0};
            sdefl_fnd(&m2, s, max_chain, m.len+1, in, p+1);
            m.len = (m2.len > m.len) ? 0 : m.len;
        }
        if (m.len >= SDEFL_MIN_MATCH) {
            sdefl_match(&q, s, m.off, m.len);
            run = m.len;
        } else sdefl_lit(&q, s, in[p]);

        while (run-- != 0) {
            unsigned h = sdefl_hash32(&in[p]);
            s->prv[p&SDEFL_WIN_MSK] = s->tbl[h];
            s->tbl[h] = p++;
        }
    }
    sdefl_put(&q, s, 0, 7); /* end of block */
    if (s->cnt) /* flush out all remaining bits */
        sdefl_put(&q, s, 0, 8 - s->cnt);
    if (flags & SDEFL_ZLIB_HDR) {
        /* optionally append adler checksum */
        unsigned a = sdefl_adler32(SDEFL_ADLER_INIT, in, in_len);
        for (p = 0; p < 4; ++p) {
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

