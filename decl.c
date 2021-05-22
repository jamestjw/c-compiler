#include "data.h"
#include "gen.h"
#include "misc.h"
#include "sym.h"

// Parse variable declarations
void var_declaration(void) {
  match(T_INT, "int");
  ident();
  addglob(Text);
  genglobsym(Text);
  semi();
}

