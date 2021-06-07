#pragma once
#include <stdexcept>
#include <iostream>
#include <vector>
#include <fstream>
#include <vulkan/vulkan.h>

/*
* checks vulkan function result and throws runtime error if it is failed
* 
* @param func - function call to check
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
* 
* @param str - string to print
*/
#define LOG(str){					\
	std::cerr << str << std::endl;	\
}

namespace vktools {
	/** @brief read binary file and store to a char vector */
	std::vector<char> readFile(const std::string& filename);
}
