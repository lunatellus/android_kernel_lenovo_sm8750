#include <linux/input.h>
#include <linux/of_gpio.h>

#ifndef TOOL_TYPE_STYLUS
#define TOOL_TYPE_STYLUS KEY_NUMERIC_POUND
#endif

#define of_get_named_gpio_flags(np, name, index, flags) \
	((*(flags) = 0), of_get_named_gpio((np), (name), (index)))
#define class_create(module, name) class_create(name)
