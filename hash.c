#include <stdio.h>
#include "hash.h"

int hash(char *s) {
        int h;

        for (h = 0; *s != '\0'; s++)
                h += (h * 37 + *s);
        return h % HSIZE;
}

Entry *find(Entry *hashtab[], char *key) {
        int h;
        Entry *ep;

        h = hash(key);
        for (ep = hashtab[h]; ep != NULL; ep = ep->next) {
                if (!strcmp(key, ep->key))
                        break;
        }
        return ep;
}

Entry *insert(Entry *hashtab[], char *key, void *value) {
        int h;
        Entry *e;

        e = malloc(sizeof(Entry));
        if (e == NULL)
                return NULL;
        e->key = strndup(key, strlen(key));
        //e->key = strdup(key); /* valgrind: Invalid read of size 4 */
        if (e->key == NULL) {
                free(e);
                return NULL;
        }
        h = hash(key);
        e->value = value;
        e->next = hashtab[h];
        hashtab[h] = e;
        return e;
}
