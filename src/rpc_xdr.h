/* rpc_xdr.h -- minimal Sun-XDR compatible primitives (prototypes).
   Companion of rpc_types.h and xdrmini.c; subset used by Moldy. */
#ifndef MOLDY_RPC_XDR_H
#define MOLDY_RPC_XDR_H

#include <stdio.h>
#include "rpc_types.h"

void   xdrstdio_create(XDR *xdrs, FILE *file, enum xdr_op op);

bool_t xdr_int(XDR *xdrs, int *ip);
bool_t xdr_long(XDR *xdrs, long *lp);
bool_t xdr_u_long(XDR *xdrs, u_long *ulp);
bool_t xdr_bool(XDR *xdrs, bool_t *bp);
bool_t xdr_float(XDR *xdrs, float *fp);
bool_t xdr_double(XDR *xdrs, double *dp);
bool_t xdr_opaque(XDR *xdrs, char *cp, unsigned cnt);
bool_t xdr_vector(XDR *xdrs, char *base, unsigned nelem,
                  unsigned elemsize, xdrproc_t elemproc);

u_long xdr_getpos(XDR *xdrs);
bool_t xdr_setpos(XDR *xdrs, u_long pos);
void   xdr_destroy(XDR *xdrs);

#endif /* MOLDY_RPC_XDR_H */
