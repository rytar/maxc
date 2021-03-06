/* implementation of integer object */
#include <inttypes.h>
#include <stdlib.h> 
#include <string.h>
#include <limits.h>

#include "object/intobject.h"
#include "error/error.h"
#include "mem.h"
#include "vm.h"

MxcValue int_copy(MxcValue v) {
    return v;
}

MxcValue int_add(MxcValue l, MxcValue r) {
    return mval_int(l.num + r.num);
}

MxcValue int_sub(MxcValue l, MxcValue r) {
    return mval_int(l.num - r.num);
}

MxcValue int_mul(MxcValue l, MxcValue r) {
    return mval_int(l.num * r.num);
}

MxcValue int_div(MxcValue l, MxcValue r) {
    if(r.num == 0) {
        return mval_invalid;
    }

    return mval_int(l.num / r.num);
}

MxcValue int_mod(MxcValue l, MxcValue r) {
    return mval_int(l.num % r.num);
}

MxcValue int_eq(MxcValue l, MxcValue r) {
    if(l.num == r.num)
        return mval_true;
    else
        return mval_false;
}

MxcValue int_noteq(MxcValue l, MxcValue r) {
    if(l.num != r.num)
        return mval_true;
    else
        return mval_false;
}

MxcValue int_lt(MxcValue l, MxcValue r) {
    if(l.num < r.num)
        return mval_true;
    else
        return mval_false;
}

MxcValue int_lte(MxcValue l, MxcValue r) {
    if(l.num <= r.num)
        return mval_true;
    else
        return mval_false;
}

MxcValue int_gt(MxcValue l, MxcValue r) {
    if(l.num > r.num)
        return mval_true;
    else
        return mval_false;
}

MxcValue int_gte(MxcValue l, MxcValue r) {
    if(l.num >= r.num)
        return mval_true;
    else
        return mval_false;
}

MxcValue int2str(MxcValue val, int base) {
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    bool neg = false;
    char buf[sizeof(int64_t) * CHAR_BIT + 1];
    char *end = buf + sizeof(buf);
    char *cur = end;
    int64_t num = val.num;

    if(base < 2 || 36 < base) {
        return mval_invalid;
    }

    if(num < 0) {
        num = -num;
        neg = true;
    }

    do {
        *--cur = digits[num % base];
    } while(num /= base);
    if(neg) {
        *--cur = '-';
    }

    return new_string_copy(cur, end - cur);
}

MxcValue int_tostring(MxcValue val) {
    return int2str(val, 10);
}
