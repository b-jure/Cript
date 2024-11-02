/*
** cdebug.c
** Debug and error reporting functions
** See Copyright Notice in cscript.h
*/

#include "cdebug.h"
#include "capi.h"
#include "ccode.h"
#include "cfunction.h"
#include "cobject.h"
#include "cstring.h"
#include "climits.h"
#include "cstate.h"
#include "cobject.h"
#include "cprotected.h"
#include <string.h>



/* get line number of instruction ('pc') */
int crD_getfuncline(const Function *fn, int pc) {
    LineInfo *li;
    int low = 0;
    int high = fn->sizelinfo - 1;
    int mid = low + ((high - low)/2);
    cr_assert(fn->sizelinfo > 0);
    while (low <= high) {
        li = &fn->linfo[mid];
        if (li->pc < pc) 
            low = mid + 1;
        else if (li->pc > pc) 
            high = mid - 1;
        else 
            break;
        mid = low + ((high - low)/2);
    }
    return li->line;
}


/* current instruction in 'CrClosure' ('Function') */
cr_sinline int currentpc(const CallFrame *cf) {
    cr_assert(cfisCScript(cf));
    return cast_int(cf->pc - cfFunction(cf)->code) - 1;
}


/* current line number */
cr_sinline int currentline(CallFrame *cf) {
    return crD_getfuncline(cfFunction(cf), currentpc(cf));
}


static const char *findvararg(CallFrame *cf, SPtr *pos, int n) {
    if (cfFunction(cf)->isvararg) {
        int nextra = cf->nvarargs;
        if (n >= -nextra) {
            *pos = cf->func.p - nextra - (n + 1);
            return "(vararg)";
        }
    }
    return NULL;
}


/*
** Find local variable at index 'n', store it in 'pos' and
** returns its name. If variable is not found return NULL.
*/
const char *crD_findlocal(cr_State *ts, CallFrame *cf, int n, SPtr *pos) {
    SPtr base = cf->func.p + 1;
    const char *name = NULL;
    if (cfisCScript(cf)) {
        if (n < 0) /* vararg ? */
            return findvararg(cf, pos, n);
        else /* otherwise local variable */
            name = crF_getlocalname(cfFunction(cf), n, currentpc(cf));
    }
    if (name == NULL) {
        SPtr limit = (cf == ts->cf) ? ts->sp.p : cf->next->func.p;
        if (limit - base >= n && n > 0) /* 'n' is in stack range ? */
            name = cfisCScript(cf) ? "(auto)" : "(C auto)";
        else
            return NULL;
    }
    if (pos)
        *pos = base + (n - 1);
    return name;
}


CR_API const char *cr_getlocal(cr_State *ts, const cr_DebugInfo *di, int n) {
    const char *name;
    cr_lock(L);
    if (di == NULL) {
        if (!ttiscrcl(s2v(ts->sp.p - 1)))
            name = NULL;
        else
            name = crF_getlocalname(crclval(s2v(ts->sp.p - 1))->fn, n, 0);
    }
    else {
        SPtr pos = NULL;
        name = crD_findlocal(ts, di->cf, n, &pos);
        if (name) { /* found ? */
            setobjs2s(ts, ts->sp.p, pos);
            api_inctop(ts);
        }
    }
    cr_unlock(L);
    return name;
}


CR_API const char *cr_setlocal (cr_State *ts, const cr_DebugInfo *ar, int n) {
    SPtr pos = NULL;
    const char *name;
    cr_lock(ts);
    name = crD_findlocal(ts, ar->cf, n, &pos);
    if (name) { /* found ? */
        setobjs2s(ts, pos, ts->sp.p - 1); /* set the value */
        ts->sp.p--; /* remove value */
    }
    cr_unlock(ts);
    return name;
}


static void getfuncinfo(Closure *cl, cr_DebugInfo *di) {
    if (!CScriptclosure(cl)) {
        di->source = "[C]";
        di->srclen = SLL("[C]");
        di->defline = -1;
        di->lastdefline = -1;
        di->what = "C";
    } else {
        const Function *fn = cl->crc.fn;
        if (fn->source) { /* have source? */
          di->source = getstrbytes(fn->source);
          di->srclen = getstrlen(fn->source);
        }
        else {
          di->source = "?";
          di->srclen = SLL("?");
        }
        di->defline = fn->defline;
        di->lastdefline = fn->deflastline;
        di->what = (di->lastdefline == 0) ? "main" : "CScript";
    }
    crS_sourceid(di->shortsrc, di->source, di->srclen);
}


static const char *funcnamefromcode(cr_State *ts, const Function *fn, int pc,
                                    const char **name) {
    cr_MM mm;
    Instruction *i = &fn->code[pc];
    switch (*i) {
        case OP_CALL: {
            /* TODO: how do we know if it is local, global, field, ... ?
            ** We are missing the stack slot of the current holder of the
            ** function. */
            *name = "function";
            return "function"; 
        }
        case OP_FORCALL: {
            *name = "for iterator";
            return "for iterator";
        }
        case OP_GETPROPERTY: case OP_GETINDEX:
        case OP_GETINDEXSTR: case OP_GETINDEXINT: {
            mm = CR_MM_GETIDX;
            break;
        }
        case OP_SETPROPERTY: case OP_SETINDEX:
        case OP_SETINDEXSTR: case OP_SETINDEXINT: {
            mm = CR_MM_SETIDX;
            break;
        }
        case OP_MBIN: {
            mm = GETARG_S(i, 0);
            break;
        }
        case OP_UNM: mm = CR_MM_UNM; break;
        case OP_BNOT: mm = CR_MM_BNOT; break;
        case OP_CONCAT: mm = CR_MM_CONCAT; break;
        case OP_EQ: mm = CR_MM_EQ; break;
        case OP_LT: case OP_LTI: case OP_GTI: mm = CR_MM_LT; break;
        case OP_LE: case OP_LEI: case OP_GEI: mm = CR_MM_LE; break;
        case OP_CLOSE: case OP_RET: mm = CR_MM_CLOSE; break;
        default: return NULL;
    }
    *name = getstrbytes(G_(ts)->mmnames[mm]) /* and skip '__' */ + 2;
    return "metamethod";
}


/* try to find a name for a function based on how it was called */
static const char *funcnamefromcall(cr_State *ts, CallFrame *cf,
                                    const char **name) {
    if (cf->status & CFST_FIN) { /* called as finalizer? */
        *name = "__gc";
        return "metamethod";
    }
    else if (cfisCScript(cf))
        return funcnamefromcode(ts, cfFunction(cf), currentpc(cf), name);
    else
        return NULL;
}


static const char *getfuncname(cr_State *ts, CallFrame *cf, const char **name) {
    if (cf != NULL)
        return funcnamefromcall(ts, cf->prev, name);
    else
        return NULL;
}


/*
** Auxiliary to 'cr_getinfo', parses 'options' and fills out the
** 'cr_DebugInfo' accordingly. If any invalid options is specified this
** returns 0.
*/
static int getinfo(cr_State *ts, const char *options, Closure *cl,
                   CallFrame *cf, cr_DebugInfo *di) {
    int status = 1;
    for (; *options; options++) {
        switch (*options) {
            case 'n': {
                di->namewhat = getfuncname(ts, cf, &di->name);
                if (di->namewhat == NULL) { /* not found ? */
                    di->namewhat = "";
                    di->name = NULL;
                }
                break;
            }
            case 's': {
                getfuncinfo(cl, di);
                break;
            }
            case 'l': {
                di->currline = (cfisCScript(cf) ? currentline(cf) : -1);
                break;
            }
            case 'u': {
                di->nupvals = (cl ? cl->cc.nupvalues : 0);
                if (cfisCScript(cf)) {
                    di->nparams = cl->crc.fn->arity;
                    di->isvararg = cl->crc.fn->isvararg;
                } else {
                    di->nparams = 0;
                    di->isvararg = 1;
                }
                break;
            }
            case 'f': /* resolved in 'cr_getinfo' */
                break;
            default: { /* invalid option */
                status = 0;
                break;
            }
        }
    }
    return status;
}


/*
** Fill out 'cr_DebugInfo' according to 'options'.
**
** '>' - Pops the function on top of the stack and loads it into 'cf'.
** 'n' - Fills in the field `name` and `namewhat`.
** 's' - Fills in the fields `source`, `shortsrc`, `defline`,
**       `lastdefline`, and `what`.
** 'l' - Fills in the field `currentline`.
** 'u' - Fills in the field `nupvals`.
** 'f' - Pushes onto the stack the function that is running at the
**       given level.
**
** This function returns 0 on error (for instance if any option in 'options'
** is invalid).
*/
CR_API int cr_getinfo(cr_State *ts, const char *options, cr_DebugInfo *di) {
    CallFrame *cf;
    Closure *cl;
    TValue *func;
    int status = 1;
    cr_lock(ts);
    api_check(ts, options, "'options' is NULL");
    if (*options == '>') {
        cf = NULL; /* not currently running */
        func = s2v(ts->sp.p - 1);
        api_check(ts, ttisfunction(func), "expect function");
        options++; /* skip '>' */
        ts->sp.p--;
    } else {
        cf = ts->cf;
        func = s2v(cf->func.p);
        cr_assert(ttisfunction(func));
    }
    cl = (ttiscrcl(func) ? clval(func) : NULL);
    status = getinfo(ts, options, cl, cf, di);
    if (strchr(options, 'f')) {
        setobj2s(ts, ts->sp.p, func);
        api_inctop(ts);
    }
    cr_unlock(ts);
    return status;
}


/* add usual debug information to 'msg' (source id and line) */
const char *crD_addinfo(cr_State *ts, const char *msg, OString *src,
                        int line) {
    char buffer[CRI_MAXSRC];
    if (src) {
        crS_sourceid(buffer, src->bytes, src->len);
    } else {
        buffer[0] = '?';
        buffer[1] = '\0';
    }
    return crS_pushfstring(ts, "%s:%d: %s", buffer, line, msg);
}


/* generic runtime error */
cr_noret crD_runerror(cr_State *ts, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const char *err = crS_pushvfstring(ts, fmt, ap);
    va_end(ap);
    if (cfisCScript(ts->cf)) { /* in Cscript function (closure) ? */
        crD_addinfo(ts, err, cfFunction(ts->cf)->source, currentline(ts->cf));
        setobj2s(ts, ts->sp.p - 2, s2v(ts->sp.p - 1));
        ts->sp.p--;
    }
    crPR_throw(ts, CR_ERRRUNTIME);
}


/* global variable related error */
cr_noret crD_globalerror(cr_State *ts, const char *err, OString *name) {
    crD_runerror(ts, "%s global variable '%s'", err, getstrbytes(name));
}


/* operation on invalid type error */
cr_noret crD_typeerror(cr_State *ts, const TValue *v, const char *op) {
    crD_runerror(ts, "tried to %s a %s value", op, typename(ttypetag(v)));
}


cr_noret crD_typeerrormeta(cr_State *ts, const TValue *v1, const TValue *v2,
                           const char * mop) {
    crD_runerror(ts, "tried to %s %s and %s values",
                     mop, typename(ttypetag(v1)), typename(ttypetag(v2)));
}


/* arithmetic operation error */
cr_noret crD_operror(cr_State *ts, const TValue *v1, const TValue *v2,
                     const char *op) {
    if (ttisnum(v1))
        v1 = v2;  /* correct error value */
    crD_typeerror(ts, v1, op);
}


/* ordering error */
cr_noret crD_ordererror(cr_State *ts, const TValue *v1, const TValue *v2) {
    const char *name1 = typename(ttype(v1));
    const char *name2 = typename(ttype(v2));
    if (name1 == name2) /* point to same entry ? */
        crD_runerror(ts, "tried to compare two %s values", name1);
    else
        crD_runerror(ts, "tried to compare %s with %s", name1, name2);
}


cr_noret crD_concaterror(cr_State *ts, const TValue *v1, const TValue *v2) {
    if (ttisstr(v1)) v1 = v2;
    crD_typeerror(ts, v1, "concatenate");
}


cr_noret crD_callerror(cr_State *ts, const TValue *o) {
    crD_typeerror(ts, o, "call");
}
