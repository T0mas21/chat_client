CC = gcc
CFLAGS = -Wall -Wextra -Werror
LDFLAGS = -pthread
TARGET = client
SRCS = client.c tcp.c udp.c tcp_functions.c udp_functions.c 
OBJS = $(SRCS:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET) $(ARGS)

clean:
	rm -f $(OBJS) $(TARGET)