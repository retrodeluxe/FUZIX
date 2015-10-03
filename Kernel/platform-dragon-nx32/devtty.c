#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <stdbool.h>
#include <devtty.h>
#include <device.h>
#include <devfd.h>
#include <vt.h>
#include <tty.h>
#include <graphics.h>

#undef  DEBUG			/* UNdefine to delete debug code sequences */

uint8_t *uart_data = (uint8_t *)0xFF04;	/* ACIA data */
uint8_t *uart_status = (uint8_t *)0xFF05;	/* ACIA status */
uint8_t *uart_command = (uint8_t *)0xFF06;	/* ACIA command */
uint8_t *uart_control = (uint8_t *)0xFF07;	/* ACIA control */

unsigned char tbuf1[TTYSIZ];
unsigned char tbuf2[TTYSIZ];

struct s_queue ttyinq[NUM_DEV_TTY + 1] = {	/* ttyinq[0] is never used */
	{NULL, NULL, NULL, 0, 0, 0},
	{tbuf1, tbuf1, tbuf1, TTYSIZ, 0, TTYSIZ / 2},
	{tbuf2, tbuf2, tbuf2, TTYSIZ, 0, TTYSIZ / 2}
};

uint8_t vtattr_cap = VTA_INVERSE|VTA_UNDERLINE|VTA_ITALIC|VTA_BOLD|
		     VTA_OVERSTRIKE|VTA_NOCURSOR;

/* tty1 is the screen tty2 is the serial port */

/* Output for the system console (kprintf etc) */
void kputchar(char c)
{
	if (c == '\n')
		tty_putc(1, '\r');
	tty_putc(1, c);
}

ttyready_t tty_writeready(uint8_t minor)
{
	uint8_t c;
	if (minor == 1)
		return TTY_READY_NOW;
	c = *uart_status;
	return (c & 16) ? TTY_READY_NOW : TTY_READY_SOON; /* TX DATA empty */
}

/* For DragonPlus we should perhaps support both monitors 8) */

void tty_putc(uint8_t minor, unsigned char c)
{
	irqflags_t irq;
	if (minor == 1) {
		/* We need a better way generally to handle keyboard v
		   VT */
		irq = di();
		vtoutput(&c, 1);
		irqrestore(irq);
	} else
		*uart_data = c;	/* Data */
}

void tty_sleeping(uint8_t minor)
{
    used(minor);
}

void tty_setup(uint8_t minor)
{
	if (minor == 2) {
		/* FIXME: do proper mode setting */
		*uart_command = 0x01;	/* DTR high, IRQ enabled, TX irq disabled 8N1 */
		*uart_control = 0x1E;	/* 9600 baud */
	}
}

int tty_carrier(uint8_t minor)
{
	/* The serial DCD is status bit 5 but not wired */
	return 1;
}

void tty_interrupt(void)
{
	uint8_t r = *uart_status;
	if (r & 0x8) {
		r = *uart_data;
		tty_inproc(2,r);
	}	
}

uint8_t keymap[8];
static uint8_t keyin[8];
static uint8_t keybyte, keybit;
static uint8_t newkey;
static int keysdown = 0;
/* FIXME: shouldn't COCO shiftmask also differ ??? 0x02 not 0x40 ?? */
static uint8_t shiftmask[8] = {
	0, 0x40, 0, 0, 0, 0, 0, 0x40
};

static uint8_t rbit[8] = {
	0xFE,
	0xFD,
	0xFB,
	0xF7,
	0xEF,
	0xDF,
	0xBF,
	0x7F,
};

/* Row inputs: multiplexed with the joystick */
static volatile uint8_t *pia_row = (uint8_t *)0xFF00;
/* Columns for scanning: multiplexed with the printer port */
static volatile uint8_t *pia_col = (uint8_t *)0xFF02;
/* Control */
static volatile uint8_t *pia_ctrl = (uint8_t *)0xFF03;

static void keyproc(void)
{
	int i;
	uint8_t key;

	for (i = 0; i < 8; i++) {
		/* We do the scan in software on the Dragon */
		*pia_col = rbit[i];
		keyin[i] = ~*pia_row;
		key = keyin[i] ^ keymap[i];
		if (key) {
			int n;
			int m = 1;
			for (n = 0; n < 7; n++) {
				if ((key & m) && (keymap[i] & m)) {
					if (!(shiftmask[i] & m))
						keysdown--;
				}
				if ((key & m) && !(keymap[i] & m)) {
					if (!(shiftmask[i] & m)) {
						keysdown++;
						newkey = 1;
						keybyte = i;
						keybit = n;
					}
				}
				m += m;
			}
		}
		keymap[i] = keyin[i];
	}
	if (system_id) { 	/* COCO series */
	  keybit += 2;
	  if (keybit > 5)
	    keybit -= 6;
        }
}

#ifdef CONFIG_COCO_KBD
uint8_t keyboard[8][7] = {
	{ '@', 'h', 'p', 'x', '0', '8', KEY_ENTER },
	{ 'a', 'i', 'q', 'y', '1', '9', 0 /* clear - used as ctrl*/ },
	{ 'b', 'j', 'r', 'z', '2', ':', KEY_ESC /* break (used for esc) */ },
	{ 'c', 'k', 's', '^' /* up */, '3', ';' , 0 /* NC */ },
	{ 'd', 'l', 't', '|' /* down */, '4', ',', 0 /* NC */ },
	{ 'e', 'm', 'u', KEY_BS /* left */, '5', '-', 0 /* NC */ },
	{ 'f', 'n', 'v', KEY_TAB /* right */, '6', '.', 0 /* NC */ },
	{ 'g', 'o', 'w', ' ', '7', '/', 0 /* shift */ },
};

uint8_t shiftkeyboard[8][7] = {
	{ '\\', 'H', 'P', 'X', '_', '(', KEY_ENTER },
	{ 'A', 'I', 'Q', 'Y', '!', ')', 0 /* clear - used as ctrl */ },
	{ 'B', 'J', 'R', 'Z', '"', '*', CTRL('C') /* break */ },
	{ 'C', 'K', 'S', '[' /* up */, '#', '+', 0 /* NC */ },
	{ 'D', 'L', 'T', ']' /* down */, '$', '<', 0 /* NC */ },
	{ 'E', 'M', 'U', '{' /* left */, '%', '=', 0 /* NC */ },
	{ 'F', 'N', 'V', '}' /* right */, '&', '>', 0 /* NC */ },
	{ 'G', 'O', 'W', ' ', '\'', '?', 0 /* shift */ },
};
#else
uint8_t keyboard[8][7] = {
	{ '0', '8', '@', 'h', 'p', 'x', KEY_ENTER },
	{ '1', '9', 'a', 'i', 'q', 'y', 0 /* clear - used as ctrl*/ },
	{ '2', ':', 'b', 'j', 'r', 'z', KEY_ESC /* break (used for esc) */ },
	{ '3', ';', 'c', 'k', 's', '^' /* up */, 0 /* NC */ },
	{ '4', ',', 'd', 'l', 't', '|' /* down */, 0 /* NC */ },
	{ '5', '-', 'e', 'm', 'u', KEY_BS /* left */, 0 /* NC */ },
	{ '6', '.', 'f', 'n', 'v', KEY_TAB /* right */, 0 /* NC */ },
	{ '7', '/', 'g', 'o', 'w', ' ', 0 /* shift */ },
};

uint8_t shiftkeyboard[8][7] = {
	{ '_', '(', '\\', 'H', 'P', 'X', KEY_ENTER },
	{ '!', ')', 'A', 'I', 'Q', 'Y', 0 /* clear - used as ctrl*/ },
	{ '"', '*', 'B', 'J', 'R', 'Z', CTRL('C') /* break */ },
	{ '#', '+', 'C', 'K', 'S', '[' /* up */, 0 /* NC */ },
	{ '$', '<', 'D', 'L', 'T', ']' /* down */, 0 /* NC */ },
	{ '%', '=', 'E', 'M', 'U', '{' /* left */, 0 /* NC */ },
	{ '&', '>', 'F', 'N', 'V', '}' /* right */, 0 /* NC */ },
	{ '\'', '?', 'G', 'O', 'W', ' ', 0 /* shift */ },
};
#endif /* COCO_KBD */

static void keydecode(void)
{
	uint8_t c;

	if (keymap[7] & 64)	/* shift */
		c = shiftkeyboard[keybyte][keybit];
	else
		c = keyboard[keybyte][keybit];
	if (keymap[1] & 64) {	/* control */
		if (c > 31 && c < 127)
			c &= 31;
	}
	tty_inproc(1, c);
}

void platform_interrupt(void)
{
	uint8_t i = *pia_ctrl;
	if (i & 0x80) {
		*pia_col;
		newkey = 0;
		keyproc();
		if (keysdown < 3 && newkey)
			keydecode();
                fd_timer_tick();
		timer_interrupt();
	}
}

/* This is used by the vt asm code, but needs to live at the top of the kernel */
uint16_t cursorpos;

static struct display display[2] = {
	{
		0,
		256, 192,
		256, 192,
		0xFF, 0xFF,		/* For now */
		FMT_MONO_WB,
		HW_UNACCEL,
		0,
		0,
		GFX_MAPPABLE,
	},
	{
		1,
		256, 192,
		256, 192,
		0xFF, 0xFF,		/* For now */
		FMT_MONO_BW,
		HW_UNACCEL,
		0,
		0,
		GFX_MAPPABLE,
	},
};

static struct videomap displaymap = {
	0,
	0,
	VIDEO_BASE,
	6 * 1024,
	0,
	0,
	0,
	MAP_FBMEM|MAP_FBMEM_SIMPLE
};

static uint8_t vmode;

#define pia1b	((volatile uint8_t *)0xFF22)
/*
 *	Start by just reporting the 256x192 mode which is memory mapped
 *	(it's effectively always in our address space). Should really
 *	support setting graphics into the other modes.
 */
int gfx_ioctl(uint8_t minor, uarg_t arg, char *ptr)
{
	if (arg >> 8 != 0x03)
		return vt_ioctl(minor, arg, ptr);
	if (arg == GFXIOC_GETINFO)
		return uput(&display[vmode], ptr, sizeof(struct display));
	if (arg == GFXIOC_MAP)
		return uput(&displaymap, ptr, sizeof(displaymap));
	if (arg == GFXIOC_GETMODE || arg == GFXIOC_SETMODE) {
		uint8_t m = ugetc(ptr);
		if (m > 1) {
			udata.u_error = EINVAL;
			return -1;
		}
		if (arg == GFXIOC_GETMODE)
			return uput(&display[m], ptr, sizeof(display));
		vmode = m;
		/* As we get more modes this will need to be done nicely */
		*pia1b = (*pia1b & 0xF7) | (m << 3);
		return 0;
	}
	udata.u_error = ENOTTY;
	return -1;
}
