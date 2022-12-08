CHERIBASE        ?= $(HOME)/cheri
SDKBASE          ?= $(CHERIBASE)/output/morello-sdk
CFLAGS           := --config cheribsd-morello-hybrid.cfg
CC               := $(SDKBASE)/bin/clang
BINDIR           := bin

RUNUSER          ?= root
RUNHOST          ?= localhost
RUNDIR           ?= /root

LIBNAME          := libcomp.so
CFILES           := $(wildcard src/*.c)
HFILES           := $(wildcard src/*.h)
SRC              := $(wildcard src/*.S src/*.s) $(CFILES)
TEST_DIR         := tests
TESTS            := $(notdir $(wildcard $(TEST_DIR)/*))
TEST_TARGETS     := $(foreach test, $(TESTS), $(BINDIR)/tests/$(test))
TEST_RUN_TARGETS := $(foreach test, $(TESTS), run-$(test))
SSH_OPTIONS      := "-o StrictHostKeyChecking no"
SCP_OPTIONS      := "-o StrictHostKeyChecking no"

CFLAGS           += -Wall -Wextra -Werror -pedantic --std=c17 -fPIC -lelf -g

.PHONY: fmt test $(TEST_RUN_TARGETS)

run-%: $(BINDIR)/tests/% $(BINDIR)/$(LIBNAME)
ifdef SSHPORT
	scp $(SCP_OPTIONS) -P $(SSHPORT) -r $$(pwd) $(RUNUSER)@$(RUNHOST):$(RUNDIR)
	ssh $(SSH_OPTIONS) -p $(SSHPORT) $(RUNUSER)@$(RUNHOST) -t 'cd $(RUNDIR)/cheri-compartments-playground && LD_LIBRARY_PATH=./bin/ ./bin/tests/$(<F)'
else
	@echo "'$@' requires SSHPORT to be defined."
	@false
endif

test: $(TEST_TARGETS)
	scp $(SCP_OPTIONS) -P $(SSHPORT) -r $$(pwd) $(RUNUSER)@$(RUNHOST):$(RUNDIR)
	SSH_OPTIONS=$(SSH_OPTIONS) RUNUSER=$(RUNUSER) RUNHOST=$(RUNHOST) RUNDIR=$(RUNDIR) SSHPORT=$(SSHPORT) ./test.bats

# clang-format refuses to format the code properly (it won't respect the
# `AlwaysBreakAfterReturnType: AllDefinitions` setting)
fmt:
	astyle \
		--break-return-type \
		--align-pointer=name \
		--style=attach \
		--indent-switches \
		--max-code-length=100 \
		--recursive './*.c,*.h' \

$(BINDIR)/$(LIBNAME): $(SRC)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -Wl,-Bsymbolic,-shared -L $(BINDIR) $+ -o $@

.SECONDEXPANSION:
$(BINDIR)/tests/%: $$(wildcard $(TEST_DIR)/%/glue.S) $$(wildcard $(TEST_DIR)/%/*.c) $(BINDIR)/$(LIBNAME)
	@mkdir -p $(BINDIR)/tests
	$(CC) $(CFLAGS) -I./src/ -lcomp -L $(BINDIR) $+ -o $@
