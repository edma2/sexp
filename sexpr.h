#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

#define BUFLEN 1024
#define SIZE 256
#define isreserved(c) (c == ')' || c == '(' || c == '\'')
#define car(p) (p->pair[0])
#define cdr(p) (p->pair[1])
#define cons(l,r) (mkpair(l,r))

enum {QUOTE, LPAREN, RPAREN, ATOM, END, ERR};
enum {TYPE_ATOM, TYPE_PAIR, TYPE_NIL};

typedef struct SExpr SExpr;
struct SExpr {
        union {
                char *atom;
                SExpr *pair[2];
        };
        int type;
        int refCount;
};

typedef struct Frame Frame;
struct Frame {
        Frame *parent;
        Entry *bindings[SIZE];
};

typedef struct {
        SExpr *body;
        Frame *env;
} Proc;

extern char buf[BUFLEN];        /* general purpose buffer */
extern Frame global;            /* global frame */
extern SExpr nil;               /* empty list */
extern int pdepth;              /* parse depth */

/* If reference counter is 0, free resources associated with SExpr. Then, if
 * SExpr is a Pair, decrease reference counters for car and cdr. */
void release(SExpr *exp);

/* Return a new atom with string as its value. */ 
SExpr *mkatom(char *str);

/* Return a new pair constructed from the car and cdr pairs. */
SExpr *mkpair(SExpr *car, SExpr *cdr);

/* Parse input stream into an SExpr. */
SExpr *parse(FILE *f);

/* Print SExpr. */
void print(SExpr *exp);

/* Lexer: reads the next token from input stream. */
int nexttok(FILE *f);

/* Evaluate SExpr and return result, or NULL on error. */
SExpr *eval(SExpr *exp);

/* Map eval to a list of SExprs. */
SExpr *evalmap(SExpr *exps);

/* Return operator of SExpr. */
SExpr *operator(SExpr *exp);

/* Return operands of SExpr. */
SExpr *operands(SExpr *exp);

/* (+ E0, ... ) */
int add(SExpr *exp);

/* Extend environment and return new frame, or NULL on error. */
Frame *extend(Frame *env);

/* Define new symbol in current frame and return value, or NULL on error. */
SExpr *define(Frame *env, char *symbol, SExpr *exp);

/* Look up symbol in environment and return value, or NULL if not found. */
SExpr *lookup(Frame *env, char *symbol);
