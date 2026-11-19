#include <common.h>
#include <command.h>
#include <linux/compiler.h>
//#include <asm/arch/clock.h>
#include <asm/system.h>
#include <asm/armv8/mmu.h>
#include <asm/global_data.h>
#include <cpu_func.h>


uint8_t g_gmac_loopback = 0;

static int do_nu_gmac(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	const char *loopback_str;

	if (argc < 2)
		return CMD_RET_USAGE;

	loopback_str = argv[1];
	g_gmac_loopback = loopback_str[0] - '0';
	printf("using type of loopback : %d\n", g_gmac_loopback);
	return 0;
}

U_BOOT_CMD(
	nu_gmac,   4,   1,     do_nu_gmac,
	"gmac debug",
	"nu_gmac <n>\n"
	"n = 1  - internal loopback "
	"n = 2  - external loopback "
);
