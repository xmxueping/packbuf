TARGET=libpackbuf.so
CC=gcc
CPPC=g++
OPTS=-fPIC -O
OBJS=packbuf.o
LIBS=-lc

all:$(TARGET)

$(TARGET):$(OBJS)
	$(CC) $(OPTS) -shared -o $(TARGET) $(OBJS) $(LIBS)

%.o:%.cpp
	$(CPPC) $(OPTS) -c $< -o $@

%.o:%.c
	$(CC) $(OPTS) -c $< -o $@

install:
#	cp -f $(TARGET) ../../lib/linux

clean:
	rm -f $(TARGET) $(OBJS)
