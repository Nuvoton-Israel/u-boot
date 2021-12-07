#ifndef _POLEG_RNG_H_
#define _POLEG_RNG_H_

struct poleg_rng_regs {
	unsigned int rngcs;
	unsigned int rngd;
	unsigned int rngmode;
};

#define RNGCS_RNGE              (1 << 0)
#define RNGCS_DVALID            (1 << 1)
#define RNGCS_CLKP(range)       ((0x0f & (range)) << 2)

#define RNGMODE_M1ROSEL_VAL     (0x02) /* Ring Oscillator Select for Method I */

/*----------------------------------------------------------------------------*/
/* Core Domain Clock Frequency Range for the selected value is higher         */
/* than or equal to the actual Core domain clock frequency                    */
/*----------------------------------------------------------------------------*/
typedef enum
{
	RNG_CLKP_80_100_MHz = 0x00, /*default */
	RNG_CLKP_60_80_MHz  = 0x01,
	RNG_CLKP_50_60_MHz  = 0x02,
	RNG_CLKP_40_50_MHz  = 0x03,
	RNG_CLKP_30_40_MHz  = 0x04,
	RNG_CLKP_25_30_MHz  = 0x05,
	RNG_CLKP_20_25_MHz  = 0x06, /* recommended by poleg spec, match APB1. */
	RNG_CLKP_5_20_MHz   = 0x07,
	RNG_CLKP_2_15_MHz   = 0x08,
	RNG_CLKP_9_12_MHz   = 0x09,
	RNG_CLKP_7_9_MHz    = 0x0A,
	RNG_CLKP_6_7_MHz    = 0x0B,
	RNG_CLKP_5_6_MHz    = 0x0C,
	RNG_CLKP_4_5_MHz    = 0x0D,
	RNG_CLKP_3_4_MHz    = 0x0E,
	RNG_NUM_OF_CLKP
} RNG_CLKP_T;

void npcm750_rng_init(void);
void npcm750_rng_disable(void);

#endif
