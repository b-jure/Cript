/* ----------------------------------------------------------------------------------------------
 * Copyright (C) 2023-2024 Jure Bagić
 *
 * This file is part of Skooma.
 * Skooma is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Skooma is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with Skooma.
 * If not, see <https://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------------------------*/

#ifndef SKHASHTABLE_H
#define SKHASHTABLE_H

#include "skcommon.h"
#include "skvalue.h"

#ifndef SKOOMA_VMACHINE_H
typedef struct VM VM;
#endif

typedef struct {
    Value key;
    Value value;
} Entry;

typedef struct {
    UInt cap; // table capacity
    UInt len; // table length
    UInt left; // inserts until load factor exceeded
    Entry* entries; // table array (array of Entry)
} HashTable;


void HashTable_init(HashTable* table);
void HashTable_free(VM* vm, HashTable* table);

uint8_t HashTable_insert(VM* vm, HashTable* table, Value key, Value value, uint8_t raw);
uint8_t HashTable_remove(VM* vm, HashTable* table, Value key, uint8_t raw);

uint8_t HashTable_get(VM* vm, HashTable* table, Value key, Value* out, uint8_t raw);
OString* HashTable_get_intern(HashTable* table, const char* str, size_t len, sk_hash hash);
uint8_t HashTable_next(VM* vm, HashTable* table, Value* key);


void HashTable_into(VM* vm, HashTable* from, HashTable* to, uint8_t raw);


uint32_t resizetable(uint32_t wanted);


void internliteral(VM* vm, const char* string);
void internfmt(VM* vm, const char* fmt, ...);


// table access
#define tableget(vm, table, key, out) (HashTable_get(vm, table, key, out, 0))
#define tableset(vm, table, key, value) (HashTable_insert(vm, table, key, value, 0))

// Raw table access
#define rawget(vm, table, key, out) (HashTable_get(vm, table, key, out, 1))
#define rawset(vm, table, key, value) (HashTable_insert(vm, table, key, value, 1))


#endif