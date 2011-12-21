#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

#define BUFLEN 1024
#define SIZE 256
#define isreserved(c) (c == ')' || c == '(' || c == '\'')

#define cons(car, cdr) (make_pair(car, cdr))
#define car(p) (p->pair[0])
#define cdr(p) (p->pair[1])
#define cadr(p) (car(cdr(p)))
#define caddr(p) (car(cdr(cdr(p))))

enum {QUOTE, LPAREN, RPAREN, ATOM, END, ERR};
enum {TYPE_ATOM, TYPE_PAIR, TYPE_NIL, TYPE_PRIM};

typedef struct Frame Frame;
struct Frame {
        Frame *parent;
        Entry *bindings[SIZE];
};

typedef struct SExpr SExpr;
struct SExpr {
        union {
                char *atom;
                SExpr *pair[2];
                SExpr *(*prim)(SExpr *);
        };
        int type;
        int refs;
        Frame *frame;
};

extern char buf[BUFLEN];
extern Frame global;
extern SExpr nil;

extern SExpr add;
extern SExpr sub;
extern SExpr mult;
extern SExpr divide;

/** Memory */
SExpr *make_atom(char *str);
SExpr *make_pair(SExpr *car, SExpr *cdr);
void dealloc(SExpr *exp);

/** I/O */
SExpr *parse(FILE *f, int depth);
int read_token(FILE *f);
void print(SExpr *exp);

/** Primitives */
SExpr *prim_add(SExpr *args);
SExpr *prim_sub(SExpr *args);
SExpr *prim_mult(SExpr *args);
SExpr *prim_divide(SExpr *args);

/** Eval */
SExpr *eval(SExpr *exp, Frame *env);
SExpr *eval_list(SExpr *ls, Frame *env);
SExpr *apply(SExpr *op, SExpr *operands);

int numeric(char *s);
int atomic(SExpr *exp);
int pair(SExpr *exp);
int empty(SExpr *exp);
int primitive(SExpr *exp);
int immortal(SExpr *exp);

int is_tagged_list(SExpr *ls, char *tag);
int is_number(SExpr *exp);
int is_define(SExpr *exp);
int is_symbol(SExpr *exp);
int is_quoted(SExpr *exp);
int is_lambda(SExpr *exp);
int is_application(SExpr *exp);

/** Environment */
void init(void);
int env_bind(char *sym, SExpr *val, Frame *env);
SExpr *env_lookup(char *sym, Frame *env);
