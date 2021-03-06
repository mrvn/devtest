CXXFLAGS := -O2 -W -Wall -std=gnu++11 -g -MD -MP
LDFLAGS := $(CXXFLAGS) -laio -lpthread

all: devtest

devtest: fd.o eventfd.o file.o iocb.o context.o iothread.o main.o
	$(CXX) $(LDFLAGS) -o $@ $+

%.o: %.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $<

clean:
	rm -f devtest *.o

distclean: clean
	rm -f *.~ *.d

-include *.d
