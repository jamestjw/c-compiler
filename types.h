int type_compatible(int *left, int *right, int onlyright);
// Given a primitive type, return the type which is a
// pointer to it.
int pointer_to(int type);
// Given a primitive pointer type, return the type
// which it points to
int value_at(int type); 
