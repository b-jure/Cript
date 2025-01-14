/*
** cstate.c
** Global and Thread state
** See Copyright Notice in cscript.h
*/


#define CS_CORE


#include "chashtable.h"
#include "carray.h"
#include "cstate.h"
#include "capi.h"
#include "cdebug.h"
#include "cfunction.h"
#include "cgc.h"
#include "cmem.h"
#include "cmeta.h"
#include "cobject.h"
#include "cprotected.h"
#include "cscript.h"
#include "cstring.h"
#include "ctrace.h"


/*
** -- Lua 5.4.7 [lstate.c]:58
** Macro for creating "random" seed when a state is created;
** seed is used for randomizing string hashes.
*/
#if !defined(csi_makeseed)
#include <time.h>
#include <string.h>

#define buffadd(b,p,e) \
    { size_t t = cast_sizet(e); \
      memcpy((b) + (p), &t, sizeof(t)); (p) += sizeof(t); }

static uint csi_makeseed(cs_State *ts) {
    char str[3 * sizeof(size_t)];
    uint seed = time(NULL); /* seed with current time */
    int n = 0;
    buffadd(str, n, ts); /* heap variable */
    buffadd(str, n, &seed); /* local variable */
    buffadd(str, n, &cs_newstate); /* public function */
    cs_assert(n == sizeof(str));
    return csS_hash(str, n, seed);
}
#endif


/*
** Preinitialize all thread fields to avoid collector
** errors.
*/
static void preinit_thread(cs_State *ts, GState *gs) {
    ts->ncf = 0;
    ts->status = CS_OK;
    ts->errfunc = 0;
    ts->nCcalls = 0;
    ts->gclist = NULL;
    ts->thwouv = ts; /* if ('ts->thwouv' == 'ts') then no upvalues */
    G_(ts) = gs;
    ts->errjmp = NULL;
    ts->stack.p = ts->sp.p = ts->stackend.p = NULL;
    ts->cf = NULL;
    ts->openupval = NULL;
    ts->tbclist.p = NULL;
}


/*
** Initialize stack and base call frame for 'ts'.
** 'maints' is a main thread state ('ts' == 'maints' only when creating new
** state).
*/
static void init_stack(cs_State *ts, cs_State *maints) {
    CallFrame *cf;
    cs_assert(!statefullybuilt(G_(ts)) == (ts == maints));
    ts->stack.p = csM_newarray(maints, INIT_STACKSIZE + EXTRA_STACK, SValue);
    ts->tbclist.p = ts->stack.p;
    for (int i = 0; i < INIT_STACKSIZE + EXTRA_STACK; i++)
        setnilval(s2v(ts->stack.p + i));
    ts->sp.p = ts->stack.p;
    ts->stackend.p = ts->stack.p + INIT_STACKSIZE;
    cf = &ts->basecf;
    cf->next = cf->prev = NULL;
    cf->func.p = ts->sp.p;
    cf->pc = NULL;
    cf->nvarargs = 0;
    cf->nresults = 0;
    cf->status = CFST_CCALL;
    setnilval(s2v(maints->sp.p)); /* 'cf' entry function */
    maints->sp.p++;
    cf->top.p = maints->stack.p + CS_MINSTACK;
    maints->cf = cf;
}


static void init_registry(cs_State *ts, GState *gs) {
    Array *registry = csA_new(ts); 
    setarrval(ts, &gs->c_registry, registry);
    csA_ensure(ts, registry, CS_RINDEX_LAST);
    /* registry[CS_RINDEX_MAINTHREAD] = ts (mainthread) */
    setthval(ts, &registry->b[CS_RINDEX_MAINTHREAD], ts);
    /* registry[CS_RINDEX_MAINTHREAD] = new hashtable (for global variables) */
    sethtval(ts, &registry->b[CS_RINDEX_GLOBALS], csH_new(ts));
}


/*
** Initializes parts of state that may cause memory allocation
** errors.
*/
static void f_newstate(cs_State *ts, void *ud) {
    GState *gs = G_(ts);
    UNUSED(ud);
    init_stack(ts, ts);
    init_registry(ts, gs);
    csS_init(ts);
    csMM_init(ts);
    csY_init(ts);
    gs->gcstop = 0;
    setnilval(&gs->nil); /* signal that state is fully built */
    csi_userstateopen(ts);
}


/* free all 'CallFrame' structures NOT in use by thread */
static void free_frames(cs_State *ts) {
    CallFrame *cf = ts->cf;
    CallFrame *next = cf->next;
    cf->next = NULL;
    while ((cf = next) != NULL) {
        next = cf->next;
        csM_free(ts, cf);
        ts->ncf--;
    }
}


/* free thread stack and call frames */
static void free_stack(cs_State *ts) {
    if (ts->stack.p != NULL) { /* stack fully built? */
        ts->cf = &ts->basecf; /* free all of the call frames */
        free_frames(ts);
        cs_assert(ts->ncf == 0 && ts->basecf.next == NULL);
        csM_freearray(ts, ts->stack.p, stacksize(ts) + EXTRA_STACK);
    }
}


/* free global state virtual method tables */
static void free_vmt(cs_State *ts) {
    GState *gs = G_(ts);
    cs_assert(ts == gs->mainthread);
    for (int i = 0; i < CS_NUM_TYPES; i++)
        if (gs->vmt[i])
            csM_freearray(ts, gs->vmt[i], CS_MM_N);
}


static void freestate(cs_State *ts) {
    GState *gs = G_(ts);
    cs_assert(ts == G_(ts)->mainthread);
    if (!statefullybuilt(gs)) { /* partially built state? */
        csG_freeallobjects(ts);
    } else { /* freeing fully built state */
        ts->cf = &ts->basecf; /* undwind call frames */
        csPR_close(ts, 1, CS_OK);
        csG_freeallobjects(ts);
        csi_userstateclose(ts);
    }
    csM_freearray(ts, gs->strtab.hash, gs->strtab.size);
    free_stack(ts);
    free_vmt(ts);
    printf("gettotalbytes = %zd, sizeof(XSG) = %zd\n", gettotalbytes(gs), sizeof(XSG));
    cs_assert(1 || gettotalbytes(gs) == sizeof(XSG)); /* TODO: fix */
    gs->falloc(fromstate(ts), sizeof(XSG), 0, gs->ud_alloc); /* free state */
}


/*
** Allocate new thread and global state with 'falloc' and
** userdata 'ud', from here on 'falloc' will be the allocator.
** The returned thread state is mainthread.
** In case of errors NULL is returned.
*/
CS_API cs_State *cs_newstate(cs_Alloc falloc, void *ud) {
    GState *gs;
    cs_State *ts;
    XSG *xsg = falloc(NULL, 0, sizeof(XSG), ud);
    if (c_unlikely(xsg == NULL)) return NULL;
    gs = &xsg->gs;
    ts = &xsg->xs.ts;
    ts->tt_ = CS_VTHREAD;
    gs->whitebit = bitmask(WHITEBIT0);
    ts->mark = csG_white(gs);
    preinit_thread(ts, gs);
    ts->next = NULL;
    incnnyc(ts);
    gs->objects = obj2gco(ts);
    gs->totalbytes = sizeof(XSG);
    gs->seed = csi_makeseed(ts); /* initial seed for hashing */
    gs->strtab.hash = NULL;
    gs->strtab.nuse = gs->strtab.size = 0;
    gs->gcdebt = 0;
    gs->gcstate = GCSpause;
    gs->gcstopem = 0;
    gs->gcstop = GCSTP; /* no GC while creating state */
    gs->gcemergency = 0;
    setgcparam(gs->gcpause, CSI_GCPAUSE);
    setgcparam(gs->gcstepmul, CSI_GCSTEPMUL);
    gs->gcstepsize = CSI_GCSTEPSIZE;
    gs->sweeppos = NULL;
    gs->fixed = gs->fin = gs->tobefin = NULL;
    gs->graylist = gs->grayagain = NULL;
    gs->weak = NULL;
    setnilval(&gs->c_registry);
    gs->falloc = falloc;
    gs->ud_alloc = ud;
    gs->fpanic = NULL; /* no panic handler by default */
    setival(&gs->nil, 0); /* signals that state is not yet fully initialized */
    gs->mainthread = ts;
    gs->thwouv = NULL;
    gs->fwarn = NULL; gs->ud_warn = NULL;
    for (int i = 0; i < CS_NUM_TYPES; i++) gs->vmt[i] = NULL;
    cs_assert(gs->totalbytes == sizeof(XSG) && gs->gcdebt == 0);
    if (csPR_rawcall(ts, f_newstate, NULL) != CS_OK) {
        freestate(ts);
        ts = NULL;
    }
    return ts;
}


/* free state (global state + mainthread) */
CS_API void cs_close(cs_State *ts) {
    cs_lock(ts);
    cs_State *mt = G_(ts)->mainthread;
    freestate(mt);
    /* user shall handle unlocking he defined himself (if any) */
}


/*
** Create new thread state.
** Argument 'mts' is the main thread created by 'csnewstate'.
*/
CS_API cs_State *cs_newthread(cs_State *mts) {
    GState *gs = G_(mts);
    cs_State *newts;
    GCObject *o;
    cs_lock(mts);
    o = csG_newoff(mts, sizeof(XS), CS_VTHREAD, offsetof(XS, ts));
    newts = gco2th(o);
    setthval2s(mts, mts->sp.p, newts);
    api_inctop(mts);
    preinit_thread(newts, gs);
    init_stack(newts, mts);
    memcpy(cs_getextraspace(newts), cs_getextraspace(gs->mainthread),
           CS_EXTRASPACE);
    csi_userstate(mts, newts);
    cs_unlock(mts);
    return newts;
}


int csT_resetthread(cs_State *ts, int status) {
    CallFrame *cf = ts->cf = &ts->basecf;
    setnilval(s2v(ts->stack.p)); /* 'basecf' func */
    cf->func.p = ts->stack.p;
    cf->status = CFST_CCALL;
    ts->status = CS_OK; /* so we can run '__close' */
    status = csPR_close(ts, 1, status);
    if (status != CS_OK) /* error? */
        csT_seterrorobj(ts, status, ts->stack.p + 1);
    else
        ts->sp.p = ts->stack.p + 1;
    cf->top.p = ts->sp.p + CS_MINSTACK;
    csT_reallocstack(ts, cf->top.p - ts->sp.p, 0);
    return status;
}


/*
** Reset thread state 'ts' by unwinding `CallFrame` list,
** closing all upvalues (and to-be-closed variables) and
** reseting the stack.
** In case of errors, error object is placed on top of the
** stack and the function returns relevant status code.
** If no errors occured `CS_OK` status is returned.
*/
CS_API int cs_resetthread(cs_State *ts) {
    int status;
    cs_lock(ts);
    status = csT_resetthread(ts, ts->status);
    cs_unlock(ts);
    return status;
}


void csT_seterrorobj(cs_State *ts, int errcode, SPtr oldtop) {
    switch (errcode) {
        case CS_ERRMEM: { /* memory error? */
            setstrval2s(ts, oldtop, G_(ts)->memerror);
            break;
        }
        case CS_ERRERROR: { /* error while handling error? */
            setstrval2s(ts, oldtop, csS_newlit(ts, "error in error handling"));
            break;
        }
        case CS_OK: { /* closing upvalue? */
            setnilval(s2v(oldtop)); /* no error message */
            break;
        }
        default: { /* real error */
            cs_assert(errcode > CS_OK);
            setobjs2s(ts, oldtop, ts->sp.p - 1); /* error msg on current top */
            break;
        }
    }
    ts->sp.p = oldtop + 1;
}


/*
** Stack size to grow the stack to when stack overflow occurs
** for error handling.
*/
#define OVERFLOWSTACKSIZE       (CSI_MAXSTACK + 200)


CallFrame *csT_newcf(cs_State *ts) {
    CallFrame *cf;
    cs_assert(ts->cf->next == NULL);
    cf = csM_new(ts, CallFrame);
    cs_assert(ts->cf->next == NULL);
    ts->cf->next = cf;
    cf->prev = ts->cf;
    cf->next = NULL;
    ts->ncf++;
    return cf;
}


/* convert stack pointers into relative stack offsets */
static void sptr2rel(cs_State *ts) {
    ts->sp.offset = savestack(ts, ts->sp.p);
    for (CallFrame *cf = ts->cf; cf != NULL; cf = cf->prev) {
        cf->func.offset = savestack(ts, cf->func.p);
        cf->top.offset = savestack(ts, cf->top.p);
    }
    for (UpVal *uv = ts->openupval; uv != NULL; uv = uv->u.open.next)
        uv->v.offset = savestack(ts, uv->v.p);
    ts->tbclist.offset = savestack(ts, ts->tbclist.p);
}


/* convert relative stack offsets into stack pointers */
static void rel2sptr(cs_State *ts) {
    ts->sp.p = restorestack(ts, ts->sp.offset);
    for (CallFrame *cf = ts->cf; cf != NULL; cf = cf->prev) {
        cf->func.p = restorestack(ts, cf->func.offset);
        cf->top.p = restorestack(ts, cf->top.offset);
    }
    for (UpVal *uv = ts->openupval; uv != NULL; uv = uv->u.open.next)
        uv->v.p = s2v(restorestack(ts, uv->v.offset));
    ts->tbclist.p = restorestack(ts, ts->tbclist.offset);
}


/* reallocate stack to new size */
int csT_reallocstack(cs_State *ts, int size, int raiseerr) {
    SPtr newstack;
    GState *gs = G_(ts);
    int old_stopem = gs->gcstopem;
    int osz = stacksize(ts);
    cs_assert(size <= CSI_MAXSTACK || size == OVERFLOWSTACKSIZE);
    sptr2rel(ts);
    gs->gcstopem = 1; /* no emergency collection when reallocating stack */
    newstack = csM_reallocarray(ts, ts->stack.p, osz + EXTRA_STACK,
                                size + EXTRA_STACK);
    gs->gcstopem = old_stopem;
    if (c_unlikely(newstack == NULL)) {
        rel2sptr(ts);
        if (raiseerr)
            csPR_throw(ts, CS_ERRMEM);
        return 0;
    }
    rel2sptr(ts);
    ts->stack.p = newstack;
    ts->stackend.p = newstack + size;
    for (int i = osz + EXTRA_STACK; i < size + EXTRA_STACK; i++)
        setnilval(s2v(newstack + i));
    return 1;
}


/* grow stack to accommodate 'n' values */
int csT_growstack(cs_State *ts, int n, int raiseerr) {
    int size = stacksize(ts);
    if (c_unlikely(size > CSI_MAXSTACK)) { /* overflowed already ? */
        cs_assert(size == OVERFLOWSTACKSIZE);
        if (raiseerr)
            csPR_throw(ts, CS_ERRERROR);
        return 0;
    }
    if (c_unlikely(n > CSI_MAXSTACK)) {
        int nsize = size * 2;
        int needed = cast_int((ts)->sp.p - (ts)->stack.p) + n;
        if (nsize > CSI_MAXSTACK)
            nsize = CSI_MAXSTACK;
        if (nsize < needed)
            nsize = needed;
        if (c_likely(nsize <= CSI_MAXSTACK))
            return csT_reallocstack(ts, nsize, raiseerr);
    }
    csT_reallocstack(ts, OVERFLOWSTACKSIZE, raiseerr);
    if (raiseerr)
        csD_runerror(ts, "stack overflow");
    return 0;
}


static int stackinuse(cs_State *ts) {
    SPtr maxtop = ts->cf->top.p;
    for (CallFrame *cf = ts->cf->prev; cf != NULL; cf = cf->prev) {
        if (maxtop < cf->top.p)
            maxtop = cf->top.p;
    }
    cs_assert(maxtop <= ts->stackend.p + EXTRA_STACK);
    int n = savestack(ts, maxtop);
    if (n < CS_MINSTACK)
        n = CS_MINSTACK;
    return n;
}


/*
** Shrink stack if the current stack size is more
** than 3 times the current use.
** This also rolls back the stack to its original maximum
** size 'CSI_MAXSTACK' in case the stack was previously
** handling stack overflow.
*/
void csT_shrinkstack(cs_State *ts) {
    int inuse = stackinuse(ts);
    int limit = (inuse >= CSI_MAXSTACK / 3 ? CSI_MAXSTACK : inuse * 3);
    if (inuse <= CSI_MAXSTACK && stacksize(ts) > limit) {
        int nsize = (inuse < (CSI_MAXSTACK / 2) ? (inuse * 2) : CSI_MAXSTACK);
        csT_reallocstack(ts, nsize, 0); /* this can fail */
    }
}


/* increment stack pointer */
void csT_incsp(cs_State *ts) {
    csT_checkstack(ts, 1);
    ts->sp.p++;
}


/*
** Called when 'getCcalls' is >= CSI_MAXCCALLS.
** If equal to CSI_MAXCCALLS then overflow error is invoked.
** Otherwise it is ignored in order to resolve the current
** overflow error, unless the number of calls is significantly
** higher than CSI_MAXCCALLS.
*/
void csT_checkCstack(cs_State *ts) {
    if (getCcalls(ts) == CSI_MAXCCALLS) /* not handling error ? */
        csD_runerror(ts, "C stack overflow");
    else if (getCcalls(ts) >= (CSI_MAXCCALLS / 10 * 11))
        csPR_throw(ts, CS_ERRERROR);
}


/* Increment number of C calls and check for overflow. */
void csT_incCstack(cs_State *ts) {
    ts->nCcalls++;
    if (getCcalls(ts) >= CSI_MAXCCALLS)
        csT_checkCstack(ts);
}


void csT_warning(cs_State *ts, const char *msg, int cont) {
    cs_WarnFunction fwarn = G_(ts)->fwarn;
    if (fwarn)
        fwarn(G_(ts)->ud_warn, msg, cont);
}


/* generate a warning from an error message */
void csT_warnerror(cs_State *ts, const char *where) {
    TValue *errobj = s2v(ts->sp.p - 1);
    const char *msg = (ttisstring(errobj))
                      ? getstr(strval(errobj))
                      : "error object is not a string";
    csT_warning(ts, "error in ", 1);
    csT_warning(ts, where, 1);
    csT_warning(ts, " (", 1);
    csT_warning(ts, msg, 1);
    csT_warning(ts, ")", 0);
}


void csT_free(cs_State *ts, cs_State *thread) {
    XS *xs = fromstate(thread);
    csF_closeupval(thread, thread->stack.p);  /* close all upvalues */
    cs_assert(thread->openupval == NULL);
    csi_userstatefree(ts, thread);
    free_stack(thread);
    csM_free(ts, xs);
}
