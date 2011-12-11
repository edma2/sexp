#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define BUFLEN 1024
#define isreserved(c) (c == ')' || c == '(' || c == '\'')

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

SExp *car(SExp *sex) {
        return sex->pair[0];
}

SExp *cdr(SExp *sex) {
        return sex->pair[1];
}

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
                if (!nesting) {
                        return atom;
                } else {
                        pair = malloc(sizeof(struct SExp));
                        if (pair == NULL)
                                return NULL;
                        pair->pair[0] = atom;
                        pair->pair[1] = parse();
                        pair->type = TYPE_PAIR;
                        return pair;
                }
        } else if (category == LPAREN) {
                nesting++;
                if (nesting == 1) {
                        return parse();
                } else {
                        pair = malloc(sizeof(struct SExp));
                        if (pair == NULL)
                                return NULL;
                        pair->pair[0] = parse();
                        pair->pair[1] = parse();
                        pair->type = TYPE_PAIR;
                        return pair;
                }
        } else if (category == RPAREN) {
                nesting--;
                return &nil;
        }
        return NULL;
}

void print(SExp *sex) {
        if (sex->type == TYPE_ATOM) {
                printf("%s", sex->atom);
        } else if (sex->type == TYPE_NIL) {
                printf("()");
        } else {
                printf("(");
                print(sex->pair[0]);
                printf(".");
                print(sex->pair[1]);
                printf(")");
        }
}

int main(void) {
        print(parse());
        printf("\n");
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
