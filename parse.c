#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define BUFLEN 1024
#define isreserved(c) (c == ')' || c == '(' || c == '\'')
#define car(p) (p->pair[0])
#define cdr(p) (p->pair[1])

char buf[BUFLEN];

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

int readToken(void);
void print(SExp *sex);

SExp nil = {{NULL}, TYPE_NIL};

int nesting = 0;

SExp *parse() {
        SExp *atom, *pair;
        int category;

        category = readToken();
        if (category == ATOM) {
                atom = malloc(sizeof(struct SExp));
                if (atom == NULL)
                        return NULL;
                atom->atom = strdup(buf);
                atom->type = TYPE_ATOM;
                if (!nesting)
                        return atom;
        } else if (category == LPAREN) {
                nesting++;
                if (nesting == 1)
                        return parse();
                atom = parse();
        } else if (category == RPAREN) {
                nesting--;
                return &nil;
        }
        pair = malloc(sizeof(struct SExp));
        if (pair == NULL)
                return NULL;
        car(pair) = atom;
        cdr(pair) = parse();
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

        sex = parse();
        print(sex);
        printf("\n");
        cleanup(sex);
        return 0;
}

/* Return the next lexeme category from standard input. */
int readToken(void) {
        char c;
        int i;

        while (1) {
                c = getchar();
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
                        ungetc(c, stdin);
                        for (i = 0; i < BUFLEN-1; i++) {
                                c = getchar();
                                if (c == EOF)
                                        return END;
                                if (isreserved(c) || isspace(c))
                                        break;
                                buf[i] = c;
                        }
                        buf[i] = '\0';
                        if (isreserved(c))
                                ungetc(c, stdin);
                        return ATOM;
                }
        }
        return ERR;
}
