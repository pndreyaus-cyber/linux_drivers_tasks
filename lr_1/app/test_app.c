/* Тестовое приложение для символьного драйвера simple_char */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <stdint.h>


typedef uint64_t u64;
typedef int64_t s64;


/* IOCTL команды (должны совпадать с драйвером) */
#define MAGIC_NUM 'a'
#define CLEAR_BUFFER _IO(MAGIC_NUM, 2)    /* Очистка буфера */
#define IS_EMPTY _IOR(MAGIC_NUM, 1, int*) /* Проверка пустоты буфера */
#define MEASURED_TIME _IOR(MAGIC_NUM, 3, u64*) 

#define DEVICE_PATH "/dev/simple_char"

int main()
{
	int fd;
	bool buf_empty;
	ssize_t ret;
        int read_int = -1;
        int write_int = 4;
        s64 elapsed_time_ms = -1;

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
	ret = write(fd, &write_int, sizeof(int));
	if (ret < 0) {
	  perror("Ошибка записи в устройство");
	  close(fd);
	  return -1;
	}
	printf("✓ Wrote number %d\n\n", write_int);

	/* Тест 3: Чтение данных из устройства */
	printf("--- Тест 3: Чтение данных из устройства ---\n");
	
	ret = read(fd, &read_int, sizeof(int));
	if (ret < 0) {
	  perror("Ошибка чтения из устройства");
	  close(fd);
	  return -1;
	}
	printf("✓ Read number %d\n\n", read_int);

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
	
	/* Test 7: Reading time difference between read and write operations*/
        printf("--- Test 7: Reading time difference between read and write operations ---\n");
        int test_num = 10000;
        u64 values[test_num];
        int cnt = 0;
        u64 min_elapsed = 100000;
        u64 max_elapsed = 0;
        for (int i = 0; i < test_num; ++i){
          ret = write(fd, &i, sizeof(int));
          
          if (ret < 0) {
	    perror("Ошибка записи в устройство");
	    printf("Error in write\n");
	    close(fd);
	    return -1;
	  }

	  
	  ret = read(fd, &read_int, sizeof(int));
	  if (ret < 0) {
	    perror("Ошибка чтения из устройства");
	    printf("Error in read\n");
	    close(fd);
	    return -1;
	  }
	  //printf("Readint: %d\n", read_int);
	  if (read_int != i) {
	    printf("Read incorrect number %d", i);
	  } else {
	    u64 elapsed;
	    if(ioctl(fd, MEASURED_TIME, &elapsed) < 0) {
              perror("Error IOCTL MEASURED_TIME");
              printf("Error in measured time\n");
              close(fd);
              return -1;
            }
            //printf("Experiment %d: %lu\n", cnt, elapsed);
	    values[cnt] = elapsed;
	    if (elapsed < min_elapsed) {
	      min_elapsed = elapsed;
	    }
	    if (elapsed > max_elapsed) {
	      max_elapsed = elapsed;
	    }
	    cnt++;
	  }
          
        }

        printf("Successfully passed %d tests\n", cnt);
        printf("Min elapsed time: %lu\n", min_elapsed);
	printf("Max elapsed time: %lu\n", max_elapsed);
	/* Закрытие устройства */
	close(fd);
	printf("=== Все тесты завершены успешно! ===\n");
	
	/* Open csv file */
	FILE *fptr;
	fptr = fopen("output.csv", "w");
	
	if (fptr == NULL) {
	  printf("Error opening file!");
          return 1;
        }
        
        for (int i = 0; i < test_num; ++i){
          fprintf(fptr, "%ld\n", values[i]);
        }
        
        fclose(fptr);
        
        printf("Experiment results written to output.csv successfully\n");
	
	return 0;
}


