#include "mpc.h"
#include "math.h"

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

enum { LERR_DIV_ZERO, LERR_MOD_ZERO, LERR_BAD_OP, LERR_BAD_NUM, LERR_BAD_VAR};
enum { LVAL_NUM, LVAL_OP, LVAL_ERR, LVAL_VAR };
enum { PLUS, MINUS, MULTIPLY, DIVIDE, MODULUS, EXPONENTIAL, FLOORDIV};

typedef struct Variable {
  char* name;
  struct Variable* next;
  mpc_ast_t* val;
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
lval lval_op(int x) {
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

    case LVAL_VAR: printf("%s", v.identifier);

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
      if (v.err == LERR_BAD_VAR)  {
        printf("Error: Undefined Variable!");
      }
    break;
  }
}
void lval_println(lval v) { printf("> "); lval_print(v); putchar('\n'); }

/// Searches for the specified Variable in the stored Variable table
Variable* lookup(Variable* table, char* name) {
  Variable* var = table;
  while(var) {
    if(strcmp(var->name, name) == 0) return var;
    var = var->next;
  }
  return NULL;
}

/// Creates a copy of the parsed expression
mpc_ast_t* mpc_ast_copy(mpc_ast_t* ast) {
  if (ast == NULL) return NULL;

  mpc_ast_t* new_ast = malloc(sizeof(mpc_ast_t));
  if (!new_ast) return NULL;

  new_ast->tag = strdup(ast->tag);
  new_ast->contents = strdup(ast->contents);
  new_ast->children_num = ast->children_num;
  new_ast->children = malloc(sizeof(mpc_ast_t*) * new_ast->children_num);

  for (size_t i = 0; i < new_ast->children_num; i++) {
    new_ast->children[i] = mpc_ast_copy(ast->children[i]);
  }

  return new_ast;
}

/// Sets information for existing variable else creates a new variable
void var_table_set(char* name, mpc_ast_t* value) {
  Variable* var = lookup(var_table, name);
  if(var) {
    mpc_ast_delete(var->val);
    var->val = mpc_ast_copy(value);
  } else {
    Variable* new_var = malloc(sizeof(Variable));
    new_var->name = strdup(name);
    new_var->val = mpc_ast_copy(value);
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
    case EXPONENTIAL: return y.num == 0 ? lval_err(LERR_MOD_ZERO) : lval_num(pow(x.num, y.num));
    default: return lval_err(LERR_BAD_OP);
  }
}
lval eval(mpc_ast_t* t) {
  //printf("%s",t->tag);

  if (strstr(t->tag, "identifier")) {
    Variable* var = lookup(var_table, t->contents);
    if (var) {
      return eval(var->val);
    } else {
      return lval_err(LERR_BAD_VAR);
    }
  }

  if (strstr(t->tag, "operator")) {
    if (strcmp(t->contents, "+") == 0) return lval_op(0);
    if (strcmp(t->contents, "-") == 0) return lval_op(1);
    if (strcmp(t->contents, "*") == 0) return lval_op(2);
    if (strcmp(t->contents, "/") == 0) return lval_op(3);
    if (strcmp(t->contents, "%") == 0) return lval_op(4);
    if (strcmp(t->contents, "^") == 0) return lval_op(5);
  }

  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    if(errno != ERANGE) {
      return lval_num(x);
    }
    return lval_err(LERR_BAD_NUM);
  }

  if (strstr(t->children[1]->tag, "assign")) {
    mpc_ast_t* expr = t->children[1]->children[2];
    var_table_set(t->children[1]->children[0]->contents, expr);
    return lval_var(t->children[1]->children[0]->contents);
  }

  if (strstr(t->tag, "bracexpr")) {
    int i;
    lval total;
    if(strstr(t->children[0]->tag, "equation")) {
      total = eval(t->children[0]->children[0]);
      i = 0;
      while(strstr(t->children[i+1]->tag,"equation")) {
        lval op = eval(t->children[i]->children[1]);
        lval next_x = eval(t->children[i+1]->children[0]);
        if(eval(t->children[i+1]->children[1]).op > MINUS) {
          while(strstr(t->children[i+2]->tag,"equation") && eval(t->children[i+1]->children[1]).op > MINUS) {
            lval next_op = eval(t->children[i+1]->children[1]);
            lval next_y = eval(t->children[i+2]->children[0]);
            if (next_op.type != LVAL_OP) {
              return lval_err(LERR_BAD_OP);
            }
            next_x = eval_op(next_x,next_op,next_y);
            i++;
          }
          if(eval(t->children[i+1]->children[1]).op > MINUS) {
            lval next_op = eval(t->children[i+1]->children[1]);
            lval next_y= eval(t->children[i+3]);
            if (op.type != LVAL_OP) {
              return lval_err(LERR_BAD_OP);
            }
            next_x = eval_op(next_x,next_op,next_y);
          }
        }
        if (op.type != LVAL_OP) {
          return lval_err(LERR_BAD_OP);
        }
        total = eval_op(total,op,next_x);
        i++;
      }
      lval op = eval(t->children[i]->children[1]);
      lval y= eval(t->children[i+2]);
      total = eval_op(total, op, y);
      i = i + 2;
      if(i < t->children_num) {
        op = eval(t->children[i+2]);
        y= eval(t->children[i+3]);
        total = eval_op(total, op, y);
      }
      if (op.type == LVAL_OP) {
        return total;
      }
    }
    total = eval(t->children[1]);
    i=2;
    if(strstr(t->children[i]->tag, "char")) {
      if(strstr(t->children[i]->tag, "operator")) {i=2;}
      else{ i=3; }
    }
    if(t->children_num == 3) {
      return total;
    }
    while(i< t->children_num -1) {
      lval op = eval(t->children[i]);
      if(strstr(t->children[i+1]->tag,"char")) {i++;}
      lval y = eval(t->children[i+1]);
      total = eval_op(total, op, y);
      i++;
      i++;
      //printf("%ld", total.num);
    }
    return total;
  }

  if (strstr(t->tag, "expr")) {
    lval total = eval(t->children[0]->children[0]);
    int i = 0;
    while(strstr(t->children[i+1]->tag,"equation")) {
      lval op = eval(t->children[i]->children[1]);
      lval next_x = eval(t->children[i+1]->children[0]);
      if(eval(t->children[i+1]->children[1]).op > MINUS) {
        while(strstr(t->children[i+2]->tag,"equation") && eval(t->children[i+1]->children[1]).op > MINUS) {
          lval next_op = eval(t->children[i+1]->children[1]);
          lval next_y = eval(t->children[i+2]->children[0]);
          if (next_op.type != LVAL_OP) {
            return lval_err(LERR_BAD_OP);
          }
          next_x = eval_op(next_x,next_op,next_y);
          i++;
        }
        if(eval(t->children[i+1]->children[1]).op > MINUS) {
          lval next_op = eval(t->children[i+1]->children[1]);
          lval next_y= eval(t->children[i+2]);
          if (op.type != LVAL_OP) {
            return lval_err(LERR_BAD_OP);
          }
          next_x = eval_op(next_x,next_op,next_y);
        }
      }
      if (op.type != LVAL_OP) {
        return lval_err(LERR_BAD_OP);
      }
      total = eval_op(total,op,next_x);
      i++;
    }

    lval op = eval(t->children[i]->children[1]);
    lval y= eval(t->children[i+1]);
    if (op.type == LVAL_OP) {
      return eval_op(total, op, y);
    }

    return lval_err(LERR_BAD_NUM);
  }

  if (strstr(t->children[1]->tag, "lemons")) {
    return eval(t->children[1]);
  }

  return lval_err(LERR_BAD_OP);
}

int main(int argc, char** argv) {

  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Identifier = mpc_new("identifier");
  mpc_parser_t* Assign = mpc_new("assign");
  mpc_parser_t* Equation = mpc_new("equation");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* BracExpr = mpc_new("bracexpr");
  mpc_parser_t* Seperator = mpc_new("seperator");
  mpc_parser_t* QExpr = mpc_new("qexpr");
  mpc_parser_t* LemonS = mpc_new("lemons");
  mpc_parser_t* Language = mpc_new("language");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                     \
      number     : /-?[0-9]+/ ;                    \
      operator   : '+'                              \
                  | '-'                              \
                  | '*'                               \
                  | '/'                                \
                  | '%'                                 \
                  | '^'  ;                               \
      identifier : /[a-zA-Z_][a-zA-Z0-9_]*/ ;             \
      assign     :  <identifier> '=' <bracexpr>            \
                  | <identifier> '=' <expr>                 \
                  | <identifier> '=' <qexpr>;                \
      equation   :  <number> <operator>                       \
                  | <identifier> <operator>;                   \
      expr       :  <equation>* <number>                        \
                  | <equation>* <identifier>;                    \
      bracexpr   :  <equation>* '('<expr>+')' <operator> <expr>+  \
                  | <equation>* '('<expr>+')'                      \
                  | <equation>* '('<bracexpr>')' <operator> <expr>+ \
                  | <equation>* '('<bracexpr>')'                     \
                  | '(' <expr>+ <operator> <bracexpr> ')'             \
                  | '(' <bracexpr> <operator> <expr>+ ')'              \
                  | '(' <bracexpr> <operator> <bracexpr> ')'            \
                  | <bracexpr> <operator> <bracexpr> ;                   \
      seperator  :  <expr> ','                                            \
                  | <bracexpr> ',' ;                                       \
      qexpr      :  '[' <seperator>* <expr> ']'                             \
                  | '[' <seperator>* <bracexpr> ']' ;                        \
      lemons     :  <assign> | <expr> | <bracexpr> | <qexpr> ;                \
      language   : /^/ <lemons> /$/ ;                                          \
    ",
    Number, Operator, Identifier, Assign, Equation, Expr, BracExpr, Seperator, QExpr, LemonS, Language);

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

  mpc_cleanup(11, Number, Operator, Identifier, Assign, Equation, Expr, BracExpr, Seperator, QExpr, LemonS, Language);

  Variable* var = var_table;
  while(var) {
    Variable* tmp = var;
    var = var->next;
    free(tmp->name);
    free(tmp);
  }
  return 0;
}