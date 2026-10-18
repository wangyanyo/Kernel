#ifndef __ARCH_I386_ATOMIC_H
#define __ARCH_I386_ATOMIC_H

/*
 * Make sure gcc doesn't try to be clever and move things around
 * on us. We need to use _exactly_ the address the user gave us,
 * not some alias that contains the same information.
 */
typedef struct { int counter; } atomic_t;

#endif