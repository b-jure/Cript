#include "crcode.h"
#include "crgc.h"
#include "crlexer.h"
#include "crbits.h"
#include "crlimits.h"
#include "crparser.h"



/* long op */
#define OPL(op)		op##L


/* check if 'ExpInfo' has jumps */
#define hasjumps(e)	((e)->t != (e)->f)



/* 
 * Add line and pc information, skip adding 'LineInfo' if previous
 * entry contained the same line.
 */
static void addlineinfo(FunctionState *fs, Function *f, int line)
{
	int len;

	len = f->lineinfo.len;
	if (len <= 0 || f->lineinfo.ptr[len - 1].line < line) {
		cr_mm_growvec(fs->l->ts, &f->lineinfo);
		f->lineinfo.ptr[len].pc = f->code.len - 1;
		f->lineinfo.ptr[f->lineinfo.len++].line = line;
	}
}


/* write instruction */
int cr_ce_code(FunctionState *fs, Instruction i)
{
	Function *f = fs->fn;

	cr_mm_growvec(fs->l->ts, &f->code);
	f->code.ptr[f->code.len++] = i;
	addlineinfo(fs, f, fs->l->line);
	return f->code.len - 1;
}


/* write short instruction parameter */
static int shortparam(FunctionState *fs, Function *f, Instruction i, cr_ubyte idx)
{
	cr_mm_growvec(fs->l->ts, &f->code);
	f->code.ptr[f->code.len++] = cast_ubyte(idx & 0xff);
	return f->code.len - 1;
}


/* write instruction 'i' and short parameter */
static int shortcode(FunctionState *fs, Instruction i, int idx)
{
	Function *f = fs->fn;
	int offset;

	offset = cr_ce_code(fs, i);
	shortparam(fs, f, i, idx);
	return offset;
}


/* write long instruction parameter */
static int longparam(FunctionState *fs, Function *f, Instruction i, int idx)
{
	cr_mm_ensurevec(fs->l->ts, &f->code, 3);
	f->code.ptr[f->code.len++] = i;
	setbytes(f->code.ptr, idx, 3);
	f->code.len += 3;
	return f->code.len - 3;
}


/* write instruction 'i' and long parameter */
static int longcode(FunctionState *fs, Instruction i, int idx)
{
	Function *f = fs->fn;
	int offset;

	offset = cr_ce_code(fs, i);
	longparam(fs, f, i, idx);
	return offset;
}


/* write instruction and its parameter depending on the parameter size */
int cr_ce_codewparam(FunctionState *fs, Instruction i, int idx)
{
	Function *f = fs->fn;
	int offset;

	offset = cr_ce_code(fs, i);
	cr_assert(idx >= 0);
	if (idx <= CRI_SHRTPARAM)
		shortparam(fs, f, i, cast_ubyte(idx));
	else
		longparam(fs, f, i, idx);
	return offset;
}


/* add constant value to the function */
static int addconstant(FunctionState *fs, TValue *constant)
{
	Function *f;

	f = fs->fn;
	if (ttiso(constant)) {
		cr_assert(ttisstr(constant));
		gcbarrier(ovalue(constant)); /* always marked */
	}
	cr_mm_growvec(fs->l->ts, &f->constants);
	f->constants.ptr[f->constants.len++] = *constant;
	return f->constants.len - 1;
}


/* write OP_CONST instruction with float parameter */
int cr_ce_flt(FunctionState *fs, cr_number n)
{
	TValue vn;
	int idx;

	vn = newfvalue(n);
	idx = addconstant(fs, &vn);
	longcode(fs, OP_CONST, idx);
	return idx;
}


/* write OP_CONST instruction with integer parameter */
int cr_ce_int(FunctionState *fs, cr_integer i)
{
	TValue vi;
	int idx;

	vi = newivalue(i);
	idx = addconstant(fs, &vi);
	longcode(fs, OP_CONST, idx);
	return idx;
}


/* write OP_CONST instruction with string parameter */
int cr_ce_string(FunctionState *fs, OString *str)
{
	TValue vs;
	int idx;

	vs = newovalue(str);
	idx = addconstant(fs, &vs);
	longcode(fs, OP_CONST, idx);
	return idx;
}


/* free stack space */
cr_sinline void freestack(FunctionState *fs, int n)
{
	cr_assert(fs->sp - n >= 0);
	fs->sp -= n;
}


void cr_ce_checkstack(FunctionState *fs, int n)
{
	int newstack;

	newstack = fs->sp + n;
	if (fs->fn->maxstack > newstack) {
		if (cr_unlikely(newstack >= CRI_LONGPARAM))
			cr_lr_syntaxerror(fs->l,
				"function requires too much stack space");
		fs->fn->maxstack = newstack;
	}
}


void cr_ce_reservestack(FunctionState *fs, int n)
{
	cr_ce_checkstack(fs, n);
	fs->sp += n;
}


static int getvar(FunctionState *fs, OpCode op, ExpInfo *e)
{
	cr_assert(op == OP_GETLVAR || op == OP_GETGVAR);
	if (e->u.idx <= CRI_SHRTPARAM)
		return shortcode(fs, op, e->u.idx);
	cr_assert(op + 1 == OP_GETLVARL || op + 1 == OP_GETGVARL);
	return longcode(fs, ++op, e->u.idx);
}


void cr_ce_setoneret(FunctionState *fs, ExpInfo *e)
{
	if (e->et == EXP_CALL) {
		/* already returns a single value */
		cr_assert(GETLPARAMV(getinstruction(fs, e), 0) == 1);
		e->et = EXP_FINEXPR;
		e->u.info = GETLPARAMV(getinstruction(fs, e), 0);
	} else if (e->et == EXP_VARARG) {
		SETLPARAM(getinstruction(fs, e), 1);
		e->et = EXP_FINEXPR;
	}
}


/* emit 'OP_SET' family of instructions */
void cr_ce_storevar(FunctionState *fs, ExpInfo *e)
{
	switch (e->et) {
	case EXP_LOCAL: 
		e->u.info = getvar(fs, OP_SETLVAR, e);
		e->et = EXP_FINEXPR;
		break;
	case EXP_UVAL: 
		e->u.info = longcode(fs, OP_SETUVAL, e->u.info);
		e->et = EXP_FINEXPR;
		break;
	case EXP_GLOBAL: 
		e->u.info = getvar(fs, OP_SETGVAR, e);
		e->et = EXP_FINEXPR;
		break;
	case EXP_INDEXK:
		e->u.info = longcode(fs, OP_SETINDEXK, e->u.idx);
		break;
	case EXP_INDEXRAW:
		e->u.info = longcode(fs, OP_SETPROPERTY, e->u.idx);
		e->et = EXP_FINEXPR;
		break;
	case EXP_INDEXED:
		freestack(fs, 1);
		e->u.info = cr_ce_code(fs, OP_SETINDEX);
		e->et = EXP_FINEXPR;
		break;
	default: 
		cr_unreachable();
		break;
	}
	freestack(fs, 1);
}


/* emit 'OP_GET' family of instructions */
void cr_ce_dischargevar(FunctionState *fs, ExpInfo *e)
{
	switch (e->et) {
	case EXP_LOCAL: 
		e->u.info = getvar(fs, OP_GETLVAR, e);
		e->et = EXP_FINEXPR;
		break;
	case EXP_UVAL: 
		e->u.info = longcode(fs, OP_GETUVAL, e->u.info);
		e->et = EXP_FINEXPR;
		break;
	case EXP_GLOBAL: 
		e->u.info = getvar(fs, OP_GETGVAR, e);
		e->et = EXP_FINEXPR;
		break;
	case EXP_INDEXK:
		freestack(fs, 1);
		e->u.info = longcode(fs, OP_GETINDEXK, e->u.idx);
		break;
	case EXP_INDEXRAW:
		freestack(fs, 1);
		e->u.info = longcode(fs, OP_GETPROPERTY, e->u.idx);
		e->et = EXP_FINEXPR;
		break;
	case EXP_INDEXRAWSUP:
		freestack(fs, 1);
		e->u.info = longcode(fs, OP_GETSUP, e->u.idx);
		e->et = EXP_FINEXPR;
		break;
	case EXP_INDEXSUP:
		freestack(fs, 1);
		e->u.info = longcode(fs, OP_GETSUPIDX, e->u.idx);
		e->et = EXP_FINEXPR;
		break;
	case EXP_INDEXED:
		freestack(fs, 2);
		e->u.info = cr_ce_code(fs, OP_GETINDEX);
		e->et = EXP_FINEXPR;
		break;
	case EXP_CALL: case EXP_VARARG:
		cr_ce_setoneret(fs, e);
		break;
	default: 
		cr_unreachable();
		break;
	}
}
