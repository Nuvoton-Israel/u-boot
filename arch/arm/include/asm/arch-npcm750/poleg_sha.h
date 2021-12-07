#ifndef _POLEG_SHA_H_
#define _POLEG_SHA_H_

#define HASH_DIG_H_NUM        8

/*----------------------------------------------------------------------------*/
/* SHA type                                                                   */
/*----------------------------------------------------------------------------*/
typedef enum
{
  npcm750_sha_type_sha2 = 0,	/*do not change - match SHA arch spec */
  npcm750_sha_type_sha1,
  npcm750_sha_type_num
} npcm750_sha_type;

struct poleg_sha_regs {
	unsigned int hash_data_in;              // 0x00
	unsigned char hash_ctr_sts;             // 0x04
	unsigned char reserved_0[0x03];         // 0x05
	unsigned char hash_cfg;                 // 0x08
	unsigned char reserved_1[0x03];         // 0x09
	unsigned char hash_ver;                 // 0x0c
	unsigned char reserved_2[0x13];     	// 0x0d
	unsigned int hash_dig[HASH_DIG_H_NUM];  // 0x20
};

#define HASH_CTR_STS_SHA_EN             (1 << 0)
#define HASH_CTR_STS_SHA_BUSY           (1 << 1)
#define HASH_CTR_STS_SHA_RST            (1 << 2)

#define HASH_CFG_SHA1_SHA2              (1 << 0)

int npcm750_sha_calc(npcm750_sha_type type, const u8* inBuff, u32 len, u8* hashDigest);
int npcm750_sha_selftest(npcm750_sha_type type);

#endif //_POLEG_SHA_H_
