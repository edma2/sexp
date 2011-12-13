#include <stdlib.h>
#include <string.h>

#define HSIZE 256

typedef struct Entry Entry;
struct Entry {
        char *key;
        void *value;
        Entry *next;
};

/* Compute hash value of string. */
int hash(char *s);

/* Insert new entry into table without checking for duplicates. Return the
 * Entry if successful, otherwise NULL. */
Entry *insert(Entry *hashtab[], char *key, void *value);

/* Return mapping associated with key, or NULL if not found. */
Entry *find(Entry *hashtab[], char *key);
