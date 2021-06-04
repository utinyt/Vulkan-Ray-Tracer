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

/*
* read binary file
* 
* @param filename - name of the file
*/
inline std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("failed to open file: " + filename);
	}

	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}