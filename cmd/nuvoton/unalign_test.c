#include <command.h>
#include <dm.h>
#include <env.h>
#include <getopt.h>
#include <lmb.h>
#include <mapmem.h>
#include <net.h>
#include <serial.h>
#include <video.h>
#include <vsprintf.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <display_options.h>



static int do_unalign(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{

	asm volatile (
	    "mov w0, #0x81000000\n\t"
	    "add x1, x0, #0xf10\n\t"
	    "ldur x0, [x1, #7]\n\t"
	);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(nu_unalign, 2, 1, do_unalign,
	   "test unalign access", "nu_unalign"
);
