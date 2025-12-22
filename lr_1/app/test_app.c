/* Приложение для проведения эксперимента по повтору операций чтения из и записи в символьный драйвер
   В драйвере замеряется время между последовательными операциями чтения и записи
   Полученные данные из 10000 экспериментов сохраняются в файле output.csv*/

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
#define MEASURED_TIME _IOR(MAGIC_NUM, 3, u64*) /* Прочитать время в наносекундах за крайнию пару операций чтения/записи */

#define DEVICE_PATH "/dev/char_driver"

int main()
{
	int fd;
	bool buf_empty;
	ssize_t ret;

	int read_int = -1; // считанное число
	int write_int = 4; // записанное число

	/* Открытие устройства */
	printf("=== Тестирование драйвера char_driver ===\n\n");
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
	printf("✓ Число %d успешно записано\n\n", write_int);

	/* Тест 3: Чтение данных из устройства */
	printf("--- Тест 3: Чтение данных из устройства ---\n");
	
	ret = read(fd, &read_int, sizeof(int));
	if (ret < 0) {
	  perror("Ошибка чтения из устройства");
	  close(fd);
	  return -1;
	}
	if (read_int == write_int) {
		printf("✓ Успешно прочитано число %d\n\n", read_int);
	} else {
		printf("✗ Ошибка: Было прочитано %d, но должно было быть %d", read_int, write_int);
	}

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
	
	/* Тест 7: Эксперимент. 10000 раз повторяется операция чтения/записи */
    printf("--- Тест 7: Эксперимент. 10000 раз повторяется операция чтения/записи ---\n");
    int test_num = 10000;
	u64 elapsed_time_arr[test_num];
    int successfull_experiment_cnt = 0;
    for (int i = 0; i < test_num; ++i){
		ret = write(fd, &i, sizeof(int));
          
        if (ret < 0) {
	    	perror("Ошибка записи в устройство");
	    	close(fd);
	    	return -1;
	 	}
		
		ret = read(fd, &read_int, sizeof(int));
	  	
		if (ret < 0) {
	    	perror("Ошибка чтения из устройства");
	    	close(fd);
	    	return -1;
	  	}
	  
	  	if (read_int != i) {
	   		printf("Ошибка: Прочитано %d, должно было быть %d", read_int, i);
	  	} else {
	   		u64 elapsed;
	    	if(ioctl(fd, MEASURED_TIME, &elapsed) < 0) {
				perror("Ошибка IOCTL MEASURED_TIME");
              	close(fd);
              	return -1;
            }
	    	elapsed_time_arr[successfull_experiment_cnt] = elapsed;
	    	successfull_experiment_cnt++;
		}      
    }

    printf("Успшно пройденных экспериментов: %d\n\n", successfull_experiment_cnt);
	/* Закрытие устройства */
	close(fd);
	printf("=== Все тесты завершены успешно! ===\n");
	
	/* Open csv file */
	printf("=== Сохранение результатов в output.csv ===\n");
	
	FILE *fptr;
	fptr = fopen("../output/output.csv", "w");
	
	if (fptr == NULL) {
		printf("Ошибка при открытии output.csv!");
    	return 1;
    }
        
    for (int i = 0; i < test_num; ++i){
    	fprintf(fptr, "%ld\n", elapsed_time_arr[i]);
    }
        
    fclose(fptr);
        
    printf("=== Результаты сохранены в output/output.csv! ===\n");
	
	return 0;
}


