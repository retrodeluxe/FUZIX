
CSRCS = devtty.c
CSRCS += devices.c main.c

ASRCS = rc2014-6303.S crt0.S
ASRCS += tricks.S commonmem.S

DSRCS = ../dev/devide.c ../dev/blkdev.c
DISCARD_DSRCS = ../dev/mbr.c ../dev/devide_discard.c

COBJS = $(CSRCS:.c=$(BINEXT))
AOBJS = $(ASRCS:.S=$(BINEXT))
DOBJS = $(patsubst ../dev/%.c,%.o, $(DSRCS))
DISCARD_DOBJS = $(patsubst ../dev/%.c,%.o, $(DISCARD_DSRCS))

OBJS  = $(COBJS) $(AOBJS) $(DOBJS) $(DISCARD_DOBJS)

CROSS_CCOPTS += -I../dev/

all:	$(OBJS)

$(COBJS): %$(BINEXT): %.c
	$(CROSS_CC) $(CROSS_CCOPTS) $(CROSS_CC_SEG1) $<

$(AOBJS): %$(BINEXT): %.S
	cpp -E $<  >$*.s
	$(CROSS_AS) $(ASOPTS) $*.s

$(DOBJS): %$(BINEXT): ../dev/%.c
	$(CROSS_CC) $(CROSS_CCOPTS) $(CROSS_CC_SEG1) -c $<
	mv ../dev/$*.o .

$(DISCARD_DOBJS): %$(BINEXT): ../dev/%.c
	$(CROSS_CC) $(CROSS_CCOPTS) $(CROSS_CC_SEGDISC) -c $<
	mv ../dev/$*.o .

clean:
	rm -f $(OBJS) *.o core *~ fuzix.bin fuzix.tmpmap

image:
	$(CROSS_LD) -b -C 0x400 -S 0xF400 -Z 0x20 -o ../fuzix.bin -m fuzix.tmpmap crt0.o commonmem.o \
	rc2014-6303.o ../start.o ../version.o ../lowlevel-6303.o \
	tricks.o main.o ../timer.o ../kdata.o devices.o \
	blkdev.o mbr.o devide.o devide_discard.o \
	../devio.o ../filesys.o ../process.o ../inode.o ../syscall_fs.o \
	../syscall_proc.o ../syscall_other.o ../mm.o ../bankfixed.o \
	../tty.o ../devsys.o ../syscall_fs2.o ../syscall_fs3.o ../syscall_exec16.o \
	../usermem.o ../usermem_std-6303.o devtty.o /opt/cc68/lib/libc.a
	perl -lpe '$$_=hex' fuzix.tmpmap | paste -d" " - fuzix.tmpmap  |  sort -n |  cut -d" " -f 2- >../fuzix.map
	dd if=../fuzix.bin of=fuzix.img bs=256 skip=4