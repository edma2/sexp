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

typedef struct SExp SExp;
struct SExp {
        union {
                char *atom;
                SExp *pair[2];
        };
        int type;
};

SExp nil = {{NULL}, TYPE_NIL};
int depth;
char buf[BUFLEN];

/* Free memory held by this S-expression object */
void cleanup(SExp *sex);

/* Return a new Atom */
SExp *mkatom(char *str);

/* Return a new Pair. */
SExp *mkpair(SExp *car, SExp *cdr);

/* Parse input stream into an S-expression. */
SExp *parse(FILE *f);
SExp *parseList(FILE *f);

/* Print S-expression. */
void print(SExp *sex);

/* Return the next lexeme category from standard input. */
int readToken(FILE *f);

int main(void) {
        SExp *sex;

        while (1) {
                sex = parse(stdin);
                if (sex == NULL)
                        break;
                print(sex);
                printf("\n");
                cleanup(sex);
        }
        return 0;
}

void cleanup(SExp *sex) {
        if (sex->type == TYPE_ATOM) {
                free(sex->atom);
        } else if (sex->type == TYPE_PAIR) {
                cleanup(car(sex));
                cleanup(cdr(sex));
        }
        if (sex != &nil)
                free(sex);
}

SExp *mkatom(char *str) {
        SExp *sex;

        if (str == NULL)
                return NULL;
        sex = malloc(sizeof(struct SExp));
        if (sex == NULL)
                return NULL;
        sex->atom = strdup(str);
        sex->type = TYPE_ATOM;
        return sex;
}

SExp *mkpair(SExp *car, SExp *cdr) {
        SExp *sex;

        if (car == NULL || cdr == NULL) {
                if (car != NULL)
                        cleanup(car);
                if (cdr != NULL)
                        cleanup(cdr);
                return NULL;
        }
        sex = malloc(sizeof(struct SExp));
        if (sex == NULL)
                return NULL;
        car(sex) = car;
        cdr(sex) = cdr;
        sex->type = TYPE_PAIR;
        return sex;
}

SExp *parse(FILE *f) {
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

SExp *parseList(FILE *f) {
        SExp *sex;

        sex = parse(f);
        return sex == &nil ? sex : cons(sex, parseList(f));
}

void print(SExp *sex) {
        if (sex->type == TYPE_ATOM) {
                printf("%s", sex->atom);
        } else if (sex->type == TYPE_NIL) {
                printf("()");
        } else {
                printf("(");
                print(car(sex));
                printf(".");
                print(cdr(sex));
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
