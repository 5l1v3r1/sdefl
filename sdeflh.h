#ifndef SDEFL_H_INCLUDED
#define SDEFL_H_INCLUDED

#define SDEFL_MAX_OFF       (1 << 15)
#define SDEFL_WIN_SIZ       SDEFL_MAX_OFF
#define SDEFL_WIN_MSK       (SDEFL_WIN_SIZ-1)

#define SDEFL_HASH_BITS     15
#define SDEFL_HASH_SIZ      (1 << SDEFL_HASH_BITS)
#define SDEFL_HASH_MSK      (SDEFL_HASH_SIZ-1)

#define SDEFL_SYM_MAX       (288)
#define SDEFL_OFF_MAX       (32)
#define SDEFL_PRE_MAX       (19)

#define SDEFL_LVL_MIN       0
#define SDEFL_LVL_DEF       5
#define SDEFL_LVL_MAX       8

struct sdefl_freq {
    unsigned lit[SDEFL_SYM_MAX];
    unsigned off[SDEFL_OFF_MAX];
};
struct sdefl_code_words {
    unsigned lit[SDEFL_SYM_MAX];
    unsigned off[SDEFL_OFF_MAX];
};
struct sdefl_lens {
    unsigned char lit[SDEFL_SYM_MAX];
    unsigned char off[SDEFL_OFF_MAX];
};
struct sdefl_codes {
    struct sdefl_code_words word;
    struct sdefl_lens len;
};
struct sdefl {
    int bits, cnt;
    int tbl[SDEFL_HASH_SIZ];
    int prv[SDEFL_WIN_SIZ];

    struct sdefl_freq freq;
    struct sdefl_codes cod;
};
extern int sdefl_bound(int in_len);
extern int sdeflate(struct sdefl *s, unsigned char *out, const unsigned char *in, int in_len, int lvl);
extern int zsdeflate(struct sdefl *s, unsigned char *out, const unsigned char *in, int in_len, int lvl);

#endif /* SDEFL_H_INCLUDED */

