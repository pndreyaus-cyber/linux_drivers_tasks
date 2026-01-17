/* Символьный драйвер. Назначение:

1) Read/write функции для четния из устройства и записи в устройство числа
2) CLEAR_BUFFER ioctl-функция для очистки записанного в устройство числа
3) IS_EMPTY ioctl-функция для проверки, что в число ничего не записано
4) MEASURED_TIME ioctl-функция для получения разницы между временем крайней операции чтения из устройства и 
крайней операции записи в устройство (в наносекундах)

 */

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
#define MEASURED_TIME _IOR(MAGIC_NUM, 3, u64*) // Время между чтением и записью, в наносекундах

/* Глобальные переменные драйвера */
static struct class  *char_driver_class;   // Класс устройства
static dev_t dev_num;                 // Номер устройства (major, minor)
static struct cdev char_driver_cdev;       // Символьное устройство
static struct device *char_driver_device;  // Устройство

/* Буфер для хранения данных */
static int my_desired_weight;         // Число для записи/чтения
static bool buf_empty = true;         // Флаг: буфер пуст


/* Переменные для сохранения времени крайней операции чтения и крайней операции записи */
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

  /* Копирование данных из ядра в пространство пользователя */
  if (copy_to_user(buf, &my_desired_weight, sizeof(int))) {
    printk(KERN_ERR "char_driver: dev_read copy_to_user ошибка!");  
    return -EFAULT;
  }

  
  printk(KERN_INFO "char_driver: dev_read успешно прочитано (%d)\n", my_desired_weight);

  read_time_ns = ktime_get_ns(); // Сохранить время, когда закончилась операция чтения из устройства

  return sizeof(int);
}

/* Функция записи в устройство */
static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
  write_time_ns = ktime_get_ns();// Сохранить время, когда началась операция записи в устройство

  /* Копирование данных из пространства пользователя в ядро */
  if (copy_from_user(&my_desired_weight, buf, sizeof(int))) {
    printk(KERN_ERR "char_driver: dev_write copy_from_user ошибка!\n");
    return -EFAULT;
  }

  buf_empty = false;

  printk(KERN_INFO "char_driver: dev_write успешно записано (%d)\n", my_desired_weight);

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
    u64 elapsed_nanoseconds = read_time_ns - write_time_ns;
    printk(KERN_INFO "Время между операциями %llu наносекунд\n", elapsed_nanoseconds);
    if(copy_to_user((u64 __user *)arg, &elapsed_nanoseconds, sizeof(u64))){
      printk(KERN_ERR "char_driver: MEASURED_TIME IOCTL ошибка!\n");
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
static int __init char_driver_init(void)
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
  cdev_init(&char_driver_cdev, &fops);
  char_driver_cdev.owner = THIS_MODULE;
  res = cdev_add(&char_driver_cdev, dev_num, 1);
  if (res < 0) {
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_ALERT "char_driver: не удалось добавить устройство\n");
    return res;
  }

  /* Шаг 3: Создание класса устройства */
  char_driver_class = class_create(DEVICE_NAME);
  if (IS_ERR(char_driver_class)) {
    cdev_del(&char_driver_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_ALERT "char_driver: не удалось создать класс\n");
    return PTR_ERR(char_driver_class);
  }

  /* Шаг 4: Создание файла устройства в /dev */
  char_driver_device = device_create(char_driver_class, NULL, dev_num, NULL, DEVICE_NAME);
  if (IS_ERR(char_driver_device)) {
    class_destroy(char_driver_class);
    cdev_del(&char_driver_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_ALERT "char_driver: не удалось создать устройство\n");
    return PTR_ERR(char_driver_device);
  }

  printk(KERN_INFO "char_driver: устройство успешно создано\n");
  return 0;
}

/* Функция выгрузки модуля */
static void __exit char_driver_exit(void)
{
  /* Удаление устройства и освобождение ресурсов в обратном порядке */
  device_destroy(char_driver_class, dev_num);
  class_destroy(char_driver_class);
  cdev_del(&char_driver_cdev);
  unregister_chrdev_region(dev_num, 1);
  printk(KERN_INFO "char_driver: устройство удалено, ресурсы освобождены\n");
}

/* Регистрация функций инициализации и выгрузки */
module_init(char_driver_init);
module_exit(char_driver_exit);

/* Метаданные модуля */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrey");
MODULE_DESCRIPTION("Cимвольный драйвер с поддержкой IOCTL");
