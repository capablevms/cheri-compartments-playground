CHERIBASE   ?= $(HOME)/cheri
SDKBASE     ?= $(CHERIBASE)/output/morello-sdk
CFLAGS      := --config cheribsd-morello-hybrid.cfg
PLATFORM    := morello-hybrid
CC          := $(SDKBASE)/bin/clang
CFORMAT     := $(SDKBASE)/bin/clang-format
BINDIR      := bin/$(PLATFORM)

RUNUSER     ?= root
RUNHOST     ?= localhost
RUNDIR      ?=

ifdef RUNDIR
RUNDIR      := $(RUNDIR)/cheri-comp
else
RUNDIR      := cheri-comp
endif

LIBNAME     := libcomp.so
SRC         := $(wildcard src/*.S) $(wildcard src/*.s) $(wildcard src/*.c)
EXAMPLE_SRC := $(wildcard example/*.S) $(wildcard example/*.s) $(wildcard example/*.c)
CFLAGS      += -Wall -Wextra -Werror -pedantic --std=c17 -fPIC -lelf -g

.PRECIOUS: $(BINDIR)/%

.PHONY: run fmt

run: $(BINDIR)/example $(BINDIR)/$(LIBNAME)
ifdef SSHPORT
	ssh $(SSH_OPTIONS) -p $(SSHPORT) $(RUNUSER)@$(RUNHOST) 'mkdir -p $(RUNDIR)'
	scp $(SCP_OPTIONS) -P $(SSHPORT) $^ $(RUNUSER)@$(RUNHOST):$(RUNDIR)
	scp $(SCP_OPTIONS) -P $(SSHPORT) -r $$(pwd) $(RUNUSER)@$(RUNHOST):$(RUNDIR)
	ssh $(SSH_OPTIONS) -p $(SSHPORT) $(RUNUSER)@$(RUNHOST) -t 'cd $(RUNDIR) && LD_LIBRARY_PATH=. ./$(<F)'
else
	@echo "'$@' requires SSHPORT to be defined."
	@false
endif

$(BINDIR)/example: $(EXAMPLE_SRC) $(BINDIR)/$(LIBNAME)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -I./src/ -lcomp -L $(BINDIR) $(EXAMPLE_SRC) -o $@

$(BINDIR)/$(LIBNAME): $(SRC)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -Wl,-Bsymbolic,-shared -L $(BINDIR) $+ -o $@

fmt:
	astyle \
		--break-return-type \
		--align-pointer=name \
		--style=attach \
		--indent-switches \
		--max-code-length=100 \
		--recursive './*.c,*.h' \
