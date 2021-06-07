#include "vulkan_utils.h"

namespace vktools {
	/*
	* read binary file
	*
	* @param filename - name of the file
	*
	* @return raw binary data stored in a vector
	*/
	std::vector<char> readFile(const std::string& filename) {
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
}
