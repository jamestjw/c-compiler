#include "data.h"
#include "decl.h"
#include "expr.h"
#include "gen.h"
#include "misc.h"
#include "scan.h"
#include "sym.h"
#include "stmt.h"
#include "tree.h"

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

  // Match the semicolon at the end of a print statement
  semi();

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

  semi();

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

struct ASTnode *compound_statement(void) {
  struct ASTnode *left = NULL;
  struct ASTnode *tree;
  
  lbrace();

  while (1) {
    switch (Token.token) {  
      case T_PRINT:
        tree = print_statement();
        break;
      case T_INT:
        var_declaration();
        tree = NULL;
        break;
      case T_IDENT:
        tree = assignment_statement();
        break;
      case T_IF:
        tree = if_statement();
        break;
      case T_RBRACE:
        // Match the right brace and return the AST
        rbrace();
        return left;
      default:
        fatald("Syntax error, token", Token.token);
    }

    // For each new tree, either save it in the left if
    // it is empty, or glue the left and the new tree together
    if (tree) {
      if (left == NULL)
	      left = tree;
      else
	      left = mkastnode(A_GLUE, left, NULL, tree, 0);
    }
  }
}
