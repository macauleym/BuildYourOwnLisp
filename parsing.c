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
long evaluate_op(long x, char* op, long y) {
    printf("evaluating op as: %li %s %li\n", x, op, y);
    long opResult = 0;
    if (strcmp(op, ADD) == 0) { opResult = x + y;         }
    if (strcmp(op, SUB) == 0) { opResult = x - y;         }
    if (strcmp(op, MUL) == 0) { opResult = x * y;         }
    if (strcmp(op, DIV) == 0) { opResult = x / y;         }
    if (strcmp(op, MOD) == 0) { opResult = x % y;         }
    if (strcmp(op, POW) == 0) { opResult = powl(x, y);    }
    if (strcmp(op, MIN) == 0) { opResult = x < y ? x : y; }
    if (strcmp(op, MAX) == 0) { opResult = x > y ? x : y; }

    return opResult;
}

long evaluate_ast(mpc_ast_t* pTree) {
    // Base case, if tag is 'number' return it.
    if (strstr(pTree->tag, "number")) {
        long num = atoi(pTree->contents);
        // atoi = ascii(?) to integer
        printf("hit a number! returning %li\n", num);
        return num;
    }

    if (strstr(pTree->tag, "expr")) {
        printf("node is an expr!\n");
    }

    // Process builtin function

    // We've got a parent branch, determine the operator.
    // We add to the offset if the current contents is a '(' character.
    // We also ignore the last index for the same reason (but for the ')' character).
    long next = 0;
    char* op = "";
    for (;;) {
        op = pTree->children[next]->contents;
        printf("value of op is: (->%s<-)\n", op);
        if (strcmp(op, "(") == 0 || strcmp(op, "") == 0) {
            next++;
            printf("op is not valid, incrementing next\n");
        } else {
            printf("op is valid, breaking\n");
            break;
        }
    }

    printf("next value: %li\n", next);

    // Store the next child.
    long x = evaluate_ast(pTree->children[next+1]);
    printf("value of 'x': %li\n", x);

    // Iterate the remaining children.
    int i = next+2;
    int hasOperated = 0;
    printf("iterating children...\n");
    while (strstr(pTree->children[i]->tag, "expr")) {
        printf("Evaluating child #%li\n", i);
        x = evaluate_op(x, op, evaluate_ast(pTree->children[i]));
        i++;
        hasOperated = 1;
    }

    // If we were given a '-' with only 1 argument, negate the argument.
    if (strcmp(op, SUB) == 0 && hasOperated == 0) {
        printf("a '-' was given a single argument! returning: %li\n", -x);
        return -x;
    }

    printf("no more processing for current tree! returning: %li\n", x);
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
        mpc_result_t result;
        if (mpc_parse("<stdin>", input, Lispish, &result)) {
            // Success!
            // Load the AST from the output.
            mpc_ast_t* ast = result.output;
            long evaluated = evaluate_ast(ast);
            printf("%li\n", evaluated);

            // Print the AST (Abstract Syntax Tree).
            mpc_ast_print(ast);

            // Delete the output.
            mpc_ast_delete(ast);
        } else {
            // Error! Failed to parse the input.
            mpc_err_print(result.error);
            mpc_err_delete(result.error);
        }

        // malloc() was called, so we need to free().
        free(input);
    }

    mpc_cleanup(4, Number, Operator, Expr, Lispish);

    return 0;
}
