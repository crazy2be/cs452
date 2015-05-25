#include "hashtable.h"
#include "util.h"

static int hash(const char *key) {
    return 0;
}

static struct hashtable_entry* alloc_entry(struct hashtable *ht) {
    if (ht->entry_pool_i < MAX_ENTRIES) {
        return &ht->entry_pool[ht->entry_pool_i++];
    } else if (ht->freelist) {
        struct hashtable_entry *e = ht->freelist;
        ht->freelist = e->next;
        return e;
    } else {
        return 0;
    }
}

static struct hashtable_entry* get(struct hashtable *ht, const char *key, const int h) {
    struct hashtable_entry *e = ht->buckets[h];
    while (e) {
        if (strcmp(e->key, key) == 0) {
            break;
        }
        e = e->next;
    }
    return e;
}

void hashtable_init(struct hashtable *ht) {
    ht->freelist = 0;
    ht->entry_pool_i = 0;
    for (int i = 0; i < BUCKET_COUNT; i++) {
        ht->buckets[i] = 0;
    }
}

int hashtable_set(struct hashtable *ht, const char *key, int val) {
    if (strlen(key) > MAX_KEYLEN) return HASHTABLE_OVERLONG_KEY;

    int h = hash(key);

    struct hashtable_entry *e = get(ht, key, h);
    if (!e) {
        e = alloc_entry(ht);
        if (!e) {
            return HASHTABLE_TOO_MANY_ENTRIES;
        }
        e->next = ht->buckets[h];
        strcpy(e->key, key);
        ht->buckets[h] = e;
    }
    e->val = val;

    return HASHTABLE_SUCCESS;
}

int hashtable_get(struct hashtable *ht, const char *key, int *val) {
    // TODO: necessary?
    // if (strlen(key) > MAX_KEYLEN) return HASHTABLE_OVERLONG_KEY;

    int h = hash(key);

    struct hashtable_entry *e = get(ht, key, h);

    if (e) {
        *val = e->val;
        return HASHTABLE_SUCCESS;
    } else {
        return HASHTABLE_KEY_NOT_FOUND;
    }
}

int hashtable_del(struct hashtable *ht, const char *key) {
    struct hashtable_entry *e = ht->buckets[hash(key)];
    struct hashtable_entry **prev = &e;

    while (e) {
        if (strcmp(e->key, key) == 0) {
            break;
        }
        prev = &e;
        e = e->next;
    }

    if (e) {
        // splice the removed node out of the list
        *prev = e->next;

        // add the node to the freed list for reuse
        e->next = ht->freelist;
        ht->freelist = e;
        return HASHTABLE_SUCCESS;
    } else {
        return HASHTABLE_KEY_NOT_FOUND;
    }
}
