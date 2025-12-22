#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x4c1dbbd9, "cdev_add" },
	{ 0xea5ac1d9, "class_create" },
	{ 0xf98f93a7, "device_create" },
	{ 0x14fcde53, "class_destroy" },
	{ 0x0c72f9ad, "cdev_del" },
	{ 0x0bc5fb0d, "unregister_chrdev_region" },
	{ 0x9479a1e8, "strnlen" },
	{ 0xa61fd7aa, "__check_object_size" },
	{ 0xe54e0a6b, "__fortify_panic" },
	{ 0x6fdeeff0, "device_destroy" },
	{ 0x092a35a2, "_copy_from_user" },
	{ 0x90a48d82, "__ubsan_handle_out_of_bounds" },
	{ 0xd272d446, "__fentry__" },
	{ 0xe8213e80, "_printk" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0x092a35a2, "_copy_to_user" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0x9f222e1e, "alloc_chrdev_region" },
	{ 0xefd5d5d8, "cdev_init" },
	{ 0xba157484, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0x4c1dbbd9,
	0xea5ac1d9,
	0xf98f93a7,
	0x14fcde53,
	0x0c72f9ad,
	0x0bc5fb0d,
	0x9479a1e8,
	0xa61fd7aa,
	0xe54e0a6b,
	0x6fdeeff0,
	0x092a35a2,
	0x90a48d82,
	0xd272d446,
	0xe8213e80,
	0xd272d446,
	0x092a35a2,
	0xd272d446,
	0x9f222e1e,
	0xefd5d5d8,
	0xba157484,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"cdev_add\0"
	"class_create\0"
	"device_create\0"
	"class_destroy\0"
	"cdev_del\0"
	"unregister_chrdev_region\0"
	"strnlen\0"
	"__check_object_size\0"
	"__fortify_panic\0"
	"device_destroy\0"
	"_copy_from_user\0"
	"__ubsan_handle_out_of_bounds\0"
	"__fentry__\0"
	"_printk\0"
	"__x86_return_thunk\0"
	"_copy_to_user\0"
	"__stack_chk_fail\0"
	"alloc_chrdev_region\0"
	"cdev_init\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "22313F59608CEA220537D94");
