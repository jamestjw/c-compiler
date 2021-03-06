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
    condAST = mkastunary(A_TOBOOL, condAST->type, condAST->ctype, condAST, NULL, 0);
  rparen();

  // AST for the compound statement for the TRUE clause
  trueAST = single_statement();

  // Check for the presence of an 'else' token and
  // parse the statement for the FALSE clause
  if (Token.token == T_ELSE) {
    scan(&Token);
    falseAST = single_statement();
  }

  return mkastnode(A_IF, P_NONE, NULL, condAST, trueAST, falseAST, NULL, 0);
}

struct ASTnode *while_statement(void) {
  struct ASTnode *condAST, *bodyAST;

  // match 'while' '('
  match(T_WHILE, "while");
  lparen();

  // Parse the conditional expression
  // and the subsequent ')'.
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    condAST = mkastunary(A_TOBOOL, condAST->type, condAST->ctype, condAST, NULL, 0);
  rparen();

  // Parse the body of the while statement
  Looplevel++;
  bodyAST = single_statement();
  Looplevel--;

  return mkastnode(A_WHILE, P_NONE, NULL, condAST, NULL, bodyAST, NULL, 0);
}

static struct ASTnode *for_statement(void) {
  struct ASTnode *condAST, *bodyAST;
  struct ASTnode *preopAST, *postopAST;
  struct ASTnode *tree;

  // Ensure we have 'for' '('
  match(T_FOR, "for");
  lparen();

  // Parse the pre_op statement and the ';'
  preopAST = expression_list(T_SEMI);
  semi();

  // Parse the condition and the ';'
  condAST = binexpr(0);
  if (condAST->op < A_EQ || condAST->op > A_GE)
    condAST = mkastunary(A_TOBOOL, condAST->type, condAST->ctype, condAST, NULL, 0);
  semi();

  // Get the post_op statement and the ')'
  postopAST = expression_list(T_RPAREN);
  rparen();

  // Get the body
  Looplevel++;
  bodyAST = single_statement();
  Looplevel--;

  // TODO: For now all 4 subtrees have to be non-null.
  // We can improve this in the future.
  tree = mkastnode(A_GLUE, P_NONE, NULL, bodyAST, NULL, postopAST, NULL, 0);
  tree = mkastnode(A_WHILE, P_NONE, NULL, condAST, NULL, tree, NULL, 0);

  return mkastnode(A_GLUE, P_NONE, NULL, preopAST, NULL, tree, NULL, 0);
}

static struct ASTnode *return_statement(void) {
  struct ASTnode *tree = NULL;
  // Return statements do not necessarily have parentheses
  int parentheses_present = 0;

  match(T_RETURN, "return");

  if (Token.token == T_LPAREN) {
    parentheses_present = 1;
    lparen();
  }

  if (Functionid->type == P_VOID) {
    if (parentheses_present || (!parentheses_present && Token.token != T_SEMI))
      fatal("Can't return from a void function");
  } else {
    if (Token.token == T_SEMI)
      fatal("Must return a value from a non-void function");

    tree = binexpr(0);

    // Ensure this is compatible with the function's type
    tree = modify_type(tree, Functionid->type, Functionid->ctype, 0);
    if (tree == NULL)
      fatal("Incompatible return type");

    if (parentheses_present)
      rparen();
  }

  tree = mkastunary(A_RETURN, P_NONE, NULL, tree, NULL, 0);

  semi();

  return tree;
}

static struct ASTnode *break_statement(void) {
  if (Looplevel == 0 && Switchlevel == 0)
    fatal("No loop to break out from");
  scan(&Token);
  semi();
  return mkastleaf(A_BREAK, P_NONE, NULL, NULL, 0);
}

static struct ASTnode *continue_statement(void) {
  if (Looplevel == 0)
    fatal("No loop to continue to");
  scan(&Token);
  semi();
  return mkastleaf(A_CONTINUE, P_NONE, NULL, NULL, 0);
}

static struct ASTnode *switch_statement(void) {
  struct ASTnode *left, *n, *c, *casetree = NULL, *casetail;
  int inloop = 1, casecount = 0;
  int seendefault = 0;
  int ASTop, casevalue;

  // Consume the 'switch' and '('
  scan(&Token);
  lparen();

  left = binexpr(0);
  rparen();
  lbrace();

  if (!inttype(left->type))
    fatal("Switch expression is not of integer type");

  // Build A_SWITCH subtree with the expression as the child
  n = mkastunary(A_SWITCH, P_NONE, NULL, left, NULL, 0);

  Switchlevel++;
  while(inloop) {
    switch (Token.token) {
      case T_RBRACE:
        // End of switch if we encounter R_BRACE
        if (casecount == 0)
          fatal("No cases in switch");
        inloop = 0;
        break;
      case T_CASE:
      case T_DEFAULT: {
        if (seendefault)
          fatal("Case or default after existing default");
        if (Token.token == T_DEFAULT) {
          ASTop = A_DEFAULT;
          seendefault = 1;
          scan(&Token);
        } else {
          ASTop = A_CASE;
          scan(&Token);
          left = binexpr(0);
          if (left->op != A_INTLIT)
            fatal("Expecting integer literal for case value");
          casevalue = left->a_intvalue;

          for (c = casetree; c != NULL; c = c->right)
            if (casevalue == c->a_intvalue)
              fatal("Duplicate case value");
        }

        match(T_COLON, ":");
        casecount++;

        if (Token.token == T_CASE)
          left = NULL;
        else
          left = compound_statement(1);

        // Build subtree with compound statement as left child
        // and link to the A_CASE tree
        if (casetree == NULL) {
          casetree = casetail = mkastunary(ASTop, P_NONE, NULL, left, NULL, casevalue);
        } else {
          casetail->right = mkastunary(ASTop, P_NONE, NULL, left, NULL, casevalue);
          casetail = casetail->right;
        }
        break;
      }
      default:
        fatals("Unexpected token in switch", Token.tokstr);
    }
  }

  Switchlevel--;

  n->a_intvalue = casecount;
  n->right = casetree;
  rbrace();

  return n;
}

static struct ASTnode *single_statement(void) {
  struct symtable *ctype;
  struct ASTnode *stmt = NULL;

  switch (Token.token) {
    case T_SEMI:
      // We allow empty statements
      semi();
      break;
    case T_LBRACE:
      lbrace();
      stmt = compound_statement(0);
      rbrace();
      return stmt;
    case T_IDENT:
      if (findtypedef(Text) == NULL) {
        stmt = binexpr(0);
        semi();
        return stmt;
      }
      // Falldown to parse type call otherwise
    case T_CHAR:
    case T_INT:
    case T_LONG:
    case T_STRUCT:
    case T_UNION:
    case T_ENUM:
    case T_TYPEDEF:
      declaration_list(&ctype, C_LOCAL, T_SEMI, T_EOF, &stmt);
      semi();
      return stmt; // Assignment statements to locals
    case T_IF:
      return if_statement();
    case T_WHILE:
      return while_statement();
    case T_FOR:
      return for_statement();
    case T_RETURN:
      return return_statement();
    case T_BREAK:
      return break_statement();
    case T_CONTINUE:
      return continue_statement();
    case T_SWITCH:
      return switch_statement();
    default:
      // TODO: `2 + 3;` is treated as a valid statement
      // for now, to fix soon.
      //
      // Handle assignment statements
      stmt = binexpr(0);
      semi();
      return stmt;
  }

  return NULL;
}

struct ASTnode *compound_statement(int inswitch) {
  struct ASTnode *left = NULL;
  struct ASTnode *tree;

  while (1) {
    // Accept empty compound statements
    if (Token.token == T_RBRACE)
      return left;

    // In switch statements, it is fine to not have braces
    // around multiline statements
    if (inswitch && (Token.token == T_CASE || Token.token == T_DEFAULT)) {
      return left;
    }

    tree = single_statement();

    if (tree != NULL) {
      if (left == NULL)
        left = tree;
      else
        left = mkastnode(A_GLUE, P_NONE, NULL, left, NULL, tree, NULL, 0);
    }
  }
  return NULL;
}

