CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGET = read_devices

all: $(TARGET)

$(TARGET): read_devices.c
	$(CC) $(CFLAGS) -o $(TARGET) read_devices.c

clean:
	rm -f $(TARGET)
