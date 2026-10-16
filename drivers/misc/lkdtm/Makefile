# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_LKDTM) += lkdtm.o
lkdtm-objs += core.o bugs.o heap.o perms.o refcount.o rodata_objcopy.o usercopy.o stackleak.o cfi.o fortify.o
lkdtm-$(CONFIG_PPC_64S_HASH_MMU) += powerpc.o

OBJCOPYFLAGS :=
OBJCOPYFLAGS_rodata_objcopy.o	:= \
			--rename-section .noinstr.text=.rodata,alloc,readonly,load,contents
targets += rodata.o rodata_objcopy.o
$(obj)/rodata_objcopy.o: $(obj)/rodata.o FORCE
	$(call if_changed,objcopy)
