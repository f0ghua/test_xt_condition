# NFTables Condition Module Makefile

# Kernel module
KDIR ?= /lib/modules/$(shell uname -r)/build
EXTRA_CFLAGS += -I$(src)/include

# User space compilation
CC ?= gcc
CFLAGS += -Wall -Werror -fPIC
CPPFLAGS += -I/usr/include/nftables -I$(src)/include
LDFLAGS += -lnftables -shared

# Module source files
obj-m += nft_condition.o
nft_condition-objs := src/nft_expr_condition.o src/condition_state.o

# User space library
USERLIB := libnft_condition.so
USERLIB_OBJS := src/expr_condition.o

# Test files
kernel_test_obj-m += test/kernel/test_condition.o
userspace_test := test/userspace/test_condition
integration_test := test/integration/test_condition_integration.sh

.PHONY: all clean install modules modules_install tests

all: modules $(USERLIB)

# Kernel module targets
modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

# User space targets
$(USERLIB_OBJS): %.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(USERLIB): $(USERLIB_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

# Test targets
tests: kernel_tests userspace_tests

kernel_tests:
	$(MAKE) -C $(KDIR) M=$(PWD)/test/kernel modules

userspace_tests: $(userspace_test)

$(userspace_test): test/userspace/test_condition.c $(USERLIB)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(LDFLAGS)

# Install targets
install: modules_install
	# Install shared library
	install -d $(DESTDIR)/usr/lib/nftables/
	install -m 755 $(USERLIB) $(DESTDIR)/usr/lib/nftables/
	# Create symlink for library
	ln -sf /usr/lib/nftables/$(USERLIB) $(DESTDIR)/usr/lib/$(USERLIB)
	# Update shared library cache
	ldconfig
	# Install integration test
	install -m 755 $(integration_test) $(DESTDIR)/usr/local/bin/

# Clean targets
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(MAKE) -C $(KDIR) M=$(PWD)/test/kernel clean
	rm -f $(USERLIB_OBJS) $(USERLIB)
	rm -f $(userspace_test)
	find . -name '*.o' -o -name '*.ko' -o -name '*.mod.c' -o -name '.*.cmd' \
		-o -name '*.order' -o -name '*.symvers' | xargs rm -f

# Help target
help:
	@echo "NFTables Condition Module Build Targets:"
	@echo "  all            - Build kernel module and user space components"
	@echo "  modules        - Build kernel module only"
	@echo "  modules_install- Install kernel module"
	@echo "  tests          - Build and run all tests"
	@echo "  install        - Install module and user space components"
	@echo "  clean          - Remove all built files"