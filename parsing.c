#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

// Only on Windows.
#ifdef _WIN32
#include <string.h>

// The input buffer for user input.
static char buffer[2048];

// Fake readline function for now.
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';

    return cpy;
}

// Fake addHistory.
void add_history(char* unused) { }

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

#define ADD "+"
#define SUB "-"
#define MUL "*"
#define DIV "/"
#define MOD "%"
#define POW "^"

#define MIN "min"
#define MAX "max"

/*
 * Definition and helper methods for the `sval` struct.
 */

// Declare a struct to use to represent our s-expression.
typedef struct sval {
    int type;

    union {
        struct { long val; double val_d;} num;
        struct { char* msg; } err;
        struct { char* c; } sym;
    };

    int count;
    struct sval** cell;
} sval;

// Enum to give result values names.
enum RESULT_TYPE { SVAL_NUM, SVAL_NUM_D, SVAL_ERR, SVAL_SYM, SVAL_SEXPR };

sval* sval_num(long x) {
    sval* v    = malloc(sizeof(sval));
    v->type    = SVAL_NUM;
    v->num.val = x;

    return v;
}

sval* sval_num_d(double  x) {
    sval* v      = malloc(sizeof(sval));
    v->type      = SVAL_NUM_D;
    v->num.val_d = x;

    return v;
}

sval* sval_err(char* m) {
    sval* v     = malloc(sizeof(sval));
    v->type     = SVAL_ERR;
    v->err.msg  = malloc(strlen(m) + 1);
    // strlen does NOT account for the null termination
    // '\0' character, so we add 1 to account for it.

    strcpy(v->err.msg, m);

    return v;
}

sval* sval_sym(char* s) {
    sval* v  = malloc(sizeof(sval));
    v->type  = SVAL_SYM;
    v->sym.c = malloc(strlen(s) + 1);
    strcpy(v->sym.c, s);

    return v;
}

sval* sval_sexpr(void) {
    sval* v  = malloc(sizeof(sval));
    v->type  = SVAL_SEXPR;
    v->count = 0;
    v->cell  = NULL;

    return v;
}

void sval_del(sval* s) {
    switch(s->type) {
        // We didn't allocate anything in the NUM case.
        case SVAL_NUM:
        case SVAL_NUM_D:
            break;

        // For symbols and errors, free the strings.
        case SVAL_ERR:
            free(s->err.msg);
        break;
        case SVAL_SYM:
            free(s->sym.c);
        break;

        // for S-Expressions, free all the inner elements...
        case SVAL_SEXPR:
            for (int i = 0; i < s->count; i++){
                sval_del(s->cell[i]);
            }

            // Then free the top-level pointers.
            free(s->cell);
        break;
    }

    // Now free the memory for the "sval" struct itself.
    free(s);
}

/*
 * Construct the S-Expr collection from the AST.
 */

sval* sval_read_num(mpc_ast_t* pTree) {
    errno = 0;
    if (strstr(pTree->contents, ".")) {
        double x = strtod(pTree->contents, NULL);

        return errno != ERANGE
               ? sval_num_d(x)
               : sval_err("invalid number");
    }

    long x = strtol(pTree->contents, NULL, 10);

    return errno != ERANGE
        ? sval_num(x)
        : sval_err("invalid number");
}

sval* sval_append(sval* fst, sval* snd) {
    fst->count++;
    fst->cell = realloc(fst->cell, sizeof(sval*) * fst->count);
    fst->cell[fst->count-1] = snd;

    return fst;
}

sval* sval_read(mpc_ast_t* pTree) {
    // If symbol or number, return the conversion
    // to that type.
    if (strstr(pTree->tag, "number")) {
        return sval_read_num(pTree);
    }

    if (strstr(pTree->tag, "symbol")){
        return sval_sym(pTree->contents);
    }

    // If we're at the root (>) or an s-expr then
    // create an empty list.
    sval* x = NULL;
    if (strcmp(pTree->tag, ">") == 0
    ||  strstr(pTree->tag, "sexpr"))
    { x = sval_sexpr(); }

    // Fill this new list with any valid expression within.
    for (int i = 0; i < pTree->children_num; i++) {
        if (strcmp(pTree->children[i]->contents, "(") == 0
        ||  strcmp(pTree->children[i]->contents, ")") == 0
        ||  strcmp(pTree->children[i]->tag, "regex")  == 0)
        { continue; }

        x = sval_append(x, sval_read(pTree->children[i]));
    }

    return x;
}

/*
 * Functions to evaluate the s-expression structure.
 */

/*
 * Pops the sval from the expression tree at the
 * i'th index. Shifting all other elements, and returning
 * the value that was popped.
 */
sval* sval_pop(sval* v, int i) {
    // Find the element desired by 'i'.
    sval* e = v->cell[i];

    // Shift the memory to account for
    // removal of the element.
    memmove(&v->cell[i], &v->cell[i+1],
            sizeof(sval*) * (v->count-i-1));

    // Decrement the count in the list.
    v->count--;

    // Reallocate the memory used.
    v->cell = realloc(v->cell, sizeof(sval*) * v->count);

    return e;
}

/*
 * Takes a single sval; returning it and deleting
 * the rest of the collection it's from.
 */
sval* sval_take(sval* v, int i) {
    sval* e = sval_pop(v, i);
    sval_del(v);

    return e;
}

double apply_op(char* op, double x, double y, char** hasErr) {
    // Arithmetic operations.
    if (strcmp(op, ADD) == 0) { return x + y; }
    if (strcmp(op, SUB) == 0) { return x - y; }
    if (strcmp(op, MUL) == 0) { return x * y; }
    if (strcmp(op, DIV) == 0) {
        if (y == 0) {
            *hasErr = "Cannot divide by 0!";
            return -1;
        }

        return x / y;
    }
    if (strcmp(op, POW) == 0) { return powl(x, y); }


    // Builtin functions.
    if (strcmp(op, MIN) == 0) { return x < y ? x : y; }
    if (strcmp(op, MAX) == 0) { return x > y ? x : y; }

    *hasErr = "Given an invalid op!";
    return -1;
}

long apply_op_l(char* op, long x, long y, char** hasErr) {
    if (strcmp(op, MOD) == 0) { return x % y; }

    return (long)apply_op(op, x, y, hasErr);
}

sval* builtin_op(sval* a, char* op) {
    // Ensure all arguments are numbers.
    for (int i = 0; i < a-> count; i++) {
        if (a->cell[i]->type != SVAL_NUM
        &&  a->cell[i]->type != SVAL_NUM_D){
            sval_del(a);
            return sval_err("Expected a number to operate on!");
        }
    }

    // Pop the first element to start evaluation.
    sval* fst = sval_pop(a, 0);

    // If there are no arguments, and we're attempting
    // a SUB operation, apply unary negation.
    if (strcmp(op, SUB) == 0 && a->count == 0) {
        fst->num.val = -fst->num.val;
    }

    // Work on each element remaining.
    while (a->count > 0) {
        // Pop the element to work on it.
        sval* snd = sval_pop(a, 0);

        char* err = "";
        if (fst->type == SVAL_NUM_D) {
            fst->num.val_d = apply_op(op, fst->num.val_d, snd->num.val_d, &err);
        } else {
            fst->num.val = apply_op_l(op, fst->num.val, snd->num.val, &err);
        }

        if (strlen(err) > 0){
            sval_del(fst);
            sval_del(snd);
            fst = sval_err(err);

            break;
        }

        // Done with the second operand.
        sval_del(snd);
    }

    // Done with original expression.
    sval_del(a);

    return fst;
}

// Forward declare the s-expression eval function.
sval* sval_eval_sexpr(sval* v);

sval* sval_eval(sval* v) {
    // Evaluate the s-expression
    if (v->type == SVAL_SEXPR) {
        return sval_eval_sexpr(v);
    }

    return v;
}

sval* sval_eval_sexpr(sval* v) {
    // Evaluate the children.
    for (int i = 0; i < v->count; i++) {
        // Had a SEGFAULT here for longer than I'd like to admit.
        // The function called here should be `sval_eval` and NOT `sval_eval_sexpr`.
        // When we eval the child element, we need to start at the beginning,
        // which is first checking if it's an s-expression value.
        v->cell[i] = sval_eval(v->cell[i]);
    }

    // Check for errors.
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == SVAL_ERR) {
            return sval_take(v, i);
        }
    }

    // Empty expression.
    if (v->count == 0) {
        return v;
    }

    // Single expression.
    if (v->count == 1) {
        return sval_take(v, 0);
    }

    // Ensure first element is a symbol.
    sval* fst = sval_pop(v, 0);
    if (fst->type != SVAL_SYM) {
        sval_del(fst);
        sval_del(v);

        return sval_err("S-Expression did not start with a symbol!");
    }

    // Call the operator function.
    sval* result = builtin_op(v, fst->sym.c);
    sval_del(fst);

    return result;
}

/*
 * Functions to print out the s-expression structure.
 */

// Forward declare, since the 2 print
// functions call each other.
void sval_print(sval* s);

void sval_expr_print(sval* s, char open, char close) {
    putchar(open);
    for (int i = 0; i < s->count; i++) {
        // Print the value in the cell.
        sval_print(s->cell[i]);

        // Skip the trailing space if we're on the last element.
        if (i != (s->count-1)){
            putchar(' ');
        }
    }

    putchar(close);
}

void sval_print(sval* s) {
    switch(s->type) {
        case SVAL_NUM:
            printf("%li", s->num.val);
        break;
        case SVAL_NUM_D:
            printf("%f", s->num.val_d);
        break;
        case SVAL_ERR:
            printf("ERROR: %s", s->err.msg);
        break;
        case SVAL_SYM:
            printf("%s", s->sym.c);
        break;
        case SVAL_SEXPR:
            sval_expr_print(s, '(', ')');
        break;
    }
}

void sval_println(sval* s) {
    sval_print(s);
    putchar('\n');
}

/*
int number_of_nodes(mpc_ast_t* pTree) {
    // Base case, when there are no children.
    if (pTree->children_num == 0) { return 1; }

    // Recurse case, where we have children to process.
    if (pTree->children_num >= 1) {
        int total = 1;
        for (int index = 0; index < pTree->children_num; index++){
            total = total + number_of_nodes(pTree->children[index]);
        }

        return total;
    }

    // Default case.
    return 0;
}
*/

int main(int argc, char** argv) {
    // Create the parsers.
    mpc_parser_t* Number  = mpc_new("number");
    mpc_parser_t* Symbol  = mpc_new("symbol");
    mpc_parser_t* Infix   = mpc_new("infix");
    mpc_parser_t* Builtin = mpc_new("builtin");
    mpc_parser_t* Sexpr   = mpc_new("sexpr");
    mpc_parser_t* Expr    = mpc_new("expr");
    mpc_parser_t* Lispish = mpc_new("lispish");

    // Define them with the following grammar.
    // (\.[0-9]+)?
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                          \
       number   : /-?[0-9]+(\\.[0-9]+)?/ ;                                          \
       symbol   : '+' | '-' | '*' | '/' | '%' | '^' ;                   \
       infix    : \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" ;     \
       builtin  : \"min\" | \"max\" ;                                   \
       sexpr    : '(' <expr>* ')' ;                                     \
       expr     : <number> | <symbol> | <infix> | <builtin> | <sexpr> ; \
       lispish  : /^/ <expr>* /$/;                                      \
    ",
    Number, Symbol, Infix, Builtin, Sexpr, Expr, Lispish);

    // Print out the version info and exit command.
    puts("Lispish Version 0.0.0\n");
    puts("Press ctrl+c to quit.\n");

    // The "loop" part of REPL.
    for(;;) {
        // Use the defined readline function, which will be
        // different based on the platform.
        char* input = readline("lispish> ");
        add_history(input);

        // Attempt to parse the input against the grammar we've created.
        mpc_result_t res;
        if (mpc_parse("<stdin>", input, Lispish, &res)) {
            // Success!
            // Load the AST from the output.
            mpc_ast_t* ast = res.output;
            sval* evaluated = sval_eval(sval_read(ast));
            sval_println(evaluated);
            sval_del(evaluated);
        } else {
            // Error! Failed to parse the input.
            mpc_err_print(res.error);
            mpc_err_delete(res.error);
        }

        // malloc() was called, so we need to free().
        free(input);
    }

    mpc_cleanup(7, Number, Symbol, Infix, Builtin, Sexpr, Expr, Lispish);

    return 0;
}
