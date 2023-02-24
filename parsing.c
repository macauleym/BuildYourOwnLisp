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

// Declare a struct to use as a result.
typedef struct {
    union {
        struct { union { long num; double numd; }; } right;
        struct { int err;  } left ;
    };
    int type;
} result;

// Enum to give result values names.
enum RESULT_TYPE { RESULT_RIGHT, RESULT_LEFT };

// Enum to give names to error type values.
enum RERR_TYPE { RERR_DIV_ZERO, RERR_BAD_OP, RERR_BAD_NUM };

// Functions to construct the valid (right) result
// and the error (left) result.
result result_right(long x) {
    result r;
    r.type = RESULT_RIGHT;
    r.right.num  = x;

    return r;
}

result result_left(int x) {
    result r;
    r.type = RESULT_LEFT;
    r.left.err  = x;

    return r;
}

char* build_err_msg(char* msg) {
    return strcat("ERROR :: ", msg);
}

void result_print(result r) {
    switch (r.type) {
        // If we get a valid result num, just print it and break.
        case RESULT_RIGHT:
            printf("%li", r.right.num);
        break;

        // If we get an error, we need to also check what type
        // of error we got.
        case RESULT_LEFT:
            if (r.left.err == RERR_DIV_ZERO) {
                printf("ERROR :: Division by Zero!");
            }

            if (r.left.err == RERR_BAD_OP) {
                printf("ERROR :: Invalid Operator!");
            }

            if (r.left.err == RERR_BAD_NUM) {
                printf("ERROR :: Invalid Number!");
            }
        break;
    }
}

void result_println(result r) {
    result_print(r);
    putchar('\n');
}

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

// Use operator string to see which operation to perform.
result evaluate_op(result leftOperand, char* op, result rightOperand) {
    long x = leftOperand.right.num;
    long y = rightOperand.right.num;

    result opResult = result_left(RERR_BAD_OP);

    // Operators.
    if (strcmp(op, ADD) == 0) { opResult = result_right(x + y);         }
    if (strcmp(op, SUB) == 0) { opResult = result_right(x - y);         }
    if (strcmp(op, MUL) == 0) { opResult = result_right(x * y);         }
    if (strcmp(op, DIV) == 0) { opResult = y == 0
                                         ? result_left(RERR_DIV_ZERO)
                                         : result_right(x / y);         }
    if (strcmp(op, MOD) == 0) { opResult = result_right(x % y);         }
    if (strcmp(op, POW) == 0) { opResult = result_right(powl(x, y));    }


    // Builtin functions.
    if (strcmp(op, MIN) == 0) { opResult = result_right(x < y ? x : y); }
    if (strcmp(op, MAX) == 0) { opResult = result_right(x > y ? x : y); }

    return opResult;
}

result evaluate_ast(mpc_ast_t* pTree) {
    // Base case, if tag is 'number' return it.
    if (strstr(pTree->tag, "number")) {
        // Check for conversion errors.
        errno = 0;
        long num = strtol(pTree->contents, NULL, 10);
        return errno != ERANGE
            ? result_right(num)
            : result_left(RERR_BAD_NUM);
    }

    if (strstr(pTree->tag, "expr")) {
        printf("node is an expr!\n");
    }

    // We've got a parent branch, determine the operator.
    // We add to the offset if the current contents is a '(' character.
    // We also ignore the last index for the same reason (but for the ')' character).
    long next = 0;
    char* op = "";
    for (;;) {
        op = pTree->children[next]->contents;
        if (strcmp(op, "(") == 0 || strcmp(op, "") == 0) {
            next++;
        } else {
            break;
        }
    }

    // Store the next child.
    result x = evaluate_ast(pTree->children[next+1]);

    // Iterate the remaining children.
    int i = next+2;
    int hasOperated = 0;
    while (strstr(pTree->children[i]->tag, "expr")) {
        x = evaluate_op(x, op, evaluate_ast(pTree->children[i]));
        i++;
        hasOperated = 1;
    }

    // If we were given a '-' with only 1 argument, negate the argument.
    if (strcmp(op, SUB) == 0 && hasOperated == 0) {
        return result_right(-x.right.num);
    }

    return x;
}

int main(int argc, char** argv) {
    // Create the parsers.
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Infix    = mpc_new("infix");
    mpc_parser_t* Builtin  = mpc_new("builtin");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Lispish  = mpc_new("lispish");

    // Define them with the following grammar.
    // (\.[0-9]+)?
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                       \
       number   : /-?[0-9]+/ ;                       \
       operator : '+'                                \
                | '-'                                \
                | '*'                                \
                | '/'                                \
                | '%'                                \
                | '^' ;                              \
       infix    : \"add\"                            \
                | \"sub\"                            \
                | \"mul\"                            \
                | \"div\"                            \
                | \"mod\" ;                          \
       builtin  : \"min\"                            \
                | \"max\" ;                          \
       expr     : <number>                           \
                | '(' <operator> <expr>+ ')'         \
                | '(' <builtin> <expr>+ ')' ;        \
       lispish  : /^/ '(' <operator> <expr>+ ')' /$/ \
                | /^/ '(' <builtin> <expr>+ ')'/$/   \
                | /^/ <expr> <infix> <expr>+ /$/ ;   \
    ",
    Number, Operator, Infix, Builtin, Expr, Lispish);

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
            result evaluated = evaluate_ast(ast);
            result_println(evaluated);

            // Print the AST (Abstract Syntax Tree).
            mpc_ast_print(ast);

            // Delete the output.
            mpc_ast_delete(ast);
        } else {
            // Error! Failed to parse the input.
            mpc_err_print(res.error);
            mpc_err_delete(res.error);
        }

        // malloc() was called, so we need to free().
        free(input);
    }

    mpc_cleanup(4, Number, Operator, Expr, Lispish);

    return 0;
}
