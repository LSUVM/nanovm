#
#  Makefile for unix version of the NanoVM
#

PROJ = NanoVM
VERSION = 1.0
CONFIG = UnixTest.config

DEFAULT_FILE = FormatterTest
#DEFAULT_FILE = FloatTest2
#DEFAULT_FILE = Int31Test
#DEFAULT_FILE = Switch
#DEFAULT_FILE = MathTest
#DEFAULT_FILE = Inheritance

CFLAGS += -DDEBUG

ROOT_DIR = ../../..

# build intel hex files
all: $(PROJ)

include ../../src/Makefile
include ../../src/unix/Makefile

CFLAGS += -g -Os -DUNIX -I. -DVERSION="\"$(VERSION)\""
CLDFLAGS += -g -lm

nvmdefault.h: Makefile

# avr specific entries
$(PROJ): $(OBJS)
	$(CC) $(CLDFLAGS) -o $@ $(OBJS)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

%.d:%.c
	set -e; $(CC) -MM $(CFLAGS) $< \
	| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@ ; \
	[ -s $@ ] || rm -f $@

# just run target from java directory
%-run: $(ROOT_DIR)/java/examples/%.java $(PROJ)
	javac -classpath $(ROOT_DIR)/java/native $(ROOT_DIR)/java/examples/$*.java
	java -noverify -jar $(ROOT_DIR)/tool/NanoVMTool.jar -f $(ROOT_DIR)/java/examples/$*.nvm $(ROOT_DIR)/tool/config/UnixTest.config $(ROOT_DIR)/java/examples $*
	./$(PROJ) $(ROOT_DIR)/java/examples/$*.nvm

%-debug: $(ROOT_DIR)/java/examples/%.java $(PROJ)
	javac -classpath $(ROOT_DIR)/java/native $(ROOT_DIR)/java/examples/$*.java
	java -noverify -jar $(ROOT_DIR)/tool/NanoVMTool.jar -f $(ROOT_DIR)/java/examples/$*.nvm $(ROOT_DIR)/tool/config/UnixTest.config $(ROOT_DIR)/java/examples $*
	./$(PROJ) -d $(ROOT_DIR)/java/examples/$*.nvm

# run target from java dir and verify with sun-jvm output
%-verify: $(ROOT_DIR)/java/examples/%.java $(PROJ)
	javac -classpath $(ROOT_DIR)/java/native $(ROOT_DIR)/java/examples/$*.java
	java -noverify -jar $(ROOT_DIR)/tool/NanoVMTool.jar -f $(ROOT_DIR)/java/examples/$*.nvm $(ROOT_DIR)/tool/config/UnixTest.config $(ROOT_DIR)/java/examples $*
	./$(PROJ) -q $(ROOT_DIR)/java/examples/$*.nvm > $(PROJ).log
	java -cp $(ROOT_DIR)/java/examples $* > java.log
	@if [ "`diff $(PROJ).log java.log`" != "" ]; then \
		echo ">>>>>>>>>>>> Verify failed!!! <<<<<<<<<<<<<<"; \
	else \
		echo "Verify ok."; \
	fi
	@rm $(PROJ).log java.log

clean:
	rm -f *.d *.o *~ nvmdefault.h NanoVM

#include $(OBJS:.o=.d)
