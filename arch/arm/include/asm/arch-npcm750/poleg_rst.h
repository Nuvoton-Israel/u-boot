#ifndef _POLEG_RST_H_
#define _POLEG_RST_H_

enum reset_type {
    PORST    = 0x01,
    CORST    = 0x02,
    WD0RST   = 0x03,
    SWR1ST   = 0x04,
    SWR2ST   = 0x05,
    SWR3ST   = 0x06,
    SWR4ST   = 0x07,
    WD1RST   = 0x08,
    WD2RST   = 0x09,
    UNKNOWN  = 0x10,
};

#define PORST 0x80000000
#define CORST 0x40000000
#define WD0RST 0x20000000
#define SWR1ST 0x10000000
#define SWR2ST 0x08000000
#define SWR3ST 0x04000000
#define SWR4ST 0x02000000
#define WD1RST 0x01000000
#define WD2RST 0x00800000
#define RESSR_MASK 0xff800000

enum reset_type npcm7xx_reset_reason(void);

#endif

