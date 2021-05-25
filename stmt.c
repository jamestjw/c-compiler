#include "data.h"
#include "decl.h"
#include "expr.h"
#include "gen.h"
#include "misc.h"
#include "scan.h"
#include "sym.h"
#include "stmt.h"
#include "tree.h"

static struct ASTnode *single_statement(void);

static struct ASTnode *print_statement(void) {
  struct ASTnode *tree;
  int reg;

  // Match a 'print' as the first token of a statement
  match(T_PRINT, "print");

  // Parse the following expression and 
  // generate the assembly code
  tree = binexpr(0);
  
  // Make a print AST tree
  tree = mkastunary(A_PRINT, tree, 0);

  return tree;
}

static struct ASTnode *assignment_statement(void) {
  struct ASTnode *left, *right, *tree;
  int id;

  // Match an identifier
  ident();

  if ((id = findglob(Text)) == -1) {
    fatals("Assigning to undeclared variable", Text);
  }

  right = mkastleaf(A_LVIDENT, id);

  match(T_ASSIGN, "=");

  left = binexpr(0);

  tree = mkastnode(A_ASSIGN, left, NULL, right, 0);

  return tree;
}

struct ASTnode *if_statement(void) {
  struct ASTnode *condAST, *trueAST, *falseAST = NULL;

  // Matching 'if' '('
  match(T_IF, "if");
  lparen();

  // Match the condition expression and the subsequent ')'
  condAST = binexpr(0);

  // TODO: Handle cases where the expression does not involve
  // a conditional operator
  if (condAST->op < A_EQ || condAST->op > A_GE)
    fatal("Bad comparison operator in if statement");
  rparen();

  // AST for the compound statement for the TRUE clause
  trueAST = compound_statement();

  // Check for the presence of an 'else' token and 
  // parse the statement for the FALSE clause
  if (Token.token == T_ELSE) {
    scan(&Token);
    falseAST = compound_statement();
  }

  return mkastnode(A_IF, condAST, trueAST, falseAST, 0);
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

  return mkastnode(A_WHILE, condAST, NULL, bodyAST, 0);
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
    fatal("Bad comparison operator in 'for' statement");
  semi();

  // Get the post_op statement and the ')'
  postopAST = single_statement();
  rparen();

  // Get the body
  bodyAST = compound_statement();

  // TODO: For now all 4 subtrees have to be non-null.
  // We can improve this in the future.
  tree = mkastnode(A_GLUE, bodyAST, NULL, postopAST, 0);
  tree = mkastnode(A_WHILE, condAST, NULL, tree, 0);

  return mkastnode(A_GLUE, preopAST, NULL, tree,  0);
}

static struct ASTnode *single_statement(void) {
  switch (Token.token) {
    case T_PRINT:
      return print_statement();
    case T_INT:
      var_declaration();
      return NULL; // No AST here
    case T_IDENT:
      return assignment_statement();
    case T_IF:
      return if_statement();
    case T_WHILE:
      return while_statement();
    case T_FOR:
      return for_statement();
    default:
      fatald("Syntax error, token", Token.token);
      return NULL;
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
        (tree->op == A_PRINT || tree->op == A_ASSIGN))
      semi();

    if (tree != NULL) {
      if (left == NULL)
        left = tree;
      else
        left = mkastnode(A_GLUE, left, NULL, tree, 0);
    }

    if (Token.token == T_RBRACE) {
      rbrace();
      return left;
    }
  }
}
