#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define BUFLEN 1024
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

SExpr nil = {{NULL}, TYPE_NIL, 1};
int depth;
char buf[BUFLEN];

/* Free memory held by exp if its reference count is zero. If exp is a Pair,
 * decrement reference counters for its car and cdr children and recursively
 * call cleanup on them. */
void cleanup(SExpr *exp);

/* Return a new Atom. If memory allocation fails, return NULL. */
SExpr *mkatom(char *str);

/* Return a new Pair and increment reference counters for car and cdr. If car
 * or cdr is NULL, call cleanup on car if car is not NULL, and cdr if cdr is
 * not NULL, then return NULL. If memory allocation fails, return NULL. */
SExpr *mkpair(SExpr *car, SExpr *cdr);

/* Parse input stream into an SExpr representing the S-expression. */
SExpr *parse(FILE *f);
SExpr *parseList(FILE *f);

/* Print exp as an S-expression. */
void print(SExpr *exp);

/* Return the next lexeme category from file stream. */
int readToken(FILE *f);

int add(SExpr *exp) {
        if (exp == &nil)
                return 0;
        return atoi(car(exp)->atom) + add(cdr(exp));
}

int main(void) {
        SExpr *exp;

        while (1) {
                exp = parse(stdin);
                if (exp == NULL)
                        break;
                print(exp);
                printf("\n");
                printf("%d\n", add(exp));
                cleanup(exp);
        }
        return 0;
}

void cleanup(SExpr *exp) {
        if (exp == &nil || exp->refCount > 0)
                return;
        if (exp->type == TYPE_ATOM) {
                free(exp->atom);
        } else if (exp->type == TYPE_PAIR) {
                car(exp)->refCount--;
                cdr(exp)->refCount--;
                cleanup(car(exp));
                cleanup(cdr(exp));
        }
        free(exp);
}

SExpr *mkatom(char *str) {
        SExpr *exp;

        if (str == NULL)
                return NULL;
        exp = malloc(sizeof(struct SExpr));
        if (exp == NULL)
                return NULL;
        exp->atom = strdup(str);
        exp->type = TYPE_ATOM;
        exp->refCount = 0;
        return exp;
}

SExpr *mkpair(SExpr *car, SExpr *cdr) {
        SExpr *exp;

        if (car == NULL || cdr == NULL) {
                if (car != NULL)
                        cleanup(car);
                if (cdr != NULL)
                        cleanup(cdr);
                return NULL;
        }
        exp = malloc(sizeof(struct SExpr));
        if (exp == NULL) {
                cleanup(car);
                cleanup(cdr);
                return NULL;
        }
        car->refCount++;
        cdr->refCount++;
        car(exp) = car;
        cdr(exp) = cdr;
        exp->type = TYPE_PAIR;
        exp->refCount = 0;
        return exp;
}

SExpr *parse(FILE *f) {
        int category;

        category = readToken(f);
        if (category == ATOM) {
                return mkatom(buf);
        } else if (category == LPAREN) {
                depth++;
                return parseList(f);
        } else if (category == RPAREN) {
                depth--;
                if (depth >= 0)
                        return &nil;
        } else if (category == QUOTE) {
                return cons(mkatom("quote"), cons(parse(f), &nil));
        }
        return NULL;
}

SExpr *parseList(FILE *f) {
        SExpr *exp;

        exp = parse(f);
        return exp == &nil ? exp : cons(exp, parseList(f));
}

void print(SExpr *exp) {
        if (exp->type == TYPE_ATOM) {
                printf("%s", exp->atom);
        } else if (exp->type == TYPE_NIL) {
                printf("()");
        } else {
                printf("(");
                print(car(exp));
                printf(".");
                print(cdr(exp));
                printf(")");
        }
}

int readToken(FILE *f) {
        char c;
        int i;

        while (1) {
                c = getc(f);
                if (c == EOF)
                        return END;
                if (isspace(c))
                        continue;
                if (c == '(')
                        return LPAREN;
                if (c == ')')
                        return RPAREN;
                if (c == '\'')
                        return QUOTE;
                else {
                        ungetc(c, f);
                        for (i = 0; i < BUFLEN-1; i++) {
                                c = getc(f);
                                if (c == EOF)
                                        return END;
                                if (isreserved(c) || isspace(c))
                                        break;
                                buf[i] = c;
                        }
                        buf[i] = '\0';
                        if (isreserved(c))
                                ungetc(c, f);
                        return ATOM;
                }
        }
        return ERR;
}
