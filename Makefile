CC=gcc
TARGET=submqtt

all: $(TARGET)

$(TARGET): clean
	$(CC) -O3 submqtt.c utils.c hfunc.c -pthread -lmosquitto -lncurses -o $@

clean:
	-rm $(TARGET)
