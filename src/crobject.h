/* ----------------------------------------------------------------------------------------------
 * Copyright (C) 2023-2024 Jure Bagić
 *
 * This file is part of cript.
 * cript is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * cript is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with cript.
 * If not, see <https://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------------------------*/

#ifndef CROBJECT_H
#define CROBJECT_H


#include "crhash.h"
#include "crhashtable.h"
#include "crmem.h"
#include "cript.h"
#include "crvalue.h"


/* object types */
typedef enum {
	OBJ_STRING = 0,
	OBJ_FUNCTION,
	OBJ_CLOSURE,
	OBJ_CRLOSURE,
	OBJ_CCLOSURE,
	OBJ_UVAL,
	OBJ_CLASS,
	OBJ_INSTANCE,
	OBJ_BOUND_METHOD,
} OType;



/* common header for objects */
#define ObjectHeader	struct GCObject* next; cr_ubyte ott; cr_ubyte mark


/* common type for collectable objects */
typedef struct GCObject {
	ObjectHeader;
} GCObject;


#define rawott(o)	((o)->ott)
#define rawomark(o)	((o)->mark)

#define ott(v)		(rawott(ovalue(v)))
#define omark(v)	(rawomark(ovalue(v)))

#define setott(v,t)	(ott(v) = (t))
#define isott(v,t)	(ott(v) == (t))


/* set value to GC object */
#define setv2o(ts,v,o,t) \
	{ TValue *v_=(v); t *o_=o; ovalue(v_) = cast(GCObject*,o_); }


/* set stack value to GC object */
#define setsv2o(ts,sv,o,t)	setv2o(ts,s2v(sv),o,t)


/* cast object to gc object */
#define objtogco(o)	cast(GCObject*, o)



/* 
 * ---------------------------------------------------------------------------
 * OString 
 * ---------------------------------------------------------------------------
 */

typedef struct OString {
	ObjectHeader;
	cr_ubyte extra; /* extra information */
	cr_ubyte bits; /* useful bits */
	int len; /* excluding null terminator */
	unsigned int hash;
	char bytes[];
} OString;


#define CR_VSTRING	makevariant(CR_TSTRING, 0)

#define ttisstr(v)	isott((v), CR_VSTRING)
#define strvalue(v)	((OString*)ovalue(v))
#define cstrvalue(v)	(strvalue(v)->bytes)


/* set value to string */
#define setv2s(ts,v,s)		setv2o(ts,v,s,OString)

/* set stack value to string */
#define setsv2s(ts,sv,s)	setv2s(ts,s2v(sv),s)


/* check equality between string and string literal */
#define streqlit(s,lit,l,h) \
	((s)->len == (l) && (s)->hash == (h) && \
	 memcmp((s)->bytes, (lit), (l)) == 0)


/* size of string */
#define sizes(s)	(sizeof(OString) + (s)->len + 1)


/* bits for string 'bits' :) */
#define STRHASHASH		(1 << 0) /* string has hash */
#define STRUSRINTERNED		(1 << 1) /* string is user interned */
#define STRINTERNED		(1 << 2) /* string is interned */
#define STRKEYWORD		(1 << 3) /* string is keyword */
#define STRVTMETHOD		(1 << 4) /* string is vtable method */

/* test 'bits' */
#define hashash(s)		((s) && ((s)->bits & STRHASHASH))
#define isusrinterned(s)	((s) && ((s)->bits & (STRUSRINTERNED | STRHASHASH)))
#define isinterned(s)		((s) && ((s)->bits & (STRINTERNED | STRHASHASH)))
#define iskeyword(s)		((s) && ((s)->bits & (STRHASHASH | STRKEYWORD)))
#define isvtmethod(s)		((s) && ((s)->bits & (STRHASHASH | STRVTMETHOD)))



/* 
 * ---------------------------------------------------------------------------
 * UValue  (upvalue)
 * ---------------------------------------------------------------------------
 */

typedef struct UValue {
	ObjectHeader;
	union {
		TValue *location; /* stack or 'closed' */
		ptrdiff_t offset; /* when reallocating stack */
	} v;
	union {
		struct { /* when open (parsing) */
			struct UValue *nextuv;
			struct UValue *prevuv;
		} open;
		TValue value; /* value stored here when closed */
	} u;
} UValue;


#define CR_VUVALUE	makevariant(CR_TUVALUE, 0)

#define ttisuval(o)	isott((v), CR_VUVALUE)
#define uvvalue(v)	((UValue *)ovalue(v))


/* set value to upvalue */
#define setv2uv(ts,v,uv)	setv2o(ts,v,uv,UValue)

/* set stack value to upvalue */
#define setsv2uv(ts,sv,uv)	setv2uv(ts,s2v(sv),uv)


/* size of upvalue */
#define sizeuv()	sizeof(UValue)



/*
 * ---------------------------------------------------------------------------
 * Function
 * ---------------------------------------------------------------------------
 */


/* upvalue variable debug information */
typedef struct UVInfo {
	OString *name;
	int idx; /* index in stack or outer function local var list */
	cr_ubyte onstack; /* is it on stack */
	cr_ubyte mod; /* type of corresponding variable */
} UVInfo;

Vec(UVInfoVec, UVInfo);



/* Local variable debug information */
typedef struct LVar {
	OString *name;
	int alivepc; /* point where variable is in scope */
	int deadpc; /* point where variable is out of scope */
} LVar;

Vec(LVarVec, LVar);



/* line information and associated instruction */
typedef struct LineInfo {
	int pc;
	int line;
} LineInfo;


Vec(LineInfoVec, LineInfo);


/* 'code' array */
typedef ubyteVec InstructionVec;


/* Cript chunk */
typedef struct Function {
	ObjectHeader;
	int maxstack; /* max stack size for this function */
	OString *name; /* function name */
	OString *source; /* source name */
	TValueVec constants; /* constant values */
	InstructionVec code; /* bytecode */
	LineInfoVec lineinfo; /* line info for instructions */
	LVarVec lvars; /* debug information for local variables */
	UVInfoVec upvalues; /* debug information for upvalues */
	int arity; /* number of arguments */
	int defline; /* function definition line */
	int deflastline; /* function definition end line */
	cr_ubyte isvararg; /* true if function takes vararg */
} Function;


#define CR_VFUNCTION	makevariant(CR_TFUNCTION, 0)

#define ttisfn(v)	isott((v), CR_VFUNCTION)
#define fnvalue(v)	((Function *)asobj(v))


/* set value to function */
#define setv2fn(ts,v,fn)	setv2o(ts,v,fn,Function)

/* set stack value to upvalue */
#define setsv2fn(ts,sv,fn)	setv2fn(ts,s2v(sv),fn)

/* size of function */
#define sizefn()	sizeof(Function)



/* 
 * ---------------------------------------------------------------------------
 * Closures
 * ---------------------------------------------------------------------------
 */


#define CR_VCRCL	makevariant(CR_TFUNCTION, 1) /* 'CriptClosure' */
#define CR_VCCL		makevariant(CR_TFUNCTION, 2) /* 'CClosure' */



/* common closure header */
#define ClosureHeader	ObjectHeader; int nupvalues;


typedef struct CriptClosure {
	ClosureHeader;
	Function *fn;
	UValue *upvalue[1];
} CriptClosure;

#define ttiscrcl(v)		isott((v), CR_VCRCL)
#define crclvalue(v)		((CriptClosure*)ovalue(v))

/* set value to cript closure */
#define setv2crcl(ts,v,crcl)		setv2o(ts,v,crcl,CriptClosure)

/* set stack value to cript closure */
#define setsv2crcl(ts,sv,crcl)		setv2crcl(ts,s2v(sv),crcl)

/* size of cript closure */
#define sizecrcl(crcl) \
	(sizeof(CriptClosure) + (crcl)->nupvalues * sizeof(UValue*))



typedef struct {
	ClosureHeader;
	cr_cfunc fn;
	TValue upvalue[1];
} CClosure;

#define ttisccl(v)		isott((v), CR_VCCL)
#define cclvalue(v)		((CClosure*)ovalue(v))

/* set value to C closure */
#define setv2ccl(ts,v,ccl)	setv2o(ts,v,ccl,CClosure)

/* set stack value to C closure */
#define setsv2ccl(ts,sv,ccl)	setv2ccl(ts,s2v(sv),ccl)

/* size of C closure */
#define sizeccl(ccl) \
	(sizeof(CClosure) + (ccl)->nupvalues * sizeof(TValue))

/* 'cl' is not a 'CriptClosure' */
#define noCriptclosure(cl)	((cl) == NULL || (cl)->cc.ott != CR_VCRCL)



typedef union Closure {
	CClosure cc;
	CriptClosure crc;
} Closure;

/* set value to closure */
#define setv2cl(ts,v,cl)	setv2o(ts,v,cl,Closure)

/* set stack value to closure */
#define setsv2cl(ts,sv,cl)	setv2cl(ts,s2v(sv),cl)

#define ttiscl(v)	(ttisccl(v) || ttiscrcl(v))
#define clvalue(v)	((Closure*)ovalue(v))




/* 
 * ---------------------------------------------------------------------------
 * OClass 
 * ---------------------------------------------------------------------------
 */

typedef struct OClass {
	ObjectHeader;
	OString *name; /* class name */
	HTable mtab; /* method table */
	GCObject *vtable[CR_NUMM]; /* overloadable methods */
} OClass;


#define CR_VCLASS	makevariant(CR_TCLASS, 0)

#define ttiscls(v)	isott((v), CR_VCLASS)
#define clsvalue(v)	((OClass*)ovalue(v))


/* set value to class */
#define setv2cls(ts,v,cls)	setv2o(ts,v,cls,OClass)

/* set stack value to class */
#define setsv2cls(ts,sv,cls)	setv2cls(ts,s2v(sv),cls)

/* size of class */
#define sizecls()	sizeof(OClass)



/*
 * ---------------------------------------------------------------------------
 *  Instance
 * ---------------------------------------------------------------------------
 */


/* 'OClass' instance */
typedef struct Instance {
	ObjectHeader;
	OClass *oclass; /* pointer to class */
	HTable fields; /* instance fields */
} Instance;


#define CR_VINSTANCE	makevariant(CR_TINSTANCE, 0)

#define ttisins(v)	isott((v), CR_VINSTANCE)
#define insvalue(v)	((Instance*)ovalue(v))


/* set value to instance */
#define setv2ins(ts,v,ins)	setv2o(ts,v,ins,Instance)

/* set stack value to instance */
#define setsv2ins(ts,sv,ins)	setv2ins(ts,s2v(sv),ins)

/* size of instance */
#define sizeins()	sizeof(Instance)



/*
 * ---------------------------------------------------------------------------
 *  InstanceMethod
 * ---------------------------------------------------------------------------
 */

/* method bound to 'receiver' (Instance) */
typedef struct InstanceMethod {
	ObjectHeader;
	Instance *receiver;
	GCObject *method;
} InstanceMethod;

#define CR_VMETHOD	makevariant(3, CR_TFUNCTION)

#define ttisim(v)	isott((v), CR_VMETHOD)
#define imvalue(v)	((InstanceMethod*)ovalue(v))


/* set value to instance method */
#define setv2im(ts,v,im)		setv2o(ts,v,im,InstanceMethod)

/* set stack value to instance method */
#define setsv2im(ts,sv,im)		setv2im(ts,s2v(sv),im)

/* size of instance method */
#define sizeim()	sizeof(InstanceMethod)

/* --------------------------------------------------------------------------- */


/* get vtable method info */
#define vtmi(mt)	(&vtmethodinfo[(mt)])

typedef struct Tuple {
	int arity;
	int nreturns;
} Tuple;

/* array of tuples for vtable method */
extern const Tuple vtmethodinfo[CR_NUMM];


void cr_ob_sourceid(char *adest, const char *src, size_t len);
int cr_ob_strtomt(TState *ts, OString *id);
OString *cr_ob_newstring(TState *ts, const char *chars, size_t len);
int cr_ob_hexvalue(int c);
void cr_ob_numtostring(TState *ts, TValue *v);
size_t cr_ob_strtonum(const char *s, TValue *o, int *of);
const char *cr_ob_pushvfstring(TState *ts, const char *fmt, va_list argp);
const char *cr_ob_pushfstring(TState *ts, const char *fmt, ...);
CClosure *cr_ob_newcclosure(TState *ts, cr_cfunc fn, int nupvalues);
Function *cr_ob_newfunction(TState *ts);;
CriptClosure *cr_ob_newcrclosure(TState *ts, Function *fn, int nupvalues);
UValue *cr_ob_newuvalue(TState *ts, TValue *vp);
OClass *cr_ob_newclass(TState *ts, OString *id);
Instance *cr_ob_newinstance(TState *ts, OClass *cls);
InstanceMethod *cr_ob_newinstancemethod(TState *ts, Instance *receiver, CriptClosure *method);
void cr_ob_free(TState *ts, GCObject *o);

#endif
