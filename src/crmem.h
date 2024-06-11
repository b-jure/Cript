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

#ifndef CRMEM_H
#define CRMEM_H


#include "crlimits.h"


#define cr_mm_rawmalloc(vm, s)		(vm)->hooks.reallocate(NULL, s, (vm)->hooks.userdata)
#define cr_mm_rawrealloc(vm, p, s)   	(vm)->hooks.reallocate(p, s, (vm)->hooks.userdata)
#define cr_mm_rawfree(vm, p)	   	(vm)->hooks.reallocate(p, 0, (vm)->hooks.userdata)

void *cr_mm_realloc(VM *vm, void *ptr, cr_umem osize, cr_umem nsize);

void *cr_mm_malloc(VM *vm, cr_umem size);
void *cr_mm_saferealloc(VM *vm, void *ptr, cr_umem osize, cr_umem nsize);
void cr_mm_free(VM *vm, void *ptr, cr_umem osize);

void *cr_mm_growarr(VM *vm, void *ptr, int len, int *sizep, int elemsize,
		int ensure, int limit, const char *what);

int cr_mm_reallocstack(VM *vm, int n);
int cr_mm_growstack(VM *vm, int n);


#define cr_mm_newarray(vm,s,t)		cr_mm_malloc(vm, (s) * sizeof(t))

#define cr_mm_reallocarray(vm,p,os,ns) \
	cr_mm_realloc(vm, (p), (os)*sizeof(*p), (ns)*sizeof(ns))

#define cr_mm_freearray(vm,p,n)	\
	cr_mm_free((vm), (p), cast_umem(n)*sizeof(*(p)))



/* declare 'name' Vec */
#define Vec(name, type) \
	typedef struct { \
		type *ptr; \
		int len; \
		int size; \
		cr_umem limit; \
		const char *what; \
	} name;


/* should be called only once */
#define cr_mm_createvec(vm,v,l,w) \
	({ (void)(vm); cr_mm_initvec(vm,v); (v)->limit = (l); (v)->what = (w); })


#define cr_mm_initvec(vm,v) \
	({ (void)(vm); (v)->ptr = NULL; (v)->len = (v)->size = 0; })


#define cr_mm_ensurevec(vm,v,n) \
	((v)->ptr = cr_mm_growarr((vm), (v)->ptr, (v)->len, &(v)->size, \
		sizeof(*(v)->ptr), (n), (v)->limit, (v)->what))


#define cr_mm_growvec(vm,v)	cr_mm_ensurevec((vm), (v), 0)


#define cr_mm_reallocvec(vm,v,ns) \
	((v)->ptr = cr_mm_realloc((vm), (v)->ptr, \
		cast_umem((v)->size*sizeof(*(v)->ptr)), (ns)))


#define freevec(vm,v) \
	cr_mm_freearray((vm), (v)->ptr, (v)->size*sizeof(*(v)->ptr))



/* commonly used 'Vec's */
Vec(ubyteVec, cr_ubyte);
Vec(intVec, int);
Vec(uintVec, int);


#endif
