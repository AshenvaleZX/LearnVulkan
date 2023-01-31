#include <iostream>
#include <vector>
#include <set>
#include <fstream>
#include <string>
using std::string;

std::ofstream ofs;
void PrepareLog() {
    // 写个空字符串清除一下上一次运行的日志
    ofs.open("../../log.txt", std::ios::out);
    ofs << "";
    ofs.close();
    // 重新以追加写入模式打开
    ofs.open("../../log.txt", std::ios::out | std::ios::app);
}
void Log(string msg) {
    ofs << msg << std::endl;
}
void EndLog() {
    ofs.close();
}

// 读二进制文件
static std::vector<char> readFile(const std::string& filename) {
    // ate:在文件末尾开始读取，从文件末尾开始读取的优点是我们可以使用读取位置来确定文件的大小并分配缓冲区
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("failed to open file!");
    
    // 使用读取位置来确定文件的大小并分配缓冲区
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    // 返回文件开头，真正读取内容
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    // 结束，返回数据
    file.close();
    return buffer;
}

// 用GLFW的话这里就不要自己去include Vulkan的头文件，用这个宏定义，让GLFW自己去处理，不然有些接口有问题
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

using namespace glm;

const int WIDTH = 800;
const int HEIGHT = 600;

// NDEBUG是C++标准宏定义，代表“不调试”，会根据VS编译时选择的Debug或Release模式来确认是否定义
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// Vulkan SDK通过请求VK_LAYER_KHRONOS_validation层，来隐式的开启有所关于诊断的layers，从而避免明确的指定所有的明确的诊断层
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    // 交换链扩展名，这个的支持也就代表了是否支持将图像绘制到显示器上(不是所有GPU都可以拿来绘图)
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Vulkan的Debug是扩展功能，所以vkCreateDebugUtilsMessengerEXT并不会自动加载
// 想调用它需要通过vkGetInstanceProcAddr手动获取函数地址
// 这个函数就是封装了一下获取vkCreateDebugUtilsMessengerEXT地址并调用它的过程
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

// 如果用上面的函数创建了VkDebugUtilsMessengerEXT变量，需要手动调用vkDestroyDebugUtilsMessengerEXT清理
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

struct QueueFamilyIndices {
    int graphicsFamily = -1;
    int presentFamily = -1;

    bool isComplete() {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

// 交换链的三大类属性设置
struct SwapChainSupportDetails {
    // 基本的surface功能属性(min/max number of images in swap chain, min/max width and height of images)
    VkSurfaceCapabilitiesKHR capabilities;
    // Surface格式(pixel format, color space)
    std::vector<VkSurfaceFormatKHR> formats;
    // 有效的presentation模式
    std::vector<VkPresentModeKHR> presentModes;
};

class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    // 物理设备
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    // 逻辑设备
    VkDevice device;
    // surface是Vulkan与窗体系统的连接桥梁，把画面渲染到窗口上需要这个
    VkSurfaceKHR surface;
    // 设备队列在设备被销毁的时候隐式清理，所以我们不需要在cleanup函数中做任何操作
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    // 显示图像的交换链
    VkSwapchainKHR swapChain;
    // 交换链的图像,图像被交换链创建,也会在交换链销毁的同时自动清理,所以我们不需要添加任何清理代码
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    // 使用任何的VkImage，包括在交换链或者渲染管线中的，我们都需要创建VkImageView对象
    // 从字面上理解它就是一个针对图像的视图或容器，通过它具体的渲染管线才能够读写渲染数据，换句话说VkImage不能与渲染管线进行交互
    // 这个不同于VkImage，是手动创建的，所以需要手动销毁
    std::vector<VkImageView> swapChainImageViews;
    // 给交换链中每一个VkImage建立一个帧缓冲区
    std::vector<VkFramebuffer> swapChainFramebuffers;
    // VkCommandPool是用来管理存储command buffer的内存和分配command buffer的
    VkCommandPool commandPool;
    // 从CommandPool里分配的，CommandPool销毁的时候会自动销毁
    VkCommandBuffer commandBuffer;
    
    VkRenderPass renderPass;
    // 这个就是glsl里面在开头写的那个layout
    VkPipelineLayout pipelineLayout;
    // 用于渲染图形的管线(做游戏基本上都是图形管线，不过Vulkan是设计成可以完成其他各种需要GPU的工作的)
    VkPipeline graphicsPipeline;

    // 已从交换链获取图形并且可以用于渲染的同步信号
    VkSemaphore imageAvailableSemaphore;
    // 渲染已完成的同步信号
    VkSemaphore renderFinishedSemaphore;
    // 一次只渲染一帧画面的同步信号
    VkFence inFlightFence;

    void initWindow() {
        glfwInit();

        // 最初GLFW是为OpenGL创建上下文，所以在这里我们需要告诉它不要调用OpenGL相关的初始化操作
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        // 在系统中查找并选择支持我们所需功能的显卡
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffer();
        createSyncObjects();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }

        // 因为drawFrame函数里调用的绘制图形的接口，很多都是异步执行的，所以while循环结束后，可能那些代码还在执行
        // 这个时候如果直接结束主循环，开始调用cleanup销毁各种Vulkan对象，很有可能销毁到正在使用的对象，就会Crash
        // 所以调用这个接口等待逻辑设备上的所有command队列执行完毕，再结束主循环
        vkDeviceWaitIdle(device);
    }

    void cleanup() {
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        }
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createInstance() {
        // 如果开启了验证层，那么我们在validationLayers里自定义的所需验证层需要全部被支持
        if (enableValidationLayers && !checkValidationLayerSupport())
            throw std::runtime_error("Validation layers requested, but not available!\nPlease check whether your SDK is installed correctly.");

        // 这些数据从技术角度是可选择的，但是它可以为驱动程序提供一些有用的信息来优化程序特殊的使用情景，比如驱动程序使用一些图形引擎的特殊行为。
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        // 指向特定的扩展结构，我们在这里使用默认初始化，将其设置为nullptr
        appInfo.pNext = nullptr;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // 获取与不同平台的窗体系统进行交互的扩展信息
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // 如果开启了Debug，创建一个Debug信息
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (enableValidationLayers) {
            // 填充当前上下文已经开启的validation layers名称集合
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            // 这搞一个debugInfo干嘛？？？
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        // 这里实际创建一个VkInstance，前面那些代码都是在给这个函数准备参数
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    // 检查我们在validationLayers里自定义的所需验证层是否全部被支持
    bool checkValidationLayerSupport() {
        // 获取所有可用的Layer数量
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        // 获取所有可用Layer的属性
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // 检查我们的validationLayers中的所有layer是否存在于availableLayers列表中
        for (const char* layerName : validationLayers) {
            bool layerFound = false;
            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    std::vector<const char*> getRequiredExtensions() {
        // Vulakn对于平台特性是零API支持的(至少暂时这样)，这意味着需要一个扩展才能与不同平台的窗体系统进行交互
        // GLFW有一个方便的内置函数，返回它有关的扩展信息
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions;
        // 添加GLFW获取的扩展
        for (unsigned int i = 0; i < glfwExtensionCount; i++) {
            extensions.push_back(glfwExtensions[i]);
        }
        // 如果开启了Debug，添加Debug的扩展
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    // 自定义的Debug回调函数，VKAPI_ATTR和VKAPI_CALL确保了正确的函数签名，从而被Vulkan调用
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        std::cerr << "My debug call back: " << pCallbackData->pMessage << std::endl;
        Log(pCallbackData->pMessage);
        // 返回一个布尔值，表明触发validation layer消息的Vulkan调用是否应被中止
        // 如果返回true，则调用将以VK_ERROR_VALIDATION_FAILED_EXT错误中止
        // 这通常用于测试validation layers本身，所以我们总是返回VK_FALSE
        return VK_FALSE;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        // 这个结构体是传递给vkCreateDebugUtilsMessengerEXT函数创建VkDebugUtilsMessengerEXT对象的（就是我们的debugMessenger变量）
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        // 调用我们自己封装vkCreateDebugUtilsMessengerEXT函数的函数，传递上面创建的结构体作为参数，用于创建VkDebugUtilsMessengerEXT对象
        // 也就是我们的debugMessenger变量
        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void createSurface() {
        // surface的具体创建过程是要区分平台的，这里直接用GLFW封装好的接口来创建
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void pickPhysicalDevice() {
        // 获取当前设备支持Vulkan的显卡数量
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        // 获取所有支持Vulkan的显卡，存放到devices里
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // 遍历所有显卡，找到第一个符合我们需求的
        // 其实可以写个多线程同时调用这些显卡，不过现在先随便用一个
        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    bool isDeviceSuitable(VkPhysicalDevice device) {
        // 获得name, type以及Vulkan版本等基本的设备属性
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        // 需要独显
        if (deviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            return false;

        // 查询对纹理压缩，64位浮点数和多视图渲染(VR非常有用)等可选功能的支持
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        // 需要支持几何着色器
        if (!deviceFeatures.geometryShader)
            return false;

        // 查询符合条件的队列簇
        QueueFamilyIndices indices = findQueueFamilies(device);
        if (!indices.isComplete())
            return false;

        // 检查是否支持所需要的扩展
        if (!checkDeviceExtensionSupport(device))
            return false;

        // 检查交换链是否完整
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        // 至少支持一个图像格式和一个Present模式
        bool swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        if (!swapChainAdequate)
            return false;

        // 如果有很多显卡，可以通过给各种硬件特性一个权重，然后优先选择最合适的
        return true;
    }

    // 检查是否支持所需要的扩展
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        // 获取物理设备支持的扩展数量
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        // 获取所支持的具体信息
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        // 把我们自定义的deviceExtensions转换成set数据结构(应该是为了避免后面2层for循环的erase，同时也不改动原数据)
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        // 遍历物理设备所支持的扩展，逐个从我们需要的扩展集合中删除
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        // 如果全部删除完了，说明我们所需要的扩展都是支持的，否则说明还有一些我们需要的扩展不被支持
        return requiredExtensions.empty();
    }

    // 几乎所有的Vulkan操作，从绘图到上传纹理，都需要将命令提交到队列中
    // 有不同类型的队列来源于不同的队列簇，每个队列簇只允许部分commands
    // 例如，可以有一个队列簇，只允许处理计算commands或者只允许内存传输commands
    // 我们需要检测设备中支持的队列簇，其中哪一个队列簇支持我们想要的commands
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        // 获取设备拥有的队列簇数量
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        // 获取所有队列簇获的属性
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        // 遍历队列簇，获取支持我们需求的队列簇
        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // 当前队列簇是否支持图形处理
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            // 是否支持VkSurfaceKHR
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            // 支持的话记录一下索引
            if (queueFamily.queueCount > 0 && presentSupport) {
                indices.presentFamily = i;
            }

            // 注意这里支持surface和graphic的队列簇不一定是同一个
            // 后续使用这些队列簇的逻辑，要么固定按照支持surface和graphic的队列簇是两个不同的来处理(这样无论是不是同一个都不会出错)
            // 要么这里查找队列簇的时候，明确指定必须同时支持surface和graphic，然后保存同时支持这两个要求的队列簇索引(性能可能会好点)

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        // 现在只返回具备图形能力的队列
        return indices;
    }

    void createLogicalDevice() {
        // 获取当前物理设备的队列簇
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        // VkDeviceQueueCreateInfo是创建逻辑设备需要在结构体中明确具体的信息
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

        float queuePriority = 1.0f;
        // 有多个队列，遍历创建
        for (int queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            // 当前队列簇对应的索引
            queueCreateInfo.queueFamilyIndex = queueFamily;
            // 队列簇中需要申请使用的队列数量
            queueCreateInfo.queueCount = 1;
            // Vulkan允许使用0.0到1.0之间的浮点数分配队列优先级来影响命令缓冲区执行的调用，即使只有一个队列也是必须的
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // 明确设备要使用的功能特性，暂时不需要任何特殊的功能
        VkPhysicalDeviceFeatures deviceFeatures = {};

        // 创建逻辑设备的信息
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        // 使用前面的两个结构体填充
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;

        // 和VkInstance一样，VkDevice可以开启扩展和验证层
        // 添加扩展信息
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        // 如果启用了验证层，把验证层信息添加进去
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        // 调用vkCreateDevice函数来创建实例化逻辑设备
        // 逻辑设备不与VkInstance直接交互，所以参数里只有物理设备
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        // 逻辑设备创建的时候，队列也一起创建了，获取队列并保存下来方便之后调用
        // 参数是逻辑设备，队列簇，队列索引和存储获取队列变量句柄的指针
        // 因为我们只是从这个队列簇创建一个队列，所以需要使用索引0
        vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
    }

    void createSwapChain() {
        // 查询硬件支持的交换链设置
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        // 选择一个图像格式
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        // 选择一个present模式(就是图像交换的模式)
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        // 选择一个合适的图像分辨率
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // 交换链中的图像数量，可以理解为交换队列的长度。它指定运行时图像的最小数量，我们将尝试大于1的图像数量，以实现三重缓冲。
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        // 如果maxImageCount等于0，就表示没有限制。如果大于0就表示有限制，那么我们最大只能设置到maxImageCount
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        // 创建交换链的结构体
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        // 绑定我们的surface
        createInfo.surface = surface;

        // 把前面获取的数据填上
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.presentMode = presentMode;
        createInfo.imageExtent = extent;

        // imageArrayLayers指定每个图像组成的层数，除非我们开发3D应用程序，否则始终为1
        createInfo.imageArrayLayers = 1;
        // 这个字段指定在交换链中对图像进行的具体操作
        // 在本小节中，我们将直接对它们进行渲染，这意味着它们作为颜色附件
        // 也可以首先将图像渲染为单独的图像，进行后处理操作
        // 在这种情况下可以使用像VK_IMAGE_USAGE_TRANSFER_DST_BIT这样的值，并使用内存操作将渲染的图像传输到交换链图像队列
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // 接下来，我们需要指定如何处理跨多个队列簇的交换链图像
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };
        // 如果graphics队列簇与presentation队列簇不同，会出现如下情形
        // 我们将从graphics队列中绘制交换链的图像，然后在另一个presentation队列中提交他们
        // 多队列处理图像有两种方法
        // VK_SHARING_MODE_EXCLUSIVE: 同一时间图像只能被一个队列簇占用，如果其他队列簇需要其所有权需要明确指定，这种方式提供了最好的性能
        // VK_SHARING_MODE_CONCURRENT: 图像可以被多个队列簇访问，不需要明确所有权从属关系
        // 如果队列簇不同，暂时使用concurrent模式，避免处理图像所有权从属关系的内容，因为这些会涉及不少概念
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            // Concurrent模式需要预先指定队列簇所有权从属关系，通过queueFamilyIndexCount和pQueueFamilyIndices参数进行共享
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        // 如果graphics队列簇和presentation队列簇相同，我们需要使用exclusive模式，因为concurrent模式需要至少两个不同的队列簇
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // 如果交换链支持(supportedTransforms in capabilities)，我们可以为交换链图像指定某些转换逻辑
        // 比如90度顺时针旋转或者水平反转。如果不需要任何transoform操作，可以简单的设置为currentTransoform
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

        // compositeAlpha字段指定alpha通道是否应用于与其他的窗体系统进行混合操作。如果忽略该功能，简单的填VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        // 如果clipped成员设置为VK_TRUE，意味着我们不关心被遮蔽的像素数据，比如由于其他的窗体置于前方时或者渲染的部分内容存在于可是区域之外
        // 除非真的需要读取这些像素获数据进行处理，否则可以开启裁剪获得最佳性能
        createInfo.clipped = VK_TRUE;

        // Vulkan运行时，交换链可能在某些条件下被替换，比如窗口调整大小或者交换链需要重新分配更大的图像队列
        // 在这种情况下，交换链实际上需要重新分配创建，并且必须在此字段中指定对旧的引用，用以回收资源
        // 这是一个比较复杂的话题，我们会在后面的章节中详细介绍。现在假设我们只会创建一个交换链。
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        // 最终创建交换链
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // 获取一下交换链里图像的数量，这个imageCount变量最开始是我们期望的图像数量，但是实际创建的不一定是这么多，所以要重新获取一下
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        // 根据实际图像数量重新设置vector大小
        swapChainImages.resize(imageCount);
        // 最后，存储交换链格式和范围到成员变量swapChainImages中
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        // 这个函数需要VkPhysicalDevice和VkSurfaceKHR窗体surface决定支持哪些具体功能
        // 所有用于查看支持功能的函数都需要这两个参数，因为它们是交换链的核心组件
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        // 查询支持的surface格式，获取到的是一个结构体列表
        // 先获取支持的格式数量
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            // 重新设置vector大小
            details.formats.resize(formatCount);
            // 填充格式数据
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        // 查询支持的surface模式，获取到的是一个结构体列表
        // 先获取支持的模式数量
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            // 重新设置vector大小
            details.presentModes.resize(presentModeCount);
            // 填充模式数据
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        // 这是最理想的情况，surface没有设置任何偏向性的格式
        // 这个时候Vulkan会通过仅返回一个VkSurfaceFormatKHR结构来表示，且该结构的format成员设置为VK_FORMAT_UNDEFINED
        // 此时我们可以自由的设置格式
        if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
            return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        }

        // 如果不能自由的设置格式，那么我们可以通过遍历列表设置具有偏向性的组合
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        // 如果以上两种方式都失效了，我们直接选择第一个格式
        // 其实这个时候我们也可以遍历一下availableFormats，自己写一个规则挑一个相对较好的出来
        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
        // 在Vulkan中有四个模式可以使用:
        // 1，VK_PRESENT_MODE_IMMEDIATE_KHR
        // 应用程序提交的图像被立即传输到屏幕呈现，这种模式可能会造成撕裂效果。
        // 2，VK_PRESENT_MODE_FIFO_KHR
        // 交换链被看作一个队列，当显示内容需要刷新的时候，显示设备从队列的前面获取图像，并且程序将渲染完成的图像插入队列的后面。如果队列是满的程序会等待。这种规模与视频游戏的垂直同步很类似。显示设备的刷新时刻被称为“垂直中断”。
        // 3，VK_PRESENT_MODE_FIFO_RELAXED_KHR
        // 该模式与上一个模式略有不同的地方为，如果应用程序存在延迟，即接受最后一个垂直同步信号时队列空了，将不会等待下一个垂直同步信号，而是将图像直接传送。这样做可能导致可见的撕裂效果。
        // 4，VK_PRESENT_MODE_MAILBOX_KHR
        // 这是第二种模式的变种。当交换链队列满的时候，选择新的替换旧的图像，从而替代阻塞应用程序的情形。这种模式通常用来实现三重缓冲区，与标准的垂直同步双缓冲相比，它可以有效避免延迟带来的撕裂效果。
        
        // 默认的模式
        VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
            else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                bestMode = availablePresentMode;
            }
        }

        return bestMode;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        // SwapExtent是指交换链图像的分辨率

        // currentExtent的高宽如果是一个特殊的uint32最大值，就直接返回？？？
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else {
            VkExtent2D actualExtent = { WIDTH, HEIGHT };

            // 收敛到minImageExtent和maxImageExtent之间
            actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }

    void createImageViews() {
        // size和swapChainImages一致
        swapChainImageViews.resize(swapChainImages.size());

        // 循环遍历swapChainImages来创建swapChainImageViews
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];

            // 可以是1D，2D，3D或者CubeMap纹理
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            // 用之前创建swapChainImages时候的格式
            createInfo.format = swapChainImageFormat;

            // components字段允许调整颜色通道的最终的映射逻辑
            // 比如，我们可以将所有颜色通道映射为红色通道，以实现单色纹理，我们也可以将通道映射具体的常量数值0和1
            // 这里用默认的
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            // subresourceRangle字段用于描述图像的使用目标是什么，以及可以被访问的有效区域
            // 这个图像用作填充color(可以是深度，stencil等)
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            // 没有mipmap
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            // 没有multiple layer (如果在编写沉浸式的3D应用程序，比如VR，就需要创建支持多层的交换链。并且通过不同的层为每一个图像创建多个视图，以满足不同层的图像在左右眼渲染时对视图的需要)
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            // 创建一个VkImageView
            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    void createRenderPass() {
        // 创建一个color buffer
        // 实际上这个VkAttachmentDescription可以是任何buffer的创建，只是这里我们创建的是一个单纯的color buffer
        VkAttachmentDescription colorAttachment = {};
        // 和交换链里的图片格式保持一致
        colorAttachment.format = swapChainImageFormat;
        // 不启用多重采样的话这里就是1bit
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // loadOp是渲染前的操作
        // VK_ATTACHMENT_LOAD_OP_LOAD: 保存已经存在于当前buffer的内容
        // VK_ATTACHMENT_LOAD_OP_CLEAR: 起始阶段以一个常量清理内容
        // VK_ATTACHMENT_LOAD_OP_DONT_CARE: 存在的内容未定义，忽略它们
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // storeOp是渲染之后怎么处理
        // VK_ATTACHMENT_STORE_OP_STORE: 渲染内容存储下来，并在之后进行读取操作
        // VK_ATTACHMENT_STORE_OP_DONT_CARE: 帧缓冲区的内容在渲染操作完毕后设置为未定义(不是很懂这个，可能是搞一些骚操作的吧)
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // 上面那个设置是用于color和depth的，stencil的单独一个
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // Vulkan里的textures和framebuffers都是用一个确定的format的VkImage来表示，但是layout是可以动态改的
        // 这个layout主要是影响内存的读写方式，可以根据你具体要做什么来选择合适的layout
        // 有3种:
        // VK_IMAGE_LAYOUT_COLOR_ATTACHMET_OPTIMAL: 图像作为颜色附件
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: 图像在交换链中被呈现
        // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : 图像作为目标，用于内存COPY操作
        // initialLayout指的是在进入当前这个render pass之前的layout，这里我们不关心，就设置为UNDEFINED
        // 但是这里如果设置为UNDEFINED，是不能保证数据可以保留下来的，不过本来我们loadOp也设置成了clear，所以无所谓
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // finalLayout指的是，当前render pass处理完了之后，会把数据设置为什么layout
        // 这里设置为PRESENT_SRC_KHR是因为我们准备直接把数据给交换链
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


        // 一个单独的渲染pass可以由多个subpass组成。subpass是一系列取决于前一个pass输出结果的渲染操作序列
        // 比如说后处理效果的序列通常每一步都依赖之前的操作
        // 如果将这些渲染操作分组到一个渲染pass中，则Vulkan能够重新排序这些操作并节省内存带宽，以获得可能更好的性能
        // 对于我们要绘制的三角形，我们只需要一个subpass

        // 每个subpass都需要引用一个或多个attachment，这些attachment是我们之前用的的结构体描述的
        // 这些引用本身是用VkAttachmentReference来表示的
        VkAttachmentReference colorAttachmentRef{};
        // 引用的attachment在attachment descriptions array里的索引，我们只有一个VkAttachmentDescription，所以用0
        colorAttachmentRef.attachment = 0;
        // 这个指定了当subpass开始的时候，我们引用的attachment会被Vulkan自动转换成哪种layout
        // 因为我们这个attachment是用作color buffer的，所以我们用这个COLOR_ATTACHMENT_OPTIMAL来达到最佳性能
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // 描述subpass的结构体
        VkSubpassDescription subpass{};
        // 做图形渲染这里就用这个，因为Vulkan也会支持一些非图形的工作
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        // 指定color buffer的引用，这个应用以及它的索引(上面的attachment = 0)现在直接对应到片元着色器里的layout(location = 0) out vec4 outColor了
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.colorAttachmentCount = 1;
        // 除了colorAttachment，还有这些可以引用
        // pInputAttachments: 从shader中读取
        // pResolveAttachments: 用于颜色附件的多重采样
        // pDepthStencilAttachment: 用于depth和stencil数据
        // pPreserveAttachments: 不被这个subpass使用，但是数据要保存

        // 创建render pass的信息
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        // 前面创建VkAttachmentReference的时候，那个索引attachment指的就是在这个pAttachments数组里的索引
        // 这里是可以传数组的，不过我们只创建了一个，所以也只传了一个
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        // 定义Subpass之间的依赖关系
        VkSubpassDependency dependency{};
        // 定义当前这个依赖是从srcSubpass到dstSubpass的，这里填pSubpasses数组的索引
        // 换句话说，当前定义的这个依赖关系，就是当pSubpasses[srcSubpass]结束，要进入pSubpasses[dstSubpass]时候的
        // 如果填VK_SUBPASS_EXTERNAL，就代表是当前这个Pass外部的
        // 比如这里srcSubpass填VK_SUBPASS_EXTERNAL，dstSubpass填0，就表示这是进入当前Pass的第一个Subpass时，需要的一些依赖关系
        // 注意Render Pass里的Subpass是按照pSubpasses数组的顺序执行的，所以这里的srcSubpass应该小于dstSubpass(如果是VK_SUBPASS_EXTERNAL就无所谓)
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;

        // 这里定义这个依赖关系主要是解决image的layout转换问题
        // 我们在创建Render Pass的VkRenderPassCreateInfo的pAttachments参数里，传入了这个Render Pass会用到的所有attachment
        // Subpass会通过VkAttachmentReference来引用这些attachment
        // 而这些attachment是设置了初始和最终layout的，即initialLayout和finalLayout
        // 
        // 而Subpass引用Render Pass的attachment的时候，定义了一个layout，即VkAttachmentReference里的layout变量
        // 这个layout代表当前Subpass开始的时候，将会自动把这个image的layout转换成什么格式
        // 如果相邻两个Subpass没有使用同一个attachment，那么它们之间不存在layout转换问题
        // 如果相邻两个Subpass使用了同一个attachment，那么这个attachment在前一个Subpass使用完之后，下一个Subpass使用之前，需要有一个layout的转换过程
        // 这个转换是Vulkan根据我们在Subpass的attachment引用种设置的layout，自动转换的，不需要我们手动转换
        // 但是我们需要明确定义这个转换发生的时机，这个是出于Vulkan希望尽量做到并发执行，让GPU不要闲置的设计理念，具体如下:
        // 假如我们没有并发，完全按照线性流程执行这些Subpass，那么问题就很简单了，attachment在第一个Subpass执行完之后，改变layout，执行下一个Subpass
        // 但是这个layout实际上只影响image的读写，而image是在整个渲染流程的后期才会用到，前面的vertex shader等阶段的执行和image无关
        // 也就是说，我们可以在第一个Subpass还没执行完的时候，就先开始并发执行第二个Subpass
        // 但是同一个attachment肯定不允许多个Subpass同时读写，也就是说虽然第二个Subpass可以不用等第一个Subpass完全结束了再开始执行
        // 但是第二个Subpass如果需要用到和第一个Subpass相同的attachment时，还是需要等待第一个Subpass用完这个attachment，第二个Subpass才能对其进行访问
        // 所以定义这个依赖关系，要解决的就是问题就是:
        // 1，前一个Subpass到底什么时候用完这个attachment
        // 2，下一个Subpass到底什么时候开始需要使用这个attachment
        // 那么一个attachment的layout的转换，就会发生在前一个Subpass使用完它之后，下一个Subpass需要使用它之前，我们就需要明确定义这两个时机具体是什么
        // 如果前一个Subpass还在使用中，下一个Subpass就已经进行到需要使用它的阶段了，那么下一个Subpass的执行就会进入等待
        // 如果下一个Subpass执行到了需要使用相同attachment的时候，前一个Subpass已经使用完了(或者不会使用相同attachment)，那么就不会出现等待，而是直接并发执行
        // 
        // 前面说了Render Pass的attachment是设置了初始和最终layout的，即initialLayout和finalLayout
        // 那么Subpass之间的layout转换关系大致可以分为3类:
        // 1，External到Subpass
        // 2，Subpass至External
        // 3，Subpass之间
        // 分别对应3种layout转换关系:
        // 1，initialLayout和首个使用该attachment的subpass所需layout不一致
        // 2，finalLayout和最后使用该attachment的subpass所需layout不一致
        // 3，两个subpass读取同一个attachement而且所需layout不同，且驱动实现上区分对待两种layout

        // 这里设置依赖关系中，需要等待上一个Subpass具体完成什么步骤
        // 即等待srcStageMask阶段的srcAccessMask操作完成后
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        // 等于0好像是需要等待这个阶段所有操作完全结束？
        dependency.srcAccessMask = 0;

        // 这里设置依赖关系中，下一个Subpass具体在什么步骤才开始需要等待上一个Subpass设置的步骤结束
        // 即在dstStageMask阶段的dstAccessMask步骤执行之前，需要完成前面srcXXX设置的具体步骤
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // 填充依赖关系数组
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("../../Shader/triangle.vert.spv");
        auto fragShaderCode = readFile("../../Shader/triangle.frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        // 指定为顶点着色器
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        // 指定包含代码的着色器模块和shader里的主函数名
        // 可以将多个片段着色器组合到单个着色器模块中，并使用不同的入口点来区分它们的行为
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
        // 这个可以给着色器指定常量值。使用单个着色器模块，通过为其中使用不同的常量值，可以在流水线创建时对行为进行配置
        // 这比在渲染时使用变量配置着色器更有效率，因为编译器可以进行优化，例如消除if值判断的语句
        vertShaderStageInfo.pSpecializationInfo = nullptr;

        // 片元着色器，和上面的一样，就不逐一解释了
        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // 结构体描述了顶点数据的格式，该结构体数据传递到vertex shader中
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        // Bindings:根据数据的间隙，确定数据是每个顶点或者是每个instance(instancing)
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        // Attribute:描述将要进行绑定及加载属性的顶点着色器中的相关属性类型
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

        // 这个结构体描述两件事情:顶点数据以什么类型的几何图元拓扑进行绘制及是否启用顶点索重新开始图元
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        // VK_PRIMITIVE_TOPOLOGY_POINT_LIST: 点图元
        // VK_PRIMITIVE_TOPOLOGY_LINE_LIST: 线图元，顶点不共用
        // VK_PRIMITIVE_TOPOLOGY_LINE_STRIP : 线图元，每个线段的结束顶点作为下一个线段的开始顶点
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : 三角形图元，顶点不共用
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : 三角形图元，每个三角形的第二个、第三个顶点都作为下一个三角形的前两个顶点
        // 感觉LIST就是OpenGL里的DrawArray，STRIP就是DrawElement
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // 这个是用于_STRIP类型的图元，这个设置为VK_TRUE后，可以用0xFFFF或者0xFFFFFFFF当作element buffer里的下标来断开和下一个图元顶点的重复使用
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapChainExtent.width;
        viewport.height = (float)swapChainExtent.height;
        // 手动指定深度范围，但是需要收敛到0-1之间，教程里说minDepth may be higher than maxDepth? 
        // 不是很懂，反正一般就设置为0和1就行
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // 裁剪区域，一般来说不做任何裁剪的话，offset和高宽(也就是extent)和viewport的保持一致就行
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;

        // viewport和scissor也可以不在创建Pipeline的时候确定，而是作为一个动态的设置在后续绘制的时候指定
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // VkViewport和scissor信息一起填充到这里
        // 这个填充了之后viewport和scissor相当于变成固定管线的一部分了，也可以弄成动态的放到command buffer里去设置
        // 见:https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions#page_Viewports-and-scissors
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        // 如果要弄成动态的，这里就不设置，如果这里直接设置了，那这个pipeline的viewport和scissor之后就不能再改了
        // viewportState.pViewports = &viewport;
        // viewportState.pScissors = &scissor;

        // 光栅化阶段信息
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        // 如果depthClampEnable设置为VK_TRUE，超过远近裁剪面的片元会进行收敛，而不是丢弃它们
        // 它在特殊的情况下比较有用，像阴影贴图。使用该功能需要得到GPU的支持
        rasterizer.depthClampEnable = VK_FALSE;
        // 如果rasterizerDiscardEnable设置为VK_TRUE，那么几何图元永远不会传递到光栅化阶段
        // 这是禁止任何数据输出到framebuffer的方法
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        // 这个指定片元如何从几何模型中产生，一般正常情况都是FILL，如果不是的话，需要开启GPU feature
        // VK_POLYGON_MODE_FILL: 多边形区域填充
        // VK_POLYGON_MODE_LINE: 多边形边缘线框绘制
        // VK_POLYGON_MODE_POINT : 多边形顶点作为描点绘制
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        // 这个变量的意思是，用多少片元的数量来描述线的宽度，一般都是1，如果要大于1，需要开启wideLines这个GPU feature
        rasterizer.lineWidth = 1.0f;
        // 这个就是FaceCull，这里用背面裁剪
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        // 设置怎么判断正面
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // 这些设置一般是渲染阴影贴图用的，暂时用不到
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional
        // 设置多重采样
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional
        // 配置色彩混合方式
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        // 这个也是设置色彩回合的一种方式，并且是一个全局的设置，如果这个开了，上面的那个设置就会失效，相当于上面的blendEnable = VK_FALSE
        // 这个还不是很懂，先不用这个
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // shader开头写的那个layout，这里暂时不设置
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
        // 创建PipelineLayout，并保存到成员变量，后续会用
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }


        // 终于开始用上面一大堆东西构建pipeLineInfo了......
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        // 因为我们只写了vert和frag shader，所以是2个shader stage
        pipelineInfo.stageCount = 2;
        // 传入具体的shader
        pipelineInfo.pStages = shaderStages;
        // 下面这堆是前面写的各种固定管线配置信息
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        // layout
        pipelineInfo.layout = pipelineLayout;
        // 这个管线用的renderPass
        // 我们这里因为只有一个管线和一个pass，所以就是一一对应的，其实可以创建一个pass然后用到很多个管线上
        // 只不过需要这些pass满足一些兼容需求:https://registry.khronos.org/vulkan/specs/1.3-extensions/html/chap8.html#renderpass-compatibility
        pipelineInfo.renderPass = renderPass;
        // 具体用的哪个subpass
        pipelineInfo.subpass = 0;

        // 这两个参数是可以实现通过集成另一个已有的管线，来创建当前管线
        // 如果两个管线之间的差别比较小，就可以通过这样继承来创建，并且切换的时候也比较高效
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional
        // 如果要用上面两个参数来指定继承的话，需要这样设置这个flags
        // pipelineInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;

        // 创建渲染管线，这个函数可以接收VkGraphicsPipelineCreateInfo数组然后一次性创建多个管线
        // 第二个参数是VkPipelineCache，这个可以存储管线创建的数据，然后在多次vkCreateGraphicsPipelines调用中使用
        // 甚至可以存到文件里，在不同的Vulkan程序里使用
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        // 管线创建好后就没用了，所以立刻删除
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    // VkShaderModule对象只是字节码缓冲区的一个包装容器。着色器并没有彼此链接，甚至没有给出目的
    // 后续需要通过VkPipelineShaderStageCreateInfo结构将着色器模块分配到管线中的顶点或者片段着色器阶段
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        // 这里貌似需要确保数据满足uint32_t的对齐要求
        // 不过数据存储在std::vector中，默认分配器已经确保数据满足最差情况下的对齐要求
        createInfo.codeSize = code.size();
        // 字节码的大小是以字节指定的，但是字节码指针是一个uint32_t类型的指针，而不是一个char指针
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        // 创建VkShaderModule
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());

        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                swapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            // 指定可以兼容的render pass(大致就是这个frame buffer和指定的render pass的attachment的数量和类型需要一致)
            framebufferInfo.renderPass = renderPass;
            // 指定attach上去的VkImageView数组和数量(数组长度)
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            // layer是指定图像数组中的层数。我们的交换链图像是单个图像，因此层数为1(没看懂)
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        // 这个flags可要可不要，如果要多个flags的话用|即可
        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: 提示命令缓冲区非常频繁的重新记录新命令(可能会改变内存分配行为)
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: 允许命令缓冲区单独重新记录，没有这个标志，所有的命令缓冲区都必须一起重置
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        // command buffer是通过在一个设备队列上提交它们来执行的，每个命令池只能分配在单一类型队列上提交的命令缓冲区
        // 我们将记录用于绘图的命令，所以用graphicsFamily
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        // VK_COMMAND_BUFFER_LEVEL_PRIMARY: 可以提交到队列执行，但不能从其他的命令缓冲区调用
        // VK_COMMAND_BUFFER_LEVEL_SECONDARY: 无法直接提交，但是可以从主命令缓冲区调用
        // 如果有一些Command Buffer是差不多的，可以通过SECONDARY来实现一些通用的操作
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        // 我们这里只申请一个command buffer
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    // 实际填充一个command buffer信息
    // imageIndex是当前交换链中图像的索引
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        // 通过vkBeginCommandBuffer来开启命令缓冲区的记录功能
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // flags标志位参数用于指定如何使用命令缓冲区，可选的参数类型如下
        // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: 命令缓冲区将在执行一次后立即重新记录
        // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: 这是一个LEVEL_SECONDARY的command buffer，它只能在一个render pass中使用
        // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution.(没看懂)
        beginInfo.flags = 0; // Optional
        // 仅拱LEVEL_SECONDARY使用
        beginInfo.pInheritanceInfo = nullptr; // Optional

        // 创建command buffer
        // 如果传入的command buffer是已经创建过的，这个调用会重置command buffer
        // It's not possible to append commands to a buffer at a later time.(我怎么感觉现在也没有append上去呢)
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // 绘制图像从使用 vkCmdBeginRenderPass 启动Render Pass开始
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        // 使用参数指定的交换链中的一个Frame Buffer(这个应该是fragment shader要写入的buffer)
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        // 这个render area定义了shader将要加载和存储的位置(没看懂)
        renderPassInfo.renderArea.offset = { 0, 0 };
        // 一般来说大小(extend)是和framebuffer的attachment一致的，如果小了会浪费，大了超出去的部分是一些未定义数值
        renderPassInfo.renderArea.extent = swapChainExtent;
        // 给VK_ATTACHMENT_LOAD_OP_CLEAR定义具体的clear color
        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        // 现在可以开始render pass了，所有record commands的接口都是以vkCmd为前缀的
        // 这些函数返回void，需要等到我们结束command的record后才会返回错误
        // 这里第三个参数可以是
        // VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed.
        // VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: The render pass commands will be executed from secondary command buffers.
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);


        // 绑定我们之前用于图形渲染的管线
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        // 我们之前在创建Pipeline的时候，把viewport和scissor设置成动态的了，所以需要在这里设置一下
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // DrawCall指令，后面4个参数分别是
        // vertexCount: 要绘制的顶点数量
        // instanceCount : GPU Instance数量，如果不用的话就填1
        // firstVertex : 在vertex buffer上的偏移量, 定义了gl_VertexIndex的最小值
        // firstInstance : GPU Instance的偏移量, 定义了gl_InstanceIndex的最小值
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        // 结束render pass
        vkCmdEndRenderPass(commandBuffer);

        // 结束command的创建
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        // 创建Semaphore的结构体，目前暂时没有任何参数需要设置，Vulkan在未来可能添加
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        // 创建Fence的结构体
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // 创建时立刻设置为signaled状态(否则第一帧的vkWaitForFences永远等不到结果)
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }

    void drawFrame() {
        // 等上一帧绘制完成
        // 参数2和3传入fense数组，参数4为true表示等待所有fense标记为已完成，false表示有任意一个完成即可，参数5是等待的最大时间(用64位最大无符号int来表示关闭最大等待时间)
        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        
        // 等待上一帧绘制结束后，手动把这些fense设置为unsignaled
        vkResetFences(device, 1, &inFlightFence);

        // 从交换链中获取一个可用的image，把这个可用的image在VkFrameBuffer里的下标写到imageIndex中，通过这个下标去实际获取image
        // 第3个参数是等待的最大时间，用用64位最大无符号int来表示关闭这个等待
        // 第4和5个参数表示需要等待变为signaled状态的Semaphore和Fence
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        // commandBuffer在每帧用之前先重置一下
        // 第二个参数填VkCommandBufferResetFlagBits，我们这里不需要设置任何标志，所以填0
        vkResetCommandBuffer(commandBuffer, 0);

        // 调用我们写的函数来record一个实际绘制图像的commandBuffer
        recordCommandBuffer(commandBuffer, imageIndex);

        // 配置提交command所需要的信息
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        // 设置需要等待的Semaphore，我们这里只需要等image可用
        VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        // 设置在渲染管线的哪个阶段等待Semaphore，这个pWaitDstStageMask是数组，和pWaitSemaphores是一一对应的
        // 就是说pWaitDstStageMask[i]需要等待pWaitSemaphores[i]
        // 因为我们等待的是可用的image，而这个image在输出color的时候才会用到，所以这里设置为VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        // 意思是，前面的那些vertex和geometry shader如果GPU有空闲可以先跑着
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.pWaitDstStageMask = waitStages;
        // 设置当前这个VkSubmitInfo是用于哪些command buffer的
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        // 设置上面那些command buffers执行结束后，哪些Semaphore需要发出信号(就是变成signaled状态)
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // 把VkSubmitInfo数组提交到指定的VkQueue里
        // 第4个参数表示这次提交的所有command buffers都执行完之后，需要变成signaled状态的VkFence
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }


        // 最后需要把执行结果重新提交到交换链上，才能最终展示到屏幕上
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        // 设置需要等待哪些Semaphore变成signaled状态后，才可以真正执行presentation
        // 这里我们就设置为上面那些commands执行完后即可
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        // 设置需要把image展示到哪些交换链的哪个image上，交换链和image都是传递的数组
        // 意思是把image提交到pSwapchains[i][pImageIndices[i]]上，所以这两个数组的长度是一样的，填一个swapchainCount就行
        // 不过绝大部分情况下，swapchainCount都是1
        VkSwapchainKHR swapChains[] = { swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        // 这是一个可选的参数，如果上面设置了多个交换链，可以用这个参数传入一个VkResult数组来接受每个交换链的presentation执行结果
        // 我们只有一个交换链的话可以不用这个参数
        presentInfo.pResults = nullptr;
        // 最终把image present到交换链上
        vkQueuePresentKHR(presentQueue, &presentInfo);
    }
};

int main() {
    PrepareLog();

    HelloTriangleApplication app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    EndLog();

    return EXIT_SUCCESS;
}