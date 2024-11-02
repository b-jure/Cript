/*
** cprotected.h
** Functions for calling functions in protected mode
** See Copyright Notice in cscript.h
*/

#ifndef CRPROTECTED_H
#define CRPROTECTED_H

#include "creader.h"
#include "cscript.h"
#include "climits.h"


/* type for functions with error handler */
typedef void (*ProtectedFn)(cr_State *ts, void *userdata);


CRI_FUNC cr_noret crPR_throw(cr_State *ts, int code);
CRI_FUNC int crPR_close(cr_State *ts, ptrdiff_t level, int status);
CRI_FUNC int crPR_rawcall(cr_State *ts, ProtectedFn fn, void *ud);
CRI_FUNC int crPR_call(cr_State *ts, ProtectedFn fn, void *ud, ptrdiff_t top,
                       ptrdiff_t errfunc);
CRI_FUNC int crPR_parse(cr_State *ts, BuffReader *br, const char *name); 

#endif
