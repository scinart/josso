// Simple implementation of cprintf console output for the kernel,
// based on printfmt() and the kernel console's cputchar().

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>

typedef unsigned uint;

static uint default_cnt_color=0x0600;
static uint current_cnt_color=0x0700;

static void
putch(int ch, int *cnt)
{
	cputchar(ch);
	*cnt++;
}

static void set_cnt_back_color(int color)
{
	current_cnt_color &= 0x0FFF;
	current_cnt_color |= ((color<<12) & 0xF000);
}
static void set_cnt_front_color(int color)
{
	current_cnt_color &= 0xF0FF;
	current_cnt_color |= ((color<<8) & 0x0F00);
}
static int get_cnt_back_color()
{
    return current_cnt_color & 0xF000;
}
static int get_cnt_front_color()
{
	return current_cnt_color & 0x0F00;
}
static void set_cnt_color(int ready_color)
{
	current_cnt_color &= 0x00FF;
	current_cnt_color |= ready_color;
}
static int get_cnt_color()
{
	return current_cnt_color;
}

void console_color_init()
{
	register_set_color_function(set_cnt_front_color,
								set_cnt_back_color,
								get_cnt_front_color,
								get_cnt_back_color,
								set_cnt_color,
								get_cnt_color);
}

static void
my_putch(int ch, int *cnt)
{
	putch(ch|current_cnt_color, cnt);
}



int
vcprintf(const char *fmt, va_list ap)
{
	int cnt = 0;

	vprintfmt((void*)my_putch, &cnt, fmt, ap);
	return cnt;
}

int
cprintf(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	return cnt;
}

