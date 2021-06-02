#include "data.h"
#include "decl.h"
#include "expr.h"
#include "gen.h"
#include "misc.h"
#include "scan.h"
#include "sym.h"
#include "stmt.h"
#include "tree.h"
#include "types.h"

static struct ASTnode *single_statement(void);

struct ASTnode *if_statement(void) {
  struct ASTnode *condAST, *trueAST, *falseAST = NULL;

  // Matching 'if' '('
  match(T_IF, "if");
  lparen();

  // Match the condition expression and the subsequent ')'
  condAST = binexpr(0);

  if (condAST->op < A_EQ || condAST->op > A_GE)
    // Convert any integer that is not a zero to a 1
    condAST = mkastunary(A_TOBOOL, condAST->type, condAST, 0);
  rparen();

  // AST for the compound statement for the TRUE clause
  trueAST = compound_statement();

  // Check for the presence of an 'else' token and 
  // parse the statement for the FALSE clause
  if (Token.token == T_ELSE) {
    scan(&Token);
    falseAST = compound_statement();
  }

  return mkastnode(A_IF, P_NONE, condAST, trueAST, falseAST, 0);
}

struct ASTnode *while_statement(void) {
  struct ASTnode *condAST, *bodyAST;

  // match 'while' '('
  match(T_WHILE, "while");
  lparen();

  // Parse the conditional expression
  // and the subsequent ')'.
  // TODO: For now, we ensure that the operation
  // is a comparison
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    fatal("Bad comparison operator in 'while' statement");
  rparen();

  // Parse the body of the while statement
  bodyAST = compound_statement();

  return mkastnode(A_WHILE, P_NONE, condAST, NULL, bodyAST, 0);
}

static struct ASTnode *for_statement(void) {
  struct ASTnode *condAST, *bodyAST;
  struct ASTnode *preopAST, *postopAST;
  struct ASTnode *tree;

  // Ensure we have 'for' '('
  match(T_FOR, "for");
  lparen();

  // Parse the pre_op statement and the ';'
  preopAST = single_statement();
  semi();

  // Parse the condition and the ';'
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    condAST = mkastunary(A_TOBOOL, condAST->type, condAST, 0);
  semi();

  // Get the post_op statement and the ')'
  postopAST = single_statement();
  rparen();

  // Get the body
  bodyAST = compound_statement();

  // TODO: For now all 4 subtrees have to be non-null.
  // We can improve this in the future.
  tree = mkastnode(A_GLUE, P_NONE, bodyAST, NULL, postopAST, 0);
  tree = mkastnode(A_WHILE, P_NONE, condAST, NULL, tree, 0);

  return mkastnode(A_GLUE, P_NONE, preopAST, NULL, tree,  0);
}

static struct ASTnode *return_statement(void) {
  struct ASTnode *tree;
  int functype;

  functype = Symtable[Functionid].type;

  if (functype == P_VOID)
    fatal("Can't return from a void function");

  match(T_RETURN, "return");
  lparen();

  tree = binexpr(0);

  tree = modify_type(tree, functype, 0);
  if (tree == NULL) fatal("Incompatible return type");

  tree = mkastunary(A_RETURN, P_NONE, tree, 0);
  
  rparen();

  return tree;
}

static struct ASTnode *single_statement(void) {
  int type;

  switch (Token.token) {
    case T_CHAR:
    case T_INT:
    case T_LONG:
      type = parse_type();
      ident();
      var_declaration(type, 1, 0);
      semi();
      return NULL; // No AST here
    case T_IF:
      return if_statement();
    case T_WHILE:
      return while_statement();
    case T_FOR:
      return for_statement();
    case T_RETURN:
      return return_statement();
    default:
      // TODO: `2 + 3;` is treated as a valid statement
      // for now, to fix soon.
      //
      // Handle assignment statements
      return binexpr(0);
  }
}

struct ASTnode *compound_statement(void) {
  struct ASTnode *left = NULL;
  struct ASTnode *tree;
  
  lbrace();

  while (1) {
    tree = single_statement();

    // Some statements must be followed by a semicolon
    if (tree != NULL && 
        (tree->op == A_ASSIGN ||
         tree->op == A_RETURN || tree->op == A_FUNCCALL))
      semi();

    if (tree != NULL) {
      if (left == NULL)
        left = tree;
      else
        left = mkastnode(A_GLUE, P_NONE, left, NULL, tree, 0);
    }

    if (Token.token == T_RBRACE) {
      rbrace();
      return left;
    }
  }
}
