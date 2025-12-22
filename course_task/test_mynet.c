#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#define MYNET_MAGIC 'N'
#define MYNET_IOCTL_UP     _IO(MYNET_MAGIC, 1)
#define MYNET_IOCTL_DOWN   _IO(MYNET_MAGIC, 2)
#define MYNET_IOCTL_STATS  _IOR(MYNET_MAGIC, 3, struct mynet_stats)

struct mynet_stats {
    int up;
    int mtu;
    unsigned long tx_packets;
    unsigned long rx_packets;
};

int main() {
    int fd = open("/dev/mynetchar", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    
    /* Bring interface UP (calls your net_open()) */
    ioctl(fd, MYNET_IOCTL_UP);
    
    /* Send packet (calls your start_xmit()) */
    char pkt[] = "Hello from userspace packet!";
    write(fd, pkt, strlen(pkt));
    
    /* Read back last packet */
    char rxbuf[1024];
    ssize_t len = read(fd, rxbuf, sizeof(rxbuf)-1);
    if (len > 0) {
        rxbuf[len] = 0;
        printf("RX: %s\n", rxbuf);
    }
    
    /* Get stats */
    struct mynet_stats stats;
    ioctl(fd, MYNET_IOCTL_STATS, &stats);
    printf("Stats: up=%d, mtu=%d, tx=%lu\n", stats.up, stats.mtu, stats.tx_packets);
    
    ioctl(fd, MYNET_IOCTL_DOWN);
    close(fd);
    return 0;
}

