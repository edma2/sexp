#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Macros */
//{{{
#define BUFLEN 1024
#define BUCKETS 256
#define MAXNODES 4096

#define isreserved(c) (c == ')' || c == '(' || c == '\'')

#define car(p) (p->pair[0])
#define cdr(p) (p->pair[1])
#define cadr(p) (car(cdr(p)))
#define caddr(p) (car(cdr(cdr(p))))

enum category { QUOTE, LPAREN, RPAREN, STR, END };
//}}}

/* Data structures */
//{{{
/* S-expressions */
typedef struct SExpr SExpr;
/* Hash table buckets */
typedef struct Entry Entry;
/* Garbage collection nodes */
typedef struct Node Node;
/* Call frames */
typedef struct Frame Frame;

struct SExpr {
        union {
                char *atom;
                SExpr *pair[2];
                SExpr *(*prim)(SExpr *);
        };
        enum {ATOM, PAIR, NIL, PRIM} type;
        Frame *env;
        int live;
};

struct Entry {
        char *key;
        void *value;
        Entry *next;
};

struct Node {
        void *data;
        enum {FRAME, SEXPR} type;
};

struct Frame {
        Frame *parent;
        Entry *bindings[BUCKETS];
        int live;
};
//}}}

/* Function declarations */
//{{{
void *alloc(int type);
SExpr *apply(SExpr *op, SExpr *operands);
int atomic(SExpr *exp);
void bind_lambda(SExpr *exp, Frame *env);
int bound_lambda(SExpr *exp);
void compact(void);
int compound(SExpr *exp);
SExpr *cons(SExpr *car, SExpr *cdr);
int data_alive(Node *n);
void data_free(Node *n);
void data_unmark(Node *n);
int empty(SExpr *exp);
int env_bind(char *sym, SExpr *val, Frame *env);
SExpr *env_lookup(char *sym, Frame *env);
SExpr *eval_list(SExpr *ls, Frame *env);
SExpr *eval(SExpr *exp, Frame *env);
Frame *extend(SExpr *args, SExpr *params, Frame *env);
Entry *find(Entry *hashtab[], char *key);
void free_frame(Frame *frame);
void free_sexpr(SExpr *exp);
int hash(char *s);
void init(void);
Entry *insert(Entry *hashtab[], char *key, void *value);
int is_application(SExpr *exp);
int is_define(SExpr *exp);
int is_lambda(SExpr *exp);
int is_number(SExpr *exp);
int is_quoted(SExpr *exp);
int is_symbol(SExpr *exp);
int is_tagged_list(SExpr *ls, char *tag);
SExpr *make_atom(char *str);
SExpr *make_pair(SExpr *car, SExpr *cdr);
SExpr *make_prim(SExpr *(*prim)(SExpr *));
void mark(void);
void mark_frame(Frame *frame);
void mark_sexpr(SExpr *exp, Frame *env);
Frame *new_frame(Frame *parent);
int numeric(char *s);
SExpr *parse(FILE *f, int depth);
SExpr *prim_add(SExpr *args);
SExpr *prim_sub(SExpr *args);
SExpr *prim_mult(SExpr *args);
SExpr *prim_div(SExpr *args);
SExpr *prim_cons(SExpr *args);
SExpr *prim_cdr(SExpr *args);
SExpr *prim_car(SExpr *args);
void print(SExpr *exp);
int read_token(FILE *f);
int primitive(SExpr *exp);
void sweep(void);
//}}}

/* Globals */
//{{{
char    buf[BUFLEN];            /* Token buffer */
int     eof;                    /* EOF flag */
Frame   *global;                /* Global environment */
Node    Nodes[MAXNODES];        /* Heap references */
int     MaxIndex = 0;           /* Next free Node */
SExpr   nil = {.type = NIL};    /* Empty list */
//}}}

/* Function definitions */
//{{{
void mark(void) {
        mark_frame(global);
}

SExpr *make_atom(char *s) {
        SExpr *exp;

        exp = alloc(SEXPR);
        if (exp == NULL)
                return NULL;
        exp->atom = strdup(s);
        if (exp->atom == NULL)
                return NULL;
        exp->type = ATOM;
        return exp;
}

SExpr *make_pair(SExpr *car, SExpr *cdr) {
        SExpr *exp;

        exp = alloc(SEXPR);
        if (exp == NULL)
                return NULL;
        car(exp) = car;
        cdr(exp) = cdr;
        exp->type = PAIR;
        return exp;
}

SExpr *make_prim(SExpr *(*prim)(SExpr *)) {
        SExpr *exp;

        exp = alloc(SEXPR);
        if (exp == NULL)
                return NULL;
        exp->prim = prim;
        exp->type = PRIM;
        return exp;
}

SExpr *cons(SExpr *car, SExpr *cdr) {
        if (car == NULL || cdr == NULL)
                return NULL;
        return make_pair(car, cdr);
}

SExpr *parse(FILE *f, int depth) {
        int category;
        SExpr *car, *cdr;

        category = read_token(f);
        if (category == LPAREN) {
                car = parse(f, depth+1);
        } else if (category == RPAREN) {
                return depth ? &nil : NULL;
        } else if (category == QUOTE) {
                car = cons(make_atom("quote"), cons(parse(f, 0), &nil));
        } else if (category == STR) {
                car = make_atom(buf);
        } else {
                eof = 1;
                return NULL;
        }
        if (!depth)
                return car;
        cdr = parse(f, depth);
        return cons(car, cdr);
}

SExpr *eval_list(SExpr *ls, Frame *env) {
        SExpr *car, *cdr;

        if (empty(ls))
                return &nil;
        car = eval(car(ls), env);
        cdr = eval_list(cdr(ls), env);
        return cons(car, cdr);
}

SExpr *eval(SExpr *exp, Frame *env) {
        SExpr *val, *op, *operands;

        if (empty(exp))
                return &nil;
        if (is_number(exp))
                return exp;
        if (is_quoted(exp))
                return cadr(exp);
        if (is_symbol(exp)) {
                for (; env != NULL; env = env->parent) {
                        val = env_lookup(exp->atom, env);
                        if (val != NULL)
                                break;
                }
                if (env == NULL) {
                        fprintf(stderr, "Unknown symbol: %s\n", exp->atom);
                        return NULL;
                }
                return val;
        }
        if (is_lambda(exp)) {
                bind_lambda(exp, env);
                return exp;
        }
        if (is_define(exp)) {
                val = eval(caddr(exp), env);
                if (val == NULL)
                        return NULL;
                if (!env_bind(cadr(exp)->atom, val, env))
                        return NULL;
                return &nil;
        }
        if (is_application(exp)) {
                op = eval(car(exp), env);
                operands = eval_list(cdr(exp), env);
                if (op == NULL || operands == NULL)
                        return NULL;
                return apply(op, operands);
        }
        fprintf(stderr, "Unknown expression!\n");
        return NULL;
}

SExpr *apply(SExpr *op, SExpr *operands) {
        Frame *env;
        SExpr *result;

        if (primitive(op))
                return op->prim(operands);
        env = extend(cadr(op), operands, op->env);
        if (env == NULL)
                return NULL;
        result = eval(caddr(op), env);
        return result;
}

Frame *new_frame(Frame *parent) {
        Frame *frame;

        frame = alloc(FRAME);
        if (frame == NULL)
                return NULL;
        frame->parent = parent;
        return frame;
}

Frame *extend(SExpr *params, SExpr *args, Frame *env) {
        Frame *frame;

        frame = new_frame(env);
        if (frame == NULL)
                return NULL;
        for (; args != &nil; args = cdr(args)) {
                if (!env_bind(car(params)->atom, car(args), frame))
                        break;
                params = cdr(params);
        }
        return frame;
}

void free_frame(Frame *frame) {
        int i;
        Entry *en, *next;

        for (i = 0; i < BUCKETS; i++) {
                for (en = frame->bindings[i]; en != NULL; en = next) {
                        next = en->next;
                        free(en->key);
                        free(en);
                }
        }
        free(frame);
}

void free_sexpr(SExpr *exp) {
        if (exp->type == NIL)
                return;
        if (exp->type == ATOM)
                free(exp->atom);
        free(exp);
}

SExpr *prim_add(SExpr *args) {
        int sum;

        for (sum = 0; !empty(args); args = cdr(args))
                sum += atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", sum);
        return make_atom(buf);
}

SExpr *prim_sub(SExpr *args) {
        int sum;

        for (sum = 0; !empty(args); args = cdr(args))
                sum -= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", sum);
        return make_atom(buf);
}

SExpr *prim_mult(SExpr *args) {
        int prod;

        for (prod = 1; !empty(args); args = cdr(args))
                prod *= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", prod);
        return make_atom(buf);
}

SExpr *prim_div(SExpr *args) {
        int quot;

        quot = atoi(car(args)->atom);
        args = cdr(args);
        for (; !empty(args); args = cdr(args))
                quot /= atoi(car(args)->atom);
        snprintf(buf, BUFLEN, "%d", quot);
        return make_atom(buf);
}

SExpr *prim_cons(SExpr *args) {
        return cons(car(args), cadr(args));
}

SExpr *prim_car(SExpr *args) {
        return car(car(args));
}

SExpr *prim_cdr(SExpr *args) {
        return cdr(car(args));
}

void init(void) {
        global = new_frame(NULL);
        insert(global->bindings, "+", make_prim(prim_add));
        insert(global->bindings, "-", make_prim(prim_sub));
        insert(global->bindings, "*", make_prim(prim_mult));
        insert(global->bindings, "/", make_prim(prim_div));
        insert(global->bindings, "cons", make_prim(prim_cons));
        insert(global->bindings, "car", make_prim(prim_car));
        insert(global->bindings, "cdr", make_prim(prim_cdr));
}

SExpr *env_lookup(char *sym, Frame *env) {
        Entry *en;

        en = find(env->bindings, sym);
        return en == NULL ? NULL : en->value;
}

int env_bind(char *sym, SExpr *val, Frame *env) {
        Entry *en;

        en = find(env->bindings, sym);
        if (en == NULL)
                en = insert(env->bindings, sym, val);
        else
                en->value = val;
        return en != NULL;
}

int atomic(SExpr *exp) {
        return exp->type == ATOM;
}

int empty(SExpr *exp) {
        return exp->type == NIL;
}

int compound(SExpr *exp) {
        return exp->type == PAIR;
}

int primitive(SExpr *exp) {
        return exp->type == PRIM;
}

int numeric(char *s) {
        for (; *s != '\0'; s++) {
                if (!isdigit(*s))
                        break;
        }
        return *s == '\0';
}

int is_application(SExpr *exp) {
        return compound(exp);
}

int is_number(SExpr *exp) {
        return atomic(exp) && numeric(exp->atom);
}

int is_symbol(SExpr *exp) {
        return atomic(exp) && !numeric(exp->atom);
}

int is_define(SExpr *exp) {
        return is_tagged_list(exp, "define");
}

int is_quoted(SExpr *exp) {
        return is_tagged_list(exp, "quote");
}

int is_lambda(SExpr *exp) {
        return is_tagged_list(exp, "lambda");
}

int is_tagged_list(SExpr *ls, char *tag) {
        return compound(ls) && atomic(car(ls)) && !strcmp(car(ls)->atom, tag);
}

void print(SExpr *exp) {
        if (atomic(exp)) {
                printf("%s", exp->atom);
        } else if (empty(exp)) {
                printf("()");
        } else if (compound(exp)) {
                printf("(");
                print(car(exp));
                printf(".");
                print(cdr(exp));
                printf(")");
        } else if (primitive(exp)) {
                printf("<built-in>");
        }
}

int read_token(FILE *f) {
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
                break;
        }
        for (i = 0; i < BUFLEN-1; i++) {
                buf[i] = c;
                c = getc(f);
                if (c == EOF)
                        return END;
                if (isreserved(c) || isspace(c))
                        break;
        }
        buf[i+1] = '\0';
        if (isreserved(c))
                ungetc(c, f);
        return STR;
}

int hash(char *s) {
        int h;

        for (h = 0; *s != '\0'; s++)
                h += (h * 37 + *s);
        return h % BUCKETS;
}

Entry *find(Entry *hashtab[], char *key) {
        int h;
        Entry *ep;

        h = hash(key);
        for (ep = hashtab[h]; ep != NULL; ep = ep->next) {
                if (!strcmp(key, ep->key))
                        break;
        }
        return ep;
}

Entry *insert(Entry *hashtab[], char *key, void *value) {
        int h;
        Entry *e;

        e = malloc(sizeof(Entry));
        if (e == NULL)
                return NULL;
        e->key = strndup(key, strlen(key));
        if (e->key == NULL) {
                free(e);
                return NULL;
        }
        h = hash(key);
        e->value = value;
        e->next = hashtab[h];
        hashtab[h] = e;
        return e;
}

void *alloc(int type) {
        void *data;
        Node *n;

        if (type == FRAME)
                data = calloc(1, sizeof(Frame));
        else
                data = calloc(1, sizeof(SExpr));
        if (data == NULL)
                return NULL;
        n = &Nodes[MaxIndex++];
        n->data = data;
        n->type = type;
        return data;
}

void mark_sexpr(SExpr *exp, Frame *env) {
        /* Avoids circular traversals */
        if (exp->live)
                return;
        exp->live = 1;
        if (exp->type == PAIR) {
                mark_sexpr(car(exp), env);
                mark_sexpr(cdr(exp), env);
                if (bound_lambda(exp))
                        mark_frame(exp->env);
        }
}

int bound_lambda(SExpr *exp) {
        return is_lambda(exp) && exp->env != NULL;
}

void bind_lambda(SExpr *exp, Frame *env) {
        exp->env = env;
}

void mark_frame(Frame *frame) {
        int i;
        Entry *en;

        if (frame->live)
                return;
        frame->live = 1;
        for (i = 0; i < BUCKETS; i++) {
                for (en = frame->bindings[i]; en != NULL; en = en->next)
                        mark_sexpr(en->value, frame);
        }
}

void sweep(void) {
        int i;
        Node *n;

        for (i = 0; i < MaxIndex; i++) {
                n = &Nodes[i];
                if (data_alive(n)) {
                        data_unmark(n);
                } else {
                        data_free(n);
                        n->data = NULL;
                }
        }
}

void compact(void) {
        Node tmp[MaxIndex];
        int i, j = 0;

        for (i = 0; i < MaxIndex; i++) {
                if (Nodes[i].data != NULL)
                        tmp[j++] = Nodes[i];
        }
        for (i = 0; i < j; i++)
                Nodes[i] = tmp[i];
        MaxIndex = j;
}

int data_alive(Node *n) {
        if (n->type == FRAME)
                return ((Frame *)n->data)->live;
        return ((SExpr *)n->data)->live;
}

void data_unmark(Node *n) {
        if (n->type == FRAME)
                ((Frame *)n->data)->live = 0;
        else
                ((SExpr *)n->data)->live = 0;
}

void data_free(Node *n) {
        if (n->type == FRAME)
                free_frame(n->data);
        else
                free_sexpr(n->data);
}
//}}}

int main(void) {
        SExpr *input, *result;

        init();
        eof = 0;
        while (!eof) {
                input = parse(stdin, 0);
                if (input == NULL)
                        continue;
                result = eval(input, global);
                if (result == NULL)
                        continue;
                print(result);
                printf("\n");
                mark();
                sweep();
                compact();
        }
        sweep();
        return 0;
}
