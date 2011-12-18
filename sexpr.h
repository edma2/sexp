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
#define cadr(p) (car(cdr(p)))
#define caddr(p) (car(cdr(cdr(p))))

enum {QUOTE, LPAREN, RPAREN, ATOM, END, ERR};
enum {TYPE_ATOM, TYPE_PAIR, TYPE_NIL};

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
        };
        int type;
        int refCount;
        Frame *frame;
};

extern char buf[BUFLEN];
extern Frame global;
extern SExpr nil;

SExpr *mkatom(char *str);
SExpr *mkpair(SExpr *car, SExpr *cdr);
SExpr *cons(SExpr *car, SExpr *cdr);
void release(SExpr *exp);

int nexttok(FILE *f);
SExpr *parse(FILE *f, int depth);

SExpr *eval(SExpr *exp, Frame *env);
SExpr *evalmap(SExpr *exps, Frame *env);
int istaggedlist(SExpr *exp, char *tag);
int isnumber(SExpr *exp);
int isdefine(SExpr *exp);
int issymbol(SExpr *exp);
int isquoted(SExpr *exp);
int islambda(SExpr *exp);

int primadd(SExpr *exp);

void print(SExpr *exp);

Frame *extend(Frame *env);
void freeEnv(Frame *f);

SExpr *define(SExpr *symbol, SExpr *exp, Frame *env);
SExpr *lookup(SExpr *symbol, Frame *env);
