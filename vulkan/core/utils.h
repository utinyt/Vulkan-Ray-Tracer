#pragma once
#include <stdexcept>
#include <iostream>
#include <vulkan/vulkan.h>

#define VK_CHECK_RESULT(func){									\
	if (func != VK_SUCCESS) {									\
		std::cerr << __FILE__ << ", line " << __LINE__ << ": ";	\
		std::string str = #func;								\
		throw std::runtime_error(str + " call has been failed")	\
	}															\
}
