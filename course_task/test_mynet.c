/*
* test_mynet.c - Тестовая программа для mynetchar.c
* Демонстрирует использование символьного устройства /dev/mynetchar
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define MYNET_MAGIC 'A'

#define MYNET_IOCTL_UP _IO(MYNET_MAGIC, 1)     /* Включить интерфейс (вызывает net_open()) */
#define MYNET_IOCTL_DOWN _IO(MYNET_MAGIC, 2)   /* Выключить интерфейс (вызывает net_stop()) */
#define MYNET_IOCTL_STATS _IOR(MYNET_MAGIC, 3, struct mynet_stats) /* Получить статистику */

/* Структура статистики - совпадает с драйвером */
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

	/* Включить интерфейс (вызывает net_open() из драйвера) */
	ioctl(fd, MYNET_IOCTL_UP);

	/* Отправить пакет (вызывает start_xmit() из драйвера) */
	char pkt[] = "Пакет 1";
	write(fd, pkt, strlen(pkt));

	/* Прочитать последний пакет (симуляция RX) */
	char rxbuf[1024];
	ssize_t len = read(fd, rxbuf, sizeof(rxbuf) - 1);
	if (len > 0) {
		rxbuf[len] = 0;
		printf("RX: %s\n", rxbuf);
	}

	/* Получить статистику */
	struct mynet_stats stats;
	ioctl(fd, MYNET_IOCTL_STATS, &stats);
	printf("Статистика: up=%d, mtu=%d, tx=%lu, rx=%lu\n", 
	       stats.up, stats.mtu, stats.tx_packets, stats.rx_packets);

	/* Выключить интерфейс */
	ioctl(fd, MYNET_IOCTL_DOWN);
	close(fd);

	return 0;
}
