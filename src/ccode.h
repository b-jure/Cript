/*
** ccode.h
** CScript bytecode and auxiliary functions
** See Copyright Notice in cscript.h
*/

#ifndef CCODE_H
#define CCODE_H


#include "cparser.h"


/* get current pc */
#define currentPC(fs)           ((fs)->pc)


/* get constant of 'ExpInfo' */
#define getconstant(fs,e)       (&(fs)->p->k[(e)->u.info])


/* get pointer to instruction of 'ExpInfo' */
#define getinstruction(fs,e)    (&(fs)->p->code[(e)->u.info])


/* instruction and argument sizes (bytes) */
#define SIZEINSTR           1
#define SIZEARGS            SIZEINSTR
#define SIZEARGL            3

/* instruction and args width */
#define WIDTHINSTR          (SIZEINSTR * 8)
#define WIDTHARGS           (SIZEARGS  * 8)
#define WIDTHARGL           (SIZEARGL  * 8)

/* maximum instruction and arg sizes */
#define INSTRMAX            ((1 << WIDTHINSTR) - 1)
#define SARGMAX             INSTRMAX
#define LARGMAX             ((1 << WIDTHARGL) - 1)
#define CODEMAX             LARGMAX


/* gets first arg pc */
#define GETARG(ip)              ((ip) + SIZEINSTR)

/* get short/long argument pc */
#define GETPC_S(ip,o)           (GETARG(ip) + ((o)*SIZEARGS))
#define GETPC_L(ip,o)           (GETARG(ip) + ((o)*SIZEARGL))


/* get/set short parameter */
#define GETARG_S(ip,o)          cast_ubyte(*GETPC_S(ip,o))
#define SETARG_S(ip,o,v)        setbyte(GETPC_S(ip,0), o, v);
#define SETARG_LS(ip,v)         setbyte(GETARG(ip), SIZEARGL, v)
#define SETARG_LLS(ip,v)        setbyte(GETARG(ip), 2*SIZEARGL, v)


/* get/set long arg */
#define GETARG_L(ip,o)          get3bytes(GETARG(ip) + ((o)*SIZEARGL))
#define SETARG_L(ip,o,v)        set3bytes(GETPC_L(ip,o), v)



/* size of instruction jump argument in bytes */
#define JMPARGSIZE      SIZEARGL

/* max code jump offset value */
#define MAXJMP          LARGMAX

/* value indicating there is no jump */
#define NOJMP           (-1)


/* 
** Grep "ORDER OPR" if you change these enums.
*/
typedef enum {
        /* arithmetic operators */
        OPR_ADD, OPR_SUB, OPR_MUL,
        OPR_DIV, OPR_MOD, OPR_POW,
        /* bitwise operators */
        OPR_SHL, OPR_SHR, OPR_BAND,
        OPR_BOR, OPR_BXOR,
        /* concat operator */
        OPR_CONCAT,
        /* comparison operators */
        OPR_NE, OPR_EQ, OPR_LT,
        OPR_LE, OPR_GT, OPR_GE,
        /* logical operators */
        OPR_AND, OPR_OR,
        OPR_NOBINOPR
} Binopr;


/* true if binary operator 'op' is foldable (it is arithmetic or bitwise) */
#define oprisfoldable(op)      ((op) <= OPR_BXOR)


/* unary operators */
typedef enum { OPR_UNM, OPR_BNOT, OPR_NOT, OPR_NOUNOPR } Unopr;



typedef enum { /* ORDER OP */
/* ------------------------------------------------------------------------
** S - short arg (8-bit)
** L - long arg (24-bit)
** V - stack value
** V{x} - stack value at index 'x'
** K{x} - constant at index 'x'
** I(x) - 'x' is immediate operand
** U{x} - upvalue at index 'x'
** OU{x} - open upvalue at index 'x'
** P{x} - private variable in 'p->private[x]'
** G(x) - global variable from 'globals', key is K{x}:string.
** L{x} - local variable in 'p->locals[x]'
**
** operation     args           description
** ------------------------------------------------------------------------ */
OP_TRUE = 0,/*                'load true constant'                          */
OP_FALSE,/*                   'load false constant'                         */
OP_NIL,/*                     'load nil constant'                           */
OP_NILN,/*         L          'load L nils'                                 */
OP_CONST,/*        S          'load K{S}'                                   */
OP_CONSTL,/*       L          'load K{L}'                                   */
OP_CONSTI,/*       L S        'load integer L (S signedness)'               */
OP_CONSTF,/*       L S        'load integer L as float (S signedness)'      */
OP_VARARGPREP,/*   L          'adjust function varargs (L function arity)'  */
OP_VARARG,/*       L          'load L-1 varargs'                            */
OP_CLOSURE,/*      L          'load closure(Enclosing->fns[L])'             */
OP_NEWARRAY,/*     S          'create and load new array of size 1<<(S-1)'  */
OP_NEWCLASS,/*                'create and load new class'                   */
OP_NEWTABLE,/*     S          'create and load new table of size 1<<(S-1)'  */
OP_METHOD,/*       L V1 V2    'define method V2 for class V1 under key K{L}'*/
OP_SETMM,/*        S V1 V2    'V1->vmt[S] = V2' (see notes)                 */
OP_POP,/*                     'pop value off the stack'                     */
OP_POPN,/*         L          'pop L values off the stack'                  */

OP_MBIN,/*         V1 V2 S    'V1 S V2'  (S is binop)                       */

OP_ADDK,/*         V L S   'V + K{L}:number'                                */
OP_SUBK,/*         V L S   'V - K{L}:number'                                */
OP_MULK,/*         V L S   'V * K{L}:number'                                */
OP_DIVK,/*         V L S   'V / K{L}:number'                                */
OP_MODK,/*         V L S   'V % K{L}:number'                                */
OP_POWK,/*         V L S   'V ** K{L}:number'                               */
OP_BSHLK,/*        V L S   'V << K{L}:number'                               */
OP_BSHRK,/*        V L S   'V >> K{L}:number'                               */
OP_BANDK,/*        V L S   'V & K{L}:number'                                */
OP_BORK,/*         V L S   'V | K{L}:number'                                */
OP_BXORK,/*        V L S   'V ^ K{L}:number'                                */

OP_ADDI,/*         V L S   'V + ((S - 1) * I(L))'                           */
OP_SUBI,/*         V L S   'V - ((S - 1) * I(L))'                           */
OP_MULI,/*         V L S   'V * ((S - 1) * I(L))'                           */
OP_DIVI,/*         V L S   'V / ((S - 1) * I(L))'                           */
OP_MODI,/*         V L S   'V % ((S - 1) * I(L))'                           */
OP_POWI,/*         V L S   'V ** ((S - 1) * I(L))'                          */
OP_BSHLI,/*        V L S   'V << ((S - 1) * I(L))'                          */
OP_BSHRI,/*        V L S   'V >> ((S - 1) * I(L))'                          */
OP_BANDI,/*        V L S   'V & ((S - 1) * I(L))'                           */
OP_BORI,/*         V L S   'V | ((S - 1) * I(L))'                           */
OP_BXORI,/*        V L S   'V ^ ((S - 1) * I(L))'                           */

OP_ADD,/*          V1 V2   'V1 + V2'                                        */
OP_SUB,/*          V1 V2   'V1 - V2'                                        */
OP_MUL,/*          V1 V2   'V1 * V2'                                        */
OP_DIV,/*          V1 V2   'V1 / V2'                                        */
OP_MOD,/*          V1 V2   'V1 % V2'                                        */
OP_POW,/*          V1 V2   'V1 ** V2'                                       */
OP_BSHL,/*         V1 V2   'V1 << V2'                                       */
OP_BSHR,/*         V1 V2   'V1 >> V2'                                       */
OP_BAND,/*         V1 V2   'V1 & V2'                                        */
OP_BOR,/*          V1 V2   'V1 | V2'                                        */
OP_BXOR,/*         V1 V2   'V1 ^ V2'                                        */

OP_CONCAT,/*       V V1    'V .. V1'                                        */

OP_EQK,/*          V L S   '(V == K{L}) == S'                               */

OP_EQI,/*          V L S1 S2      '(V == I(L) * (S1 - 1)) == S2'            */
OP_LTI,/*          V L S1         'V < (S1 - 1) * I(L)'                     */
OP_LEI,/*          V L S1         'V <= (S1 - 1) * I(L)'                    */
OP_GTI,/*          V L S1         'V > (S1 - 1) * I(L)'                     */
OP_GEI,/*          V L S1         'V >= (S1 - 1) * I(L)'                    */ 

OP_EQ,/*           V1 V2 S     '(V1 == V2) == S'                            */
OP_LT,/*           V1 V2       '(V1 < V2)'                                  */
OP_LE,/*           V1 V2       '(V1 <= V2)'                                 */

OP_EQPRESERVE,/*   V1 V2   'V1 == V2 (preserves V1 operand)'                */

OP_NOT,/*          V       'not V'                                          */
OP_UNM,/*          V       '-V'                                             */
OP_BNOT,/*         V       '~V'                                             */

OP_JMP,/*          L       'pc += L'                                        */
OP_JMPS,/*         L       'pc -= L'                                        */

OP_TEST,/*         V L S   'if (!c_isfalse(V) == S) pc += L'                */
OP_TESTORPOP,/*    V L S   'if (!c_isfalse(V) == S) pc += L; else pop V;'   */
OP_TESTANDPOP,/*   V L S   'if (!c_isfalse(V) == S) { pc += L; pop V; }'    */
OP_TESTPOP,/*      V L S   'if (!c_isfalse(V) == S) { pc += L; } pop V;'    */

OP_CALL,/** L1 L2  'V{L1},...,V{L1+L2-1} = V{L1}(V{L1+1},...,V{offsp-1})'
                    (check info)                                            */

OP_CLOSE,/*        L           'close all upvalues >= OU{L}                 */
OP_TBC,/*          L           'mark L{L} as to-be-closed'                  */

OP_GETLOCAL,/*     L           'L{L}'                                       */
OP_SETLOCAL,/*     V L         'L{L} = V'                                   */

OP_GETUVAL,/*      L           'U{L}'                                       */
OP_SETUVAL,/*      V L         'U{L} = V'                                   */

OP_SETARRAY,/*     L S         'V{-S}[L+i] = V{-S+i}, 1 <= i <= S           */

OP_SETPROPERTY,/*  V1 V2 L     'V1.K{L} = V2'                               */
OP_GETPROPERTY,/*  V  L        'V.K{L}'                                     */

OP_GETINDEX,/*     V1 V2       'V1[V2]'                                     */
OP_SETINDEX,/*     V1 V2 V3    'V1[V2] = V3'                                */

OP_GETINDEXSTR,/*  V L         'V[K{L}:string]'                             */
OP_SETINDEXSTR,/*  V1 V2 L     'V1[K{L}:string] = V2'                       */

OP_GETINDEXINT,/*  V L         'V[I(L):integer]'                            */
OP_SETINDEXINT,/*  V1 V2 L     'V[I(L):integer] = V2'                       */

OP_GETSUP,/*       V1 V2 L     'V2:super.K{L}:string' (V1 is instance)      */
OP_GETSUPIDX,/*    V1 V2 V3    'V2:super[V3]' (V1 is instance)              */
OP_GETSUPIDXSTR,/* V1 V2 L     'V2:super[K{L}:string]' (V1 is instance)     */

OP_INHERIT,/*     V1 V2  'V2:class inherit V1                               */
OP_FORPREP,/*     L1 L2  'create upvalue V{L1+3}; pc += L2'                 */
OP_FORCALL,/*     L1 L2  'V{L1+4},...,V{L1+3+L2} = V{L1}(V{L1+1}, V{L1+2});'*/
OP_FORLOOP,/*     L1 L2  'if V{L1+4} != nil { V{L1} = V{L1+2}; pc -= L2 }'  */

OP_RET,/*    L1 L2 S  'return V{L1}, ... ,V{L1+L2-2}' (check notes)         */
} OpCode;


/*
** Notes:
** [OP_SETMM]
** Sets virtual method table entry value at index S.
** 
** [OP_CALL]
** L1 is the base stack offset where the value being called is located.
** L2 is the number of expected results biased with +1.
** If L2-1 == 0, then top is set to last return_result+1.
** 
** [OP_RET]
** L2 is biased with +1, in order to represent multiple returns when the
** number of results is only known during runtime. For example (L2 == 0)
** represents CS_MULRET, in this case we would return all values up to the
** top. S indicates if current function needs to close upvalues or tbc
** variables before returning.
*/


/* number of 'OpCode's */
#define NUM_OPCODES     (OP_RET + 1)


/* OpCode format */
enum OpFormat { 
    FormatI,
    FormatIS,
    FormatISS,
    FormatIL,
    FormatILS,
    FormatILSS,
    FormatILL,
    FormatILLS,
    FormatN,
};


/*
** bits 0-2: format ('OpFormat')
** bit 3: instruction is a test (TProp)
** bit 4: instruction is a jump (JProp)
** bit 5: instruction is metamethod call (MProp)
** bit 6-7: unused
*/
CSI_DEC(const cs_ubyte csC_opProp[NUM_OPCODES];)

#define getOpFormat(p)      (csC_opProp[p] & 0x07)
#define testTProp(p)        (csC_opProp[p] & (1 << 3))
#define testJProp(p)        (csC_opProp[p] & (1 << 4))
#define testMProp(p)        (csC_opProp[p] & (1 << 5))

#define opProp(mm,j,t,f)    (((mm) << 5) | ((j) << 4) | ((t) << 3) | (f))



/* Instruction format sizes in bytes (aka as bytecode) */
CSI_DEC(const cs_ubyte csC_opSize[FormatN];)

#define getOpSize(p)        csC_opSize[getOpFormat(p)]

CSI_DEC(const char *csC_opSizeFormat[FormatN];)

#define getOpSizeFormat(p)  csC_opSizeFormat[getOpFormat(p)]


/* OpCode names table */ 
CSI_DEC(const char *csC_opName[NUM_OPCODES];)

#define getOpName(p)        csC_opName[p]


/* 
** Number of array items to accumulate before a SETARRAY instruction.
** Keep this value under SARGMAX or change the instruction format aka
** the second argument size to long arg in order to fit up to LARGMAX.
*/
#define ARRFIELDS_PER_FLUSH     50


#define csC_setmulret(fs,e)     csC_setreturns(fs, e, CS_MULRET)


CSI_FUNC int csC_emitI(FunctionState *fs, Instruction i);
CSI_FUNC int csC_emitIS(FunctionState *fs, Instruction i, int a);
CSI_FUNC int csC_emitIL(FunctionState *fs, Instruction i, int a);
CSI_FUNC int csC_emitILS(FunctionState *fs, Instruction op, int a, int b);
CSI_FUNC int csC_emitILL(FunctionState *fs, Instruction i, int a, int b);
CSI_FUNC void csC_checkstack(FunctionState *fs, int n);
CSI_FUNC void csC_reserveslots(FunctionState *fs, int n);
CSI_FUNC void csC_setoneret(FunctionState *fs, ExpInfo *e);
CSI_FUNC void csC_setreturns(FunctionState *fs, ExpInfo *e, int nreturns);
CSI_FUNC int csC_nil(FunctionState *fs, int n);
CSI_FUNC int csC_pop(FunctionState *fs, int n);
CSI_FUNC int csC_ret(FunctionState *fs, int base, int nreturns);
CSI_FUNC void csC_method(FunctionState *fs, ExpInfo *e);
CSI_FUNC void csC_storevar(FunctionState *fs, ExpInfo *var);
CSI_FUNC void csC_defineglobal(FunctionState *fs, ExpInfo *e);
CSI_FUNC void csC_setarraysize(FunctionState *fs, int pc, int sz);
CSI_FUNC void csC_setarray(FunctionState *fs, int nelems, int tostore);
CSI_FUNC void csC_settablesize(FunctionState *fs, int pc, int hsize);
CSI_FUNC void csC_varexp2stack(FunctionState *fs, ExpInfo *e);
CSI_FUNC void csC_exp2stack(FunctionState *fs, ExpInfo *e);
CSI_FUNC void csC_getfield(FunctionState *fs, ExpInfo *var,
                              ExpInfo *keystr, int super);
CSI_FUNC void csC_indexed(FunctionState *fs, ExpInfo *var, ExpInfo *key,
                          int super);
CSI_FUNC void csC_unary(FunctionState *fs, ExpInfo *e, Unopr opr);
CSI_FUNC int csC_jmp(FunctionState *fs, OpCode jop);
CSI_FUNC void csC_concatjmp(FunctionState *fs, int *l1, int l2);
CSI_FUNC void csC_patch(FunctionState *fs, int pc, int target);
CSI_FUNC void csC_patchtohere(FunctionState *fs, int pc);
CSI_FUNC int csC_test(FunctionState *fs, OpCode testop, int cond);
CSI_FUNC void csC_prebinary(FunctionState *fs, ExpInfo *e, Binopr op);
CSI_FUNC void csC_binary(FunctionState *fs, ExpInfo *e1, ExpInfo *e2,
                         Binopr opr);
CSI_FUNC void csC_finish(FunctionState *fs);

#endif
