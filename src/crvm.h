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

#ifndef CRVM_H
#define CRVM_H

#include "crgc.h"
#include "crchunk.h"
#include "crcommon.h"
#include "crhashtable.h"
#include "crvalue.h"

#include <setjmp.h>



/*
 * --------------
 * Error recovery
 * --------------
 */

/* protected function */
typedef void (*ProtectedFn)(VM *vm, void *userdata);


/* Wrapper around 'jmp_buf' with some additional
 * information, such as status code of the function that
 * was called and a previous cr_longjmp in order to handle
 * nested protected calls.
 * Additionally 'status' gets updated from the callee in
 * case of runtime error, this is the reason why
 * status is marked as volatile to assure the automatic
 * variable is updated and not only put in a register.
 * The whole structure lives on a stack right before calling
 * the C function.
 * Check 'protectedcall' in 'crvm.c' for a reference. */
typedef struct cr_longjmp {
	struct cr_longjmp *prev;
	jmp_buf buf;
	volatile int status;
} cr_longjmp;



/*
 * ---------
 * CallFrame
 * ---------
 */


/* get Cript 'Function' */
#define cffn(cf)	(ascrclosure(s2v((cf)->callee.p))->fn)


/* active function is 'CClosure' */
#define cfisccl(cf)	((cf) && iscclosure((cf)->callee))

/* active function is 'CriptClosure' */
#define cfiscl(cf)	((cf) && isclosure((cf)->callee))

/* active function is 'cr_cfunc' */
#define cfisc(cf)	((cf) && iscfunction((cf)->callee))


/* 'cfstatus' bits */
#define CFcript		0 /* in Cript call */
#define CFccall		1 /* in C call */


/* 'CallFrame' function is Cript function */
#define cfiscript(cf)	(!((cf)->cfstatus & CFccall))


/* function CallFrame */
typedef struct CallFrame {
	SIndex callee;
	SIndex top;
	const Instruction *pc;	/* only for non-C callee */
	int nvarargs;		/* only for non-C callee */
	int nreturns;
	cr_ubyte cfstatus;
} CallFrame;



/*
 * ---------
 * GlobalVar
 * ---------
 */


/* 
 * This is a wrapper around 'Value' with additional 'constant' flag.
 * Local variables do not require this wrapper as they are resolved
 * during compilation. 
 */
typedef struct {
	Value value;
	cr_ubyte constant;
} GlobalVar;



/*
 * -----
 * Hooks
 * -----
 */

typedef struct {
	cr_alloc reallocate; /* allocator */
	void *userdata; /* userdata for allocator */
	cr_reader reader; /* script reader */
	cr_cfunc panic; /* panic handler */
} Hooks; 



/*
 * ---------
 * VM 'Vec's
 * ---------
 */

Vec(GCObjectVec, GCObject*);
Vec(SIndexVec, SIndex);
Vec(GlobalVarVec, GlobalVar);
Vec(OStringVec, OString*);
Vec(CallFrameVec, CallFrame);



/*
 * ---------------
 * Virtual Machine
 * ---------------
 */


/*
 * Extra stack space used mostly when calling overload-able
 * methods. Useful in avoiding stack checks (branching).
 * Only VM stack uses this and nothing else.
 */
#define EXTRA_STACK	5


/* initial stack size */
#define STACKSIZE_INIT	(CR_MINSTACK*4)


/* 
 * Virtual Machine (thread state).
 * Note: As of version 1.0.0 there is no separation
 * of global and thread state.
 * This means that Cript does not have internal support
 * for asynchronous or concurrent code execution.
 */
struct VM {
	int seed; /* initial seed for hashing */
	int status; /* status code */
	SIndex stacktop; /* first free slot in the stack */
	SIndex stackend; /* slot beyond the last valid slot */
	SIndex stack; /* stack base */
	CallFrame *aframe; /* currently active frame in 'frames' */
	CallFrameVec frames; /* nested function call frames */
	SIndexVec callstart; /* start of call values */
	SIndexVec retstart; /* start of return values */
	HashTable gids; /* global variable names */
	GlobalVarVec gvars; /* global variable values */
	ValueVec temp; /* temporary storage for return values TODO*/
	cr_longjmp *errjmp; /* error recovery */
	HashTable weakrefs; /* interned strings (unmarked) */
	OStringVec interned; /* interned strings (marked) */
	Hooks hooks; /* hooks to external code */
	GC gc; /* garbage collector params */
	UValue *openuv; /* unclosed closure values */
	OString *faststatic[SS_N]; /* preallocated static strings */
	OString *memerror; /* preallocated string object for memory errors */
	Value nil; /* nil value */
	Value *gs; /* gray stack */
	cr_umem gslen; /* gs length */
	cr_umem gscap; /* gs capacity */
};


/* boolean static string value */
#define ssvbool(vm, b) \
	OBJ_VAL((b) ? (vm)->faststatic[SS_TRUE] : (vm)->faststatic[SS_FALSE])


/* get static string value with tag 't' */
#ifndef ssv
#define ssv(vm, t) OBJ_VAL((vm)->faststatic[(t)])
#endif


/* 'init' member is set to nil 'Value' if VM is fully initialized */
#define vminitialized(vm) (isnil(&(vm)->nil))


/* Fetch the last call frame (current) */
#define last_frame(vm) ((vm)->frames[(vm)->fc - 1])


/* ensure stack space for 'n' values */
#define ensurestack(vm, n) ValueVec_ensure(&(vm)->stack, n)

/* ensure 'EXTRASTACK' stack space */
#define ensureminstack(vm) (ensurestack(vm, EXTRASTACK))


/* TODO: rewrite */
#define restore_stack(vm, n) cast(Value *, (cast_charp((vm)->stack) + (n)))
#define save_stack(vm, ptr)  (cast_charp(ptr) - cast_charp((vm)->stack))
#define stkpeek(top)	     ((vm)->sp - ((top) + 1))


/* decrement stack pointer */
#define decsp(vm)	((vm)->sp--)
#define pop(vm)		(*--(vm)->sp)
#define popn(vm, n) 	((vm)->sp -= n)

/* increment stack pointer */
#define incsp(vm)	((vm)->sp++)

/* push value/s on the stack */

void push(VM *vm, Value val);
#define pushn(vm, n, val)  { int cnt = (n); while (cnt-- > 0) push(vm, val); }



/* concatenate on stack */
#define vmconcatstk(vm) \
	{ *stkpeek(1) = OBJ_VAL(vmconcat(vm, *stkpeek(1), *stkpeek(0))); decsp(vm); }


void initvm(VM *vm);
void resetvm(VM *vm, int status);

void vminterpret(VM *vm, const char *source, const char *filename);
void vmrun(VM *vm);
void vmcall(VM *vm, SIndex *retstart, SIndex fn, int nreturns);
void vmpcall(VM *vm, ProtectedFn fn, void *userdata, ptrdiff_t oldtop);
cr_ubyte vmcompile(VM *vm, void *userdata, const char *name, int gscope);

void vmcloseupval(VM *vm, SIndex *last);

cr_ubyte vmbindmethod(VM *vm, OClass *oclass, SIndex name, SIndex receiver);

Value vmconcat(VM *vm, SIndex l, SIndex r);

cr_ubyte vmequal(VM *vm, SIndex l, SIndex r);
cr_ubyte vmeqraw(SIndex l, SIndex r);
cr_ubyte vmeq(VM *vm, SIndex l, SIndex r);
cr_ubyte vmne(VM *vm, SIndex l, SIndex r);
cr_ubyte vmlt(VM *vm, SIndex l, SIndex r);
cr_ubyte vmgt(VM *vm, SIndex l, SIndex r);
cr_ubyte vmle(VM *vm, SIndex l, SIndex r);
cr_ubyte vmge(VM *vm, SIndex l, SIndex r);

#endif