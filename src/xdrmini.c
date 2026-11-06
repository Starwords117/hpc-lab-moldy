/* xdrmini.c -- minimal stdio-backed Sun-XDR implementation.
   Replaces the <rpc/xdr.h> routines removed from glibc >= 2.32.
   Wire format is canonical XDR: big-endian 32-bit units, IEEE-754 floats,
   4-byte padding for opaque data.  Byte order of the host is irrelevant:
   floating point is moved as an exact byte pattern. */
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "rpc_types.h"
#include "rpc_xdr.h"

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define HOST_IS_BIG_ENDIAN 1
#endif

/*----------------------------- stream backend -----------------------------*/

struct xdr_ops {
    bool_t (*x_getlong)(XDR *xdrs, long *lp);
    bool_t (*x_putlong)(XDR *xdrs, const long *lp);
    bool_t (*x_getbytes)(XDR *xdrs, char *addr, unsigned len);
    bool_t (*x_putbytes)(XDR *xdrs, const char *addr, unsigned len);
    u_long (*x_getpostn)(XDR *xdrs);
    bool_t (*x_setpostn)(XDR *xdrs, u_long pos);
    void   (*x_destroy)(XDR *xdrs);
};

static bool_t stdio_getlong(XDR *xdrs, long *lp)
{
    unsigned char b[4];

    if( fread(b, 1, 4, (FILE *)xdrs->x_private) != 4 )
        return FALSE;
    *lp = ((unsigned long)b[0] << 24) | ((unsigned long)b[1] << 16) |
          ((unsigned long)b[2] << 8)  |  (unsigned long)b[3];
    return TRUE;
}

static bool_t stdio_putlong(XDR *xdrs, const long *lp)
{
    unsigned char  b[4];
    unsigned long  v = (unsigned long)*lp & 0xFFFFFFFFUL;

    b[0] = (unsigned char)(v >> 24);
    b[1] = (unsigned char)(v >> 16);
    b[2] = (unsigned char)(v >> 8);
    b[3] = (unsigned char)v;
    return fwrite(b, 1, 4, (FILE *)xdrs->x_private) == 4 ? TRUE : FALSE;
}

static bool_t stdio_getbytes(XDR *xdrs, char *addr, unsigned len)
{
    if( len == 0 )
        return TRUE;
    return fread(addr, 1, len, (FILE *)xdrs->x_private) == len ? TRUE : FALSE;
}

static bool_t stdio_putbytes(XDR *xdrs, const char *addr, unsigned len)
{
    if( len == 0 )
        return TRUE;
    return fwrite(addr, 1, len, (FILE *)xdrs->x_private) == len ? TRUE : FALSE;
}

static u_long stdio_getpostn(XDR *xdrs)
{
    return (u_long)ftell((FILE *)xdrs->x_private);
}

static bool_t stdio_setpostn(XDR *xdrs, u_long pos)
{
    return fseek((FILE *)xdrs->x_private, (long)pos, SEEK_SET) == 0 ? TRUE : FALSE;
}

static void stdio_destroy(XDR *xdrs)
{
    (void)fflush((FILE *)xdrs->x_private);
}

static struct xdr_ops stdio_ops = {
    stdio_getlong, stdio_putlong,
    stdio_getbytes, stdio_putbytes,
    stdio_getpostn, stdio_setpostn,
    stdio_destroy
};

void xdrstdio_create(XDR *xdrs, FILE *file, enum xdr_op op)
{
    xdrs->x_op       = op;
    xdrs->x_ops      = &stdio_ops;
    xdrs->x_private  = (void *)file;
    xdrs->x_public   = NULL;
}

/*------------------------- primitive converters ---------------------------*/

bool_t xdr_int(XDR *xdrs, int *ip)
{
    long v;

    if( xdrs->x_op == XDR_FREE )
        return TRUE;
    if( xdrs->x_op == XDR_ENCODE )
    {
        v = (long)*ip;
        return stdio_putlong(xdrs, &v);
    }
    if( !stdio_getlong(xdrs, &v) )
        return FALSE;
    *ip = (int)(v & 0xFFFFFFFFUL);  /* two's-complement low 32 bits */
    return TRUE;
}

bool_t xdr_long(XDR *xdrs, long *lp)
{
    long v;

    if( xdrs->x_op == XDR_FREE )
        return TRUE;
    if( xdrs->x_op == XDR_ENCODE )
    {
        v = *lp;
        return stdio_putlong(xdrs, &v);
    }
    if( !stdio_getlong(xdrs, &v) )
        return FALSE;
    *lp = (v & 0x80000000L) ? (v | 0xFFFFFFFF80000000UL)   /* sign-extend */
                            : (v & 0xFFFFFFFFUL);
    return TRUE;
}

bool_t xdr_u_long(XDR *xdrs, u_long *ulp)
{
    long v;

    if( xdrs->x_op == XDR_FREE )
        return TRUE;
    if( xdrs->x_op == XDR_ENCODE )
    {
        v = (long)(*ulp & 0xFFFFFFFFUL);
        return stdio_putlong(xdrs, &v);
    }
    if( !stdio_getlong(xdrs, &v) )
        return FALSE;
    *ulp = (u_long)(v & 0xFFFFFFFFUL);
    return TRUE;
}

bool_t xdr_bool(XDR *xdrs, bool_t *bp)
{
    long v;

    if( xdrs->x_op == XDR_FREE )
        return TRUE;
    if( xdrs->x_op == XDR_ENCODE )
    {
        v = *bp ? 1L : 0L;
        return stdio_putlong(xdrs, &v);
    }
    if( !stdio_getlong(xdrs, &v) )
        return FALSE;
    *bp = (v != 0);
    return TRUE;
}

/* Floating point: move the exact host bit pattern, byte order adapted. */

bool_t xdr_float(XDR *xdrs, float *fp)
{
    unsigned char b[4];

    if( xdrs->x_op == XDR_FREE )
        return TRUE;
    if( xdrs->x_op == XDR_ENCODE )
    {
        memcpy(b, fp, 4);
#ifdef HOST_IS_BIG_ENDIAN
        return stdio_putbytes(xdrs, (char *)b, 4);
#else
        { unsigned char t[4] = { b[3], b[2], b[1], b[0] };
          return stdio_putbytes(xdrs, (char *)t, 4); }
#endif
    }
    if( !stdio_getbytes(xdrs, (char *)b, 4) )
        return FALSE;
#ifdef HOST_IS_BIG_ENDIAN
    memcpy(fp, b, 4);
#else
    { unsigned char t[4] = { b[3], b[2], b[1], b[0] };
      memcpy(fp, t, 4); }
#endif
    return TRUE;
}

bool_t xdr_double(XDR *xdrs, double *dp)
{
    unsigned char b[8];

    if( xdrs->x_op == XDR_FREE )
        return TRUE;
    if( xdrs->x_op == XDR_ENCODE )
    {
        memcpy(b, dp, 8);
#ifdef HOST_IS_BIG_ENDIAN
        return stdio_putbytes(xdrs, (char *)b, 8);
#else
        { unsigned char t[8];
          int i;
          for( i = 0; i < 8; i++ ) t[i] = b[7 - i];
          return stdio_putbytes(xdrs, (char *)t, 8); }
#endif
    }
    if( !stdio_getbytes(xdrs, (char *)b, 8) )
        return FALSE;
#ifdef HOST_IS_BIG_ENDIAN
    memcpy(dp, b, 8);
#else
    { unsigned char t[8];
      int i;
      for( i = 0; i < 8; i++ ) t[i] = b[7 - i];
      memcpy(dp, t, 8); }
#endif
    return TRUE;
}

bool_t xdr_opaque(XDR *xdrs, char *cp, unsigned cnt)
{
    static const char zeros[4] = { 0, 0, 0, 0 };
    char            junk[4];
    unsigned        pad = (4 - (cnt & 3)) & 3;

    if( xdrs->x_op == XDR_FREE )
        return TRUE;
    if( xdrs->x_op == XDR_ENCODE )
        return stdio_putbytes(xdrs, cp, cnt) &&
               (pad == 0 || stdio_putbytes(xdrs, zeros, pad));
    if( !stdio_getbytes(xdrs, cp, cnt) )
        return FALSE;
    return pad == 0 || stdio_getbytes(xdrs, junk, pad);
}

bool_t xdr_vector(XDR *xdrs, char *base, unsigned nelem,
                  unsigned elemsize, xdrproc_t elemproc)
{
    unsigned i;

    for( i = 0; i < nelem; i++ )
        if( !(*elemproc)(xdrs, base + i * elemsize) )
            return FALSE;
    return TRUE;
}

/*--------------------------- position handling ----------------------------*/

u_long xdr_getpos(XDR *xdrs)
{
    return (*xdrs->x_ops->x_getpostn)(xdrs);
}

bool_t xdr_setpos(XDR *xdrs, u_long pos)
{
    return (*xdrs->x_ops->x_setpostn)(xdrs, pos);
}

void xdr_destroy(XDR *xdrs)
{
    (*xdrs->x_ops->x_destroy)(xdrs);
}
