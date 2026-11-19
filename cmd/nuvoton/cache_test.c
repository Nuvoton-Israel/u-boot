#include <common.h>
#include <command.h>
#include <linux/compiler.h>
//#include <asm/arch/clock.h>
#include <asm/system.h>
#include <asm/armv8/mmu.h>
#include <asm/global_data.h>
#include <cpu_func.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_ARCH_NPCM9XX

#define CACHED_ADDR	0x90000000
#define MIRROR_ADDR	0xb0000000

#else

#define CACHED_ADDR	0x10000000
#define MIRROR_ADDR	0x50000000

#endif

#define Test4Bops() \
{\
	asm volatile("add x0, x0, #1"); \
}

#define Test8Bops() \
{\
	Test4Bops();\
	Test4Bops();\
}
#define Test32Bops() \
{\
	Test8Bops();\
	Test8Bops();\
	Test8Bops();\
	Test8Bops();\
}
#define Test128Bops() \
{\
	Test32Bops();\
	Test32Bops();\
	Test32Bops();\
	Test32Bops();\
}
#define Test512Bops() \
{\
	Test128Bops();\
	Test128Bops();\
	Test128Bops();\
	Test128Bops();\
}
#define Test2Kops() \
{\
	Test512Bops();\
	Test512Bops();\
	Test512Bops();\
	Test512Bops();\
}
#define Test8Kops() \
{\
	Test2Kops();\
	Test2Kops();\
	Test2Kops();\
	Test2Kops();\
}
#define Test32Kops() \
{\
	Test8Kops();\
	Test8Kops();\
	Test8Kops();\
	Test8Kops();\
}

extern struct mm_region *mem_map;

#ifdef CONFIG_ARCH_NPCM9XX

static volatile u32 memtest_src_addr = 0x80600000;

#else

static volatile u32 memtest_src_addr = 0x00600000;

#endif
static bool mmu_map_changed = false;

struct cache_policy {
	char *desc;
	u64 attr;
};
#define DEFAULT_ATTR	12
static struct cache_policy cache_options[] = {
	{"No alloction", 0x44},
	{"inner/outer Write-through transient, write-allocate", 0x11},
	{"inner/outer Write-through transient, read-allocate", 0x22},
	{"inner/outer Write-through transient, read/write-allocate", 0x33},
	{"inner/outer Write-through non-transient, write-allocate", 0x99},
	{"inner/outer Write-through non-transient, read-allocate", 0xAA},
	{"inner/outer Write-through non-transient, readwrite-allocate", 0xBB},
	{"inner/outer Write-back transient, write-allocate", 0x55},
	{"inner/outer Write-back transient, read-allocate", 0x66},
	{"inner/outer Write-back transient, read/write-allocate", 0x77},
	{"inner/outer Write-back non-transient, write-allocate", 0xDD},
	{"inner/outer Write-back non-transient, read-allocate", 0xEE},
	{"inner/outer Write-back non-transient, readwrite-allocate", 0xFF},

};

static struct mm_region *saved_mem_map;

#ifdef CONFIG_ARCH_NPCM9XX

static struct mm_region cache_test_mem_map[1 + CONFIG_NR_DRAM_BANKS + 2] = {
	{
		/* DRAM */
		.phys = 0x80000000UL,
		.virt = 0x80000000UL,
		.size = 0x20000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	},
	{
		/* DRAM non-cached*/
		.phys = 0x80000000UL,
		.virt = 0xa0000000UL,
		.size = 0x18000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			 PTE_BLOCK_INNER_SHARE
	},
	{
		/* DRAM */
		.phys = 0xb8000000UL,
		.virt = 0xb8000000UL,
		.size = 0x08000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	},
	{
		.phys = 0x00000000UL,
		.virt = 0x00000000UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	},
//	{
//		.phys = 0x80000000UL,
//		.virt = 0x80000000UL,
//		.size = 0x80000000UL,
//		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
//			 PTE_BLOCK_NON_SHARE |
//			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
//	},
	{
		/* List terminator */
		0,
	}
};


#else

static struct mm_region cache_test_mem_map[1 + CONFIG_NR_DRAM_BANKS + 2] = {
	{
		/* DRAM */
		.phys = 0x0UL,
		.virt = 0x0UL,
		.size = 0x40000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	},
	{
		/* DRAM non-cached */
		.phys = 0x0UL,
		.virt = 0x40000000UL,
		.size = 0x40000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			 PTE_BLOCK_INNER_SHARE
	},
	{
		.phys = 0x80000000UL,
		.virt = 0x80000000UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	},
	{
		/* List terminator */
		0,
	},
};

#endif

void __weak invalidate_icache_all(void)
{
	/* please define arch specific invalidate_icache_all */
	puts("No arch specific invalidate_icache_all available!\n");
}

void __weak flush_dcache_all(void)
{
	puts("No arch specific flush_dcache_all available!\n");
	/* please define arch specific flush_dcache_all */
}

static void test_code(void)
{
	Test32Kops();
}

static void disable_dcache(void)
{
	uint32_t sctlr;

	sctlr = get_sctlr();

	/* if cache isn't enabled no need to disable */
	if (!(sctlr & CR_C))
		return;

	set_sctlr(sctlr & ~CR_C);

	flush_dcache_all();
	__asm_invalidate_tlb_all();
}

static void enable_dcache(void)
{
	uint32_t sctlr;

	sctlr = get_sctlr();
	if (!(sctlr & CR_M))
		return;

	set_sctlr(sctlr | CR_C);
}

static void change_mmu_map(void)
{
	if (mmu_map_changed)
		return;

	gd->arch.tlb_fillptr = 0;

	flush_dcache_all();
	__asm_invalidate_tlb_all();
	invalidate_dcache_all();
	invalidate_icache_all();

	dcache_disable();
	saved_mem_map = mem_map;
	mem_map = cache_test_mem_map;
	dcache_enable();
	mmu_map_changed = true;
}

static void restore_mmu_map(void)
{
	if (!mmu_map_changed)
		return;

	flush_dcache_all();
	__asm_invalidate_tlb_all();
	invalidate_dcache_all();
	invalidate_icache_all();

	dcache_disable();
	mem_map = saved_mem_map;
	dcache_enable();
	mmu_map_changed = false;
}


static void change_mem_attr(u64 memattrs)
{
	int el;

	/* Disable cache and MMU */
	dcache_disable();	/* TLBs are invalidated */
	invalidate_icache_all();

	/* change mem attr */
	el = current_el();
	set_ttbr_tcr_mair(el, gd->arch.tlb_addr, get_tcr(NULL, NULL), memattrs);

	/* enable cache and MMU */
	set_sctlr(get_sctlr() | CR_M | CR_C);
}

static void L1fill(u64 target)
{
	u64 memtest_addr = target;
	volatile u32 *ptr;
	int i, j;
	int set = 128;
	int way = 4;
	u64 addr;

	for (i = 0; i < set; i++) {
		for (j = 0; j < way; j++) {
			addr = memtest_addr | (i << 6) | (j << 13);
			ptr = (u32*)addr;
			*ptr = (u32)addr;
			dmb();
			isb();
		}
	}
}
static void L2fill(u64 target)
{
	u64 memtest_addr = target;
	volatile u32 *ptr = (u32 *)0x0;
	int i, j;
	int set = 1024;
	int way = 8;
	u64 addr;

	for (i = 0; i < set; i++) {
		for (j = 1; j <= way; j++) {
			addr = memtest_addr | (i << 6) | (j << 16);
			ptr = (u32*)addr;
			*ptr = (u32)addr;
			dmb();
			isb();
		}
	}
}
static void my_memwrite(u32 source, u32 size)
{
	int i;
	u32 *ptr = (u32 *)(uintptr_t)source;

	for (i = 0; i < size/4; i++)
	    *(ptr + i) = source + i;
}

static void my_memread(u32 source, u32 size)
{
	int i;
	volatile u32 *ptr = (u32 *)(uintptr_t)source;
	u32 sigma = 0;

	/* since 'sigma' will not be use but 'ptr' is volatile, the assembly
	 * will do only reads into a register w/o adding to sigma
	 * we also do loop unrolling to minimize instruction not related to
	 * loading from memory, so we get one assembly line per read
	 * it is recommended to verify this in disassembly
	 */

	for (i = 0, sigma = 0; i < size/4; i+=64){
 	    sigma += *(ptr + i +  0);
 	    sigma += *(ptr + i +  1);
 	    sigma += *(ptr + i +  2);
 	    sigma += *(ptr + i +  3);
 	    sigma += *(ptr + i +  4);
 	    sigma += *(ptr + i +  5);
 	    sigma += *(ptr + i +  6);
 	    sigma += *(ptr + i +  7);
 	    sigma += *(ptr + i +  8);
 	    sigma += *(ptr + i +  9);
	    sigma += *(ptr + i + 10);
	    sigma += *(ptr + i + 11);
	    sigma += *(ptr + i + 12);
	    sigma += *(ptr + i + 13);
	    sigma += *(ptr + i + 14);
	    sigma += *(ptr + i + 15);
	    sigma += *(ptr + i + 16);
	    sigma += *(ptr + i + 17);
	    sigma += *(ptr + i + 18);
	    sigma += *(ptr + i + 19);
	    sigma += *(ptr + i + 20);
	    sigma += *(ptr + i + 21);
	    sigma += *(ptr + i + 22);
	    sigma += *(ptr + i + 23);
	    sigma += *(ptr + i + 24);
	    sigma += *(ptr + i + 25);
	    sigma += *(ptr + i + 26);
	    sigma += *(ptr + i + 27);
	    sigma += *(ptr + i + 28);
	    sigma += *(ptr + i + 29);
	    sigma += *(ptr + i + 30);
	    sigma += *(ptr + i + 31);
	    sigma += *(ptr + i + 32);
	    sigma += *(ptr + i + 33);
	    sigma += *(ptr + i + 34);
	    sigma += *(ptr + i + 35);
	    sigma += *(ptr + i + 36);
	    sigma += *(ptr + i + 37);
	    sigma += *(ptr + i + 38);
	    sigma += *(ptr + i + 39);
	    sigma += *(ptr + i + 40);
	    sigma += *(ptr + i + 41);
	    sigma += *(ptr + i + 42);
	    sigma += *(ptr + i + 43);
	    sigma += *(ptr + i + 44);
	    sigma += *(ptr + i + 45);
	    sigma += *(ptr + i + 46);
	    sigma += *(ptr + i + 47);
	    sigma += *(ptr + i + 48);
	    sigma += *(ptr + i + 49);
	    sigma += *(ptr + i + 50);
	    sigma += *(ptr + i + 51);
	    sigma += *(ptr + i + 52);
	    sigma += *(ptr + i + 53);
	    sigma += *(ptr + i + 54);
	    sigma += *(ptr + i + 55);
	    sigma += *(ptr + i + 56);
	    sigma += *(ptr + i + 57);
	    sigma += *(ptr + i + 58);
	    sigma += *(ptr + i + 59);
	    sigma += *(ptr + i + 60);
	    sigma += *(ptr + i + 61);
	    sigma += *(ptr + i + 62);
	    sigma += *(ptr + i + 63);
	}
}

static int do_sysinfo(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int el;
	u64 attr=0;
	u32 val = 0;
	u64 val64 = 0;

	el = current_el();
	printf("current el = %d\n", el);
	printf("sctlr = 0x%x\n", get_sctlr());

	if (el == 1) {
		asm volatile("mrs %0, mair_el1" : "=r" (attr) : : "cc");
	} else if (el == 2) {
		asm volatile("mrs %0, mair_el2" : "=r" (attr) : : "cc");
	} else if (el == 3) {
		asm volatile("mrs %0, mair_el3" : "=r" (attr) : : "cc");
	}
	printf("mair = 0x%llx\n", attr);

	if (el > 0) {
		asm volatile("mrs %0, csselr_el1" : "=r" (val) : : "cc");
		printf("csselr_el1=0x%x\n", val);
		asm volatile("mrs %0, ccsidr_el1" : "=r" (val) : : "cc");
		printf("ccsidr_el1=0x%x\n", val);

		/* select L1 I-cache */
		val = 1;
		asm volatile("msr csselr_el1, %0" : : "r" (val) : "cc");
		asm volatile("mrs %0, csselr_el1" : "=r" (val) : : "cc");
		printf("csselr_el1=0x%x\n", val);
		asm volatile("mrs %0, ccsidr_el1" : "=r" (val) : : "cc");
		printf("ccsidr_el1=0x%x\n", val);

		/* select L2 unified-cache */
		val = 2;
		asm volatile("msr csselr_el1, %0" : : "r" (val) : "cc");
		asm volatile("mrs %0, csselr_el1" : "=r" (val) : : "cc");
		printf("csselr_el1=0x%x\n", val);
		asm volatile("mrs %0, ccsidr_el1" : "=r" (val) : : "cc");
		printf("ccsidr_el1=0x%x\n", val);
	}
	/* cpuactlr_el1 */
	asm volatile("mrs %0, S3_1_c15_c2_0" : "=r" (val64) : : "cc");
	printf("cpuactlr_el1 = 0x%llx\n", val64);
	return 0;
}

static int do_cpuinfo(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int el;
	u64 attr=0;
	u32 val = 0;
	u64 val64 = 0;

	el = current_el();
	printf("current el = %d\n", el);
	printf("sctlr = 0x%x\n", get_sctlr());

	if (el == 1) {
		asm volatile("mrs %0, mair_el1" : "=r" (attr) : : "cc");
	} else if (el == 2) {
		asm volatile("mrs %0, mair_el2" : "=r" (attr) : : "cc");
	} else if (el == 3) {
		asm volatile("mrs %0, mair_el3" : "=r" (attr) : : "cc");
	}
	printf("mair = 0x%llx\n", attr);

	if (el > 0) {
		asm volatile("mrs %0, csselr_el1" : "=r" (val) : : "cc");
		printf("csselr_el1=0x%x\n", val);
		asm volatile("mrs %0, ccsidr_el1" : "=r" (val) : : "cc");
		printf("ccsidr_el1=0x%x\n", val);

		/* select L1 I-cache */
		val = 1;
		asm volatile("msr csselr_el1, %0" : : "r" (val) : "cc");
		asm volatile("mrs %0, csselr_el1" : "=r" (val) : : "cc");
		printf("csselr_el1=0x%x\n", val);
		asm volatile("mrs %0, ccsidr_el1" : "=r" (val) : : "cc");
		printf("ccsidr_el1=0x%x\n", val);

		/* select L2 unified-cache */
		val = 2;
		asm volatile("msr csselr_el1, %0" : : "r" (val) : "cc");
		asm volatile("mrs %0, csselr_el1" : "=r" (val) : : "cc");
		printf("csselr_el1=0x%x\n", val);
		asm volatile("mrs %0, ccsidr_el1" : "=r" (val) : : "cc");
		printf("ccsidr_el1=0x%x\n", val);
	}
	/* cpuactlr_el1 */
	asm volatile("mrs %0, S3_1_c15_c2_0" : "=r" (val64) : : "cc");
	printf("cpuactlr_el1 = 0x%llx\n", val64);

	/* RMR */
	asm volatile("mrs %0, rmr_el3" : "=r" (val) : : "cc");
	printf("rmr_el3 = 0x%x\n", val);

	return 0;
}

int do_icache_test(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	u64 start, end, diff;
	u64 val;

	change_mmu_map();
	flush_dcache_all();
	disable_dcache();
	invalidate_icache_all();

	start = timer_get_us();
	asm volatile("mov X0, #0");
	test_code();
	asm volatile("mov %0, X0" : "=r" (val) : : "cc");
	end = timer_get_us();
	diff = end - start;
	printf("icache test run1: x0=%llu, take %llu usec\n", val, diff);

	start = timer_get_us();
	asm volatile("mov X0, #0");
	test_code();
	asm volatile("mov %0, X0" : "=r" (val) : : "cc");
	end = timer_get_us();
	diff = end - start;
	printf("icache test run2: x0=%llu, take %llu usec\n", val, diff);

	invalidate_icache_all();
	start = timer_get_us();
	asm volatile("mov X0, #0");
	test_code();
	asm volatile("mov %0, X0" : "=r" (val) : : "cc");
	end = timer_get_us();
	diff = end - start;
	printf("icache test run3: x0=%llu, take %llu usec\n", val, diff);

	enable_dcache();
	restore_mmu_map();

	return 0;
}

static int do_eviction_test(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int el;
	u64 attr = 0, memattrs;
	volatile u32 *mirror_addr = (u32 *)MIRROR_ADDR;
	volatile u32 *cached_addr = (u32 *)CACHED_ADDR;

	change_mmu_map();
	memattrs = (MEMORY_ATTRIBUTES & ~((u64)0xFF << (MT_NORMAL*8))) |
		(cache_options[DEFAULT_ATTR].attr << (MT_NORMAL*8));
	change_mem_attr(memattrs);

	el = current_el();

	if (el == 1) {
		asm volatile("mrs %0, mair_el1" : "=r" (attr) : : "cc");
	} else if (el == 2) {
		asm volatile("mrs %0, mair_el2" : "=r" (attr) : : "cc");
	} else if (el == 3) {
		asm volatile("mrs %0, mair_el3" : "=r" (attr) : : "cc");
	}
	printf("mair = 0x%llx\n", attr);

	*mirror_addr = 0;
	flush_dcache_all();

	printf("word at addr = 0x%x\n", *cached_addr);
	printf("word at mirror addr = 0x%x\n", *mirror_addr);

	printf("write pattern 0x1111 to mirror addr\n");
	*mirror_addr = 0x1111;
	printf("word at addr = 0x%x\n", *cached_addr);
	printf("word at mirror addr = 0x%x\n", *mirror_addr);

	printf("fill up cache by reading 32+512KB\n");
	L1fill(CACHED_ADDR + 0x01000000);
	L2fill(CACHED_ADDR + 0x02000000);

	printf("word at addr = 0x%x\n", *cached_addr);
	printf("word at mirror addr = 0x%x\n", *mirror_addr);
	restore_mmu_map();

	return 0;
}

static int do_valid_test(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int i, el;
	int numOptions = ARRAY_SIZE(cache_options);
	volatile u32 *mirror_addr = (u32 *)MIRROR_ADDR;
	volatile u32 *cached_addr = (u32 *)CACHED_ADDR;
	u64 attr = 0, memattrs;
	bool cache_off = false;

	if ((argc == 2) && (strcmp(argv[1], "cache_off") == 0))
		cache_off = true;

	change_mmu_map();
	invalidate_dcache_all();

	for (i = 0; i < numOptions; i++) {
		printf("\nChange to Cache option %s\n", cache_options[i].desc);

		*mirror_addr = 0;
		memattrs = (MEMORY_ATTRIBUTES & ~((u64)0xFF << (MT_NORMAL*8))) |
			(cache_options[i].attr << (MT_NORMAL*8));

		change_mem_attr(memattrs);
		if (cache_off)
			disable_dcache();

		el = current_el();

		if (el == 1) {
			asm volatile("mrs %0, mair_el1" : "=r" (attr) : : "cc");
		} else if (el == 2) {
			asm volatile("mrs %0, mair_el2" : "=r" (attr) : : "cc");
		} else if (el == 3) {
			asm volatile("mrs %0, mair_el3" : "=r" (attr) : : "cc");
		}
		printf("mair = 0x%llx\n", attr);

		printf("word at addr = 0x%x\n", *cached_addr);
		dmb();
		printf("word at mirror addr = 0x%x\n", *mirror_addr);
		dmb();

		printf("write pattern 0x%x to mirror addr\n", 0x1110 + i);
		*mirror_addr = (0x1110+i);
		dmb();

		printf("word at addr = 0x%x\n", *cached_addr);
		dmb();
		printf("word at mirror addr = 0x%x\n", *mirror_addr);
		dmb();

		printf("invalidate dcache\n");
		invalidate_dcache_all();
		printf("word at addr = 0x%x\n", *cached_addr);
		dmb();
		printf("word at mirror addr = 0x%x\n", *mirror_addr);
		dmb();
	}
	restore_mmu_map();

	return 0;
}

static void test_read_performance(void)
{
	u32 i,j,chunk_size = 4096;
	u32 loops = 4096;
	u32 src_addr = memtest_src_addr;
	u64 start, end, ticks;

	for (j=0; j<10; j++)
	{
		chunk_size <<= 1;
		loops >>=1;
		/* Setup the buffer */
		/* fill the memory and cache in case of allocating on write */
		my_memwrite(src_addr, chunk_size);

		/* read the memory once before measuring time in order to fill
		 * the cacche in case of allocate on read
		 */
		my_memread(src_addr, chunk_size);

		start = timer_get_us();

		for (i = 0; i < loops; i++)
			my_memread(src_addr, chunk_size);

		end = timer_get_us();
		ticks = end - start;
		printf("Duration = %7lld usec loops= %4d ",ticks, loops );
		printf("chunk_size = %4d KB: Performance %7lld KBps \n", chunk_size/1024, chunk_size*loops*100/ticks*10);
		src_addr = memtest_src_addr + chunk_size; // this is done in order not to use a used location
	}
}

static void test_write_performance(void)
{
	u32 i,j,chunk_size = 4096;
	u32 loops = 1024;
	u32 src_addr = memtest_src_addr;
	u64 start, end, ticks;

	for (j=0; j<10; j++)
	{
		chunk_size <<= 1;
		loops >>=1;
		/* read the memory once before measuring time in order to fill
		 * the cacche in case of allocate on read
		 */
		my_memread(src_addr, chunk_size);
		/* Setup the buffer */
		/* fill the memory and cache in case of allocating on write */
		my_memwrite(src_addr, chunk_size);


		start = timer_get_us();

		for (i = 0; i < loops; i++)
			my_memwrite(src_addr, chunk_size);

		end = timer_get_us();
		ticks = end - start;
		printf("Duration = %7lld usec loops= %4d ", ticks, loops );
		printf("chunk_size = %4d KB: Performance %7lld KBps \n",
				chunk_size/1024, chunk_size*loops*100/ticks*10);
		src_addr = memtest_src_addr + chunk_size; // this is done in order not to use a used location
	}
}
int do_cache_perf_test_all(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int i, el;
	int numOptions = ARRAY_SIZE(cache_options);
	u64 attr = 0, memattrs;
	bool write_test = false;
	bool transient_hint = false;
	u64 val64 = 0;
	uint8_t sanity = 0;

	if (argc < 2)
		return 0;

	change_mmu_map();
	if ((argc >=2) && strcmp(argv[1], "write") == 0)
		write_test = true;
	if ((argc == 3) && strcmp(argv[2], "transient") == 0)
		transient_hint = true;
	if ((argc == 4) && strcmp(argv[3], "sanity") == 0)
		sanity = 1;

	printf("cache %s perf test\n", write_test ? "write" : "read");
	if (transient_hint) {
		/* clear CPUACTLR_EL1.DTAH */
		printf("clear CPUACTLR_EL1.DTAH\n");
		asm volatile("mrs %0, S3_1_c15_c2_0" : "=r" (val64) : : "cc");
		val64 &= ~(1 << 24);
		asm volatile("msr S3_1_c15_c2_0, %0" : : "r" (val64) : "cc");
		asm volatile("mrs %0, S3_1_c15_c2_0" : "=r" (val64) : : "cc");
		printf("cpuactlr_el1 = 0x%llx\n", val64);
	}

	for (i = 7; i < 8; i++) {
	//for (i = 0; i < numOptions; i++) {

		if (sanity) i = 7; // just do 1 test

		printf("\nChange to Cache option %s\n", cache_options[i].desc);

		memattrs = (MEMORY_ATTRIBUTES & ~((u64)0xFF << (MT_NORMAL*8))) |
			(cache_options[i].attr << (MT_NORMAL*8));
		change_mem_attr(memattrs);

		el = current_el();

		if (el == 1) {
			asm volatile("mrs %0, mair_el1" : "=r" (attr) : : "cc");
		} else if (el == 2) {
			asm volatile("mrs %0, mair_el2" : "=r" (attr) : : "cc");
		} else if (el == 3) {
			asm volatile("mrs %0, mair_el3" : "=r" (attr) : : "cc");
		}
		printf("mair = 0x%llx\n", attr);

		if (write_test)
			test_write_performance();
		else
			test_read_performance();

		if (sanity) break;

	}
	if (transient_hint) {
		/* set CPUACTLR_EL1.DTAH */
		printf("set CPUACTLR_EL1.DTAH\n");
		asm volatile("mrs %0, S3_1_c15_c2_0" : "=r" (val64) : : "cc");
		val64 |= (1 << 24);
		asm volatile("msr S3_1_c15_c2_0, %0" : : "r" (val64) : "cc");
		asm volatile("mrs %0, S3_1_c15_c2_0" : "=r" (val64) : : "cc");
		printf("cpuactlr_el1 = 0x%llx\n", val64);
	}
	restore_mmu_map();

	return 0;
}

static int do_cache_invalidate(
		struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	invalidate_dcache_all();
	invalidate_icache_all();

	return 0;
}

U_BOOT_CMD(
	icache_test,   1,   1,     do_icache_test,
	"ICache test",
	"\n"
);
U_BOOT_CMD(
	cache_eviction_test,   1,   1,     do_eviction_test,
	"Cache eviction test",
	"\n"
);
U_BOOT_CMD(
	cache_valid_test,   2,   1,     do_valid_test,
	"Cache valid test",
	"\n"
);
U_BOOT_CMD(
	cache_perf_test_all,   4,   1,     do_cache_perf_test_all,
	"Cache performance test according to L1 and L2 cache sizes",
	"\n"
	" If data and instruction cache are enabled:\n"
	"   performance should be fastest when      chunk_size <= L1\n"
	"   performance should be fast    when L1 < chunk_size <= L2\n"
	"   performance should be slow    when L2 < chunk_size\n"
	" If data cache is disabled:\n"
	"   performance should be slowest on all chunk_size cases"
);
U_BOOT_CMD(
	sysinfo,   1,   1,     do_sysinfo,
	"Dump System info",
	"\n"
);

U_BOOT_CMD(
	cpuinfo,   1,   1,     do_cpuinfo,
	"Dump CPU info",
	"\n"
);

U_BOOT_CMD(
	cache_invalidate,   1,   1,     do_cache_invalidate,
	"Invalidate all cache",
	"\n"
);
