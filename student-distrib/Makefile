# Makefile for OS project
# To build, first `make dep`, them `make`. Everything should be automatic.
# Will compile all *.c and *.S files in the current directory.


# Flags to use when compiling, preprocessing, assembling, and linking
CFLAGS 	+= -Wall -fno-builtin -fno-stack-protector -nostdlib
ASFLAGS +=
LDFLAGS += -nostdlib -static
UNAME := $(shell uname)
ARCH := $(shell arch)

ifeq ($(UNAME), Linux)
    ifeq ($(ARCH), x86_64)
        CC=/usr/local/cross/bin/i386-elf-gcc
        RUN_SCRIPT = ./linux_run.sh
        CFLAGS += -Wno-packed-bitfield-compat
        BUILD_SCRIPT = ./mac_debug.sh
    else
        CC=gcc
        RUN_SCRIPT = ./ews_run.sh
        BUILD_SCRIPT = ./debug.sh
    endif
endif
ifeq ($(UNAME), Darwin)
    CC=i386-elf-gcc-4.3.2
    BUILD_SCRIPT = ./mac_debug.sh
    RUN_SCRIPT = ./mac_run.sh
endif

#If you have any .h files in another directory, add -I<dir> to this line
CPPFLAGS +=-nostdinc -g

# This generates the list of source files
SRC =  $(wildcard *.S) $(wildcard *.c)

# This generates the list of .o files. The order matters, boot.o must be first
OBJS  = boot.o
OBJS += $(filter-out boot.o,$(patsubst %.S,%.o,$(filter %.S,$(SRC))))
OBJS += $(patsubst %.c,%.o,$(filter %.c,$(SRC))) 


bootimg: Makefile $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -Ttext=0x400000 -o bootimg
	$(BUILD_SCRIPT)
	
dep: Makefile.dep

doc:
	doxygen Doxyfile

Makefile.dep: $(SRC)
	$(CC) -MM $(CPPFLAGS) $(SRC) > $@

.PHONY: run
run:
	$(RUN_SCRIPT)

.PHONY: clean
clean: 
	rm -f *.o Makefile.dep bootimg

ifneq ($(MAKECMDGOALS),dep)
    ifneq ($(MAKECMDGOALS),clean)
        include Makefile.dep
    endif
endif
