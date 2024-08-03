#include "mpc.h"

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

enum { LERR_DIV_ZERO, LERR_MOD_ZERO, LERR_BAD_OP, LERR_BAD_NUM };
enum { LVAL_NUM, LVAL_OP, LVAL_ERR, LVAL_VAR };
enum {PLUS,MINUS,MULTIPLY,DIVIDE,MODULUS};

typedef struct Variable {
  char* name;
  long value;
  struct Variable* next;
} Variable;
Variable* var_table = NULL;

typedef struct {
  int type;
  int op;
  long num;
  int err;
  char* identifier;
} lval;
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}
lval lval_op(long x) {
  lval v;
  v.type = LVAL_OP;
  v.op = x;
  return v;
}
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}
lval lval_var(char* x) {
  lval v;
  v.type = LVAL_VAR;
  v.identifier = x;
  return v;
}

///Prints appropriate errors for invalid operations
void lval_print(lval v) {
  switch (v.type) {
    case LVAL_NUM: printf("%li", v.num); break;

    case LVAL_ERR:
      if (v.err == LERR_DIV_ZERO) {
        printf("Error: Division By Zero!");
      }
      if (v.err == LERR_MOD_ZERO) {
        printf("Error: Modulus By Zero!");
      }
      if (v.err == LERR_BAD_OP)   {
        printf("Error: Invalid Operator!");
      }
      if (v.err == LERR_BAD_NUM)  {
        printf("Error: Invalid Number!");
      }
    break;
  }
}
void lval_println(lval v) { lval_print(v); putchar('\n'); }

/// Searches for the specified Variable in the stored Variable table
Variable* lookup(Variable* table, char* name) {
  Variable* var = table;
  while(var) {
    if(strcmp(var->name, name) == 0) return var;
    var = var->next;
  }
  return NULL;
}

/// Sets information for existing variable else creates a new variable
void var_table_set(char* name, long value) {
  Variable* var = lookup(var_table, name);
  if(var) {
    var->value = value;
  } else {
    Variable* new_var = malloc(sizeof(Variable));
    new_var->name = strdup(name);
    new_var->value = value;
    new_var->next = var_table;
    var_table = new_var;
  }
}

///Evaluates provided inputs
lval eval_op(lval x, lval op, lval y) {
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  switch (op.op) {
    case PLUS: return lval_num(x.num + y.num);
    case MINUS: return lval_num(x.num - y.num);
    case MULTIPLY: return lval_num(x.num * y.num);
    case DIVIDE: return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
    case MODULUS: return y.num == 0 ? lval_err(LERR_MOD_ZERO) : lval_num(x.num % y.num);
    default: return lval_err(LERR_BAD_OP);
  }
}
lval eval(mpc_ast_t* t) {

  if (strstr(t->tag, "identifier")) {
    Variable* var = lookup(var_table, t->contents);
    if (var) {
      return lval_num(var->value);
    } else {
      return lval_err(LERR_BAD_OP);
    }
  }

  if (strstr(t->tag, "operator")) {
    if (strcmp(t->contents, "+") == 0) return lval_op(0);
    if (strcmp(t->contents, "-") == 0) return lval_op(1);
    if (strcmp(t->contents, "*") == 0) return lval_op(2);
    if (strcmp(t->contents, "/") == 0) return lval_op(3);
    if (strcmp(t->contents, "%") == 0) return lval_op(4);
  }

  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  if (strstr(t->children[1]->tag, "assign")) {
    lval x = eval(t->children[1]->children[2]);
    if (x.type == LVAL_NUM) {
      var_table_set(t->children[1]->children[0]->contents, x.num);
      return x;
    }
    return lval_err(LERR_BAD_NUM);
  }


  if (strstr(t->children[1]->tag, "lemons")) {
    if(t->children[1]->children_num > 0) {
      lval x = eval(t->children[1]->children[0]);
      lval op = eval(t->children[1]->children[1]);
      lval y = eval(t->children[1]->children[2]);

      printf("%i",x.type);

      if (op.type == LVAL_OP) {
        return eval_op(x, op, y);
      }
    }
    return eval(t->children[1]);
  }

  return lval_err(LERR_BAD_OP);
}

int main(int argc, char** argv) {

  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Identifier = mpc_new("identifier");
  mpc_parser_t* Assign = mpc_new("assign");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* LemonS = mpc_new("lemons");
  mpc_parser_t* Language = mpc_new("language");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                           \
      number     : /-?[0-9]+/ ;                          \
      operator   : '+' | '-' | '*' | '/' | '%' ;          \
      identifier : /[a-zA-Z_][a-zA-Z0-9_]*/ ;              \
      assign     : <identifier> '=' <expr> ;                \
      expr       : <number>                                  \
                  | '(' <expr> <operator> <expr> ')'          \
                  | <assign>                                   \
                  | <identifier> ;                              \
      lemons      : <expr> <operator> <expr> | <expr>  ;          \
      language   : /^/ <lemons> /$/ ;                              \
    ",
    Number, Operator, Identifier, Assign, Expr, LemonS, Language);

  puts("LemonScript Version 0.0.0.0.4");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    char* input = readline("lemons> ");
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Language, &r)) {
      mpc_ast_t* root = r.output;

      //mpc_ast_print(root);
      lval_println(eval(root));

      mpc_ast_delete(root);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(7, Number, Operator, Identifier, Assign, Expr, LemonS, Language);

  Variable* var = var_table;
  while(var) {
    Variable* tmp = var;
    var = var->next;
    free(tmp->name);
    free(tmp);
  }
  return 0;
}