#include "data.h"
#include "misc.h"
#include "sym.h"

struct symtable Gsym[NSYMBOLS];
int Globs = 0;

int findglob(char *s) {
  int i;

  for (i = 0; i < Globs; i++) {
    // Check if 1st char matches before doing strcmp
    if (*s == *Gsym[i].name && !strcmp(s, Gsym[i].name)) {
      return i;
    }
  }

  return -1;
}

// Get position of new global symbol slot, crashes if
// no remaining slots are available
static int newglob(void) {
  int p;

  if ((p = Globs++) >= NSYMBOLS) {
    fatal("Too many global symbols");
  }

  return p;
}

int addglob(char *name, int type, int stype, int endlabel) {
  int y;

  // If the symbol is already in the table, just return it
  if ((y = findglob(name)) != -1) {
    return y;
  }
  
  // Get a new slot and fill it otherwise
  y = newglob();
  Gsym[y].name = strdup(name);
  Gsym[y].type = type;
  Gsym[y].stype = stype;
  Gsym[y].endlabel = endlabel;

  return y;
}
