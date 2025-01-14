/*
** ccorelib.c
** Basic library
** See Copyright Notice in cscript.h
*/


// TODO: documentation (just copy lua documentation as most functions are identical)
#define CS_LIB


#include <ctype.h>
#include <string.h>

#include "cauxlib.h"
#include "cscript.h"



static int csB_error(cs_State *ts) {
    int level = csL_opt_integer(ts, 1, 0);
    cs_setntop(ts, 1); /* leave only message on top */
    if (cs_type(ts, 0) == CS_TSTRING && level >= 0) {
        csL_where(ts, level); /* add extra information */
        cs_push(ts, 0); /* push original error message... */
        cs_concat(ts, 2); /* ...and concatenate it with extra information */
        /* error message with extra information is now on top */
    }
    return cs_error(ts);
}


static int csB_assert(cs_State *ts) {
    if (c_likely(cs_to_bool(ts, -1))) { /* true? */
        return cs_nvalues(ts); /* get all arguments */
    } else { /* failed assert (error) */
        csL_check_any(ts, 0); /* must have a condition */
        cs_remove(ts, 0); /* remove condition */
        cs_push_literal(ts, "assertion failed"); /* push default err message */
        cs_setntop(ts, 1); /* leave only one message on top */
        return csB_error(ts);
    }
}


/* check if 'optnum' for 'cs_gc' was valid */
#define checkres(res)   { if (res == -1) break; }

static int csB_gc(cs_State *ts) {
    static const char *const opts[] = {"stop", "restart", "collect", "count",
        "step", "setpause", "setstepmul", "isrunning", NULL};
    static const int numopts[] = {CS_GCSTOP, CS_GCRESTART, CS_GCCOLLECT,
        CS_GCCOUNT, CS_GCSTEP, CS_GCSETPAUSE, CS_GCSETSTEPMUL, CS_GCISRUNNING};
    int optnum = numopts[csL_check_option(ts, 0, "collect", opts)];
    switch (optnum) {
        case CS_GCCOUNT: {
            int kb = cs_gc(ts, optnum); /* kibibytes */
            int b = cs_gc(ts, CS_GCCOUNTBYTES); /* leftover bytes */
            checkres(kb);
            cs_push_number(ts, (cs_Number)kb + ((cs_Number)b/1024));
            return 1;
        }
        case CS_GCSTEP: {
            int nstep = csL_opt_integer(ts, 1, 0);
            int completecycle = cs_gc(ts, optnum, nstep);
            checkres(completecycle);
            cs_push_bool(ts, completecycle);
            return 1;
        }
        case CS_GCSETPAUSE: case CS_GCSETSTEPMUL: {
            int arg = csL_opt_integer(ts, 1, 0);
            int prev = cs_gc(ts, optnum, arg);
            checkres(prev);
            cs_push_integer(ts, prev);
            return 1;
        }
        case CS_GCISRUNNING: {
            int running = cs_gc(ts, optnum);
            checkres(running);
            cs_push_bool(ts, running);
            return 1;
        }
        default: {
            int res = cs_gc(ts, optnum);
            checkres(res);
            cs_push_integer(ts, res);
            return 1;
        }
    }
    csL_push_fail(ts);
    return 0;
}


/*
** Reserved slot, above all arguments, to hold a copy of the returned
** string to avoid it being collected while parsed. 'load' has two
** optional arguments (chunk and source name).
*/
#define RESERVEDSLOT  2


static const char *loadreader(cs_State *ts, void *ud, size_t *sz) {
    (void)ud; /* unused */
    csL_check_stack(ts, 2, "too many nested functions");
    cs_push(ts, 0); /* push func... */
    cs_call(ts, 0, 1); /* ...and call it */
    if (cs_is_nil(ts, -1)) { /* nothing else to read? */
        cs_pop(ts, 1); /* pop result (nil) */
        *sz = 0;
        return NULL;
    } else if (c_unlikely(!cs_is_string(ts, -1))) { /* top is not a string? */
        csL_error(ts, "reader function must return a string");
    }
    cs_replace(ts, RESERVEDSLOT); /* move string into reserved slot */
    return csL_to_lstring(ts, RESERVEDSLOT, sz);
}


static int auxload(cs_State *ts, int status) {
    if (c_unlikely(status != CS_OK)) {
        csL_push_fail(ts); /* push fail */
        cs_insert(ts, -2); /* and put it in front of error message */
        return 2; /* nil + error message */
    }
    return 1; /* compiled function */
}


static int csB_load(cs_State *ts) {
    int status;
    size_t sz;
    const char *chunkname;
    const char *chunk = cs_to_lstring(ts, 0, &sz);
    if (chunk != NULL) { /* 'chunk' is a string? */
        chunkname = csL_opt_string(ts, 1, chunk);
        status = csL_loadbuffer(ts, chunk, sz, chunkname);
    } else { /* 'chunk' is not a string */
        chunkname = csL_opt_string(ts, 1, "(load)");
        csL_check_type(ts, 0, CS_TFUNCTION); /* 'chunk' must be a function */
        status = cs_load(ts, loadreader, NULL, chunkname);
    }
    return auxload(ts, status);
}


static int csB_loadfile(cs_State *ts) {
    const char *filename = csL_opt_string(ts, 0, NULL);
    int status = csL_loadfile(ts, filename);
    return auxload(ts, status);
}


static int csB_runfile(cs_State *ts) {
    const char *filename = csL_opt_string(ts, -1, NULL);
    cs_setntop(ts, 1);
    if (c_unlikely(csL_loadfile(ts, filename) != CS_OK))
        return cs_error(ts);
    cs_call(ts, 0, CS_MULRET);
    return cs_nvalues(ts) - 1; /* all not including 'filename' */
}


static int csB_getmetamethod(cs_State *ts) {
    static const char * const opts[CS_MM_N + 1] = {"__init", "__getidx",
        "__setidx", "__gc", "__close", "__call", "__concat", "__add", "__sub",
        "__mul", "__div", "__mod", "__pow", "__shl", "__shr", "__band",
        "__bor", "__xor", "__unm", "__bnot", "__eq", "__lt", "__le", NULL};
    static const cs_MM mmnum[] = {CS_MM_INIT, CS_MM_GETIDX, CS_MM_SETIDX,
        CS_MM_GC, CS_MM_CLOSE, CS_MM_CALL, CS_MM_CONCAT, CS_MM_ADD, CS_MM_SUB,
        CS_MM_MUL, CS_MM_DIV, CS_MM_MOD, CS_MM_POW, CS_MM_BSHL, CS_MM_BSHR,
        CS_MM_BAND, CS_MM_BOR, CS_MM_BXOR, CS_MM_UNM, CS_MM_BNOT, CS_MM_EQ,
        CS_MM_LT, CS_MM_LE};
    cs_MM mm;
    csL_check_any(ts, 0); /* object with metamethods */
    mm = mmnum[csL_check_option(ts, 1, NULL, opts)];
    if (!cs_hasvmt(ts, 0) || (cs_get_metamethod(ts, 0, mm) == CS_TNONE))
        cs_push_nil(ts);
    return 1;
}


/*
** This function allows traversal of all fields of a hashtable
** or instance `obj`. First argument is `obj` and its second argument
** is an index `idx`. `next` returns the next index of the `obj` and its
** associated value. When called with `nil` as its second argument, `next`
** returns an initial index and its associated value.
** When called with the last index, or with `nil` in an empty table, `next`
** returns `nil`. If the second argument is absent, then it is interpreted
** as `nil`.
**
** The order in which the indices are enumerated is not specified.
** 
** The behavior of `next` is `undefined` if, during the traversal, you
** assign any value to a non-existent field in the `obj`. You may however
** modify existing fields. In particular, you may clear existing fields.
*/
static int csB_next(cs_State *ts) {
    int tt = cs_type(ts, 0);
    csL_expect_arg(ts, (tt == CS_TINSTANCE || tt == CS_THTABLE), 0,
                       "instance or table");
    cs_setntop(ts, 2); /* if 2nd argument is missing create it */
    if (cs_next(ts, 0)) { /* found field? */
        return 2; /* key (index) + value */
    } else {
        cs_push_nil(ts);
        return 1;
    }
}


/*
** Returns `next` function, the hashtable or instance `obj`, and `nil`.
*/
static int csB_pairs(cs_State *ts) {
    csL_check_any(ts, 0);
    cs_push_cfunction(ts, csB_next);    /* will return generator, */
    cs_push(ts, 0);                     /* state, */
    cs_push_nil(ts);                    /* and initial value */
    return 3;
}


static int ipairsaux(cs_State *ts) {
    cs_Integer i;
    csL_check_type(ts, 0, CS_TARRAY);
    i = csL_check_integer(ts, 1);
    i = csL_intop(+, i, 1);
    cs_push_integer(ts, i);
    return (cs_get_index(ts, 0, i) == CS_TNIL ? 1 : 2);
}


static int csB_ipairs(cs_State *ts) {
    csL_check_type(ts, 0, CS_TARRAY);
    cs_push_cfunction(ts, ipairsaux); /* iteration function */
    cs_push(ts, 0); /* state */
    cs_push_integer(ts, -1); /* initial value */
    return 3;
}


static int finishpcall(cs_State *ts, int status, int extra) {
    if (c_unlikely(status != CS_OK)) {
        cs_push_bool(ts, 0);    /* false */
        cs_push(ts, -2);        /* error message */
        return 2;               /* return false, message */
    } else
        return cs_nvalues(ts) - extra; /* return all */
}


static int csB_pcall(cs_State *ts) {
    int status;
    csL_check_any(ts, 0);
    cs_push_bool(ts, 1); /* first result if no errors */
    cs_insert(ts, 0); /* insert it before the object being called */
    status = cs_pcall(ts, cs_nvalues(ts) - 2, CS_MULRET, 0);
    return finishpcall(ts, status, 0);
}


static int csB_xpcall(cs_State *ts) {
    int status;
    int nargs = cs_nvalues(ts) - 2;
    csL_check_type(ts, 1, CS_TFUNCTION);
    cs_push_bool(ts, 1); /* first result if no errors */
    cs_push(ts, 0); /* function */
    cs_rotate(ts, 2, 2); /* move them below the function's arguments */
    status = cs_pcall(ts, nargs, CS_MULRET, 1);
    return finishpcall(ts, status, 1);
}


static int csB_print(cs_State *ts) {
    int n = cs_nvalues(ts);
    for (int i = 0; i < n; i++) {
        size_t len;
        const char *str = csL_to_lstring(ts, i, &len);
        if (i > 0)
            cs_writelen(stdout, "\t", 1);
        cs_writelen(stdout, str, len);
        cs_pop(ts, 1); /* pop result from 'csL_to_string' */
    }
    cs_writeline(stdout);
    return 0;
}


static int csB_warn(cs_State *ts) {
    int n = cs_nvalues(ts);
    int i;
    csL_check_string(ts, 0); /* at least one string */
    for (i = 1; i < n; i++)
        csL_check_string(ts, i);
    for (i = 0; i < n - 1; i++)
        cs_warning(ts, cs_to_string(ts, i), 1);
    cs_warning(ts, cs_to_string(ts, n - 1), 0);
    return 0;
}


static int csB_len(cs_State *ts) {
    int t = cs_type(ts, 0);
    csL_check_arg(ts, t == CS_TARRAY || t == CS_THTABLE ||
                      t == CS_TINSTANCE || t == CS_TSTRING, 0,
                      "array, hashtable, instance or string");
    cs_push_integer(ts, cs_len(ts, 0));
    return 1;
}


static int csB_rawequal(cs_State *ts) {
    csL_check_any(ts, 0); /* lhs */
    csL_check_any(ts, 1); /* rhs */
    cs_push_bool(ts, cs_rawequal(ts, 0, 1));
    return 1;
}


static int csB_rawget(cs_State *ts) {
    csL_check_type(ts, 0, CS_TINSTANCE);
    csL_check_any(ts, 1); /* index */
    cs_setntop(ts, 2);
    cs_get_raw(ts, 0); /* this pops index */
    return 1; /* return property */
}


static int csB_rawset(cs_State *ts) {
    csL_check_type(ts, 0, CS_TINSTANCE);
    csL_check_any(ts, 1); /* index */
    csL_check_any(ts, 2); /* value */
    cs_setntop(ts, 3);
    cs_set_raw(ts, 1); /* this pops index and value */
    return 1; /* return instance */
}


static int csB_getargs(cs_State *ts) {
    int n = cs_nvalues(ts);
    if (cs_type(ts, 0) == CS_TSTRING) {
        const char *what = cs_to_string(ts, 0);
        if (strcmp(what, "array") == 0) { /* array? */
            cs_push_array(ts, n); /* push the array */
            while (--n) /* set the array indices */
                cs_set_index(ts, 0, n);
        } else if (strcmp(what, "table") == 0) { /* hashset? */
            cs_push_table(ts, n); /* push the table (hashset) */
            while (--n) { /* set the table fields */
                cs_push_bool(ts, 1);
                cs_set_field(ts, 0);
            }
        } else if (strcmp(what, "len") == 0) { /* len? */
            cs_push_integer(ts, n - 1); /* push total number of args */
        } else {
            csL_arg_error(ts, 0,
            "invalid string value, expected \"array\", \"table\" or \"len\"");
        }
        cs_replace(ts, 0); /* replace the option with the value */
        return 1; /* return the value */
    } else {
        cs_Integer i = csL_check_integer(ts, 0);
        if (i < 0) i = n + i;
        else if (++i > n) i = n - 1;
        csL_check_arg(ts, 0 <= i, 0, "index out of range");
        return n - (int)i;
    }
}


/* lookup table for digit values; -1==255>=36 -> invalid */
static const unsigned char numeraltable[] = { -1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
-1,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
25,26,27,28,29,30,31,32,33,34,35,-1,-1,-1,-1,-1,
-1,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
25,26,27,28,29,30,31,32,33,34,35,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

/* space characters to skip */
#define SPACECHARS      " \f\n\r\t\v"

/*
** Converts string to 'cs_Integer', skips leading and trailing whitespace,
** checks for overflows and underflows and checks if 's' is valid numeral
** string. Conversion works for bases 2-36 and hexadecimal and octal literal
** strings.
*/
static const char *strtoint(const char *s, int base, cs_Integer *pn, int *of) {
    const unsigned char *val = numeraltable + 1;
    const cs_Unsigned lowlim = CS_INTEGER_MIN;
    const cs_Unsigned lim = CS_UNSIGNED_MAX;
    cs_Unsigned n = 0;
    int c, neg = 0;
    *of = 0; /* reset overflow flag */
    s += strspn(s, SPACECHARS); /* skip leading whitespace */
    c = *s;
    if (c == '+' || c == '-') { /* have sign? */
        neg = -(c == '-');
        c = *s++;
    }
    if ((base == 8 || base == 16) && c == '0') { /* hexadecimal or octal? */
        c = *s++;
        /* (c | 32) => tolower */
        if ((c | 32) == 'x') { /* X or x ? */
            c = *s++;
            if (val[c] >= 16) return NULL; /* missing first digit */
            base = 16; /* set hexadecimal base */
        } else if (base == 0) { /* must be octal */
            base = 8; /* set octal base */
        }
    } else if (val[c] >= base) {
        return NULL;
    }
    if (base == 10) { /* decimal base? */
        for (; isdigit(c) && n <= lim/10 && 10*n <= lim - (c - '0'); c = *s++)
            n = n * 10 + (c - '0');
        if (!isdigit(c)) goto done;
    } else if (!(base & (base-1))) { /* base is power of 2? */
        /* get the number of bit shifts depending on the value of base */
        int bs = "\0\1\2\4\7\3\6\5"[(0x17 * base) >> 5 & 7];
        for (; isalnum(c) && val[c] < base && n <= lim >> bs; c = *s++)
            n = n<<bs | val[c];
    } else {
        for (; isalnum(c) && val[c] < base && n <= lim / base &&
                base * n <= lim - val[c]; c = *s++)
            n = n * base + val[c];
    }
    if (isalnum(c) && val[c] < base) { /* overflow? */
        *of = 1; /* signal it */
        do {c = *s++;} while(isalnum(c) && val[c] < base); /* skip numerals */
        n = lowlim;
    }
done:
    s--; s += strspn(s, SPACECHARS); /* skip trailing whitespace */
    if (n >= lowlim) { /* potential overflow? */
        if (!neg) { /* overflow */
            *of = 1;
            *pn = lim;
            return s;
        } else if (n > lowlim) { /* underflow? */
            *of = -1;
            *pn = lowlim;
            return s;
        }
    }
    *pn = (cs_Integer)((n^neg) - neg); /* resolve sign and store the result */
    return s;
}


static int csB_tonumber(cs_State *ts) {
    int overflow = 0;
    if (cs_is_noneornil(ts, 1)) { /* no base? */
        if (cs_type(ts, 0) == CS_TNUMBER) { /* number ? */
            cs_setntop(ts, 1); /* set it as top */
            return 1; /* return it */
        } else { /* must be string */
            const char *s = cs_to_string(ts, 0);
            if (s != NULL && cs_stringtonumber(ts, s, &overflow)) {
                cs_push_bool(ts, overflow);
                return 1;
            }
            csL_check_any(ts, 0);
        }
    } else { /* have base */
        size_t l;
        const char *s;
        cs_Integer n;
        cs_Integer i = csL_check_integer(ts, 1); /* base */
        csL_check_type(ts, 0, CS_TSTRING); /* string to convert */
        s = cs_to_lstring(ts, 0, &l);
        csL_check_arg(ts, 2 <= i && i <= 32, 1, "base out of range");
        if (strtoint(s, i, &n, &overflow) == s + l) { /* conversion ok? */
            cs_push_integer(ts, n); /* push the conversion number */
            cs_push_bool(ts, overflow); /* push overflow boolean */
            return 2;
        }
    }
    csL_push_fail(ts); /* conversion failed */
    return 1; /* return fail */
}


static int csB_tostring(cs_State *ts) {
    csL_check_number(ts, 0);
    csL_to_lstring(ts, 0, NULL);
    return 1;
}


static int csB_typeof(cs_State *ts) {
    int tt = cs_type(ts, 0);
    csL_check_arg(ts, tt != CS_TNONE, 0, "value expected");
    cs_push_string(ts, cs_typename(ts, 0));
    return 1;
}


static const cs_Entry basic_funcs[] = {
    {"error", csB_error},
    {"assert", csB_assert},
    {"gc", csB_gc},
    {"load", csB_load},
    {"loadfile", csB_loadfile},
    {"runfile", csB_runfile},
    {"getmetamethod", csB_getmetamethod},
    {"next", csB_next},
    {"pairs", csB_pairs},
    {"ipairs", csB_ipairs},
    {"pcall", csB_pcall},
    {"xpcall", csB_xpcall},
    {"print", csB_print},
    {"warn", csB_warn},
    {"len", csB_len},
    {"rawequal", csB_rawequal},
    {"rawget", csB_rawget},
    {"rawset", csB_rawset},
    {"getargs", csB_getargs},
    {"tonumber", csB_tonumber},
    {"tostring", csB_tostring},
    {"typeof", csB_typeof},
    /* placeholders */
    {CS_GNAME, NULL},
    {"__VERSION", NULL},
    {NULL, NULL},
};


CSMOD_API int csL_open_basic(cs_State *ts) {
    /* open lib into global instance */
    cs_push_globaltable(ts);
    csL_set_funcs(ts, basic_funcs, 0);
    /* set global __G */
    cs_push(ts, -1);
    cs_set_fieldstr(ts, -2, CS_GNAME);
    /* set global __VERSION */
    cs_push_literal(ts, CS_VERSION);
    cs_set_global(ts, "__VERSION");
    return 1;
}
