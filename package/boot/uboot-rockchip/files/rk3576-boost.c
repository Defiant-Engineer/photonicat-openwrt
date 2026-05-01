/* RK3576 SD-card BootROM workaround */
static inline void writel(unsigned int val, unsigned long addr)
{
    *((volatile unsigned int *)addr) = val;
}

void boost(void)
{
    writel(0x3ffff800, 0x3ff803b0);
}
