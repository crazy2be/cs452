#pragma once

/**
 * @file
 *
 * Maps string keys to integer values.
 * Because we're doing this in a micro-kernel, we are constrained in the
 * amount of memory we preallocate for the hashtable.
 * Therefore, we limit MAX_KEYLEN & MAX_ENTRIES
 */

#define MAX_KEYLEN 23 // chosen to make the size of hashtable_entry = 32
#define MAX_ENTRIES 256
#define BUCKET_COUNT 32

#define HASHTABLE_SUCCESS 0
#define HASHTABLE_KEY_NOT_FOUND -1
#define HASHTABLE_OVERLONG_KEY -2
#define HASHTABLE_TOO_MANY_ENTRIES -3

struct hashtable_entry {
	char key[MAX_KEYLEN + 1];
	int val;
	struct hashtable_entry *next;
};

struct hashtable {
	struct hashtable_entry entry_pool[MAX_ENTRIES];
	int entry_pool_i;
	struct hashtable_entry *freelist;

	struct hashtable_entry *buckets[BUCKET_COUNT];
};

void hashtable_init(struct hashtable *ht);

// non-zero return from any of these indicates an error
int hashtable_set(struct hashtable *ht, const char *key, int val);
int hashtable_get(struct hashtable *ht, const char *key, int *val);
int hashtable_del(struct hashtable *ht, const char *key);
