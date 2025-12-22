/*
* mynetchar.c - Минимальный сетевой драйвер, преобразованный в символьное устройство

* КОНЦЕПЦИЯ: Этот драйвер берет основную логику типичного сетевого драйвера (open, stop, start_xmit)
* и повторно предоставляет ее через интерфейс СИМВОЛЬНОГО УСТРОЙСТВА (/dev/mynetchar) вместо
* регистрации как net_device в сетевом стеке.

* СЕТЕВЫЕ ФУНКЦИИ, ОТОБРАЖЕННЫЕ НА СИМВОЛЬНОЕ УСТРОЙСТВО:
* - net_device->ndo_open() → ioctl(MYNET_IOCTL_UP) или первое .open()
* - net_device->ndo_stop() → ioctl(MYNET_IOCTL_DOWN) или последнее .release()
* - net_device->ndo_start_xmit() → системный вызов .write()

* БЕЗ ИНТЕГРАЦИИ С СЕТЕВЫМ СТЕКОМ: Этот драйвер НЕ регистрирует net_device, поэтому он не
* отображается в 'ip link' или 'ifconfig'. Это чисто символьное устройство, которое ПЕРЕИСПОЛЬЗУЕТ
* внутреннюю логику вашего сетевого драйвера в целях демонстрации/обучения.

* ИСПОЛЬЗОВАНИЕ: Пользовательское пространство открывает /dev/mynetchar, использует ioctl для управления UP/DOWN,
* записывает пакеты для вызова start_xmit(), читает симулированные RX-данные.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#define MYNET_MAGIC 'A'

#define MYNET_IOCTL_UP _IO(MYNET_MAGIC, 1) /* Включить интерфейс (как ifconfig up) */
#define MYNET_IOCTL_DOWN _IO(MYNET_MAGIC, 2) /* Выключить интерфейс (как ifconfig down) */
#define MYNET_IOCTL_STATS _IOR(MYNET_MAGIC, 3, struct mynet_stats) /* Получить статистику */

#define DEVICE_NAME "mynetchar"
#define MAX_PKTLEN 1500

/* Структура приватных данных - аналогична net_device->priv в реальных сетевых драйверах */
struct mynet_priv {
	struct cdev cdev; /* Регистрация символьного устройства */
	int up; /* Состояние интерфейса (UP/DOWN) */
	int mtu; /* Максимальный размер пакета */
	unsigned long tx_packets; /* Счетчик TX-пакетов */
	unsigned long rx_packets; /* Счетчик RX-пакетов (симулированный) */
	char last_tx[MAX_PKTLEN]; /* Хранение последнего переданного пакета для read() */
};

static struct mynet_priv mynet_data = {
	.mtu = 1500,
};

/* Структура статистики - передается в пользовательское пространство через ioctl */
struct mynet_stats {
	int up;
	int mtu;
	unsigned long tx_packets;
	unsigned long rx_packets;
};

static dev_t dev_num; /* Номер устройства major:minor */
static struct device *mynet_device;
static struct class *mynet_class; /* Sysfs-класс для автоматического узла /dev */

/*
* net_open() - аналог ndo_open() ВАШЕГО ИСХОДНОГО СЕТОВОГО ДРАЙВЕРА
* Вызывается при включении интерфейса
*/
static int net_open(void) {
	pr_info("MYNET: net_open() вызван - интерфейс UP\n");
	mynet_data.up = 1;
	return 0;
}

/*
* net_stop() - аналог ndo_stop() ВАШЕГО ИСХОДНОГО СЕТОВОГО ДРАЙВЕРА
* Вызывается при выключении интерфейса. Останавливает оборудование/освобождает ресурсы.
*/
static int net_stop(void) {
	pr_info("MYNET: net_stop() вызван - интерфейс DOWN\n");
	mynet_data.up = 0;
	return 0;
}

/*
* start_xmit() - аналог ndo_start_xmit() ВАШЕГО ИСХОДНОГО СЕТОВОГО ДРАЙВЕРА
* Вызывается сетевым стеком ядра (или здесь через .write()) для передачи пакета.
* В реальном драйвере: DMA на оборудование. Здесь: просто логирование и хранение.
*/
static int start_xmit(const char *skb_data, size_t len) {

	if (len >= MAX_PKTLEN) {
		pr_warn("MYNET: Пакет слишком большой: %zu > MTU %d\n", len, mynet_data.mtu);
		return 0;
	}

	/* Симуляция копирования skb->data - в реальном драйвере */
	if (copy_from_user(mynet_data.last_tx, skb_data, len))
		return 0;

	mynet_data.last_tx[len] = 0; /* Завершающий нуль для печати */
	mynet_data.tx_packets++; /* Обновление статистики */
	pr_info("MYNET: start_xmit() %zu байт: %.20s...\n", len, mynet_data.last_tx);

	return 0;
}

/*
* mynet_open() - callback .open() символьного устройства
* Соответствует подсчету ссылок сетевого драйвера. Вызывает net_open() при ПЕРВОМ открытии.
*/
static int mynet_open(struct inode *inode, struct file *file) {
	/* Подсчет ссылок: вызываем net_open() только если еще не включен */
	if (!mynet_data.up)
		net_open();

	pr_info("MYNET: /dev/%s открыт\n", DEVICE_NAME);
	return 0;
}

/*
* mynet_release() - callback .release() символьного устройства
* Соответствует подсчету ссылок сетевого драйвера. Вызывает net_stop() при ПОСЛЕДНЕМ закрытии.
*/
static int mynet_release(struct inode *inode, struct file *file) {

	/* Подсчет ссылок: вызываем net_stop() только если это последнее закрытие */
	if (mynet_data.up)
		net_stop();

	pr_info("MYNET: /dev/%s закрыт\n", DEVICE_NAME);
	return 0;
}

/*
* mynet_write() - callback .write() символьного устройства
* Пользовательское пространство: echo "данные пакета" > /dev/mynetchar
* Это напрямую вызывает логику start_xmit()!
*/
static ssize_t mynet_write(struct file *file, const char __user *buf,
	size_t count, loff_t *offset) {

	if (!mynet_data.up) {
		pr_warn("MYNET: Интерфейс выключен - данные нельзя передавать\n");
		return -ENETDOWN;
	}

	/* ПЕРЕИСПОЛЬЗУЕМ start_xmit() - точно так же, как сетевой стек ядра */
	start_xmit(buf, count);

	return count; /* Сообщаем пользовательскому пространству, что все байты обработаны */
}

/*
* mynet_read() - callback .read() символьного устройства (симуляция RX)
* Пользовательское пространство: cat /dev/mynetchar
* Для демо: возвращает последний TX-пакет (симуляция loopback).
*/
static ssize_t mynet_read(struct file *file, char __user *buf,
	size_t count, loff_t *offset) {
	int len = strlen(mynet_data.last_tx);

	if (len == 0) return 0; /* Нечего читать */

	/* Симуляция RX-пакета - копируем последний TX в пользовательское пространство */
	if (copy_to_user(buf, mynet_data.last_tx, len))
		return -EFAULT;

	mynet_data.rx_packets++; /* Обновление RX-статистики */
	return len;
}

/*
* mynet_ioctl() - callback .unlocked_ioctl() символьного устройства
* Команды управления из пользовательского пространства: UP/DOWN/ПОЛУЧИТЬ_СТАТИСТИКУ (как ifconfig/ip link)
*/
static long mynet_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	//struct mynet_priv *priv = file->private_data;
	struct mynet_stats stats;

	switch (cmd) {
	case MYNET_IOCTL_UP:
		pr_info("MYNET: ioctl UP\n");
		return net_open(); /* ndo_open() */
	case MYNET_IOCTL_DOWN:
		pr_info("MYNET: ioctl DOWN\n");
		return net_stop(); /* ndo_stop() */
	case MYNET_IOCTL_STATS:
		/* Копируем статистику в пользовательское пространство */
		stats.up = mynet_data.up;
		stats.mtu = mynet_data.mtu;
		stats.tx_packets = mynet_data.tx_packets;
		stats.rx_packets = mynet_data.rx_packets;

		if (copy_to_user((struct mynet_stats __user *)arg, &stats, sizeof(stats)))
			return -EFAULT;
		return 0;
	default:
		return -ENOTTY; /* Неизвестная команда */
	}
	return 0;
}

/* Таблица операций с файлом символьного устройства */
static const struct file_operations mynet_fops = {
	.owner = THIS_MODULE,
	.open = mynet_open, /* Соответствует net_open() */
	.release = mynet_release, /* Соответствует net_stop() */
	.read = mynet_read, /* Симуляция RX */
	.write = mynet_write, /* Вызов start_xmit() */
	.unlocked_ioctl = mynet_ioctl, /* Управление UP/DOWN/статистикой */
};

/*
* Инициализация модуля - Регистрация символьного устройства /dev/mynetchar
* Создает: /dev/mynetchar (major=XXX minor=0)
*/
static int __init mynet_init(void) {
	int ret;

	/* 1. Получаем уникальные номера устройств (major:minor) */
	ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		printk(KERN_ERR "MYNET: Ошибка выделения области chrdev\n");
		return ret;
	}
	printk(KERN_INFO "MYNET: зарегистирован драйвер <major %d, minor %d>\n", MAJOR(dev_num), MINOR(dev_num));
	
	/* 2. Инициализация и добавлние символьного устройства */
	cdev_init(&mynet_data.cdev, &mynet_fops);
	mynet_data.cdev.owner = THIS_MODULE;
	mynet_data.mtu = 1500; /* MTU по умолчанию */

	/* 3. Регистрация cdev */
	ret = cdev_add(&mynet_data.cdev, dev_num, 1);
	if (ret < 0) {
		unregister_chrdev_region(dev_num, 1);
		printk(KERN_ALERT "MYNET: не удалось добавить устройство\n");
		return ret;
	}

	/* 4. Создание класса устройства */
	mynet_class = class_create(DEVICE_NAME);
	if (IS_ERR(mynet_class)) {
		cdev_del(&mynet_data.cdev);
		unregister_chrdev_region(dev_num, 1);
		printk(KERN_ALERT "MYNET: не удалось создать класс\n");
		return PTR_ERR(mynet_class);
	}

	/* 5. Создание файла устройства в /dev */
	mynet_device = device_create(mynet_class, NULL, dev_num, NULL, DEVICE_NAME);
	if (IS_ERR(mynet_device)) {
		class_destroy(mynet_class);
		cdev_del(&mynet_data.cdev);
		unregister_chrdev_region(dev_num, 1);
		printk(KERN_ALERT "MYNET: не удалось создать устройство\n");
		return PTR_ERR(mynet_device);
	}

	printk(KERN_ALERT "MYNET: устройство успешно создано\n");
	return 0;
}

/* Выход модуля - Очистка всего */
static void __exit mynet_exit(void) {
	/* Удаление устройства и освобождение ресурсов в обратном порядке*/
	device_destroy(mynet_class, dev_num);
	class_destroy(mynet_class);
	cdev_del(&mynet_data.cdev);
	unregister_chrdev_region(dev_num, 1);
	pr_info("MYNET: Устройство удалено, ресурсы освобождены\n");
}

module_init(mynet_init);
module_exit(mynet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrei Petrov");
MODULE_DESCRIPTION("Логика сетевого драйвера как символьного драйвера");
