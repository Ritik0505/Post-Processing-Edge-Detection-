#if !defined(STB_IMAGE_IMPLEMENTATION)
#define STB_IMAGE_IMPLEMENTATION
#endif

#include "VulkanBase.h"
#include "PostProcess.h"

int main() {

	PostProcess p;

	while (!glfwWindowShouldClose(p.ReturnWindowHandle())) {
		glfwPollEvents();
		p.Draw();
	}
	return 0;
}