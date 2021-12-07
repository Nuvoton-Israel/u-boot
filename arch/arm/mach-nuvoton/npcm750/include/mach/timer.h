#ifndef _POLEG_TIMER_H_
#define _POLEG_TIMER_H_

struct poleg_gptimer_regs {
	unsigned int tcsr0;
	unsigned int tcsr1;
	unsigned int ticr0;
	unsigned int ticr1;
	unsigned int tdr0;
	unsigned int tdr1;
	unsigned int tisr;
	unsigned int wtcr;
	unsigned int tcsr2;
	unsigned int tcsr3;
	unsigned int ticr2;
	unsigned int ticr3;
	unsigned int tdr2;
	unsigned int tdr3;
	unsigned char res1[4];	// 0x38
	unsigned int tcsr4;
	unsigned char res2[4];	// 0x44
	unsigned int ticr4;
	unsigned char res3[4];	// 0x4c
	unsigned int tdr4;
};
/* tcsr */
#define TCSR_EN		(1 << 30)
#define TCSR_MODE_PERIODIC	(1 << 27)
#define TCSR_PRESCALE_25	(25 - 1)

#endif
