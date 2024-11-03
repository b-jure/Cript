/*
** cjmptable.h
** Jump Table for CScript Interpreter
** See Copyright Notice in cscript.h
*/

#ifndef CRJMPTABLE_H
#define CRJMPTABLE_H


#undef vm_dispatch
#undef vm_case
#undef vm_break

#define vm_dispatch(x)      goto *jmptable[x];
#define vm_case(label)      L_##label:
#define vm_break            vm_dispatch(fetch())


/* Make sure the order is the same as in the OpCode enum */
static const void *const jmptable[NUM_OPCODES] = {
    &&L_OP_TRUE,
    &&L_OP_FALSE,
    &&L_OP_NIL,
    &&L_OP_NILN,
    &&L_OP_CONST,
    &&L_OP_CONSTL,
    &&L_OP_CONSTI,
    &&L_OP_CONSTF,
    &&L_OP_VARARGPREP,
    &&L_OP_VARARG,
    &&L_OP_CLOSURE,
    &&L_OP_ARRAY,
    &&L_OP_ARRAYELEMS,
    &&L_OP_CLASS,
    &&L_OP_METHOD,
    &&L_OP_SETMM,
    &&L_OP_POP,
    &&L_OP_POPN,
    &&L_OP_MBIN,
    &&L_OP_ADDK,
    &&L_OP_SUBK,
    &&L_OP_MULK,
    &&L_OP_DIVK,
    &&L_OP_MODK,
    &&L_OP_POWK,
    &&L_OP_BSHLK,
    &&L_OP_BSHRK,
    &&L_OP_BANDK,
    &&L_OP_BORK,
    &&L_OP_BXORK,
    &&L_OP_ADDI,
    &&L_OP_SUBI,
    &&L_OP_MULI,
    &&L_OP_DIVI,
    &&L_OP_MODI,
    &&L_OP_POWI,
    &&L_OP_BSHLI,
    &&L_OP_BSHRI,
    &&L_OP_BANDI,
    &&L_OP_BORI,
    &&L_OP_BXORI,
    &&L_OP_ADD,
    &&L_OP_SUB,
    &&L_OP_MUL,
    &&L_OP_DIV,
    &&L_OP_MOD,
    &&L_OP_POW,
    &&L_OP_BSHL,
    &&L_OP_BSHR,
    &&L_OP_BAND,
    &&L_OP_BOR,
    &&L_OP_BXOR,
    &&L_OP_CONCAT,
    &&L_OP_EQK,
    &&L_OP_EQI,
    &&L_OP_LTI,
    &&L_OP_LEI,
    &&L_OP_GTI,
    &&L_OP_GEI,
    &&L_OP_EQ,
    &&L_OP_LT,
    &&L_OP_LE,
    &&L_OP_NOT,
    &&L_OP_UNM,
    &&L_OP_BNOT,
    &&L_OP_EQPRESERVE,
    &&L_OP_JMP,
    &&L_OP_JMPS,
    &&L_OP_TEST,
    &&L_OP_TESTORPOP,
    &&L_OP_TESTANDPOP,
    &&L_OP_TESTPOP,
    &&L_OP_CALL,
    &&L_OP_CLOSE,
    &&L_OP_TBC,
    &&L_OP_GETLOCAL,
    &&L_OP_SETLOCAL,
    &&L_OP_GETPRIVATE,
    &&L_OP_SETPRIVATE,
    &&L_OP_GETUVAL,
    &&L_OP_SETUVAL,
    &&L_OP_DEFGLOBAL,
    &&L_OP_GETGLOBAL,
    &&L_OP_SETGLOBAL,
    &&L_OP_SETPROPERTY,
    &&L_OP_GETPROPERTY,
    &&L_OP_GETINDEX,
    &&L_OP_SETINDEX,
    &&L_OP_GETINDEXSTR,
    &&L_OP_SETINDEXSTR,
    &&L_OP_GETINDEXINT,
    &&L_OP_SETINDEXINT,
    &&L_OP_GETSUP,
    &&L_OP_GETSUPIDX,
    &&L_OP_GETSUPIDXSTR,
    &&L_OP_INHERIT,
    &&L_OP_FORPREP,
    &&L_OP_FORCALL,
    &&L_OP_FORLOOP,
    &&L_OP_RET,
};

#endif
