/* Простой символьный драйвер с поддержкой IOCTL */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/device/class.h>
#include <linux/ioctl.h>

/* Константы */
#define DEVICE_NAME "simple_char"  // Имя устройства
#define BUFFER_SIZE 20              // Размер буфера

/* IOCTL команды */
#define CLEAR_BUFFER _IO('a', 2)    // Очистка буфера
#define IS_EMPTY _IOR('a', 1, int*) // Проверка пуст ли буфер

/* Глобальные переменные драйвера */
static struct class  *simple_class;   // Класс устройства
static dev_t dev_num;                 // Номер устройства (major, minor)
static struct cdev simple_cdev;       // Символьное устройство
static struct device *simple_device;  // Устройство

/* Буфер для хранения данных */
static char local_buf[BUFFER_SIZE];
static bool buf_empty = true;         // Флаг: буфер пуст

/* Функция открытия устройства */
static int dev_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "simple_char: устройство открыто\n");
	return 0;
}

/* Функция закрытия устройства */
static int dev_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "simple_char: устройство закрыто\n");
	return 0;
}

/* Функция чтения из устройства */
static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	int bytes_to_read = BUFFER_SIZE;

	/* Проверка: достигнут конец буфера */
	if (*offset >= bytes_to_read)
		return 0;

	/* Копирование данных из ядра в пространство пользователя */
	if (copy_to_user(buf, local_buf, bytes_to_read))
		return -EFAULT;

	*offset += bytes_to_read;
	printk(KERN_INFO "simple_char: прочитано %d байт\n", bytes_to_read);
	return bytes_to_read;
}

/* Функция записи в устройство */
static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
	/* Проверка: данные не должны превышать размер буфера */
	if (len >= BUFFER_SIZE) {
		printk(KERN_WARNING "simple_char: слишком много данных (максимум %d байт)\n", BUFFER_SIZE - 1);
		return -EINVAL;
	}

	/* Копирование данных из пространства пользователя в ядро */
	if (copy_from_user(local_buf, buf, len)) {
		printk(KERN_ERR "simple_char: ошибка копирования данных\n");
		return -EFAULT;
	}

	/* Добавление нуль-терминатора */
	local_buf[len] = '\0';
	buf_empty = false;

	printk(KERN_INFO "simple_char: записано %zu байт: '%s'\n", len, local_buf);
	return len;
}

/* Функция обработки IOCTL команд */
static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int i;

	switch (cmd) {
	case CLEAR_BUFFER:
		/* Очистка буфера */
		for (i = 0; i < BUFFER_SIZE; i++) {
			local_buf[i] = '\0';
		}
		buf_empty = true;
		printk(KERN_INFO "simple_char: буфер очищен\n");
		break;

	case IS_EMPTY:
		/* Проверка: пуст ли буфер */
		if (copy_to_user((int32_t*)arg, &buf_empty, sizeof(buf_empty))) {
			printk(KERN_ERR "simple_char: ошибка копирования статуса\n");
			return -EFAULT;
		}
		break;

	default:
		printk(KERN_ERR "simple_char: неизвестная IOCTL команда\n");
		return -ENOTTY;
	}

	return 0;
}

/* Структура файловых операций */
static struct file_operations fops = {
	.owner          = THIS_MODULE,
	.open           = dev_open,
	.release        = dev_release,
	.read           = dev_read,
	.write          = dev_write,
	.unlocked_ioctl = dev_ioctl,
};

/* Функция инициализации модуля */
static int __init simple_init(void)
{
	int res;

	/* Шаг 1: Выделение номера устройства (major, minor) */
	res = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (res < 0) {
		printk(KERN_ALERT "simple_char: не удалось выделить номер устройства\n");
		return res;
	}
	printk(KERN_INFO "simple_char: зарегистрирован <major %d, minor %d>\n",
	       MAJOR(dev_num), MINOR(dev_num));

	/* Шаг 2: Инициализация и добавление символьного устройства */
	cdev_init(&simple_cdev, &fops);
	simple_cdev.owner = THIS_MODULE;
	res = cdev_add(&simple_cdev, dev_num, 1);
	if (res < 0) {
		unregister_chrdev_region(dev_num, 1);
		printk(KERN_ALERT "simple_char: не удалось добавить устройство\n");
		return res;
	}

	/* Шаг 3: Создание класса устройства */
	simple_class = class_create(DEVICE_NAME);
	if (IS_ERR(simple_class)) {
		cdev_del(&simple_cdev);
		unregister_chrdev_region(dev_num, 1);
		printk(KERN_ALERT "simple_char: не удалось создать класс\n");
		return PTR_ERR(simple_class);
	}

	/* Шаг 4: Создание файла устройства в /dev */
	simple_device = device_create(simple_class, NULL, dev_num, NULL, DEVICE_NAME);
	if (IS_ERR(simple_device)) {
		class_destroy(simple_class);
		cdev_del(&simple_cdev);
		unregister_chrdev_region(dev_num, 1);
		printk(KERN_ALERT "simple_char: не удалось создать устройство\n");
		return PTR_ERR(simple_device);
	}

	printk(KERN_INFO "simple_char: устройство успешно создано\n");
	return 0;
}

/* Функция выгрузки модуля */
static void __exit simple_exit(void)
{
	/* Удаление устройства и освобождение ресурсов в обратном порядке */
	device_destroy(simple_class, dev_num);
	class_destroy(simple_class);
	cdev_del(&simple_cdev);
	unregister_chrdev_region(dev_num, 1);
	printk(KERN_INFO "simple_char: устройство удалено, ресурсы освобождены\n");
}

/* Регистрация функций инициализации и выгрузки */
module_init(simple_init);
module_exit(simple_exit);

/* Метаданные модуля */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrey");
MODULE_DESCRIPTION("Простой символьный драйвер с поддержкой IOCTL");
