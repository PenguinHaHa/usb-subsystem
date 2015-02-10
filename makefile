TARGET = usbinfo
OBJ = main.o libsysfs.o libusbfs.o
CC = gcc

$(TARGET) : $(OBJ)
	$(CC) -o $(TARGET) $(OBJ)

main.o : main.c libsysfs.c libusbfs.c
	$(CC) $(CFLAGS) -c main.c

libsysfs.o : libsysfs.c libsysfs.h
	$(CC) $(CFLAGS) -c libsysfs.c

libusbfs.o : libusbfs.c libusbfs.h
	$(CC) $(CFLAGS) -c libusbfs.c

clean:
	rm $(TARGET) $(OBJ)
