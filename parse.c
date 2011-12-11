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
char buf[BUFLEN];

int readToken(FILE *f);
SExp *parse(FILE *f);
SExp *parselist(FILE *f);
void print(SExp *sex);
void cleanup(SExp *sex);
SExp *mkatom(char *str);
SExp *mkpair(SExp *car, SExp *cdr);

SExp *parse(FILE *f) {
        int category;

        category = readToken(f);
        if (category == ATOM)
                return mkatom(buf);
        else if (category == LPAREN)
                return parselist(f);
        return &nil;
}

SExp *parselist(FILE *f) {
        SExp *car, *cdr;
        int category;

        category = readToken(f);
        if (category == ATOM)
                car = mkatom(buf);
        else if (category == LPAREN)
                car = parselist(f);
        else if (category == RPAREN)
                return &nil;
        if (car == NULL)
                return NULL;
        cdr = parselist(f);
        if (cdr == NULL) {
                cleanup(car);
                return NULL;
        }
        return cons(car, cdr);
}

SExp *mkatom(char *str) {
        SExp *sex;

        sex = malloc(sizeof(struct SExp));
        if (sex == NULL)
                return NULL;
        sex->atom = strdup(buf);
        sex->type = TYPE_ATOM;
        return sex;
}

SExp *mkpair(SExp *car, SExp *cdr) {
        SExp *sex;

        sex = malloc(sizeof(struct SExp));
        if (sex == NULL)
                return NULL;
        car(sex) = car;
        cdr(sex) = cdr;
        sex->type = TYPE_PAIR;
        return sex;
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

int main(void) {
        SExp *sex;

        sex = parse(stdin);
        print(sex);
        printf("\n");
        cleanup(sex);

        return 0;
}

/* Return the next lexeme category from standard input. */
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
