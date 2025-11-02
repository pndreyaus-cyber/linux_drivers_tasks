/* Тестовое приложение для символьного драйвера simple_char */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdbool.h>

/* IOCTL команды (должны совпадать с драйвером) */
#define CLEAR_BUFFER _IO('a', 2)    /* Очистка буфера */
#define IS_EMPTY _IOR('a', 1, int*) /* Проверка пустоты буфера */

#define DEVICE_PATH "/dev/simple_char"

int main()
{
	int fd;
	char read_buf[100];
	bool buf_empty;
	ssize_t ret;
	const char *test_string = "Test write\n";

	/* Открытие устройства */
	printf("=== Тестирование драйвера simple_char ===\n\n");
	fd = open(DEVICE_PATH, O_RDWR);
	if (fd < 0) {
		perror("Ошибка открытия устройства");
		return -1;
	}
	printf("✓ Устройство успешно открыто\n\n");

	/* Тест 1: Проверка пустоты буфера (должен быть пустым) */
	printf("--- Тест 1: Проверка начального состояния буфера ---\n");
	buf_empty = false;
	if (ioctl(fd, IS_EMPTY, &buf_empty) < 0) {
		perror("Ошибка IOCTL IS_EMPTY");
		close(fd);
		return -1;
	}
	if (buf_empty) {
		printf("✓ Буфер пустой (как и ожидалось)\n\n");
	} else {
		printf("✗ Ошибка: буфер должен быть пустым!\n\n");
	}

	/* Тест 2: Запись данных в устройство */
	printf("--- Тест 2: Запись данных в устройство ---\n");
	ret = write(fd, test_string, strlen(test_string));
	if (ret < 0) {
		perror("Ошибка записи в устройство");
		close(fd);
		return -1;
	}
	printf("✓ Записано %zd байт: '%s'\n", ret, test_string);

	/* Тест 3: Чтение данных из устройства */
	printf("--- Тест 3: Чтение данных из устройства ---\n");
	memset(read_buf, 0, sizeof(read_buf));
	ret = read(fd, read_buf, sizeof(read_buf));
	if (ret < 0) {
		perror("Ошибка чтения из устройства");
		close(fd);
		return -1;
	}
	printf("✓ Прочитано %zd байт: '%s'\n", ret, read_buf);

	/* Тест 4: Проверка что буфер НЕ пустой после записи */
	printf("--- Тест 4: Проверка состояния буфера после записи ---\n");
	if (ioctl(fd, IS_EMPTY, &buf_empty) < 0) {
		perror("Ошибка IOCTL IS_EMPTY");
		close(fd);
		return -1;
	}
	if (!buf_empty) {
		printf("✓ Буфер не пустой (как и ожидалось)\n\n");
	} else {
		printf("✗ Ошибка: буфер должен содержать данные!\n\n");
	}

	/* Тест 5: Очистка буфера */
	printf("--- Тест 5: Очистка буфера ---\n");
	if (ioctl(fd, CLEAR_BUFFER) < 0) {
		perror("Ошибка IOCTL CLEAR_BUFFER");
		close(fd);
		return -1;
	}
	printf("✓ Буфер очищен\n\n");

	/* Тест 6: Проверка что буфер пустой после очистки */
	printf("--- Тест 6: Проверка состояния буфера после очистки ---\n");
	if (ioctl(fd, IS_EMPTY, &buf_empty) < 0) {
		perror("Ошибка IOCTL IS_EMPTY");
		close(fd);
		return -1;
	}
	if (buf_empty) {
		printf("✓ Буфер пустой (как и ожидалось)\n\n");
	} else {
		printf("✗ Ошибка: буфер должен быть пустым!\n\n");
	}

	/* Закрытие устройства */
	close(fd);
	printf("=== Все тесты завершены успешно! ===\n");
	return 0;
}


