/* Простой символьный драйвер с поддержкой IOCTL */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/device/class.h>
#include <linux/ioctl.h>
#include <linux/timekeeping.h>
#include <linux/types.h>

/* Константы */
#define DEVICE_NAME "char_driver"  // Имя устройства
#define BUFFER_SIZE 20              // Размер буфера

/* IOCTL команды */
#define MAGIC_NUM 'a'
#define CLEAR_BUFFER _IO(MAGIC_NUM, 2)    // Очистка буфера
#define IS_EMPTY _IOR(MAGIC_NUM, 1, int*) // Проверка пуст ли буфер
#define MEASURED_TIME _IOR(MAGIC_NUM, 3, u64*)

/* Глобальные переменные драйвера */
static struct class  *simple_class;   // Класс устройства
static dev_t dev_num;                 // Номер устройства (major, minor)
static struct cdev simple_cdev;       // Символьное устройство
static struct device *simple_device;  // Устройство

/* Буфер для хранения данных */
static int my_desired_weight;
static bool buf_empty = true;         // Флаг: буфер пуст

/* Variables, that store time of read and write operations */
static ktime_t read_time;
static ktime_t write_time;
static u64 read_time_ns;
static u64 write_time_ns;

/* Функция открытия устройства */
static int dev_open(struct inode *inode, struct file *file)
{    
  printk(KERN_INFO "char_driver: устройство открыто\n");
  return 0;
    
}

/* Функция закрытия устройства */
static int dev_release(struct inode *inode, struct file *file)
{
  printk(KERN_INFO "char_driver: устройство закрыто\n");
  return 0;
}

/* Функция чтения из устройства */
static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
  //int bytes_to_read = BUFFER_SIZE;

  /* Проверка: достигнут конец буфера */
  //if (*offset >= bytes_to_read)
  //	  return 0;

  /* Копирование данных из ядра в пространство пользователя */
  if (copy_to_user(buf, &my_desired_weight, sizeof(int))) {
    printk(KERN_ERR "char_driver: dev_read copy_to_user error!");  
    return -EFAULT;
  }

  //printk(KERN_INFO "char_driver: прочитано %d байт\n", bytes_to_read);
  printk(KERN_INFO "char_driver: dev_read success (%d)\n", my_desired_weight);
  read_time = ktime_get();
  read_time_ns = ktime_get_ns();
  return sizeof(int);
}

/* Функция записи в устройство */
static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
  write_time = ktime_get(); 
  write_time_ns = ktime_get_ns();
  /* Проверка: данные не должны превышать размер буфера */
  if (len < sizeof(int)) {
    printk(KERN_WARNING "char_driver: buffer too small");
    return -EINVAL;
  }

  /* Копирование данных из пространства пользователя в ядро */
  if (copy_from_user(&my_desired_weight, buf, sizeof(int))) {
    printk(KERN_ERR "char_driver: dev_write copy_from_user error!\n");
    return -EFAULT;
  }

  buf_empty = false;

  printk(KERN_INFO "char_driver: wrote %d\n", my_desired_weight);

  return len;
}

/* Функция обработки IOCTL команд */
static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  //int i;

  switch (cmd) {
  case CLEAR_BUFFER:
    /* Очистка буфера */
    my_desired_weight = -1;
    buf_empty = true;
    printk(KERN_INFO "char_driver: буфер очищен\n");
    break;

  case IS_EMPTY:
    /* Проверка: пуст ли буфер */
    if (copy_to_user((int32_t*)arg, &buf_empty, sizeof(buf_empty))) {
      printk(KERN_ERR "char_driver: ошибка копирования статуса\n");
      return -EFAULT;
    }
    break;
	  
  case MEASURED_TIME:
    ktime_t elapsed = ktime_sub(read_time, write_time);
    s64 elapsed_microseconds = ktime_to_us(elapsed);
    u64 elapsed_nanoseconds = read_time_ns - write_time_ns;
    printk(KERN_INFO "Operation took %llu microseconds (us).\n", elapsed_nanoseconds);
    if(copy_to_user((u64 __user *)arg, &elapsed_nanoseconds, sizeof(u64))){
      printk(KERN_ERR "char_driver: error in MEASURED_TIME ioctl\n");
      return -EFAULT;
    }
    break;

  default:
    printk(KERN_ERR "char_driver: неизвестная IOCTL команда\n");
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
    printk(KERN_ALERT "char_driver: не удалось выделить номер устройства\n");
    return res;
  }
  printk(KERN_INFO "char_driver: зарегистрирован <major %d, minor %d>\n",
    MAJOR(dev_num), MINOR(dev_num));

  /* Шаг 2: Инициализация и добавление символьного устройства */
  cdev_init(&simple_cdev, &fops);
  simple_cdev.owner = THIS_MODULE;
  res = cdev_add(&simple_cdev, dev_num, 1);
  if (res < 0) {
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_ALERT "char_driver: не удалось добавить устройство\n");
    return res;
  }

  /* Шаг 3: Создание класса устройства */
  simple_class = class_create(DEVICE_NAME);
  if (IS_ERR(simple_class)) {
    cdev_del(&simple_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_ALERT "char_driver: не удалось создать класс\n");
    return PTR_ERR(simple_class);
  }

  /* Шаг 4: Создание файла устройства в /dev */
  simple_device = device_create(simple_class, NULL, dev_num, NULL, DEVICE_NAME);
  if (IS_ERR(simple_device)) {
    class_destroy(simple_class);
    cdev_del(&simple_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_ALERT "char_driver: не удалось создать устройство\n");
    return PTR_ERR(simple_device);
  }

  printk(KERN_INFO "char_driver: устройство успешно создано\n");
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
  printk(KERN_INFO "char_driver: устройство удалено, ресурсы освобождены\n");
}

/* Регистрация функций инициализации и выгрузки */
module_init(simple_init);
module_exit(simple_exit);

/* Метаданные модуля */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrey");
MODULE_DESCRIPTION("Cимвольный драйвер с поддержкой IOCTL");
