#include <stdio.h>
#include "./rcr32.c"

void ks(u8* key, u8* iv, u32 keysize, u32 ivsize, u8* keystream){
	internal_state ctx;
	keysetup(&ctx, key, keysize, ivsize);
	ivsetup(&ctx, iv);
	keystream_bytes(&ctx, keystream, (u32) 4);
}