#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifndef HASH_H
#define HASH_H
#include "hash.h"
#endif

#ifndef GC_H
#define GC_H
#include "gc.h"
#endif

#define BUFLEN 1024
#define SIZE 256
#define isreserved(c) (c == ')' || c == '(' || c == '\'')

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
        int live;
};

typedef struct SExpr SExpr;
struct SExpr {
        union {
                char *atom;
                SExpr *pair[2];
                SExpr *(*prim)(SExpr *);
        };
        int type;
        int live;
        Frame *env;
};

extern char buf[BUFLEN];
extern Frame global;
extern SExpr nil;
extern int eof;

/** Memory */
SExpr *make_atom(char *str);
SExpr *make_pair(SExpr *car, SExpr *cdr);
SExpr *make_prim(SExpr *(*prim)(SExpr *));
SExpr *cons(SExpr *car, SExpr *cdr);

/** I/O */
SExpr *parse(FILE *f, int depth);
int read_token(FILE *f);
void print(SExpr *exp);

/** Primitives */
SExpr *prim_add(SExpr *args);
SExpr *prim_sub(SExpr *args);
SExpr *prim_mult(SExpr *args);
SExpr *prim_div(SExpr *args);
SExpr *prim_cons(SExpr *args);
SExpr *prim_cdr(SExpr *args);
SExpr *prim_car(SExpr *args);

/** Eval */
SExpr *eval(SExpr *exp, Frame *env);
SExpr *eval_list(SExpr *ls, Frame *env);
SExpr *apply(SExpr *op, SExpr *operands);

void init(void);

int is_tagged_list(SExpr *ls, char *tag);
int is_number(SExpr *exp);
int is_define(SExpr *exp);
int is_symbol(SExpr *exp);
int is_quoted(SExpr *exp);
int is_lambda(SExpr *exp);
int is_application(SExpr *exp);

/** Attributes */
int numeric(char *s);
int atomic(SExpr *exp);
int compound(SExpr *exp);
int empty(SExpr *exp);
int primitive(SExpr *exp);

/** Environment */
int env_bind(char *sym, SExpr *val, Frame *env);
SExpr *env_lookup(char *sym, Frame *env);
Frame *extend(SExpr *args, SExpr *params, Frame *env);
Frame *new_frame(Frame *parent);
void free_frame(Frame *fr);
void free_sexpr(SExpr *exp);
