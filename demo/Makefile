srcdir	= ..
include ../Makefile.vars

CFLAGS += -I.. -static
LDFLAGS += -L.. -lcxl

CAPI_HOST:=root@9.3.182.48
SSH_KEY :=dummy

# Add tests here
tests = hello_capi.c

# Add any .o files tests may depend on
test_deps = memcpy_afu.o

all: $(tests:.c=)
tests: all

libcxl_objs = libcxl.a
libcxl_deps = $(foreach dep, $(libcxl_objs), $(srcdir)/$(dep))

-include $(tests:.c=.d)
-include $(test_deps:.o=.d)
include ../Makefile.rules

$(libcxl_deps):
	$(MAKE) -C$(srcdir) $(@F)

$(test_deps):

%.o : %.S
	$(call Q,CC, $(CC) -MM $(CFLAGS) $< > $*.d, $*.d)
	$(call Q,SED, sed -i -e "s/$@.o/$@/" $*.d, $*.d)
	$(call Q,CC, $(CC) $(CFLAGS) -c $<, $<)

% : %.c $(libcxl_deps) $(test_deps)
	$(call Q,CC, $(CC) -MM $(CFLAGS) $< > $*.d, $*.d)
	$(call Q,SED, sed -i -e "s/$@.o/$@/" $*.d, $*.d)
	$(call Q,CC, $(CC) $(CFLAGS) $(filter %.c %.a %.o,$^) -o $@, $@)

clean:
	/bin/rm -f $(tests:.c=) $(patsubst %.c,%.d,$(tests)) $(test_deps) $(patsubst %.o,%.d,$(test_deps))

run:
	sshpass -p '${SSH_KEY}' scp -C $(patsubst %.c,%,$(tests))  "${CAPI_HOST}:~"
	sshpass -p '${SSH_KEY}' ssh '${CAPI_HOST}' '~/$(patsubst %.c,%,$(tests))'

.PHONY: clean all tests run
