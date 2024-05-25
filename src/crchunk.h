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

#ifndef CRCHUNK_H
#define CRCHUNK_H

#include "crcommon.h"
#include "crhashtable.h"
#include "crvalue.h"
#include "crvec.h"

/* instruction count */
#define OP_N cast(cr_uint, OP_RET + 1)

/*
 * Instructions/operations (bytecode).
 *
 * All op codes are size of 1 byte.
 * Some operations are 'long' operations and they are
 * indicated by extra 'L' at the end of their name.
 *
 * Long instructions have total size of up to 4 bytes
 * (this is including their arguments).
 * 1 byte instruction + 3 bytes argument = long instruction
 *
 * Short instructions have total size of up to 2 bytes
 * (this is including their arguments).
 * 1 byte instruction + 1 byte argument = short instruction
 */
typedef enum {
	OP_TRUE = 0, /* Push true literal on the stack */
	OP_FALSE, /* Push false literal on the stack */
	OP_NIL, /* Push nil literal on the stack */
	OP_NILN,
	OP_NEG, /* Negate the value on top of the stack */
	OP_ADD, /* [Pop two values of the stack and] add them [pushing the result] */
	OP_SUB, /* -||- subtract them -||- */
	OP_MUL, /* -||- multiply them -||- */
	OP_DIV, /* -||- divide them -||- */
	OP_MOD, /* -||- modulo them -||- */
	OP_POW, /* Binary exponentiation operation (power of 2) */
	OP_NOT, /* Apply logical negation on the value on top of the stack. */
	OP_VALIST, /* '...' literal, variable argument list */
	OP_NEQ, /* [Pop two values of the stack and] check for inequality */
	OP_EQ, /* -||- check for equality */
	OP_EQUAL, /* Check two values for equality, pop only value on top of the stack
            */
	OP_GT, /* [Pop two values of the stack and] check if left greater than
                   right */
	OP_GE, /* -||- check if left greater or equal than right */
	OP_LT, /* -||- check if left is less than right */
	OP_LE, /* -||- check if left is less or equal than right */
	OP_POP, /* Pop the value of the stack */
	OP_POPN, /* Pop 'n' values of the stack */
	OP_CONST, /* Push constant on the stack */
	OP_DEFINE_GLOBAL, /* Pop global value off the stack (8-bit idx) and store it
                         in chunk table for globals. */
	OP_DEFINE_GLOBALL, /* Pop global value off the stack (24-bit idx) and store
                          it in chunk table for globals */
	OP_GET_GLOBAL, /* Push global on the stack */
	OP_GET_GLOBALL, /* Push global on the stack long */
	OP_SET_GLOBAL, /* Set global variable */
	OP_SET_GLOBALL, /* Set global variable long */
	OP_GET_LOCAL, /* Push local variable on the stack */
	OP_GET_LOCALL, /* Push local variable on the stack long */
	OP_SET_LOCAL, /* Set local variable */
	OP_SET_LOCALL, /* Set local variable long */
	OP_JMP_IF_FALSE, /* Conditional jump to instruction */
	OP_JMP_IF_FALSE_POP, /* Jump if false and pop unconditionally */
	OP_JMP_IF_FALSE_OR_POP, /* Conditional jump to instruction or pop stack value
                             */
	OP_JMP_IF_FALSE_AND_POP, /* Conditional jump to instruction and pop stack
                                value */
	OP_JMP, /* Jump to instruction */
	OP_JMP_AND_POP, /* Jump to instruction and pop the value of the stack */
	OP_LOOP, /* Jump backwards unconditionally */
	OP_CALL0, /* Call instruction without arguments */
	OP_CALL1, /* Call instruction with a single argument */
	OP_CALL, /* Call instruction */
	OP_CLOSURE, /* Create a closure */
	OP_GET_UPVALUE, /* Push the upvalue on the stack */
	OP_SET_UPVALUE, /* Set upvalue */
	OP_CLOSE_UPVAL, /* Close upvalue */
	OP_CLOSE_UPVALN, /* Close 'n' upvalues */
	OP_CLASS, /* Push class constant on the stack */
	OP_SET_PROPERTY, /* Set class instance property */
	OP_GET_PROPERTY, /* Get class instance property */
	OP_INDEX, /* Index operator */
	OP_SET_INDEX, /* Set class instance property dynamically */
	OP_METHOD, /* Create class method */
	OP_INVOKE0, /* Invoke class method with no arguments */
	OP_INVOKE1, /* Invoke class method with single argument */
	OP_INVOKE, /* Invoke class method */
	OP_OVERLOAD, /* Overload operator or initializer for a class */
	OP_INHERIT, /* Inherit class properties. */
	OP_GET_SUPER, /* Fetch superclass method */
	OP_INVOKE_SUPER0, /* Invoke superclass method call with no arguments */
	OP_INVOKE_SUPER1, /* Invoke superclass method call with single argument */
	OP_INVOKE_SUPER, /* Invoke superclass method call */
	OP_CALLSTART, /* Start of call arguments */
	OP_RETSTART, /* Start of return values */
	OP_FOREACH, /* Generic for loop */
	OP_FOREACH_PREP, /* Generic for loop stack prep */
	OP_RET0, /* No return value, overrides frame's 'retcnt' */
	OP_RET1, /* Return single value, optimization */
	OP_RET, /* Return from function, generic return */
} OpCode;


VecGeneric(ValueVec, Value);

typedef struct {
	ValueVec constants;
	uintVec lines;
	ubyteVec code;
} Chunk;

void initchunk(Chunk *chunk, VM *vm);
cr_uint writechunk(Chunk *chunk, cr_ubyte byte, cr_uint line);
cr_uint writechunk_codewparam(Chunk *chunk, OpCode code, cr_uint idx, cr_uint line);
cr_uint writechunk_constant(VM *vm, Chunk *chunk, Value value);
void freechunk(Chunk *chunk);

#endif
