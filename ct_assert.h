#ifndef CT_ASSERT

// This works because bit-fields can't have a negative width
#define CT_ASSERT(cond) (sizeof (struct { int _ : (cond) ? 1 : -1; }))

#endif
