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
};

SExpr nil = {{NULL}, TYPE_NIL};
int depth;
char buf[BUFLEN];

/* Free memory held by this S-expression object */
void cleanup(SExpr *exp);

/* Return a new Atom */
SExpr *mkatom(char *str);

/* Return a new Pair. */
SExpr *mkpair(SExpr *car, SExpr *cdr);

/* Parse input stream into an S-expression. */
SExpr *parse(FILE *f);
SExpr *parseList(FILE *f);

/* Print S-expression. */
void print(SExpr *exp);

/* Return the next lexeme category from standard input. */
int readToken(FILE *f);

int main(void) {
        SExpr *exp;

        while (1) {
                exp = parse(stdin);
                if (exp == NULL)
                        break;
                print(exp);
                printf("\n");
                cleanup(exp);
        }
        return 0;
}

void cleanup(SExpr *exp) {
        if (exp->type == TYPE_ATOM) {
                free(exp->atom);
        } else if (exp->type == TYPE_PAIR) {
                cleanup(car(exp));
                cleanup(cdr(exp));
        }
        if (exp != &nil)
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
        if (exp == NULL)
                return NULL;
        car(exp) = car;
        cdr(exp) = cdr;
        exp->type = TYPE_PAIR;
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
                if (depth < 0)
                        return NULL;
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
