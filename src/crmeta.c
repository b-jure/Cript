#include "crmeta.h"
#include "crstring.h"
#include "crdebug.h"
#include "crstate.h"
#include "crhashtable.h"
#include "crobject.h"
#include "crgc.h"
#include "crvm.h"
#include "crmem.h"


void crMM_init(cr_State *ts) {
    static const char *vmtnames[CR_NUM_MM] = {
        "__init", "__tostring", "__getidx", "__setidx",
        "__gc", "__close", "__add", "__sub", "__mul",
        "__div", "__mod", "__pow", "__not", "__bnot",
        "__shl", "__shr", "__band", "__bor", "__xor",
        "__eq", "__lt", "__le",
    };
    for (int i = 0; i < CR_NUM_MM; i++) {
        OString *s = crS_new(ts, vmtnames[i]);
        s->bits = bitmask(STRVMTBIT);
        s->extra = i;
        G_(ts)->vmtnames[i] = s;
        crG_fix(ts, obj2gco(G_(ts)->vmtnames[i]));
    }
}


OClass *crMM_newclass(cr_State *ts) {
    OClass *cls = crG_new(ts, sizeof(OClass), CR_VCLASS, OClass);
    cls->methods = NULL;
    cls->vmt = NULL;
    cls->methods = NULL;
    return cls;
}


Instance *crMM_newinstance(cr_State *ts, OClass *cls) {
    Instance *ins = crG_new(ts, sizeof(Instance), CR_VINSTANCE, Instance);
    ins->oclass = cls;
    ins->fields = NULL;
    setins2s(ts, ts->sp.p++, ins);
    ins->fields = crH_new(ts);
    ts->sp.p--;
    return ins;
}


InstanceMethod *crMM_newinstancemethod(cr_State *ts, Instance *receiver,
                                       CrClosure *method)
{
    InstanceMethod *im = crG_new(ts, sizeof(InstanceMethod), CR_VMETHOD,
                                   InstanceMethod);
    im->receiver = receiver;
    im->method = obj2gco(method);
    return im;
}


/* TODO: move to 'crapi' */
// static void parsevmt(cr_State *ts, TValue *actual, cr_VMT *vmt) {
//     for (int i = 0; i < CR_NUM_MM; i++) {
//         switch (vmt->methods[i].mtt) {
//         case CR_METAT_CFUNCTION: {
//             // TODO
//         }
//         case CR_METAT_INDEX: {
//             // TODO
//         }
//         case CR_METAT_NONE: default:
//             setnilval(&actual[i]);
//             break;
//         }
//     }
// }


UserData *crMM_newuserdata(cr_State *ts, size_t size, int nuv) {
    UserData *ud = crG_new(ts, sizeofud(nuv, size), CR_VUDATA, UserData);
    ud->vmt = NULL;
    ud->nuv = nuv;
    ud->size = size;
    return ud;
}


/* get method 'mm' */
const TValue *crMM_get(cr_State *ts, const TValue *v, cr_MM mm) {
    UNUSED(ts);
    cr_assert(CR_MM_INIT <= mt && mt < CR_NUM_MM);
    switch (ttypetag(v)) {
    case CR_VCLASS: return &gco2cls(v)->vmt[mm];
    case CR_VUDATA: return &gco2ud(v)->vmt[mm];
    default: break; /* try basic type */
    } /* FALLTHRU */
    TValue *ht = G_(ts)->vmt[ttype(v)];
    return (ht ? &ht[mm] : &G_(ts)->nil);
}


/* call binary method and store the result in 'res' */
void crMM_callbinres(cr_State *ts, const TValue *fn, const TValue *selfarg,
                     const TValue *v1, const TValue *v2, SPtr res)
{
    /* assuming EXTRA_STACK */
    ptrdiff_t result = savestack(ts, res);
    SPtr func = ts->sp.p;
    setobj2s(ts, func, fn); /* push function */
    setobj2s(ts, func + 1, selfarg); /* 'self' */
    setobj2s(ts, func + 2, v1); /* 1st arg */
    setobj2s(ts, func + 3, v2); /* 2nd arg */
    ts->sp.p += 4;
    crV_call(ts, func, 1);
    res = restorestack(ts, result);
    setobj2s(ts, res, s2v(--ts->sp.p));
}


static int callbinaux(cr_State *ts, const TValue *v1, const TValue *v2,
                      SPtr res, int mt)
{
    const TValue *selfarg = v1;
    const TValue *fn = crMM_get(ts, v1, mt);
    if (ttisnil(fn)) {
        selfarg = v2;
        fn = crMM_get(ts, v2, mt);
        if (cr_unlikely(ttisnil(fn)))
            return 0;
    }
    crMM_callbinres(ts, fn, selfarg, v1, v2, res);
    return 1;
}


/* try to call binary method */
void crMM_trybin(cr_State *ts, const TValue *v1, const TValue *v2, SPtr res,
                  cr_MM mm)
{
    if (cr_unlikely(ttypetag(v1) != ttypetag(v2) /* types don't match */
                || !callbinaux(ts, v1, v2, res, mm))) { /* or no method ? */
        switch (mm) {
        case CR_MM_BNOT: case CR_MM_BSHL: case CR_MM_BSHR:
        case CR_MM_BAND: case CR_MM_BOR: case CR_MM_BXOR:
            crD_aritherror(ts, v1, v2);
            /* UNREACHED */
        default:
            crD_bitwerror(ts, v1, v2);
            /* UNREACHED */
        }
    }
}


/* call unary method and store result in 'res' */
void crMM_callunaryres(cr_State *ts, const TValue *fn, const TValue *v,
                       SPtr res)
{
    ptrdiff_t result = savestack(ts, res);
    SPtr func = ts->sp.p;
    setobj2s(ts, func, fn); /* push function */
    setobj2s(ts, func + 1, v); /* 'self' */
    res = restorestack(ts, result);
    setobj2s(ts, res, s2v(--ts->sp.p));
}


static int callunaryaux(cr_State *ts, const TValue *v, SPtr res, int mt)
{
    const TValue *fn = crMM_get(ts, v, mt);
    if (ttisnil(fn)) return 0;
    crMM_callunaryres(ts, fn, v, res);
    return 1;
}


/* try to call unary method */
void crMM_tryunary(cr_State *ts, const TValue *v, SPtr res, cr_MM mm) {
    if (cr_unlikely(!callunaryaux(ts, v, res, mm))) {
        switch (mm) {
        case CR_MM_BNOT: {
            crD_bitwerror(ts, v, v);
            break; /* UNREACHED */
        }
        case CR_MM_UNM: {
            crD_aritherror(ts, v, v);
            break; /* UNREACHED */
        }
        default: cr_unreachable(); break;
        }
    }
}


/* call order method */
int crMM_order(cr_State *ts, const TValue *v1, const TValue *v2, cr_MM mm) {
    cr_assert(CR_MM_EQ <= mt && mt <= CR_NUM_LE);
    if (cr_likely(callbinaux(ts, v1, v2, ts->sp.p, mm)))
        return cri_isfalse(s2v(ts->sp.p));
    crD_ordererror(ts, v1, v2);
    /* UNREACHED */
    return 0;
}


/*
** Same as 'crMM_order' except the second operand is an
** immediate value.
*/
int crMM_orderI(cr_State *ts, const TValue *v1, int v2, int flip, int isflt,
                cr_MM mm)
{
    const TValue *v2_;
    TValue aux;
    if (isflt) {
        setfval(&aux, cast_num(v2));
    } else
        setival(&aux, v2);
    v2_ = (flip ? v1 : &aux);
    return crMM_order(ts, v1, v2_, mm);
}


void crMM_freeclass(cr_State *ts, OClass *cls) {
    if (cls->vmt)
        crM_free(ts, cls->vmt, SIZEVMT);
    crH_free(ts, cls->methods);
    crM_free(ts, cls, sizeof(*cls));
}


void crMM_freeinstance(cr_State *ts, Instance *ins) {
    crH_free(ts, ins->fields);
    crM_free(ts, ins, sizeof(*ins));
}


void crMM_freeuserdata(cr_State *ts, UserData *ud) {
    if (ud->vmt)
        crM_free(ts, ud->vmt, SIZEVMT);
    crM_free(ts, ud, sizeofud(ud->nuv, ud->size));
}
