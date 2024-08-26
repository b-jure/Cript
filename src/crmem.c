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

#include "crdebug.h"
#include "crmem.h"
#include "crconf.h"
#include "crstate.h"


#define cr_mem_rawmalloc(gs, s)		    (gs)->realloc(NULL, s, (gs)->udrealloc)
#define cr_mem_rawrealloc(gs, p, s)   	(gs)->realloc(p, s, (gs)->udrealloc)
#define cr_mem_rawfree(gs, p)	   	    (gs)->realloc(p, 0, (gs)->udrealloc)


/* can try to allocate second time */
#define cantryagain(ts)     (tsinitialized(ts) && !(gs)->gc.stopem)



/* Auxiliary to 'cr_mem_realloc' and 'cr_malloc'. */
cr_sinline void *tryagain(cr_State *ts, void *ptr, size_t osize, size_t nsize)
{
    GState *gs = GS(ts);
    UNUSED(osize);
    if (cantryagain(gs)) {
        cr_gc_full(ts, 1);
        return cr_mem_rawrealloc(gs, ptr, nsize);
    }
    return NULL;
}


void *cr_mem_realloc(cr_State *ts, void *ptr, size_t osize, size_t nsize)
{
    GState *gs = GS(ts);
    cr_assert((osize == 0) == (ptr == NULL));
    void *memblock = cr_mem_rawrealloc(gs, ptr, nsize);
    if (cr_unlikely(!memblock && nsize != 0)) {
        memblock = tryagain(ts, ptr, osize, nsize);
        if (cr_unlikely(memblock == NULL))
            return NULL;
    }
    cr_assert((nsize == 0) == (memblock == NULL));
    gs->gc.debt += nsize - osize;
    return memblock;
}


void *cr_mem_saferealloc(cr_State *ts, void *ptr, size_t osize, size_t nsize)
{
    void *memblock = cr_mem_realloc(ts, ptr, osize, nsize);
    if (cr_unlikely(memblock == NULL && nsize != 0))
        cr_assert(0 && "out of memory");
    return memblock;
}


void *cr_mem_malloc(cr_State *ts, size_t size)
{
    if (size == 0)
        return NULL;
    GState *gs = GS(ts);
    void *memblock = cr_mem_rawmalloc(gs, size);
    if (cr_unlikely(memblock == NULL)) {
        memblock = tryagain(ts, NULL, 0, size);
        if (cr_unlikely(memblock == NULL))
            cr_assert(0 && "out of memory");
    }
    gs->gc.debt += size;
    return memblock;
}


void *cr_mem_growarr(cr_State *ts, void *ptr, int len, int *sizep,
        int elemsize, int extra, int limit, const char *what)
{
    int size = *sizep;
    if (len + extra <= size)
        return ptr;
    size += extra;
    if (size >= limit / 2) {
        if (cr_unlikely(size >= limit))
            cr_debug_runerror(ts, "%s size limit", what);
        size = limit;
        cr_assert(size >= CRI_MINARRSIZE);
    } else {
        size *= 2;
        if (size < CRI_MINARRSIZE)
            size = CRI_MINARRSIZE;
    }
    ptr = cr_mem_saferealloc(ts, ptr, *sizep * elemsize, size * elemsize);
    *sizep = size;
    return ptr;
}


void *cr_mem_shrinkarr_(cr_State *ts, void *ptr, int *sizep, int final,
        int elemsize)
{
    size_t oldsize = cast_sizet(*sizep * elemsize);
    size_t newsize = cast_sizet(final * elemsize);
    cr_assert(newsize <= oldsize);
    ptr = cr_mem_saferealloc(ts, ptr, oldsize, newsize);
    *sizep = final;
    return ptr;
}


void cr_mem_free(cr_State *ts, void *ptr, size_t osize)
{
    GState *gs = GS(ts);
    cr_assert((osize == 0) == (ptr == NULL));
    cr_mem_rawfree(gs, ptr);
    gs->gc.debt -= osize;
}
