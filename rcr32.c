#include <stdio.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

#define PYSIZE 260
#define YMININD (-3)
#define YMAXIND (256)

#define U32V(v) ((u32)(v) & (u32)(0xFFFFFFFF))
#define ROTL32(v, n) (U32V((v) << (n)) | ((v) >> (32 - (n))))

typedef struct
{
  u32 KPY0[PYSIZE];
  /* KPY and PY must be consecutive, as we use them as one large array in IVsetup */
  u32 PY0[PYSIZE+3];
  /* If there are accesses after PY, dummy should take these values and ignore them */
  u32 dummy0[3][2];
  u32 KPY1[PYSIZE];
  /* KPY and PY must be consecutive, as we use them as one large array in IVsetup */
  u32 PY1[PYSIZE+3];
  /* If there are accesses after PY, dummy should take these values and ignore them */
  u32 dummy1[3][2];
  u32 s;
  int keysize;
  int ivsize;
} internal_state;

/* This is a permutation of all the values between 0 and 255, */
/* used by the key setup and IV setup.                        */
/* It is computed by the init function. It can also be        */
/* computed in advance and put into the code.                 */
u8 IP[256];

/*
 * Key setup. It is the user's responsibility to select a legal
 * keysize and ivsize, from the set of supported value.
 */
void keysetup(
  internal_state* ctx, 
  const u8* key, 
  u32 keysize,                /* Key size in bits. */ 
  u32 ivsize)                 /* IV size in bits. */ 
{ 
  int i, j;
  u32 s;

  ctx->keysize=keysize;
  ctx->ivsize=ivsize;
  int kb=(keysize+7)>>3;
  int ivb=(ivsize+7)>>3;

#define Y(x) (ctx->KPY0[(x)-(YMININD)])

  s = (u32)IP[kb-1];
  s = (s<<8) | (u32)IP[(s ^ (ivb-1))&0xFF];
  s = (s<<8) | (u32)IP[(s ^ key[0])&0xFF];
  s = (s<<8) | (u32)IP[(s ^ key[kb-1])&0xFF];

  for(j=0; j<kb; j++)
    {
      s = s + key[j];
      u8 s0 = IP[s&0xFF];
      s = ROTL32(s, 8) ^ (u32)s0;
    }
  /* Again */
  for(j=0; j<kb; j++)
    {
      s = s + key[j];
      u8 s0 = IP[s&0xFF];
      s ^= ROTL32(s, 8) + (u32)s0;
    }

  for(i=YMININD, j=0; i<=YMAXIND; i++)
    {
      s = s + key[j];
      u8 s0 = IP[s&0xFF];
      s = ROTL32(s, 8) ^ (u32)s0;
      Y(i) = s;
      j++;
      if(j<kb)
		continue;
      j=0;
    }
}
#undef P
#undef Y

/*
 * IV setup. After having called ECRYPT_keysetup(), the user is
 * allowed to call ECRYPT_ivsetup() different times in order to
 * encrypt/decrypt different messages with the same key but different
 * IV's.
 */
void ivsetup(
  internal_state* ctx, 
  const u8* iv)
{
  int i;
  u32 s;

  int keysize=ctx->keysize;
  int ivsize=ctx->ivsize;
  int ivb=(ivsize+7)>>3;

#define P(x) (((u8*)&(ctx->KPY1[x]))[0])
#define KY(x) (ctx->KPY0[(x)-(YMININD)])
#define EIV(x) (((u8*)&(ctx->KPY1[x+256-ivb]))[2])

  /* Create an initial permutation */
  u8 v= iv[0] ^ ((KY(0)>>16)&0xFF);
  u8 d=(iv[1%ivb] ^ ((KY(1)>>16)&0xFF))|1;
  for(i=0; i<256; i++)
    {
      P(i)=IP[v];
      v+=d;
    }


  /* Initial s */
  s = ((u32)v<<24)^((u32)d<<16)^((u32)P(254)<<8)^((u32)P(255));
  s ^= KY(YMININD)+KY(YMAXIND);
  for(i=0; i<ivb; i++)
    {
      s = s + iv[i] + KY(YMININD+i);
      u8 s0 = P(s&0xFF);
      EIV(i) = s0;
      s = ROTL32(s, 8) ^ (u32)s0;
    }
  /* Again, but with the last words of KY, and update EIV */
  for(i=0; i<ivb; i++)
    {
      s = s + EIV((i+ivb-1)%ivb) + KY(YMAXIND-i);
      u8 s0 = P(s&0xFF);
      EIV(i) += s0;
      s = ROTL32(s, 8) ^ (u32)s0;
    }

#undef P
#undef Y
#undef EIV
#define P(i4,j) (((u8*)(ctx->KPY1))[(i4)+4*(j)])  /* access P[i+j] where i4=4*i. */
                                           /* P is byte 0 of the 4-byte record */
#define EIV(i4,j) (((u8*)(ctx->KPY1))[(i4)+4*(j+256-ivb)+2])
                                           /* EIV is byte 2 of the 4-byte record */
                                           /* access EIV[i+j] where i4=4*i. */
#define Y(i4,j) (((u32*)&(((u8*)(ctx->KPY0))[(i4)+4*((j)-(YMININD))]))[0])
                                           /* access Y[i+j] where i4=4*i. */

  for(i=0; i<PYSIZE*4; i+=4)
    {
      u32 x0 = EIV(i,ivb) = EIV(i,0)^(s&0xFF);
      P(i,256)=P(i,x0);
      P(i,x0)=P(i,0);
      s=ROTL32(s,8)+Y(i,YMAXIND);
      Y(i,YMAXIND+1) = Y(i,YMININD) + (s^Y(i,x0));
    }

  s=s+Y(i,26)+Y(i,153)+Y(i,208);
  if(s==0)
    s=keysize+(ivsize<<16)+0x87654321;
  ctx->s=s;

}

#undef P
#undef Y

#define NUMBLOCKSATONCE 4000 /* A near-optimal value for the static array */
/* All computations are made in multiples of                 */
/* NUMBLOCKSATONCE*4 bytes, if possible                      */

static u32 PY[2][(NUMBLOCKSATONCE+PYSIZE)];
#define P(i4,j) (((u8*)(PY[1]))[(i4)+4*(j)])  /* access P[i+j] where i4=4*i. */
		 			   /* P is byte 0 of the 4-byte record */
#define Y(i4,j) (((u32*)&((((u8*)(PY[0])))[(i4)+4*((j)-(YMININD))]))[0])
		 			   /* access Y[i+j] where i4=4*i. */

void process_bytes(
  int action,                 /* 0 = encrypt; 1 = decrypt; */
  internal_state* ctx, 
  const u8* input, 
  u8* output, 
  u32 msglen)                /* Message length in bytes.     */ 
     /* If the msglen is not a multiple of 8, this must be   */
     /* the last call for this stream, or either you will    */
     /* loose a few bytes                                    */
{
  int i;

  u32 s=ctx->s;

  while(msglen>=NUMBLOCKSATONCE*4)
    {
      memcpy((void*)PY[0], (void*)ctx->PY0, PYSIZE*4);
      memcpy((void*)PY[1], (void*)ctx->PY1, PYSIZE*4);

      for(i=0; i<NUMBLOCKSATONCE*4; i+=4)
		{
		  u32 x0=(Y(i,185)&0xFF);
		  P(i,256)=P(i,x0);
		  P(i,x0)=P(i,0);
		  s+=Y(i,P(i,1+72));
		  s-=Y(i,P(i,1+239));
		  s=ROTL32(s, 1);
		  Y(i,YMAXIND+1)=(s^Y(i,YMININD))+Y(i,P(i,1+153));
      s=ROTL32(s, 18);
		  u32 output2=(s^Y(i,-1))+Y(i,P(i,1+208));
		  ((u32*)(output+i))[0]=output2^((u32*)(input+i))[0];
		}

      memcpy(ctx->PY0, ((u8*)(PY[0]))+i, PYSIZE*4);
      memcpy(ctx->PY1, ((u8*)(PY[1]))+i, PYSIZE*4);
      msglen-=NUMBLOCKSATONCE*4;
      input+=NUMBLOCKSATONCE*4;
      output+=NUMBLOCKSATONCE*4;
    }
  if(msglen>0)
    {

      memcpy(PY[0], ctx->PY0, PYSIZE*4);
      memcpy(PY[1], ctx->PY1, PYSIZE*4);

      for(i=0; i<(msglen&(~3)); i+=4)
		{
		  u32 x0=(Y(i,185)&0xFF);
		  P(i,256)=P(i,x0);
		  P(i,x0)=P(i,0);
		  s+=Y(i,P(i,1+72));
		  s-=Y(i,P(i,1+239));
		  s=ROTL32(s, 1);
		  Y(i,YMAXIND+1)=(s^Y(i,YMININD))+Y(i,P(i,1+153));
      s=ROTL32(s, 18);
		  u32 output2=(s^Y(i,-1))+Y(i,P(i,1+208));
		  ((u32*)(output+i))[0]=output2^((u32*)(input+i))[0];
		}

      if(msglen&3)
		{
		  int ii;
		  u32 x0=(Y(i,185)&0xFF);
		  P(i,256)=P(i,x0);
		  P(i,x0)=P(i,0);
		  s+=Y(i,P(i,1+72));
		  s-=Y(i,P(i,1+239));
		  s=ROTL32(s, 1);
		  Y(i,YMAXIND+1)=(s^Y(i,YMININD))+Y(i,P(i,1+153));
      s=ROTL32(s, 18);
		  u8 outputb[8];
		  u32 output2=(s^Y(i,-1))+Y(i,P(i,1+208));
		  ((u32*)(outputb))[0] = output2;
		  for(ii=0; ii<(msglen&3); ii++)
		    (output+i)[ii]=outputb[ii]^(input+i)[ii];
		}

      memcpy(ctx->PY0, ((u8*)(PY[0]))+i, PYSIZE*4);
      memcpy(ctx->PY1, ((u8*)(PY[1]))+i, PYSIZE*4);
    }

  ctx->s=s;
}

void keystream_bytes(
  internal_state* ctx,
  u8* keystream,
  u32 length)                /* Length of keystream in bytes. */
     /* If the length is not a multiple of 8, this must be the */
     /* last call for this stream, or else you will loose a few bytes */
{
  int i;

  u32 s=ctx->s;

  while(length>=NUMBLOCKSATONCE*4)
    {
      memcpy((void*)PY[0], (void*)ctx->PY0, PYSIZE*4);
      memcpy((void*)PY[1], (void*)ctx->PY1, PYSIZE*4);

      for(i=0; i<NUMBLOCKSATONCE*4; i+=4)
		{
		  u32 x0=(Y(i,185)&0xFF);
		  P(i,256)=P(i,x0);
		  P(i,x0)=P(i,0);
		  s+=Y(i,P(i,1+72));
		  s-=Y(i,P(i,1+239));
		  s=ROTL32(s, 1);
		  Y(i,YMAXIND+1)=(s^Y(i,YMININD))+Y(i,P(i,1+153));
      s=ROTL32(s, 18);
		  u32 output2=(s^Y(i,-1))+Y(i,P(i,1+208));
		  ((u32*)(keystream+i))[0] = output2;
		}
	
      memcpy(ctx->PY0, ((u8*)(PY[0]))+i, PYSIZE*4);
      memcpy(ctx->PY1, ((u8*)(PY[1]))+i, PYSIZE*4);
      length-=NUMBLOCKSATONCE*4;
      keystream+=NUMBLOCKSATONCE*4;
    }
  if(length>0)
    {

      memcpy(PY[0], ctx->PY0, PYSIZE*4);
      memcpy(PY[1], ctx->PY1, PYSIZE*4);

      for(i=0; i<(length&(~3)); i+=4)
		{
		  u32 x0=(Y(i,185)&0xFF);
		  P(i,256)=P(i,x0);
		  P(i,x0)=P(i,0);
		  s+=Y(i,P(i,1+72));
		  s-=Y(i,P(i,1+239));
      s=ROTL32(s, 1);
		  Y(i,YMAXIND+1)=(s^Y(i,YMININD))+Y(i,P(i,1+153));
      s=ROTL32(s, 18);
		  u32 output2=(s^Y(i,-1))+Y(i,P(i,1+208));
		  ((u32*)(keystream+i))[0] = output2;
		}

      if(length&3)
		{
		  int ii;
		  u32 x0=(Y(i,185)&0xFF);
		  P(i,256)=P(i,x0);
		  P(i,x0)=P(i,0);
		  s+=Y(i,P(i,1+72));
		  s-=Y(i,P(i,1+239));
		  s=ROTL32(s, 1);
		  Y(i,YMAXIND+1)=(s^Y(i,YMININD))+Y(i,P(i,1+153));
      s=ROTL32(s, 18);
		  u8 outputb[8];
		  u32 output2=(s^Y(i,-1))+Y(i,P(i,1+208));
		  ((u32*)(outputb))[0] = output2;
		  for(ii=0; ii<(length&3); ii++)
		    (keystream+i)[ii]=outputb[ii];
		}

      memcpy(ctx->PY0, ((u8*)(PY[0]))+i, PYSIZE*4);
      memcpy(ctx->PY1, ((u8*)(PY[1]))+i, PYSIZE*4);
    }

  ctx->s=s;
}
#undef P
#undef Y

void init(void)
{
  int i;
  u8 j=0;
  static char str[] =
    "This is the seed for generating the fixed internal permutation for RCR. "
    "The permutation is used in the key setup and IV setup as a source of nonlinearity. "
    "The shifted special keys on a keyboard are ~!@#$%^&*()_+{}:|<>?";
  u8 *p=(u8*)str;

  for(i=0; i<256; i++)
    IP[i] = i;

  for(i=0; i<256*16; i++)
    {
      j+=p[0];
      u8 tmp=IP[i&0xFF];
      IP[i&0xFF] = IP[j&0xFF];
      IP[j&0xFF] = tmp;
      p++;
      if(p[0] == 0)
	      p=(u8*)str;
    }
}
