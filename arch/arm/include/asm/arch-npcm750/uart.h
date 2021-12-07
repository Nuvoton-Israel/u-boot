#ifndef _NPCM_UART_H_
#define _NPCM_UART_H_

struct npcm_uart {
	union {
		unsigned int	rbr;
		unsigned int	thr;
		unsigned int	dll;
	};
	union {
		unsigned int	ier;
		unsigned int	dlm;
	};
	union {
		unsigned int	iir;
		unsigned int	fcr;
	};
	unsigned int	lcr;
	unsigned int	mcr;
	unsigned int	lsr;
	unsigned int	msr;
	unsigned int	tor;
};

#define	IER_DBGACK	(1 << 4)
#define	IER_MSIE	(1 << 3)
#define	IER_RLSE	(1 << 2)
#define	IER_THREIE	(1 << 1)
#define	IER_RDAIE	(1 << 0)

#define	IIR_FMES	(1 << 7)
#define	IIR_RFTLS	(1 << 5)
#define	IIR_DMS		(1 << 4)
#define	IIR_IID		(1 << 1)
#define	IIR_NIP		(1 << 0)

#define	FCR_RFITL_1B	(0 << 4)
#define	FCR_RFITL_4B	(4 << 4)
#define	FCR_RFITL_8B	(8 << 4)
#define	FCR_RFITL_14B	(12 << 4)
#define	FCR_DMS			(1 << 3)
#define	FCR_TFR			(1 << 2)
#define	FCR_RFR			(1 << 1)
#define	FCR_FME			(1 << 0)

#define	LCR_DLAB	(1 << 7)
#define	LCR_BCB		(1 << 6)
#define	LCR_SPE		(1 << 5)
#define	LCR_EPS		(1 << 4)
#define	LCR_PBE		(1 << 3)
#define	LCR_NSB		(1 << 2)
#define	LCR_WLS_8b	(3 << 0)
#define	LCR_WLS_7b	(2 << 0)
#define	LCR_WLS_6b	(1 << 0)
#define	LCR_WLS_5b	(0 << 0)

#define	MCR_LBME	(1 << 4)
#define	MCR_OUT2	(1 << 3)
#define	MCR_RTS		(1 << 1)
#define	MCR_DTR		(1 << 0)

#define	LSR_ERR_RX	(1 << 7)
#define	LSR_TE		(1 << 6)
#define	LSR_THRE	(1 << 5)
#define	LSR_BII		(1 << 4)
#define	LSR_FEI		(1 << 3)
#define	LSR_PEI		(1 << 2)
#define	LSR_OEI		(1 << 1)
#define	LSR_RFDR	(1 << 0)

#define	MSR_DCD		(1 << 7)
#define	MSR_RI		(1 << 6)
#define	MSR_DSR		(1 << 5)
#define	MSR_CTS		(1 << 4)
#define	MSR_DDCD	(1 << 3)
#define	MSR_DRI		(1 << 2)
#define	MSR_DDSR	(1 << 1)
#define	MSR_DCTS	(1 << 0)

#endif
