#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

#include "internal.h"
#include "object/object.h"
#include "object/integerobject.h"
#include "mem.h"

MxcValue cstr2integer(char *, int, int);

#define maxpow_fitin64bit_by(base) _maxpow_fitin64bit_by[(base)-2]

static const uint64_t _maxpow_fitin64bit_by[35] = {
    9223372036854775808u, 12157665459056928801u, 4611686018427387904u,
    7450580596923828125u, 4738381338321616896u, 3909821048582988049u,
    9223372036854775808u, 12157665459056928801u, 10000000000000000000u,
    5559917313492231481u, 2218611106740436992u, 8650415919381337933u,
    2177953337809371136u, 6568408355712890625u, 1152921504606846976u,
    2862423051509815793u, 6746640616477458432u, 15181127029874798299u,
    1638400000000000000u, 3243919932521508681u, 6221821273427820544u,
    11592836324538749809u, 876488338465357824u, 1490116119384765625u,
    2481152873203736576u, 4052555153018976267u, 6502111422497947648u,
    10260628712958602189u, 15943230000000000000u, 787662783788549761u,
    1152921504606846976u, 1667889514952984961u, 2386420683693101056u,
    3379220508056640625u, 4738381338321616896u,
};

MxcValue new_integer(char *str, int base) {
    char *s = str;
    int sign = SIGN_PLUS;
    if(*s == '-') {
        sign = SIGN_MINUS;
        s++;
    }
    else if(*s == '+') {
        s++;
    }
    while(*s == '0') s++;
    if(*s == '\0') {
        return mval_int(0);
    }

    return cstr2integer(s, base, sign);
}

MxcInteger *new_integer_capa(size_t capa, int sign) {
    MxcInteger *ob = Mxc_malloc(sizeof(MxcInteger));
    ob->digit = malloc(sizeof(digit_t) * capa);
    ob->len = capa;
    ob->sign = sign;

    return ob;
}

MxcValue cstr2integer(char *str, int base, int sign) {
    char *s = str;
    MxcInteger *ob = Mxc_malloc(sizeof(MxcInteger));
    ob->sign = sign;
    size_t slen = strlen(str);
    size_t dslen = 1;
    ob->digit = malloc(sizeof(digit_t) * 50);/* TODO: really 50? */
    digit_t *digs = ob->digit;
    digit2_t d;
    unsigned int i = 0;
    
    while(*s) {
        d = intern_ascii_to_numtable[(int)*s++];
        if(d >= (unsigned int)base) {
            continue;
            // TODO: raise error
        }
        i = 0;

redo:
        while(i < dslen) {
            d += (digit2_t)digs[i] * base;
            digs[i++] = d & DIGIT_MAX;
            d = d >> DIGIT_POW;
        }
        if(d) {   /* carry occurs */
            ++dslen;
            goto redo;
        }
    }

    ob->len = dslen;

    for(unsigned int j = 0; j < dslen; ++j) {
        printf("%d is %u\n", j, digs[j]);
    }

    return mval_obj(ob);
}

static MxcValue integer_norm(MxcInteger *x) {
    size_t i = x->len;
    size_t tmp = i;
    while(i > 0 && x->digit[i - 1] == 0) {
        --i;
    }
    if(i != tmp) {
        x->len = i;
    }

    return mval_obj(x);
}

static MxcValue iadd_intern(MxcValue a, MxcValue b) {
    size_t alen = oint(a)->len, blen = oint(b)->len;
    /* always alen >= blen */
    if(alen < blen) {
        MxcValue tv = a; a = b; b = tv;
        size_t tl = alen; alen = blen; blen = tl;
    }

    MxcInteger *x = oint(a),
               *y = oint(b),
               *r = new_integer_capa(alen + 1, SIGN_PLUS);
    digit2_t carry = 0;
    size_t i = 0;
    for(; i < blen; i++) {
        carry += (digit2_t)x->digit[i] + y->digit[i];
        r->digit[i] = carry & DIGIT_MAX;
        carry >>= DIGIT_POW;
    }
    for(; i < alen; i++) {
        carry += x->digit[i];
        r->digit[i] = carry & DIGIT_MAX;
        carry >>= DIGIT_POW;
    }
    r->digit[i] = carry;

    return integer_norm(r);
}

static MxcValue isub_intern(MxcValue a, MxcValue b) {
    size_t alen = oint(a)->len, blen = oint(b)->len;
    int sign = SIGN_PLUS;
    /* always alen >= blen */
    if(alen < blen) {
        sign = SIGN_MINUS;
        MxcValue tv = a; a = b; b = tv;
        size_t tl = alen; alen = blen; blen = tl;
    }
    MxcInteger *x = oint(a),
               *y = oint(b);
    if(alen == blen) {
        ssize_t i = alen - 1;
        while(i >= 0 && x->digit[i] == y->digit[i]) {
            --i;
        }
        if(i < 0) return mval_int(0); /* a == b */
        if(x->digit[i] < y->digit[i]) {
            sign = SIGN_MINUS;
            MxcInteger *tmp = x; x = y; y = tmp;
        }
        alen = blen = i;
    }
    MxcInteger *r = new_integer_capa(alen, sign);
    size_t i = 0;
    sdigit2_t borrow = 0;
    for(; i < blen; i++) {
        borrow = (sdigit2_t)x->digit[i] - y->digit[i] - borrow;
        r->digit[i] = borrow & DIGIT_MAX;
        borrow >>= DIGIT_POW;
        borrow &= 1;
    }
    for(; i < alen; i++) {
        borrow = (sdigit2_t)x->digit[i] - borrow;
        r->digit[i] = borrow & DIGIT_MAX;
        borrow >>= DIGIT_POW;
        borrow &= 1;
    }

    return integer_norm(r);
}

MxcValue integer_add(MxcValue a, MxcValue b) {
    return iadd_intern(a, b);
}

MxcValue integer_sub(MxcValue a, MxcValue b) {
    return isub_intern(a, b);
}

/* r += a * b */
static void imuladd_digit_t(digit_t *rd, size_t rlen, MxcValue a, digit_t b) {
    if(b == 0) return;
    digit2_t carry = 0;
    size_t i = 0;
    size_t alen = oint(a)->len;
    digit_t *ad = oint(a)->digit;
    for(; i < alen; i++) {
    }
}

static MxcValue imul_intern(MxcValue a, MxcValue b) {
    size_t alen = oint(a)->len, blen = oint(b)->len;
    digit_t *bd = oint(b)->digit;
    MxcInteger *r = new_integer_capa(alen + blen,
            oint(a)->sign == oint(b)->sign);
    memset(r->digit, 0, sizeof(digit_t) * r->len);
    for(size_t i = 0; i < alen; i++) {
        imuladd_digit_t(r->digit + i, r->len - i, a, bd[i]);
    }

    return integer_norm(r);
}

static digit2_t digits_to_digit2(digit_t *digs, size_t ndig) {
    if(ndig == 2) return digs[0] | (digit2_t)digs[1] << 32;
    if(ndig == 1) return digs[0];
    return 0;
}

static void integer2str(MxcObject *self, int base, char *res) {
    MxcInteger *i = (MxcInteger *)self;
    char buf[sizeof(digit2_t) * CHAR_BIT + 1];
    char *end = buf + sizeof(buf);
    char *cur = end;
    digit2_t num = digits_to_digit2(i->digit, i->len);

    if(!res) {
        do {
            *--cur = mxc_36digits[num % base];
        } while(num /= base);
        size_t len = end - cur;
        res = malloc(sizeof(char) * len + 1);
        memcpy(res, cur, sizeof(char) * len);
    }
}

MxcValue integer_tostring(MxcObject *ob) {
    ;
}
