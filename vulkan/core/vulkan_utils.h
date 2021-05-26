#pragma once
#include <stdexcept>
#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>

/*
* checks vulkan function result and throws runtime error if it is failed
*/
#define VK_CHECK_RESULT(func){									\
	if (func != VK_SUCCESS) {									\
		std::cerr << __FILE__ << ", line " << __LINE__ << ": ";	\
		std::string str = #func;								\
		throw std::runtime_error(str + " call has been failed");\
	}															\
}

/*
* simple stderr print
*/
#define LOG(str){					\
	std::cerr << str << std::endl;	\
}
