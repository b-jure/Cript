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

#ifndef CRPARSER_H
#define CRPARSER_H


#include "crobject.h"
#include "crlexer.h"
#include "crvalue.h"




/* --------------------------------------------------------------------------
 * Expression info
 * -------------------------------------------------------------------------- */

/* check expression type */
#define eisvar(e)           ((e)->et >= EXP_UVAL && (e)->et <= EXP_DOTSUPER)
#define eisconstant(e)      ((e)->et >= EXP_NIL && (e)->et <= EXP_K)
#define eismulret(e)        ((e)->et == EXP_CALL || (e)->et == EXP_VARARG)
#define eissuper(e)         ((e)->et == EXP_INDEXRAWSUP || (e)->et == EXP_INDEXSUP)
#define eistrue(e)          ((e)->et >= EXP_TRUE && (e)->et <= EXP_K)
#define eisfalse(e)         ((e)->et == EXP_NIL || (e)->et == EXP_FALSE)


/* expression types */
typedef enum expt {
    /* no expression */
    EXP_VOID,
    /* expression is nil constant */
    EXP_NIL,
    /* expression is false constant */
    EXP_FALSE,
    /* expression is true constant */
    EXP_TRUE,
    /* string constant;
     * 'str' = string value; */
    EXP_STRING,
    /* integer constant;
     * 'i' = integer value; */
    EXP_INT,
    /* floating constant;
     * 'n' = floating value; */
    EXP_FLT,
    /* registered constant value;
     * 'info' = index in 'constants'; */
    EXP_K,
    /* upvalue variable;
     * 'info' = index of upvalue in 'upvals'; */
    EXP_UVAL,
    /* local variable;
     * 'info' = stack index; */
    EXP_LOCAL,
    /* static variable;
     * 'info' = 'svars' index; */
    EXP_STATIC,
    /* global variable;
     * 'str' = global id; */
    EXP_GLOBAL,
    /* indexed variable; */
    EXP_INDEXED,
    /* variable indexed with literal string;
     * 'info' = index in 'constants'; */
    EXP_INDEXSTR,
    /* variable indexed with constant integer;
     * 'info' = index in 'constants'; */
    EXP_INDEXINT,
    /* indexed 'super'; */
    EXP_INDEXSUPER,
    /* indexed 'super' with literal string;
     * 'info' = index in 'constants'; */
    EXP_INDEXSUPERSTR,
    /* indexed variable with '.';
     * 'info' = index in 'constants'; */
    EXP_DOT,
    /* indexed 'super' with '.'; */
    EXP_DOTSUPER,
    /* function call;
     * 'info' = pc; */
    EXP_CALL,
    /* vararg expression '...';
     * 'info' = pc; */
    EXP_VARARG,
    /* expression is a jump test;
     * 'info' = pc; */
    EXP_JMP,
    /* finalized expression */
    EXP_FINEXPR,
} expt;


/*
 * Expression information.
 * Parser builds up the expression information and feeds it into
 * functions that generate bytecode.
 * Then those functions also fill the 'ExpInfo' accordingly.
 * So the codegen functions are essentially consumers of 'ExpInfo'.
 */
typedef struct ExpInfo {
    expt et;
    union {
        cr_number n; /* floating constant */
        cr_integer i; /* integer constant  */
        OString *str; /* string literal */
        int info; /* pc or some other generic information */
    } u;
    int t; /* jmp to patch if true */
    int f; /* jmp to patch if false */
} ExpInfo;



/* --------------------------------------------------------------------------
 * Function state
 * -------------------------------------------------------------------------- */

/* variable kind (stored in 'mod') */
#define VARCONST        0 /* constant */
#define VARSTATIC       1 /* static */
#define VARTBC          2 /* to-be-closed */

/* bit mask of all valid modifiers in 'mod' */
#define VARBITMASK      (bit2mask(VARCONST, VARSTATIC) | bitmask(VARTBC))


/* active local variable compiler information */
typedef union LVar {
    struct {
        TValueFields;
        int idx; /* index into 'locals' */
        OString *name;
    } s;
    TValue val; /* constant value */
} LVar;



/* list of jump instructions to patch */
typedef struct BreakList {
    int len;
    int size;
    int *arr;
} PatchList;



/* class declaration information */
typedef struct ClassState {
    struct ClassState *prev;
    cr_ubyte super; /* true if class has superclass */
} ClassState;



/*
 * Dynamic data used by parser.
 * It is stored inside 'Lexer' because there is
 * only one lexer for every 'FunctionState'.
 */
typedef struct ParserState {
    struct {
        int len;
        int size;
        LVar *arr;
    } lvars; /* local vars */
    struct ClassState *cs;
} ParserState;


/* dynamic data context (for optimizations) */
typedef struct DynCtx {
    int loopstart;
    int sp;
    int nfuncs;
    int nk;
    int nstatics;
    int pc;
    int nlinfo;
    int nlocals;
    int nupvals;
    int nbrks;
    int needclose;
} DynCtx;


/* state for currently compiled 'Function' */
typedef struct FunctionState {
    Function *fn; /* current function */
    struct FunctionState *prev; /* implicit linked-list */
    struct Lexer *lx; /* lexer */
    struct Scope *scope; /* scope information */
    struct Scope *loopscope; /* innermost loop scope */
    struct Scope *switchscope; /* innermost switch scope */
    int loopstart; /* innermost loop start offset */
    int sp; /* first free compiler stack index */
    int activelocals; /* number of active local variables */
    int firstlocal; /* index of first local in 'lvars' */
    int nfuncs; /* number of elements in 'funcs' */
    int nk; /* number of elements in 'k' */
    int nstatics; /* number of elements in 'statics' */
    int pc; /* number of elements in 'code' (equialent to 'ncode') */
    int nlinfo; /* number of elements in 'linfo' */
    int nlocals; /* number of elements in 'locals' */
    int nupvals; /* number of elements in 'upvals' */
    int nswscopes; /* number of 'switch' scopes */
    DynCtx deadcode; /* context before "dead" (unreachable) code */
    struct {
        int len; /* number of elements in 'list' */
        int size; /* size of 'list' */
        PatchList *list; /* list of patch lists */
    } patches; /* 2Dlist */
    cr_ubyte needclose; /* true if needs to close upvalues before returning */
    cr_ubyte lastwasret; /* last statement is 'return' */
} FunctionState;



CRI_FUNC cr_noret cr_parser_semerror(Lexer *lx, const char *err);
CRI_FUNC void cr_parser_pparse(cr_State *ts, cr_reader fn, void *userdata,
                               const char *name);

#endif
