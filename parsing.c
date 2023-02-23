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

int main(int argc, char** argv) {
    // Create the parsers.
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Infix    = mpc_new("infix");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Lispish  = mpc_new("lispish");

    // Define them with the following grammar.
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                        \
       number   : /-?([0-9]+)(\.[0-9]+)?/ ;           \
       operator : '+'                                 \
                | '-'                                 \
                | '*'                                 \
                | '/'                                 \
                | '%' ;                               \
       infix    : \"add\"                             \
                | \"sub\"                             \
                | \"mul\"                             \
                | \"div\"                             \
                | \"mod\" ;                           \
       expr     : <number>                            \
                | '(' <operator> <expr>+ ')' ;        \
       lispish  : /^/ '(' <operator> <expr>+ ')' /$/  \
                | /^/ <expr> <infix> <expr>+ /$/ ;    \
    ",
    Number, Operator, Infix, Expr, Lispish);

    // Print out the version info and exit command.
    puts("Lispish Version 0.0.0");
    puts("Press ctrl+c to quit.\n");

    // The "loop" part of REPL.
    for(;;) {
        // Use the defined readline function, which will be
        // different based on the platform.
        char* input = readline("lispish>");
        add_history(input);

        // Attempt to parse the input against the grammar we've created.
        mpc_result_t result;
        if (mpc_parse("<stdin>", input, Lispish, &result)) {
            // Success! Print the AST (Abstract Syntax Tree).
            mpc_ast_print(result.output);
            mpc_ast_delete(result.output);
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
