#ifndef _POLEG_OTP_H_
#define _POLEG_OTP_H_

typedef enum {
	NPCM750_KEY_SA    = 0,
	NPCM750_FUSE_SA   = 1,
	NPCM750_NUM_OF_SA = 2
} poleg_otp_storage_array;

// arrray images in flash, to program during fisrt boot (offsets in sector)
#define SA_KEYS_FLASH_IMAGE_OFFSET      (0x000)
#define SA_FUSE_FLASH_IMAGE_OFFSET      (0x400)
#define SA_TAG_FLASH_IMAGE_OFFSET       (0x800)
//                                        F     U     S     E     I     M     G     S
#define SA_TAG_FLASH_IMAGE_VAL          {0x46, 0x55, 0x53, 0x45, 0x49, 0x4d, 0x47, 0x53}
#define SA_TAG_FLASH_IMAGE_SIZE         (8)

#define SA_FUSE_FUSTRAP_OFFSET          (0x00)
#define SA_FUSE_FUSTRAP_OSECBOOT_MASK   (0x00800000)

struct poleg_otp_regs {
	unsigned int fst;
	unsigned int faddr;
	unsigned int fdata;
	unsigned int fcfg;
	unsigned int fustrap_fkeyind;
	unsigned int fctl;
};

#define FST_RDY                 (1 << 0)
#define FST_RDST                (1 << 1)
#define FST_RIEN                (1 << 2)

#define FADDR_BYTEADDR(addr)    ((addr) << 0)
#define FADDR_BITPOS(pos)       ((pos) << 10)
#define FADDR_VAL(addr, pos)    (FADDR_BYTEADDR(addr) | FADDR_BITPOS(pos))

#define FDATA_MASK              (0xff)

#define FUSTRAP_O_SECBOOT       (1 << 23)

#define FCFG_FDIS               (1 << 31)

#define FKEYIND_KVAL            (1 << 0)
#define FKEYIND_KSIZE_MASK      (0x00000070)
#define FKEYIND_KSIZE_128       (0x4 << 4)
#define FKEYIND_KSIZE_192       (0x5 << 4)
#define FKEYIND_KSIZE_256       (0x6 << 4)
#define FKEYIND_KIND_MASK       (0x000c0000)
#define FKEYIND_KIND_KEY(indx)  ((indx) << 18)

// Program cycle initiation values (sequence of two adjacent writes)
#define PROGRAM_ARM             0x1
#define PROGRAM_INIT            0xBF79E5D0

#define OTP2_BASE               0xF018A000
#define FUSTRAP                 (OTP2_BASE + 0x10)

// Read cycle initiation value
#define READ_INIT               0x02

// Value to clean FDATA contents
#define FDATA_CLEAN_VALUE       0x01


#define NPCM750_OTP_ARR_BYTE_SIZE        1024
#define MIN_PROGRAM_PULSES               4
#define MAX_PROGRAM_PULSES               20

int fuse_prog_image(u32 bank, uintptr_t address);
int  fuse_program_data(u32 bank, u32 word, u8 *data, u32 size);
int  npcm750_otp_select_key(u8 key_index);
bool npcm750_otp_is_fuse_array_disabled(poleg_otp_storage_array arr);
void npcm750_otp_nibble_parity_ecc_encode(u8 *datain, u8 *dataout, u32 size);
void npcm750_otp_majority_rule_ecc_encode(u8 *datain, u8 *dataout, u32 size);

#endif
