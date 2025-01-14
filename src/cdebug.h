/*
** cdebug.h
** Debug and error reporting functions
** See Copyright Notice in cscript.h
*/

#ifndef CDEBUG_H
#define CDEBUG_H


#include "cobject.h"
#include "cstate.h"


#define csD_aritherror(ts,v1,v2) \
    csD_operror(ts, v1, v2, "perform arithmetic operation on")

#define csD_bitwerror(ts,v1,v2) \
    csD_operror(ts, v1, v2, "perform bitwise operation on")


/*
** Mark for entries in 'lineinfo' array that has absolute information in
** 'abslineinfo' array.
*/
#define ABSLINEINFO     (-0x80)


/*
** MAXimum number of successive Instructions WiTHout ABSolute line
** information. (A power of two allows fast divisions.)
*/
#if !defined(MAXIWTHABS)
#define MAXIWTHABS     128
#endif



CSI_FUNC int csD_getfuncline(const Proto *fn, int pc);
CSI_FUNC const char *csD_findlocal(cs_State *ts, CallFrame *cf, int n,
                                   SPtr *pos);
CSI_FUNC const char *csD_addinfo(cs_State *ts, const char *msg, OString *src,
                                 int line);
CSI_FUNC c_noret csD_runerror(cs_State *ts, const char *fmt, ...);
CSI_FUNC c_noret csD_globalerror(cs_State *ts, const char *err, OString *name);
CSI_FUNC c_noret csD_typeerror(cs_State *ts, const TValue *v,
                                const char *op);
CSI_FUNC c_noret csD_ordererror(cs_State *ts, const TValue *v1,
                                 const TValue *v2);
CSI_FUNC c_noret csD_typeerrormeta(cs_State *ts, const TValue *v1,
                                    const TValue *v2, const char * mop);
CSI_FUNC c_noret csD_operror(cs_State *ts, const TValue *v1,
                              const TValue *v2, const char *op);
CSI_FUNC c_noret csD_callerror(cs_State *ts, const TValue *obj);
CSI_FUNC c_noret csD_concaterror(cs_State *ts, const TValue *v1,
                                  const TValue *v2);
CSI_FUNC c_noret csD_indexerror(cs_State *ts, cs_Integer index,
                                 const char *what);
CSI_FUNC c_noret csD_indextypeerror(cs_State *ts, const TValue *index);
CSI_FUNC c_noret csD_errormsg(cs_State *ts);

#endif
