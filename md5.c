/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.	This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

/* Brutally hacked by John Walker back from ANSI C to K&R (no
   prototypes) to maintain the tradition that Netfone will compile
   with Sun's original "cc". */

#include <memory.h>		 /* for memcpy() */
#include <windows.h>
#include "md5.h"
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

#define a esi
#define b edi
#define c edx
#define d ebx
#define tmp1 eax
#define tmp2 ecx

#define x(i) [x+4*i]

#define FF(a, b, c, d, x, s, ac) \
  __asm mov tmp1,b \
  __asm and tmp1,c \
  __asm mov tmp2,b \
  __asm not tmp2 \
  __asm and tmp2,d \
  __asm or tmp2,tmp1 \
  __asm lea a,[tmp2+a+ac] \
  __asm add a,x \
  __asm rol a,s \
  __asm add a,b 

#define GG(a, b, c, d, x, s, ac) \
  __asm mov tmp1,b \
  __asm and tmp1,d \
  __asm mov tmp2,d \
  __asm not tmp2 \
  __asm and tmp2,c \
  __asm or tmp2,tmp1 \
  __asm lea a,[tmp2+a+ac] \
  __asm add a,x \
  __asm rol a,s \
  __asm add a,b 

#define HH(a,b,c, d, x, s, ac) \
  __asm mov tmp2,b \
  __asm xor tmp2,c \
  __asm xor tmp2,d \
  __asm lea a,[tmp2+a+ac] \
  __asm add a,x \
  __asm rol a,s \
  __asm add a,b

#define II(a, b, c, d, x, s, ac) \
  __asm mov tmp2,d \
  __asm not tmp2 \
  __asm or tmp2,b \
  __asm xor tmp2,c \
  __asm lea a,[tmp2+a+ac] \
  __asm add a,x \
  __asm rol a,s \
  __asm add a,b

#ifndef HIGHFIRST
#define byteReverse(buf, len)	/* Nothing */
#else
/*
 * Note: this code is harmless on little-endian machines.
 */
void byteReverse(buf, longs)
    unsigned char *buf; unsigned longs;
{
    uint32 t;
    do {
	t = (uint32) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
	    ((unsigned) buf[1] << 8 | buf[0]);
	*(uint32 *) buf = t;
	buf += 4;
    } while (--longs);
}
#endif

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void MD5Init(ctx)
    struct MD5Context *ctx;
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;

    ctx->bits[0] = 0;
    ctx->bits[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void MD5Update(ctx, buf, len)
    struct MD5Context *ctx; unsigned char *buf; unsigned len;
{
    uint32 t;

    /* Update bitcount */

    t = ctx->bits[0];
    if ((ctx->bits[0] = t + ((uint32) len << 3)) < t)
	ctx->bits[1]++; 	/* Carry from low to high */
    ctx->bits[1] += len >> 29;

    t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */

    /* Handle any leading odd-sized chunks */

    if (t) {
	unsigned char *p = (unsigned char *) ctx->in + t;

	t = 64 - t;
	if (len < t) {
	    memcpy(p, buf, len);
	    return;
	}
	memcpy(p, buf, t);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (uint32 *) ctx->in);
	buf += t;
	len -= t;
    }
    /* Process data in 64-byte chunks */

    while (len >= 64) {
	memcpy(ctx->in, buf, 64);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (uint32 *) ctx->in);
	buf += 64;
	len -= 64;
    }

    /* Handle any remaining bytes of data. */

    memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
void MD5Final(digest, ctx)
    unsigned char digest[16]; struct MD5Context *ctx;
{
    unsigned count;
    unsigned char *p;

    /* Compute number of bytes mod 64 */
    count = (ctx->bits[0] >> 3) & 0x3F;

    /* Set the first char of padding to 0x80.  This is safe since there is
       always at least one byte free */
    p = ctx->in + count;
    *p++ = 0x80;

    /* Bytes of padding needed to make 64 bytes */
    count = 64 - 1 - count;

    /* Pad out to 56 mod 64 */
    if (count < 8) {
	/* Two lots of padding:  Pad the first block to 64 bytes */
	memset(p, 0, count);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (uint32 *) ctx->in);

	/* Now fill the next block with 56 bytes */
	memset(ctx->in, 0, 56);
    } else {
	/* Pad block to 56 bytes */
	memset(p, 0, count - 8);
    }
    byteReverse(ctx->in, 14);

    /* Append length in bits and transform */
    ((uint32 *) ctx->in)[14] = ctx->bits[0];
    ((uint32 *) ctx->in)[15] = ctx->bits[1];

    MD5Transform(ctx->buf, (uint32 *) ctx->in);
    byteReverse((unsigned char *) ctx->buf, 4);
    memcpy(digest, ctx->buf, 16);
    memset(ctx, 0, sizeof(ctx));        /* In case it's sensitive */
}



/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
void MD5Transform(buf, in)
    uint32 buf[4]; uint32 in[16];
{
 DWORD x[16];
  __asm {
    //initial
    mov a,0x67452301
    mov b,0xefcdab89
    mov c,0x98badcfe
    mov d,0x10325476
    //copy string from `in' to `buf'
    //考虑到用API会影响寄存器，所以自己实现这一段内存拷贝
    push esi
    push edi
      
    xor ecx,ecx
    mov esi,dword ptr [in]
    lea edi,[x]
ROLL:
    mov eax,dword ptr [esi+ecx]
    mov dword ptr [edi+ecx],eax
    add ecx,4
    cmp ecx,64
    jb  ROLL
      
    pop edi
    pop esi
  }
  
  /* Round 1 */
  FF(a, b, c, d, x( 0), S11, 0xd76aa478); /* 1 */
  FF(d, a, b, c, x( 1), S12, 0xe8c7b756); /* 2 */
  FF(c, d, a, b, x( 2), S13, 0x242070db); /* 3 */
  FF(b, c, d, a, x( 3), S14, 0xc1bdceee); /* 4 */
  FF(a, b, c, d, x( 4), S11, 0xf57c0faf); /* 5 */
  FF(d, a, b, c, x( 5), S12, 0x4787c62a); /* 6 */
  FF(c, d, a, b, x( 6), S13, 0xa8304613); /* 7 */
  FF(b, c, d, a, x( 7), S14, 0xfd469501); /* 8 */
  FF(a, b, c, d, x( 8), S11, 0x698098d8); /* 9 */
  FF(d, a, b, c, x( 9), S12, 0x8b44f7af); /* 10 */
  FF(c, d, a, b, x(10), S13, 0xffff5bb1); /* 11 */
  FF(b, c, d, a, x(11), S14, 0x895cd7be); /* 12 */
  FF(a, b, c, d, x(12), S11, 0x6b901122); /* 13 */
  FF(d, a, b, c, x(13), S12, 0xfd987193); /* 14 */
  FF(c, d, a, b, x(14), S13, 0xa679438e); /* 15 */
  FF(b, c, d, a, x(15), S14, 0x49b40821); /* 16 */
  
  /* Round 2 */
  GG (a, b, c, d, x( 1), S21, 0xf61e2562); /* 17 */
  GG (d, a, b, c, x( 6), S22, 0xc040b340); /* 18 */
  GG (c, d, a, b, x(11), S23, 0x265e5a51); /* 19 */
  GG (b, c, d, a, x( 0), S24, 0xe9b6c7aa); /* 20 */
  GG (a, b, c, d, x( 5), S21, 0xd62f105d); /* 21 */
  GG (d, a, b, c, x(10), S22,  0x2441453); /* 22 */
  GG (c, d, a, b, x(15), S23, 0xd8a1e681); /* 23 */
  GG (b, c, d, a, x( 4), S24, 0xe7d3fbc8); /* 24 */
  GG (a, b, c, d, x( 9), S21, 0x21e1cde6); /* 25 */
  GG (d, a, b, c, x(14), S22, 0xc33707d6); /* 26 */
  GG (c, d, a, b, x( 3), S23, 0xf4d50d87); /* 27 */
  GG (b, c, d, a, x( 8), S24, 0x455a14ed); /* 28 */
  GG (a, b, c, d, x(13), S21, 0xa9e3e905); /* 29 */
  GG (d, a, b, c, x( 2), S22, 0xfcefa3f8); /* 30 */
  GG (c, d, a, b, x( 7), S23, 0x676f02d9); /* 31 */
  GG (b, c, d, a, x(12), S24, 0x8d2a4c8a); /* 32 */
  
  /* Round 3 */
  HH (a, b, c, d, x( 5), S31, 0xfffa3942); /* 33 */
  HH (d, a, b, c, x( 8), S32, 0x8771f681); /* 34 */
  HH (c, d, a, b, x(11), S33, 0x6d9d6122); /* 35 */
  HH (b, c, d, a, x(14), S34, 0xfde5380c); /* 36 */
  HH (a, b, c, d, x( 1), S31, 0xa4beea44); /* 37 */
  HH (d, a, b, c, x( 4), S32, 0x4bdecfa9); /* 38 */
  HH (c, d, a, b, x( 7), S33, 0xf6bb4b60); /* 39 */
  HH (b, c, d, a, x(10), S34, 0xbebfbc70); /* 40 */
  HH (a, b, c, d, x(13), S31, 0x289b7ec6); /* 41 */
  HH (d, a, b, c, x( 0), S32, 0xeaa127fa); /* 42 */
  HH (c, d, a, b, x( 3), S33, 0xd4ef3085); /* 43 */
  HH (b, c, d, a, x( 6), S34,  0x4881d05); /* 44 */
  HH (a, b, c, d, x( 9), S31, 0xd9d4d039); /* 45 */
  HH (d, a, b, c, x(12), S32, 0xe6db99e5); /* 46 */
  HH (c, d, a, b, x(15), S33, 0x1fa27cf8); /* 47 */
  HH (b, c, d, a, x( 2), S34, 0xc4ac5665); /* 48 */
  
  /* Round 4 */
  II (a, b, c, d, x( 0), S41, 0xf4292244); /* 49 */
  II (d, a, b, c, x( 7), S42, 0x432aff97); /* 50 */
  II (c, d, a, b, x(14), S43, 0xab9423a7); /* 51 */
  II (b, c, d, a, x( 5), S44, 0xfc93a039); /* 52 */
  II (a, b, c, d, x(12), S41, 0x655b59c3); /* 53 */
  II (d, a, b, c, x( 3), S42, 0x8f0ccc92); /* 54 */
  II (c, d, a, b, x(10), S43, 0xffeff47d); /* 55 */
  II (b, c, d, a, x( 1), S44, 0x85845dd1); /* 56 */
  II (a, b, c, d, x( 8), S41, 0x6fa87e4f); /* 57 */
  II (d, a, b, c, x(15), S42, 0xfe2ce6e0); /* 58 */
  II (c, d, a, b, x( 6), S43, 0xa3014314); /* 59 */
  II (b, c, d, a, x(13), S44, 0x4e0811a1); /* 60 */
  II (a, b, c, d, x( 4), S41, 0xf7537e82); /* 61 */
  II (d, a, b, c, x(11), S42, 0xbd3af235); /* 62 */
  II (c, d, a, b, x( 2), S43, 0x2ad7d2bb); /* 63 */
  II (b, c, d, a, x( 9), S44, 0xeb86d391); /* 64 */
  
  __asm {
    mov tmp1,DWORD PTR [buf]
    add DWORD PTR [tmp1],a
    add DWORD PTR [tmp1+4],b
    add DWORD PTR [tmp1+8],c
    add DWORD PTR [tmp1+12],d
  }
}
