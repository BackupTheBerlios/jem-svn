//=============================================================================
// This file is part of Kcfg, a simple configuration system in a Linux kernel
// module for embedded Linux applications.
//
// Copyright (C) 2007 JavaDevices Software LLC.
// Copyright (C) 2006 Jeffrey S. Dutky
//
// This file was derived directly from the simple configuration system,
// which is a user space library developed by Jeffrey S. Dutky.
//
// This file is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License, version 2.1, as published
// by the Free Software Foundation.
//
// Kcfg is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You will find documentation for Kcfg at http://www.javadevices.com
//
// You will find the maintainers and current source code of Kcfg at BerliOS:
//    http://developer.berlios.de/projects/jem/
//
//=============================================================================
// hashtable.c
//
// Generic hash table library implementation
//
//=============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include "hashtable.h"


#define HASHTABLE_TAG 0x74486261 /* HTbl */
#define HASHBUCKET_TAG 0x62486b63 /* Hbck */
#define HASHBLOCK_TAG 0x62486b6c /* Hblk */

struct hashbucket_struct
{
    int tag;
    struct hashbucket_struct *link;
    void *key;
    void *val;
};

#define BLOCK_SIZE 100

struct hashblock_struct
{
    int tag;
    struct hashblock_struct *link;
    struct hashbucket_struct bucket[BLOCK_SIZE];
};

/* hashtable is implemented in a heap-friendly manner. By allocating the
** hash table structure, initial slot table and a bunch of buckets in a
** single malloc() call, we minimize heap fragmentation. Further buckets
** are reused after being removed from the table, and new buckets are
** allocated in large blocks, further minimizing heap fragmentation.
**
** These allocation policies also server to avoid use of the system
** memory allocator, which is generally pretty slow, so this will speed
** up most inserts (except when we need a new block of buckets). */

struct hashtable_struct
{
    int tag;
    unsigned long size;
    struct hashbucket_struct **table;
    unsigned int (*hash)(void *key);
    int (*comp)(void *key1, void *key2);
    void (*kdel)(void *key);
    void (*vdel)(void *val);
    struct hashbucket_struct *pool;
    struct hashblock_struct *blocks;
    struct hashbucket_struct **old_table; /* prep for auto table resize */
    unsigned long old_size, curr_slot;    /* prep for auto table resize */
    unsigned long item_count;             /* prep for auto table resize */
    struct hashbucket_struct *itable[1];  /* prep for auto table resize */
};

/* with auto table resize we will detect when the current table is overfull
** and allocate a new, larger, table. Then, with each lookup, insert or
** remove we will transfer one slot's worth of buckets from the old table
** to the new table. Each lookup will first be performed in the new table,
** but, if the lookup fails, it will be performed again against the old
** table. Only if both lookups fail will we return a lookup failure.
**
** Any item lookedup in or inserted into the table after an auto resise
** will go into the new table. After the lookup or insert is complete,
** a slot in the old table will be selected to be transferred from the
** old table to the new one.
**
** The big problem with auto resize is knowing when to do the resize,
** what we really want to do is detect when the table is no hashing well
** (when we are putting lots of entries in the same slot) but that may
** be hard to do reliably. */

/* create a new hashtable
**
** returns a valid hashtable pointer on success, NULL on failure */
hashtable *ht_create(unsigned long p_size,
    unsigned int (*p_hash)(void *key),
    int (*p_comp)(void *key1, void *key2),
    void (*p_kdel)(void *key),
    void (*p_vdel)(void *val))
{
    hashtable *ht = NULL;
    struct hashblock_struct *hblk;
    int i;

    if(p_hash == NULL || p_comp == NULL)
        return NULL;

    /* allocate the hashtable structure, the slot table and the first block
    ** of buckets all in one shot. The itable field will automatically point
    ** to the slot table, but we need to find the address of the bucket block
    ** by some hinky pointer arithmetic */
    ht = (hashtable*)kzalloc(sizeof(hashtable)
        +sizeof(struct hashbucket_struct*)*p_size
        +sizeof(struct hashblock_struct), GFP_KERNEL);
    if(ht == NULL)
    {
        return NULL;
    }
    ht->table = ht->itable;
    memset(ht->table, 0, sizeof(struct hashbucket_struct*)*p_size);
    ht->size = p_size;
    /* here is the hinky pointer math for find the bucket block */
    hblk = (struct hashblock_struct*)(((char*)(ht+1))+
        sizeof(struct hashbucket_struct*)*p_size);
    hblk->tag = HASHBLOCK_TAG;
    hblk->link = NULL;
    ht->pool = NULL;
    for(i = 0; i < BLOCK_SIZE; i++)
    { /* put the bucket from the new block into the bucket pool */
        hblk->bucket[i].tag = 0;
        hblk->bucket[i].key = NULL;
        hblk->bucket[i].val = NULL;
        hblk->bucket[i].link = ht->pool;
        ht->pool = hblk->bucket+i;
    }
    /* now that the buckets are in the pool, there is no need to keep track
    ** of the initial block of buckets (it's part of the hash table structure
    ** allocation, so it will get deleted with the hash table strudture */
    ht->blocks = NULL;
    ht->hash = p_hash;
    ht->comp = p_comp;
    ht->kdel = p_kdel;
    ht->vdel = p_vdel;
    ht->tag = HASHTABLE_TAG;
    ht->old_table = NULL; /* pointer to old table during resize */
    ht->old_size = 0; /* size of old table during resize */
    ht->curr_slot = 0; /* current slot to copy to new table */
    ht->item_count = 0; /* total number of items in hash table (old and new) */

    return ht;
}

/* delete a hashtable created with ht_create() */
void ht_delete(hashtable *p_ht)
{
    struct hashbucket_struct *hbkt;
    struct hashblock_struct *hblk, *hblkl;
    int i;

    if(p_ht == NULL || p_ht->tag != HASHTABLE_TAG)
        return;

    if(p_ht->size > 0 && p_ht->table != NULL)
    {
        if(p_ht->kdel != NULL)
        { /* delete all keys in the table */
            for(i = 0; i < p_ht->size; i++)
            {
                hbkt = p_ht->table[i];
                while(hbkt != NULL)
                {
                    p_ht->kdel(hbkt->key);
                    hbkt = hbkt->link;
                }
            }
        }
        if(p_ht->vdel != NULL)
        { /* delete all values in the table */
            for(i = 0; i < p_ht->size; i++)
            {
                hbkt = p_ht->table[i];
                while(hbkt != NULL)
                {
                    p_ht->vdel(hbkt->val);
                    hbkt = hbkt->link;
                }
            }
        }
        if(p_ht->table != p_ht->itable)
            kfree(p_ht->table); /* delete the table itself */
    }
    p_ht->table = NULL;
    p_ht->size = 0;

    if(p_ht->old_size > 0 && p_ht->old_table != NULL)
    {
        if(p_ht->kdel != NULL)
        { /* delete all keys in the old table */
            for(i = 0; i < p_ht->old_size; i++)
            {
                hbkt = p_ht->old_table[i];
                while(hbkt != NULL)
                {
                    p_ht->kdel(hbkt->key);
                    hbkt = hbkt->link;
                }
            }
        }
        if(p_ht->vdel != NULL)
        { /* delete all values in the old table */
            for(i = 0; i < p_ht->old_size; i++)
            {
                hbkt = p_ht->old_table[i];
                while(hbkt != NULL)
                {
                    p_ht->vdel(hbkt->key);
                    hbkt = hbkt->link;
                }
            }
        }
        if(p_ht->old_table != p_ht->itable)
            kfree(p_ht->table); /* delete the old table itself */
    }
    p_ht->old_table = NULL;
    p_ht->old_size = 0;

    hblk = p_ht->blocks;
    while(hblk != NULL)
    { /* delete the bucket blocks */
        hblkl = hblk->link;
        memset(hblk, 0, sizeof(struct hashblock_struct));
        kfree(hblk);
        hblk = hblkl;
    }
    p_ht->blocks = NULL;

    p_ht->curr_slot = 0;
    p_ht->item_count = 0;

    p_ht->tag = 0;
    p_ht->hash = NULL;
    p_ht->kdel = NULL;
    p_ht->vdel = NULL;
    p_ht->comp = NULL;
    kfree(p_ht); /* delete the hash table structure */

    return;
}

/* internal lookup routine, returns a pointer to the target slot, the
** target bucket (if any found) and the bucket before the target (again,
** if any found). Returns 0 (zero) on success, positive non-zero on
** failure and negative one (-1) when no bucket was found (but a slot
** has been selected) */
static int lookup(hashtable *p_ht, void *p_key, void **p_val,
    struct hashbucket_struct ***p_slot,
    struct hashbucket_struct **p_bucket,
    struct hashbucket_struct **p_previous)
{
    struct hashbucket_struct *hb, *prev, **slot;
    int hc;

    hc = p_ht->hash(p_key); /* find the slot for this key */
    hb = p_ht->table[hc%p_ht->size];
    slot = &(p_ht->table[hc%p_ht->size]);
    prev = NULL;
    while(hb != NULL)
    { /* search the bucket list for this slot for a maching key */
        if(hb->tag != HASHBUCKET_TAG)
            return 4;
        if(p_ht->comp(p_key, hb->key) == 0)
            break;
        prev = hb;
        hb = hb->link;
    }

    if(hb == NULL && p_ht->old_table != NULL && p_ht->old_size > 0)
    { /* look in the old table, move bucket to new table if found */
        /*** IMPLEMENT ME! */
    }

    /* fill in slot and bucket parameters, even if no bucket was found */
    if(p_slot != NULL)
        *p_slot = slot;
    if(p_bucket != NULL)
        *p_bucket = hb;

    if(hb == NULL)
    {
        if(p_previous != NULL)
            *p_previous = NULL;
        if(p_val != NULL)
            *p_val = NULL;
        return -1; /* bucket found for this key */
    }

    if(p_previous != NULL)
        *p_previous = prev;
    if(p_val != NULL)
        *p_val = hb->val;

    return 0;
}

/* insert key and value into the hash table or update an existing key/value
** pair if the key is already in the table with another value */
int ht_insert(hashtable *p_ht, void *p_key, void *p_val)
{
    struct hashbucket_struct *hb = NULL, **slot = NULL;

    if(p_ht == NULL || p_ht->tag != HASHTABLE_TAG)
        return 1;

    /*** IMPLEMENT ME! check table for 'fullness' and resize if necessary */

    if(lookup(p_ht, p_key, NULL, &slot, &hb, NULL) > 0) /* find slot for key */
        return 2;
    if(slot == NULL)
        return 3; /* no slot found (this shouldn't happen) */

    if(hb != NULL)
    { /* key already in table, update the bucket with new key and value */
        if(p_ht->kdel != NULL)
            p_ht->kdel(hb->key);
        if(p_ht->vdel != NULL)
            p_ht->vdel(hb->val);
        hb->key = p_key;
        hb->val = p_val;

        return 0; /* maybe we should be returning an update indicator? */
    }

    if(p_ht->pool == NULL)
    {
        struct hashblock_struct *hblk;
        int i;

        hblk = (struct hashblock_struct*)kzalloc(sizeof(struct hashblock_struct), GFP_KERNEL);
        if(hblk == NULL)
            return 4;
        hblk->tag = HASHBLOCK_TAG;
        for(i = 0; i < BLOCK_SIZE; i++)
        { /* put the buckets from the new block into the bucket pool */
            hblk->bucket[i].tag = 0;
            hblk->bucket[i].key = NULL;
            hblk->bucket[i].val = NULL;
            hblk->bucket[i].link = p_ht->pool;
            p_ht->pool = hblk->bucket+i;
        }
        hblk->link = p_ht->blocks;
        p_ht->blocks = hblk;
    }
    /* remove a bucket from the bucket pool */
    hb = p_ht->pool;
    p_ht->pool = hb->link;
    hb->link = NULL;

    /* add bucket to selected table slot bucket list */
    hb->val = p_val;
    hb->key = p_key;
    hb->link = *slot;
    *slot = hb;
    hb->tag = HASHBUCKET_TAG;

    p_ht->item_count++;

    return 0;
}

/* remove a key/value pair from the hashtable, return 0 (zero) on success,
** non-zero on failure */
int ht_remove(hashtable *p_ht, void *p_key, void **p_val)
{
    struct hashbucket_struct *hb, *prev, **slot;

    if(p_ht == NULL || p_ht->tag != HASHTABLE_TAG)
        return 1; /* no valid hashtable provided */

    if(lookup(p_ht, p_key, p_val, &slot, &hb, &prev) != 0) /* find bucket with key */
        return 2; /* lookup failed */

    if(hb == NULL)
        return 3; /* no bucket found for this key */
    if(p_ht->kdel != NULL)
        p_ht->kdel(hb->key); /* delete key */
    if(p_ht->vdel != NULL)
        p_ht->vdel(hb->val); /* delete value */

    /* remove the bucket from the slot bucket list */
    if(prev != NULL)
        prev->link = hb->link;
    if(*slot == hb)
        *slot = hb->link;

    /* return the bucket to the bucket pool */
    hb->key = NULL;
    hb->val = NULL;
    hb->link = p_ht->pool;
    p_ht->pool = hb;
    hb->tag = 0;

    p_ht->item_count--;

    return 0;
}

/* lookup a key in the hashtable, return the corresponding value in p_val
** (if any found). Returns 0 (zero) on success, non-zero on failure */
int ht_lookup(hashtable *p_ht, void *p_key, void **p_val)
{
    struct hashbucket_struct *hb;

    if(p_ht == NULL || p_ht->tag != HASHTABLE_TAG)
        return 1;

    if(lookup(p_ht, p_key, p_val, NULL, &hb, NULL) != 0) /* find bucket with key */
        return 2;

    if(hb == NULL)
        return 3;

    return 0;
}

/* apply a user-supplied function to each entry in the hashtable */
int ht_iterate(hashtable *p_ht, int (*p_func)(unsigned long slot,
    void *key, void *val), int *p_rv)
{
    unsigned long i;
    int rv = 0;
    struct hashbucket_struct *hb;

    if(p_ht == NULL || p_ht->tag != HASHTABLE_TAG)
        return 1;

    if(p_func == NULL)
        return 0;

    for(i = 0; i < p_ht->size; i++)
    { /* for each slot in the table, iterate over each bucket in the list */
        hb = p_ht->table[i];
        while(hb != NULL)
        { /* iterate over the bucket list for this slot */
            if(hb->tag != HASHBUCKET_TAG)
                return 4;
            rv = p_func(i, hb->key, hb->val); /* call user iteation function */
            if(rv != 0)
            {
                if(p_rv != NULL)
                    *p_rv = rv;
                return 5;
            }
            hb = hb->link;
        }
    }

    for(i = 0; i < p_ht->old_size; i++)
    { /* for each slot in the old table, iterate over each bucket in the lsit */
        hb = p_ht->old_table[i];
        while(hb != NULL)
        { /* iterate over the bucket list for this slot */
            if(hb->tag != HASHBUCKET_TAG)
                return 4;
            rv = p_func(i, hb->key, hb->val); /* call user iteration function */
            if(rv != 0)
            {
                if(p_rv != NULL)
                    *p_rv = rv;
                return 5;
            }
            hb = hb->link;
        }
    }

    return 0;
}

