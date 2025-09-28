#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
int open_and_read(const char *device_name){
	int fd;
	char buf[100];
	ssize_t n;

	fd = open(device_name, O_RDONLY);
	if (fd < 0){
		perror("open");
		return 1;
	}

	n = read(fd, buf, sizeof(buf));
	if (n < 0){
		perror("read");
		close(fd);
		return 1;
	}

	printf("Read %zd bytes from %s\n", n, device_name);
	for(int i = 0; i < n; ++i){
		printf("%02x", buf[i]);
	}
	printf("\n");

	close(fd);
	return 0;

}

int main(){
  open_and_read("/dev/zero");
  open_and_read("/dev/random");
  return 0;
}
