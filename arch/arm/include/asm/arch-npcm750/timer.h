#ifndef _NPCM_TIMER_H_
#define _NPCM_TIMER_H_

struct npcm_gptimer_regs {
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
	unsigned char res1[0x8];
	unsigned int tcsr4;
	unsigned char res2[0x4];
	unsigned int ticr4;
	unsigned char res3[0x4];
	unsigned int tdr4;
	unsigned char res4[0x28];
	unsigned int timver;
};
/* tcsr */
#define TCSR_EN		(1 << 30)
#define TCSR_MODE_PERIODIC	(1 << 27)
#define TCSR_PRESCALE_25	(25 - 1)

#endif
