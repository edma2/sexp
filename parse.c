#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define BUFLEN 1024
#define isreserved(c) (c == ')' || c == '(' || c == '\'')
#define car(p) (p->pair[0])
#define cdr(p) (p->pair[1])

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

int readToken(FILE *f);
SExp *parse(FILE *f);
void print(SExp *sex);
void cleanup(SExp *sex);

int nesting = 0;
char buf[BUFLEN];

SExp *parse(FILE *f) {
        SExp *car, *pair;
        int category;

        category = readToken(f);
        if (category == ATOM) {
                car = malloc(sizeof(struct SExp));
                if (car == NULL)
                        return NULL;
                car->atom = strdup(buf);
                car->type = TYPE_ATOM;
                if (!nesting)
                        return car;
        } else if (category == LPAREN) {
                nesting++;
                if (nesting == 1)
                        return parse(f);
                car = parse(f);
        } else if (category == RPAREN) {
                nesting--;
                return &nil;
        }
        pair = malloc(sizeof(struct SExp));
        if (pair == NULL)
                return NULL;
        car(pair) = car;
        cdr(pair) = parse(f);
        pair->type = TYPE_PAIR;
        return pair;
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
