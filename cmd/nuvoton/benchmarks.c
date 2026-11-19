

#include <command.h>
#include <linux/printk.h>

extern void main_dry (int num_of_runs);
extern ulong simple_strtoul(const char *cp, char **endp, uint base);

static int do_dhry(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int num_of_runs = 1;

	if (1 < argc)
	{
		num_of_runs = simple_strtoul(argv[1], NULL, 0);
	}

	main_dry(num_of_runs);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(dhry, 2, 1, do_dhry,
	   "run dhrystone benchmark", "dhry [loops]"
);



extern int main_whet (int num_of_loops);

static int do_whet(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int num_of_runs = 1;

	if (1 < argc)
	{
		num_of_runs = simple_strtoul(argv[1], NULL, 0);
	}

	main_whet(num_of_runs);

	return CMD_RET_SUCCESS;
}


U_BOOT_CMD(whet, 2, 1, do_whet,
	   "run whetstone benchmark", "whet [loops]"
);
