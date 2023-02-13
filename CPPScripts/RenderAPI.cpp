#include "RenderAPI.h"

static void WindowResizeCallback(GLFWwindow* window, int width, int height)
{
    // ͨ���ո����õ�thisָ���ȡ�����ǵ�demoʵ��
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

    // ���GLFW��ΪOpenGL���������ģ�����������������Ҫ��������Ҫ����OpenGL��صĳ�ʼ������
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(400, 400, "Vulkan", nullptr, nullptr);
    // ��������ӿڴ���һ���Զ���ָ���window������ص������ȡthis
    glfwSetWindowUserPointer(window, this);
    // ���ô��ڴ�С�仯�Ļص�
    glfwSetFramebufferSizeCallback(window, WindowResizeCallback);

    CreateVkInstance();
    CreateDebugMessenger();
    CreatePhysicalDevice();
    CreateLogicalDevice();
    CreateMemoryAllocator();
    CreateSurface();
    CreateSwapChain();
}
