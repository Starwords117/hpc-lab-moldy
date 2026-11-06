/* rpc_types.h -- minimal Sun-XDR compatible types.
   Shipped because glibc >= 2.32 removed <rpc/types.h>; provides exactly
   the subset Moldy's xdr.h / xdrmini.c rely on. */
#ifndef MOLDY_RPC_TYPES_H
#define MOLDY_RPC_TYPES_H

#include <sys/types.h>          /* u_char..u_long (glibc, __USE_MISC) */

/* MinGW/Windows 的 <sys/types.h> 不定义这些 BSD 类型, 兜底 typedef。 */
#if defined(_WIN32) || defined(__MINGW32__)
typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned long  u_long;
#endif

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

typedef int bool_t;
typedef int enum_t;

enum xdr_op {
    XDR_FREE   = 0,
    XDR_ENCODE = 1,
    XDR_DECODE = 2
};

struct xdr_ops;

typedef struct XDR {
    enum xdr_op     x_op;
    struct xdr_ops *x_ops;
    void           *x_private;  /* FILE * for the stdio backend */
    void           *x_public;
} XDR;

typedef bool_t (*xdrproc_t)();

#endif /* MOLDY_RPC_TYPES_H */
