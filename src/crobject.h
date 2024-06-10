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
	OBJ_CLOSURE,	/* 'Closure' */
	OBJ_CRLOSURE,	/* 'CriptClosure' */
	OBJ_CCLOSURE,	/* 'CClosure' */
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



/* 
 * ---------------------------------------------------------------------------
 * OString 
 * ---------------------------------------------------------------------------
 */

typedef struct OString {
	ObjectHeader;
	int len; /* excluding null terminator */
	cr_ubyte hashash;
	unsigned int hash;
	char bytes[];
} OString;


#define CR_VSTRING	makevariant(CR_TSTRING, 0)

#define ttisstr(v)	isott((v), CR_VSTRING)
#define strvalue(v)	((OString*)ovalue(v))
#define cstrvalue(v)	(strvalue(v)->bytes)

/* string is equal to string literal */
#define streqlit(s,lit,l,h) \
	((s)->len == (l) && (s)->hash == (h) && \
	 memcmp((s)->bytes, (lit), (l)) == 0)



/* 
 * ---------------------------------------------------------------------------
 * UValue 
 * ---------------------------------------------------------------------------
 */

typedef struct UValue {
	ObjectHeader;
	TValue closed; /* value T*/
	TValue *location; /* stack or 'closed' */
	struct UValue *nextuv; /* chain */
} UValue;


#define CR_VUVALUE	makevariant(CR_TUVALUE, 0)

#define ttisuval(o)	isott((v), CR_VUVALUE)
#define uvvalue(v)	((UValue *)ovalue(v))



/*
 * ---------------------------------------------------------------------------
 * Function
 * ---------------------------------------------------------------------------
 */

/* line information and associated instruction */
typedef struct LineInfo {
	int pc;
	int line;
} LineInfo;


Vec(LineInfoVec, LineInfo);


/* 'code' array */
typedef ubyteVec InstructionVec;


typedef struct Function {
	ObjectHeader;
	OString *name; /* function name */
	OString *source; /* source name */
	TValueVec constants;
	LineInfoVec lineinfo;
	InstructionVec code;
	int arity; /* number of arguments */
	int defline; /* function definition line */
	int deflastline; /* function definition end line */
	cr_ubyte isvararg; /* set if contains '...' */
} Function;


#define CR_VFUNCTION	makevariant(CR_TFUNCTION, 0)

#define ttisfn(v)	isott((v), CR_VFUNCTION)
#define fnvalue(v)	((Function *)asobj(v))




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



typedef struct {
	ClosureHeader;
	cr_cfunc fn;
	TValue upvalue[1];
} CClosure;

#define ttisccl(v)		isott((v), CR_VCCL)
#define cclvalue(v)		((CClosure*)ovalue(v))

/* 'cl' is not a 'CriptClosure' */
#define noCriptclosure(cl)	((cl) == NULL || (cl)->cc.ott != CR_VCRCL)



typedef union Closure {
	CClosure cc;
	CriptClosure crc;
} Closure;

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
	GCObject *vtable[CR_MN]; /* overloadable methods */
} OClass;


#define CR_VCLASS	makevariant(CR_TCLASS, 0)

#define ttiscls(v)	isott((v), CR_VCLASS)
#define clsvalue(v)	((OClass*)ovalue(v))



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


/*
 * ---------------------------------------------------------------------------
 *  InstanceMethod
 * ---------------------------------------------------------------------------
 */

#define ismethod(v)	isot(v, OBJ_BOUND_METHOD)
#define asmethod(v) 	((InstanceMethod *)asobj(v))


/* method bound to 'receiver' (Instance) */
typedef struct InstanceMethod {
	ObjectHeader;
	Instance *receiver;
	GCObject *method;
} InstanceMethod;



#define CR_VINSTANCE	makevariant(CR_TINSTANCE, 0)

#define ttisins(v)	isott((v), CR_VINSTANCE)
#define insvalue(v)	((Instance*)ovalue(v))

/* --------------------------------------------------------------------------- */


/* Array holding return count and arity for each overload-able method. */
// extern const struct Tuple ominfo[];


#define cr_ot_newstringlit(vm, lit)	OString_new((vm), (lit), SLL(lit))

void cr_ot_sourceid(char *adest, const char *src, size_t len);
const char *cr_ot_pushfstring(VM *vm, const char *fmt, ...);
int cr_ot_eqstring(OString *s1, OString *s2);
int32_t cr_ot_id2mtag(VM *vm, OString *id);
OString *cr_ot_newstring(VM *vm, const char *chars, size_t len);
OString *cr_ot_newvstringf(VM *vm, const char *fmt, va_list argp);
OString *cr_ot_newstringf(VM *vm, const char *fmt, ...);
OString *cr_ot_concatenate(VM *vm, GCObject* a, GCObject* b);
InstanceMethod *cr_ot_newinstancemethod(VM *vm, TValue receiver, CriptClosure *method);
Instance *cr_ot_newinstance(VM *vm, OClass *cclass);
OClass *cr_ot_newclass(VM *vm, OString *name);
UValue *cr_ot_newuvalue(VM *vm, TValue *var_ref);
CriptClosure *cr_ot_newcrclosure(VM *vm, Function *fn);
CClosure *cr_ot_newcclosure(VM *vm, OString *name, cr_cfunc fn, int32_t arity, cr_ubyte isvararg, int upvals);
Function *cr_ot_newfunction(VM *vm);
cr_ubyte cr_ot_vtcall(VM *vm, TValue instance, int tag);
cr_ubyte cr_ot_rawindex(VM *vm, TValue instance, cr_ubyte get);
void otypeprint(OType type);
OString *cr_ot_tostr(VM *vm, GCObject *o, cr_ubyte raw);
void cr_ot_tryop(VM *vm, TValue a, TValue b, int op, TValue *res);
void oprint(VM *vm, TValue value, cr_ubyte raw, FILE *stream);
void oeq(VM *vm, TValue l, TValue r);
void one(VM *vm, TValue l, TValue r);
void olt(VM *vm, TValue l, TValue r);
void ogt(VM *vm, TValue l, TValue r);
void ole(VM *vm, TValue l, TValue r);
void oge(VM *vm, TValue l, TValue r);
void cr_ot_free(VM *vm, GCObject *object);

#endif
