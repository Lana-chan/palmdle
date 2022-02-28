TOOLCHAIN		?=	buildtools/toolchain/bin/
SDK					?=	buildtools/palm-os-sdk-master/sdk-5r3/include
PILRC				=	buildtools/pilrc3_2/bin/pilrc
CC					=	$(TOOLCHAIN)/m68k-none-elf-gcc
LD					=	$(TOOLCHAIN)/m68k-none-elf-gcc
OBJCOPY			=	$(TOOLCHAIN)/m68k-none-elf-objcopy
COMMON			=	-Wno-multichar -funsafe-math-optimizations -Os -m68000 -mno-align-int -mpcrel -fpic -fshort-enums -mshort
WARN				=	-Wsign-compare -Wextra -Wall -Werror -Wno-unused-parameter -Wno-old-style-declaration -Wno-unused-function -Wno-unused-variable -Wno-error=cpp -Wno-error=switch
LKR					=	linker.lkr
CCFLAGS			=	$(LTO) $(WARN) $(COMMON) -I. -ffunction-sections -fdata-sections
LDFLAGS			=	$(LTO) $(WARN) $(COMMON) -Wl,--gc-sections -Wl,-T $(LKR)
SRCS				=	Src/palmdle.c
RCP					=	Src/palmdle.rcp
RSC					=	Src/
OBJS				=	$(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SRCS)))
TYPE				=	appl
TARGET			= Palmdle

#add PalmOS SDK
INCS			+=	-isystem$(SDK)
INCS			+=	-isystem$(SDK)/Core
INCS			+=	-isystem$(SDK)/Core/Hardware
INCS			+=	-isystem$(SDK)/Core/System
INCS			+=	-isystem$(SDK)/Core/UI
INCS			+=	-isystem$(SDK)/Dynamic
INCS			+=	-isystem$(SDK)/Libraries
INCS			+=	-isystem$(SDK)/Libraries/PalmOSGlue



$(TARGET).prc: code0001.bin
	$(PILRC) -ro -o $(TARGET).prc -type $(TYPE) -name $(TARGET) -I $(RSC) $(RCP) && rm code0001.bin

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@ -j.vec -j.text -j.rodata

%.elf: $(OBJS)
	$(LD) -o $@ $(LDFLAGS) $^

%.o : %.c Makefile
	$(CC) $(CCFLAGS)  $(INCS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(NAME).elf
 
.PHONY: clean