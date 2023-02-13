#include "RenderAPI.h"

static void WindowResizeCallback(GLFWwindow* window, int width, int height)
{
    // 通过刚刚设置的this指针获取到我们的demo实例
    auto api = reinterpret_cast<RenderAPI*>(glfwGetWindowUserPointer(window));
    api->windowResized = true;
}

RenderAPI* RenderAPI::mInstance = nullptr;

void RenderAPI::Creat()
{
	mInstance = new RenderAPI();
}

RenderAPI* RenderAPI::GetInstance()
{
	return mInstance;
}

RenderAPI::RenderAPI()
{
    glfwInit();

    // 最初GLFW是为OpenGL创建上下文，所以在这里我们需要告诉它不要调用OpenGL相关的初始化操作
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(400, 400, "Vulkan", nullptr, nullptr);
    // 调用这个接口传入一个自定义指针给window，方便回调里面获取this
    glfwSetWindowUserPointer(window, this);
    // 设置窗口大小变化的回调
    glfwSetFramebufferSizeCallback(window, WindowResizeCallback);

    CreateVkInstance();
    CreateDebugMessenger();
    CreatePhysicalDevice();
    CreateLogicalDevice();
    CreateMemoryAllocator();
    CreateSurface();
    CreateSwapChain();
}
