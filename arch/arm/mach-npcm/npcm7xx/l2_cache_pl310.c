#include <common.h>
#include <asm/pl310.h>
#include <asm/io.h>

extern  void l2_pl310_init(void);
void set_pl310_ctrl(u32 enable);



void set_pl310_ctrl(u32 enable)
{
	struct pl310_regs *const pl310 = (struct pl310_regs *)CONFIG_SYS_PL310_BASE;
	
	writel(enable, &pl310->pl310_ctrl);

}
#ifndef CONFIG_SYS_L2CACHE_OFF
void v7_outer_cache_enable(void)
{
	printf("l2_pl310_init\n");
	l2_pl310_init();

	set_pl310_ctrl(1);
}

void v7_outer_cache_disable(void)
{
	set_pl310_ctrl(0);
}
#endif /* !CONFIG_SYS_L2CACHE_OFF */
