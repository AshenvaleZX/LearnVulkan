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
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
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