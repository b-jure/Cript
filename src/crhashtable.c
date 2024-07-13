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

#include "crhashtable.h"
#include "crstate.h"
#include "crconf.h"
#include "crmem.h"
#include "crobject.h"
#include "crvalue.h"



/* set/check tombstone node */
#define istomb(n)	(keytt(n) == CR_VTOMB)
#define puttomb(n)	(keytt(n) = CR_VTOMB)


/* get table load factor */
#define loadfactor(ts,t) \
	cri_numdiv((ts), cast_num((t)->nnodes), cast_num(htsize(t)))


/* slots left until table needs to grow */
#define slotsleft(ts,t) \
	cast_int((cast_num(CRI_MAXHTABLOAD) - loadfactor((ts),(t))) * htsize(t))



/* get hash slot */
#define hashslot(b,h,s)		(&b[(h)&((s)-1)])



/* empty node constant */
static const Node emptynode = {{0,CR_VEMPTY,0,CR_VEMPTY,0}};



/* initialize hash table */
HTable *cr_htable_new(cr_State *ts)
{
	HTable *ht;

	ht = cr_gc_new(ts, sizeof(HTable), CR_VHTABLE, HTable);
	memset(ht + OBJHEADERSIZE, 0, sizeof(HTable) - OBJHEADERSIZE);
	return ht;
}


/* create string hash table */
void cr_htable_newstab(cr_State *ts, HTable *tab)
{
	tab->size = cr_value_ceillog2(CRI_MINSTRHTABSIZE);
	tab->nnodes = 0;
	tab->left = slotsleft(ts, tab);
	tab->mem = cr_mem_newarray(ts, CRI_MINSTRHTABSIZE, Node);
}


/*
 * Find the main position (slot) for key 'k' inside
 * the table array.
 */
static Node *mainposition(const Node *mem, int size, const TValue *k)
{
	void *p;
	cr_cfunc f;
	OString *str;

	switch (vtt(k)) {
		case CR_VTRUE:
			return cast_node(hashslot(mem, cr_value_hashbool(1), size));
		case CR_VFALSE:
			return cast_node(hashslot(mem, cr_value_hashbool(0), size));
		case CR_VNUMINT:
			return cast_node(hashslot(mem, cr_value_hashint(ival(k)), size));
		case CR_VNUMFLT:
			return cast_node(hashslot(mem, cr_value_hashnum(fval(k)), size));
		case CR_VLUDATA:
			p = pval(k);
			return cast_node(hashslot(mem, cr_value_hashp(p), size));
		case CR_VCFUNCTION:
			f = cfval(k);
			return cast_node(hashslot(mem, cr_value_hashp(p), size));
		case CR_VSTRING:
			str = strval(k);
			cr_assert(hashash(str));
			return cast_node(hashslot(mem, str->hash, size));
		default:
			cr_assert(!ttisnil(k) && ttiso(k));
			return cast_node(hashslot(mem, cr_value_hashp(oval(k)), size));
	}
}


/* raw equality without calling vtable methods */
static int eqkey(const TValue *k, const Node *n)
{
	cr_assert(!ttisempty(k));
	if (vtt(k) != keytt(n))
		return 0;
	switch (vtt(k)) {
		case CR_VTRUE: case CR_VFALSE:
			return 1;
		case CR_VNUMINT:
			return (ival(k) == keyival(n));
		case CR_VNUMFLT:
			return cri_numeq(fval(k), keyfval(n));
		case CR_VLUDATA:
			return (pval(k) == keypval(n));
		case CR_VCFUNCTION:
			return (cfval(k) == keycfval(n));
		default: /* all equal objects have equal pointers */
			cr_assert(vtt(k) == CR_TOBJECT);
			return (oval(k) == keyoval(n));
	}
	return 0;
}


/*
 * Find slot by linear probing.
 * Size of the table 'mem' array is always power of 2.
 * In case of finding a 'tomb' node keep probing until the
 * same 'key' was found or empty spot.
 * If the empty slot was found and there was a 'tomb', then
 * return the tomb, otherwise return the entry containing the
 * same key. 'slot' won't overflow because load factor is tracked.
 */
cr_sinline Node *gehtnode(const Node *mem, int size, const TValue *k)
{
	Node *tomb;
	Node *slot;

	tomb = NULL;
	for (slot = mainposition(mem, size, k);;slot++) { /* linear probing */
		if (keyisempty(slot)) {
			if (!istomb(slot))
				return (tomb ? tomb : slot);
			if (!tomb)
				tomb = slot;
		} else if (eqkey(k, slot)) {
			return slot;
		}
	}
}


/* auxliary function to 'cr_htable_next' */
static unsigned int getindex(cr_State *ts, HTable *tab, const TValue *k)
{
	Node *slot;

	slot = gehtnode(tab->mem, htsize(tab), k);
	if (cr_unlikely(keyisempty(slot)))
		cr_assert(0 && "invalid key passed to 'next'");
	return cast_int(slot - htnode(tab, 0));
}


/*
 * Find next table entry after 'key' entry.
 * If table had next entry then top of the stack will contain
 * key of that entry and its value (in that order).
 */
int cr_htable_next(cr_State *ts, HTable *tab, SIndex *k)
{
	Node *slot;
	TValue *v;
	unsigned int i;

	v = s2v(k->p);
	i = getindex(ts, tab, v);
	for (; i < htsize(tab); i++) {
		if (!keyisempty(htnode(tab, i))) {
			slot = htnode(tab, i);
			getnodekey(ts, v, slot);
			setval(ts, v+1, htnodevalue(slot));
			return 1;
		}
	}
	return 0;
}


/* insert all the 'keys' from 'stab' into 'dtab' */
void cr_htable_copykeys(cr_State *ts, HTable *stab, HTable *dtab)
{
	Node *slot;
	TValue k;
	int i;

	for (i = 0; i < htsize(stab); i++) {
		slot = htnode(stab, i);
		if (!keyisempty(slot)) {
			getnodekey(ts, &k, slot);
			cr_htable_set(ts, dtab, &k, htnodevalue(slot));
		}
	}
}


static void rehash(cr_State *ts, const Node *omem, int osize, Node *nmem, int nsize)
{
	const Node *slot;
	Node *dest;
	TValue *k;
	int i;

	for (i = 0; i < osize; i++) {
		slot = omem + i;
		if (keyisempty(slot))
			continue;
		getnodekey(ts, k, slot);
		dest = gehtnode(nmem, nsize, k);
		*dest = *slot;
	}
}


/* sets table array slots to 'emptynode' */
cr_sinline void auxsetempty(Node * restrict mem, unsigned int size)
{
	Node *slot;
	int i;

	cr_assert(ispow2(size) && size >= 4);
	slot = mem;
	for (i = 0; i < size; i+=4) { /* unroll */
		*slot++ = emptynode;
		*slot++ = emptynode;
		*slot++ = emptynode;
		*slot++ = emptynode;
	}
}


/* expand hash table array */
static void expandmem(cr_State *ts, HTable *tab)
{
	Node *newmem;
	unsigned int nsize;
	int osize;

	osize = twoto(tab->size++);
	nsize = twoto(tab->size);
	if (cr_unlikely(nsize >= CRI_MAXHTABSIZE))
		cr_assert(0 && "hashtable overflow");
	newmem = cr_mem_newarray(ts, nsize, Node);
	auxsetempty(newmem, nsize);
	rehash(ts, tab->mem, osize, newmem, cast_int(nsize));
	if (tab->mem != NULL)
		cr_mem_freearray(ts, tab->mem, osize);
	tab->mem = newmem;
	tab->size = nsize;
	tab->left = slotsleft(ts, tab);
}


/*
 * Set value for the given key.
 * If the 'key' was not found insert it together with the 'value'.
 * If the 'key' already exists set its 'value'.
 */
int cr_htable_set(cr_State *ts, HTable *tab, const TValue *key, const TValue *val)
{
	Node *slot;
	int newk;

	slot = gehtnode(tab->mem, htsize(tab), key);
	if ((newk = keyisempty(slot))) { /* new key */
		if (!istomb(slot)) tab->left--;
		if (cr_unlikely(tab->left <= 0)) {
			expandmem(ts, tab);
			cr_htable_set(ts, tab, key, val);
			return 1;
		}
		tab->nnodes++;
	}
	setnodekey(ts, slot, key);
	*htnodevalue(slot) = *val;
	return newk;
}


/*
 * Removes the slot belonging to the table 'tab' directly
 * without probing for it.
 */
void cr_htable_removedirect(HTable *tab, Node *slot)
{
	cr_assert(!istomb(n) && !keyisempty(n));
	cr_assert(htfirstnode(tab) <= slot && slot <= htlastnode(tab));
	puttomb(slot);
	tab->nnodes--;
}


/*
 * Remove 'key' from the table.
 * If the 'key' was found (and removed) return non-zero and place the tombstone.
 * Tombstones count as entries so this will not decrement 'left'.
 */
int cr_htable_remove(HTable *tab, const TValue *key)
{
	Node *slot;

	slot = gehtnode(tab->mem, htsize(tab), key);
	if (keyisempty(slot))
		return 0;
	puttomb(slot);
	tab->nnodes--;
	return 1;
}


/* try to get interned string */
OString *cr_htable_getstring(HTable *tab, const char *str, size_t len, unsigned int hash)
{
	int size;
	Node *slot;
	OString *s;

	if (tab->nnodes == 0)
		return NULL;
	size = htsize(tab);
	for (slot = hashslot(tab->mem, hash, size);;slot++) {
		if (keyisempty(slot)) {
			if (!istomb(slot)) return NULL;
		} else {
			s = keystrval(slot);
			if (s->hash == hash && s->len == len &&
				memcmp(s->bytes, str, len) == 0)
				return s;
		}
	};
}


/* get key value */
int cr_htable_get(HTable *tab, const TValue *key, TValue *o)
{
	Node *slot;

	cr_assert(o != NULL);
	if (tab->nnodes == 0) return 0;
	slot = gehtnode(tab->mem, htsize(tab), key);
	if (keyisempty(slot)) return 0;
	setval(cast(cr_State*, NULL), o, htnodevalue(slot));
	return 1;
}


void cr_htable_free(cr_State *ts, HTable *ht)
{
	if (ht->mem != NULL)
		cr_mem_free(ts, ht->mem, twoto(ht->size) * sizeof(Node));
	cr_mem_free(ts, ht, sizeof(HTable));
}
