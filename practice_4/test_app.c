#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdbool.h>

#define CLEAR_BUFFER _IO('a', 2)
#define IS_EMPTY _IOR('a', 1, int*)

int main(){
	int fd;
	char buf[100];

	fd = open("/dev/simple_char", O_RDWR);
	if (fd < 0){
		perror("Failed to open device");
		return -1;
	}

	bool buf_empty = false;
	if(ioctl(fd, IS_EMPTY, &buf_empty) < 0) {
	  perror("IOCTL IS_EMPTY failed");
	  close(fd);
	  return -1;
	}
	if(buf_empty){
		printf("Buffer empty -- true\n");
	} else {
		printf("IOCTL error. Buffer is empty!\n");
	}

        char *str = "Test write\n";
        ssize_t ret = write(fd, str, strlen(str));
        if(ret < 0){
          printf("Failed to write to device");
        } else {
          printf("Write successfull\n");
        }

	read(fd, buf, sizeof(buf));
	printf("Read from device: %s\n", buf);

	if(ioctl(fd, IS_EMPTY, &buf_empty) < 0) {
	  perror("IOCTL IS_EMPTY failed");
	  close(fd);
	  return -1;
	}
	if(!buf_empty){
		printf("Buffer is not empty -- true\n");
	} else {
		printf("IOCTL error. Buffer is not empty!\n");
	}

        if(ioctl(fd, CLEAR_BUFFER) < 0) {
          perror("IOCTL CLEAR_BUFFER failed\n");
          close(fd);
          return -1;
        }
        printf("Buffer cleared\n");

	if(ioctl(fd, IS_EMPTY, &buf_empty) < 0) {
		perror("IOCTL IS_EMPTY failed\n");
		close(fd);
		return -1;
	}
	if(buf_empty){
		printf("Buffer is empty -- true\n");
	} else {
		printf("IOCTL error. Buffer is empty!\n");
	}

	close(fd);
	return 0;
}


