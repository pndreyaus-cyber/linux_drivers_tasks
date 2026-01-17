#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(){
	int fd;
	char buf[100];

	fd = open("/dev/simple_char", O_RDONLY);
	if (fd < 0){
		perror("open");
		return 1;
	}

	read(fd, buf, sizeof(buf));
	printf("Read from device: %s\n", buf);

	close(fd);
	return 0;
}


