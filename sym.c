#include "data.h"
#include "gen.h"
#include "misc.h"
#include "sym.h"

struct symtable Symtable[NSYMBOLS];
int Globs = 0;
int Locls = NSYMBOLS - 1;

// Get position of new global symbol slot, crashes if
// no remaining slots are available
static int newglob(void) {
  int p;

  if ((p = Globs++) >= Locls) {
    fatal("Too many global symbols");
  }

  return p;
}

static int newlocl(void) {
  int p;

  if ((p = Locls--) <= Globs)
    fatal("Too many local symbols");

  return p;
}

// Update a symbol at a given slot number in the symbol table
static void updatesym(int slot, char *name, int type, int stype,
                      int class, int endlabel, int size, int posn) {
  if (slot < 0 || slot >= NSYMBOLS)
    fatal("Invalid slot symbol in updatesym");

  Symtable[slot].name = strdup(name);
  Symtable[slot].type = type;
  Symtable[slot].stype = stype;
  Symtable[slot].class = class;
  Symtable[slot].endlabel = endlabel;
  Symtable[slot].size = size;
  Symtable[slot].posn = posn;
}

int findglob(char *s) {
  int i;

  for (i = 0; i < Globs; i++) {
    // Check if 1st char matches before doing strcmp
    if (*s == *Symtable[i].name && !strcmp(s, Symtable[i].name)) {
      return i;
    }
  }

  return -1;
}

int addglob(char *name, int type, int stype, int endlabel, int size) {
  int slot;

  // If the symbol is already in the table, just return it
  if ((slot = findglob(name)) != -1) {
    return slot;
  }
  
  // Get a new slot and fill it otherwise
  slot = newglob();
  updatesym(slot, name, type, stype, C_GLOBAL, endlabel, size, 0);
  genglobsym(slot);
  return slot;
}

int findlocl(char *s) {
  int i;

  for (i = Locls + 1; i < NSYMBOLS; i++) {
    if (*s == *Symtable[i].name && !strcmp(s, Symtable[i].name))
      return i;
  }

  return -1;
}

int addlocl(char *name, int type, int stype, int endlabel, int size) {
  int slot, posn;

  // Return existing slot if it already exists in the symbol table
  if ((slot = findlocl(name)) != -1)
    return slot;

  // Otherwise, we get a new slot and position for this local
  slot = newlocl();
  posn = gengetlocaloffset(type, 0);
  updatesym(slot, name, type, stype, C_LOCAL, endlabel, size, posn);

  return slot;
}

int findsymbol(char *s) {
  int slot;

  slot = findlocl(s);
  if (slot == -1)
    slot = findglob(s);

  return slot;
}
