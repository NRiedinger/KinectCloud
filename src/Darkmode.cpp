#include "Darkmode.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw/glfw3native.h>

fnSetWindowCompositionAttribute _SetWindowCompositionAttribute = reinterpret_cast<fnSetWindowCompositionAttribute>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));

void set_darkmode(GLFWwindow* window)
{
	BOOL dark = TRUE;
	WINDOWCOMPOSITIONATTRIBDATA data = { WCA_USEDARKMODECOLORS, &dark, sizeof(dark) };
	auto hwnd = glfwGetWin32Window(window);
	_SetWindowCompositionAttribute(hwnd, &data);
}
