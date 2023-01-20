#include <iostream>
#include <vector>
#include <set>
#include <fstream>
#include <string>
using std::string;

std::ofstream ofs;
void PrepareLog() {
    // д�����ַ������һ����һ�����е���־
    ofs.open("../../log.txt", std::ios::out);
    ofs << "";
    ofs.close();
    // ������׷��д��ģʽ��
    ofs.open("../../log.txt", std::ios::out | std::ios::app);
}
void Log(string msg) {
    ofs << msg << std::endl;
}
void EndLog() {
    ofs.close();
}

// ��GLFW�Ļ�����Ͳ�Ҫ�Լ�ȥinclude Vulkan��ͷ�ļ���������궨�壬��GLFW�Լ�ȥ������Ȼ��Щ�ӿ�������
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

using namespace glm;

const int WIDTH = 800;
const int HEIGHT = 600;

// NDEBUG��C++��׼�궨�壬���������ԡ��������VS����ʱѡ���Debug��Releaseģʽ��ȷ���Ƿ���
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// Vulkan SDKͨ������VK_LAYER_KHRONOS_validation�㣬����ʽ�Ŀ�������������ϵ�layers���Ӷ�������ȷ��ָ�����е���ȷ����ϲ�
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    // ��������չ���������֧��Ҳ�ʹ������Ƿ�֧�ֽ�ͼ����Ƶ���ʾ����(��������GPU������������ͼ)
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Vulkan��Debug����չ���ܣ�����vkCreateDebugUtilsMessengerEXT�������Զ�����
// ���������Ҫͨ��vkGetInstanceProcAddr�ֶ���ȡ������ַ
// ����������Ƿ�װ��һ�»�ȡvkCreateDebugUtilsMessengerEXT��ַ���������Ĺ���
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

// ���������ĺ���������VkDebugUtilsMessengerEXT��������Ҫ�ֶ�����vkDestroyDebugUtilsMessengerEXT����
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

// ����������������������
struct SwapChainSupportDetails {
    // ������surface��������(min/max number of images in swap chain, min/max width and height of images)
    VkSurfaceCapabilitiesKHR capabilities;
    // Surface��ʽ(pixel format, color space)
    std::vector<VkSurfaceFormatKHR> formats;
    // ��Ч��presentationģʽ
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
    // �����豸
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    // �߼��豸
    VkDevice device;
    // surface��Vulkan�봰��ϵͳ�������������ѻ�����Ⱦ����������Ҫ���
    VkSurfaceKHR surface;
    // �豸�������豸�����ٵ�ʱ����ʽ�����������ǲ���Ҫ��cleanup���������κβ���
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    // ��ʾͼ��Ľ�����
    VkSwapchainKHR swapChain;
    // ��������ͼ��,ͼ�񱻽���������,Ҳ���ڽ��������ٵ�ͬʱ�Զ�����,�������ǲ���Ҫ����κ��������
    std::vector<VkImage> swapChainImages;

    void initWindow() {
        glfwInit();

        // ���GLFW��ΪOpenGL���������ģ�����������������Ҫ��������Ҫ����OpenGL��صĳ�ʼ������
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        // ��ϵͳ�в��Ҳ�ѡ��֧���������蹦�ܵ��Կ�
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
        // �����������֤�㣬��ô������validationLayers���Զ����������֤����Ҫȫ����֧��
        if (enableValidationLayers && !checkValidationLayerSupport())
            throw std::runtime_error("Validation layers requested, but not available!\nPlease check whether your SDK is installed correctly.");

        // ��Щ���ݴӼ����Ƕ��ǿ�ѡ��ģ�����������Ϊ���������ṩһЩ���õ���Ϣ���Ż����������ʹ���龰��������������ʹ��һЩͼ�������������Ϊ��
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        // ָ���ض�����չ�ṹ������������ʹ��Ĭ�ϳ�ʼ������������Ϊnullptr
        appInfo.pNext = nullptr;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // ��ȡ�벻ͬƽ̨�Ĵ���ϵͳ���н�������չ��Ϣ
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // ���������Debug������һ��Debug��Ϣ
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (enableValidationLayers) {
            // ��䵱ǰ�������Ѿ�������validation layers���Ƽ���
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            // ���һ��debugInfo�������
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        // ����ʵ�ʴ���һ��VkInstance��ǰ����Щ���붼���ڸ��������׼������
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    // ���������validationLayers���Զ����������֤���Ƿ�ȫ����֧��
    bool checkValidationLayerSupport() {
        // ��ȡ���п��õ�Layer����
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        // ��ȡ���п���Layer������
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // ������ǵ�validationLayers�е�����layer�Ƿ������availableLayers�б���
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
        // Vulakn����ƽ̨��������API֧�ֵ�(������ʱ����)������ζ����Ҫһ����չ�����벻ͬƽ̨�Ĵ���ϵͳ���н���
        // GLFW��һ����������ú������������йص���չ��Ϣ
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions;
        // ���GLFW��ȡ����չ
        for (unsigned int i = 0; i < glfwExtensionCount; i++) {
            extensions.push_back(glfwExtensions[i]);
        }
        // ���������Debug�����Debug����չ
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

    // �Զ����Debug�ص�������VKAPI_ATTR��VKAPI_CALLȷ������ȷ�ĺ���ǩ�����Ӷ���Vulkan����
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        std::cerr << "My debug call back: " << pCallbackData->pMessage << std::endl;
        Log(pCallbackData->pMessage);
        // ����һ������ֵ����������validation layer��Ϣ��Vulkan�����Ƿ�Ӧ����ֹ
        // �������true������ý���VK_ERROR_VALIDATION_FAILED_EXT������ֹ
        // ��ͨ�����ڲ���validation layers���������������Ƿ���VK_FALSE
        return VK_FALSE;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        // ����ṹ���Ǵ��ݸ�vkCreateDebugUtilsMessengerEXT��������VkDebugUtilsMessengerEXT����ģ��������ǵ�debugMessenger������
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        // ���������Լ���װvkCreateDebugUtilsMessengerEXT�����ĺ������������洴���Ľṹ����Ϊ���������ڴ���VkDebugUtilsMessengerEXT����
        // Ҳ�������ǵ�debugMessenger����
        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void createSurface() {
        // surface�ľ��崴��������Ҫ����ƽ̨�ģ�����ֱ����GLFW��װ�õĽӿ�������
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void pickPhysicalDevice() {
        // ��ȡ��ǰ�豸֧��Vulkan���Կ�����
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        // ��ȡ����֧��Vulkan���Կ�����ŵ�devices��
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // ���������Կ����ҵ���һ���������������
        // ��ʵ����д�����߳�ͬʱ������Щ�Կ������������������һ��
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
        // ���name, type�Լ�Vulkan�汾�Ȼ������豸����
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        // ��Ҫ����
        if (deviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            return false;

        // ��ѯ������ѹ����64λ�������Ͷ���ͼ��Ⱦ(VR�ǳ�����)�ȿ�ѡ���ܵ�֧��
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        // ��Ҫ֧�ּ�����ɫ��
        if (!deviceFeatures.geometryShader)
            return false;

        // ��ѯ���������Ķ��д�
        QueueFamilyIndices indices = findQueueFamilies(device);
        if (!indices.isComplete())
            return false;

        // ����Ƿ�֧������Ҫ����չ
        if (!checkDeviceExtensionSupport(device))
            return false;

        // ��齻�����Ƿ�����
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        // ����֧��һ��ͼ���ʽ��һ��Presentģʽ
        bool swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        if (!swapChainAdequate)
            return false;

        // ����кܶ��Կ�������ͨ��������Ӳ������һ��Ȩ�أ�Ȼ������ѡ������ʵ�
        return true;
    }

    // ����Ƿ�֧������Ҫ����չ
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        // ��ȡ�����豸֧�ֵ���չ����
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        // ��ȡ��֧�ֵľ�����Ϣ
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        // �������Զ����deviceExtensionsת����set���ݽṹ(Ӧ����Ϊ�˱������2��forѭ����erase��ͬʱҲ���Ķ�ԭ����)
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        // ���������豸��֧�ֵ���չ�������������Ҫ����չ������ɾ��
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        // ���ȫ��ɾ�����ˣ�˵����������Ҫ����չ����֧�ֵģ�����˵������һЩ������Ҫ����չ����֧��
        return requiredExtensions.empty();
    }

    // �������е�Vulkan�������ӻ�ͼ���ϴ���������Ҫ�������ύ��������
    // �в�ͬ���͵Ķ�����Դ�ڲ�ͬ�Ķ��дأ�ÿ�����д�ֻ������commands
    // ���磬������һ�����дأ�ֻ���������commands����ֻ�����ڴ洫��commands
    // ������Ҫ����豸��֧�ֵĶ��дأ�������һ�����д�֧��������Ҫ��commands
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        // ��ȡ�豸ӵ�еĶ��д�����
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        // ��ȡ���ж��дػ������
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        // �������дأ���ȡ֧����������Ķ��д�
        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // ��ǰ���д��Ƿ�֧��ͼ�δ���
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            // �Ƿ�֧��VkSurfaceKHR
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            // ֧�ֵĻ���¼һ������
            if (queueFamily.queueCount > 0 && presentSupport) {
                indices.presentFamily = i;
            }

            // ע������֧��surface��graphic�Ķ��дز�һ����ͬһ��
            // ����ʹ����Щ���дص��߼���Ҫô�̶�����֧��surface��graphic�Ķ��д���������ͬ��������(���������ǲ���ͬһ�����������)
            // Ҫô������Ҷ��дص�ʱ����ȷָ������ͬʱ֧��surface��graphic��Ȼ�󱣴�ͬʱ֧��������Ҫ��Ķ��д�����(���ܿ��ܻ�õ�)

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        // ����ֻ���ؾ߱�ͼ�������Ķ���
        return indices;
    }

    void createLogicalDevice() {
        // ��ȡ��ǰ�����豸�Ķ��д�
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        // VkDeviceQueueCreateInfo�Ǵ����߼��豸��Ҫ�ڽṹ������ȷ�������Ϣ
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

        float queuePriority = 1.0f;
        // �ж�����У���������
        for (int queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            // ��ǰ���дض�Ӧ������
            queueCreateInfo.queueFamilyIndex = queueFamily;
            // ���д�����Ҫ����ʹ�õĶ�������
            queueCreateInfo.queueCount = 1;
            // Vulkan����ʹ��0.0��1.0֮��ĸ���������������ȼ���Ӱ���������ִ�еĵ��ã���ʹֻ��һ������Ҳ�Ǳ����
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // ��ȷ�豸Ҫʹ�õĹ������ԣ���ʱ����Ҫ�κ�����Ĺ���
        VkPhysicalDeviceFeatures deviceFeatures = {};

        // �����߼��豸����Ϣ
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        // ʹ��ǰ��������ṹ�����
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;

        // ��VkInstanceһ����VkDevice���Կ�����չ����֤��
        // �����չ��Ϣ
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        // �����������֤�㣬����֤����Ϣ��ӽ�ȥ
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        // ����vkCreateDevice����������ʵ�����߼��豸
        // �߼��豸����VkInstanceֱ�ӽ��������Բ�����ֻ�������豸
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        // �߼��豸������ʱ�򣬶���Ҳһ�𴴽��ˣ���ȡ���в�������������֮�����
        // �������߼��豸�����дأ����������ʹ洢��ȡ���б��������ָ��
        // ��Ϊ����ֻ�Ǵ�������дش���һ�����У�������Ҫʹ������0
        vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
    }

    void createSwapChain() {
        // ��ѯӲ��֧�ֵĽ���������
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        // ѡ��һ��ͼ���ʽ
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        // ѡ��һ��presentģʽ(����ͼ�񽻻���ģʽ)
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        // ѡ��һ�����ʵ�ͼ��ֱ���
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // �������е�ͼ���������������Ϊ�������еĳ��ȡ���ָ������ʱͼ�����С���������ǽ����Դ���1��ͼ����������ʵ�����ػ��塣
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        // ���maxImageCount����0���ͱ�ʾû�����ơ��������0�ͱ�ʾ�����ƣ���ô�������ֻ�����õ�maxImageCount
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        // �����������Ľṹ��
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        // �����ǵ�surface
        createInfo.surface = surface;

        // ��ǰ���ȡ����������
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.presentMode = presentMode;
        createInfo.imageExtent = extent;

        // imageArrayLayersָ��ÿ��ͼ����ɵĲ������������ǿ���3DӦ�ó��򣬷���ʼ��Ϊ1
        createInfo.imageArrayLayers = 1;
        // ����ֶ�ָ���ڽ������ж�ͼ����еľ������
        // �ڱ�С���У����ǽ�ֱ�Ӷ����ǽ�����Ⱦ������ζ��������Ϊ��ɫ����
        // Ҳ�������Ƚ�ͼ����ȾΪ������ͼ�񣬽��к������
        // ����������¿���ʹ����VK_IMAGE_USAGE_TRANSFER_DST_BIT������ֵ����ʹ���ڴ��������Ⱦ��ͼ���䵽������ͼ�����
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // ��������������Ҫָ����δ���������дصĽ�����ͼ��
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };
        // ���graphics���д���presentation���дز�ͬ���������������
        // ���ǽ���graphics�����л��ƽ�������ͼ��Ȼ������һ��presentation�������ύ����
        // ����д���ͼ�������ַ���
        // VK_SHARING_MODE_EXCLUSIVE: ͬһʱ��ͼ��ֻ�ܱ�һ�����д�ռ�ã�����������д���Ҫ������Ȩ��Ҫ��ȷָ�������ַ�ʽ�ṩ����õ�����
        // VK_SHARING_MODE_CONCURRENT: ͼ����Ա�������дط��ʣ�����Ҫ��ȷ����Ȩ������ϵ
        // ������дز�ͬ����ʱʹ��concurrentģʽ�����⴦��ͼ������Ȩ������ϵ�����ݣ���Ϊ��Щ���漰���ٸ���
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            // Concurrentģʽ��ҪԤ��ָ�����д�����Ȩ������ϵ��ͨ��queueFamilyIndexCount��pQueueFamilyIndices�������й���
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        // ���graphics���дغ�presentation���д���ͬ��������Ҫʹ��exclusiveģʽ����Ϊconcurrentģʽ��Ҫ����������ͬ�Ķ��д�
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // ���������֧��(supportedTransforms in capabilities)�����ǿ���Ϊ������ͼ��ָ��ĳЩת���߼�
        // ����90��˳ʱ����ת����ˮƽ��ת���������Ҫ�κ�transoform���������Լ򵥵�����ΪcurrentTransoform
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

        // compositeAlpha�ֶ�ָ��alphaͨ���Ƿ�Ӧ�����������Ĵ���ϵͳ���л�ϲ�����������Ըù��ܣ��򵥵���VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        // ���clipped��Ա����ΪVK_TRUE����ζ�����ǲ����ı��ڱε��������ݣ��������������Ĵ�������ǰ��ʱ������Ⱦ�Ĳ������ݴ����ڿ�������֮��
        // ���������Ҫ��ȡ��Щ���ػ����ݽ��д���������Կ����ü�����������
        createInfo.clipped = VK_TRUE;

        // Vulkan����ʱ��������������ĳЩ�����±��滻�����細�ڵ�����С���߽�������Ҫ���·�������ͼ�����
        // ����������£�������ʵ������Ҫ���·��䴴�������ұ����ڴ��ֶ���ָ���Ծɵ����ã����Ի�����Դ
        // ����һ���Ƚϸ��ӵĻ��⣬���ǻ��ں�����½�����ϸ���ܡ����ڼ�������ֻ�ᴴ��һ����������
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        // ���մ���������
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // ��ȡһ�½�������ͼ������������imageCount�����ʼ������������ͼ������������ʵ�ʴ����Ĳ�һ������ô�࣬����Ҫ���»�ȡһ��
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        // ����ʵ��ͼ��������������vector��С
        swapChainImages.resize(imageCount);
        // ��󣬴洢��������ʽ�ͷ�Χ����Ա����swapChainImages��
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        // ���������ҪVkPhysicalDevice��VkSurfaceKHR����surface����֧����Щ���幦��
        // �������ڲ鿴֧�ֹ��ܵĺ�������Ҫ��������������Ϊ�����ǽ������ĺ������
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        // ��ѯ֧�ֵ�surface��ʽ����ȡ������һ���ṹ���б�
        // �Ȼ�ȡ֧�ֵĸ�ʽ����
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            // ��������vector��С
            details.formats.resize(formatCount);
            // ����ʽ����
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        // ��ѯ֧�ֵ�surfaceģʽ����ȡ������һ���ṹ���б�
        // �Ȼ�ȡ֧�ֵ�ģʽ����
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            // ��������vector��С
            details.presentModes.resize(presentModeCount);
            // ���ģʽ����
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        // ����������������surfaceû�������κ�ƫ���Եĸ�ʽ
        // ���ʱ��Vulkan��ͨ��������һ��VkSurfaceFormatKHR�ṹ����ʾ���Ҹýṹ��format��Ա����ΪVK_FORMAT_UNDEFINED
        // ��ʱ���ǿ������ɵ����ø�ʽ
        if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
            return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        }

        // ����������ɵ����ø�ʽ����ô���ǿ���ͨ�������б����þ���ƫ���Ե����
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        // ����������ַ�ʽ��ʧЧ�ˣ�����ֱ��ѡ���һ����ʽ
        // ��ʵ���ʱ������Ҳ���Ա���һ��availableFormats���Լ�дһ��������һ����ԽϺõĳ���
        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
        // ��Vulkan�����ĸ�ģʽ����ʹ��:
        // 1��VK_PRESENT_MODE_IMMEDIATE_KHR
        // Ӧ�ó����ύ��ͼ���������䵽��Ļ���֣�����ģʽ���ܻ����˺��Ч����
        // 2��VK_PRESENT_MODE_FIFO_KHR
        // ������������һ�����У�����ʾ������Ҫˢ�µ�ʱ����ʾ�豸�Ӷ��е�ǰ���ȡͼ�񣬲��ҳ�����Ⱦ��ɵ�ͼ�������еĺ��档������������ĳ����ȴ������ֹ�ģ����Ƶ��Ϸ�Ĵ�ֱͬ�������ơ���ʾ�豸��ˢ��ʱ�̱���Ϊ����ֱ�жϡ���
        // 3��VK_PRESENT_MODE_FIFO_RELAXED_KHR
        // ��ģʽ����һ��ģʽ���в�ͬ�ĵط�Ϊ�����Ӧ�ó�������ӳ٣����������һ����ֱͬ���ź�ʱ���п��ˣ�������ȴ���һ����ֱͬ���źţ����ǽ�ͼ��ֱ�Ӵ��͡����������ܵ��¿ɼ���˺��Ч����
        // 4��VK_PRESENT_MODE_MAILBOX_KHR
        // ���ǵڶ���ģʽ�ı��֡�����������������ʱ��ѡ���µ��滻�ɵ�ͼ�񣬴Ӷ��������Ӧ�ó�������Ρ�����ģʽͨ������ʵ�����ػ����������׼�Ĵ�ֱͬ��˫������ȣ���������Ч�����ӳٴ�����˺��Ч����
        
        // Ĭ�ϵ�ģʽ
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
        // SwapExtent��ָ������ͼ��ķֱ���

        // currentExtent�ĸ߿������һ�������uint32���ֵ����ֱ�ӷ��أ�����
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else {
            VkExtent2D actualExtent = { WIDTH, HEIGHT };

            // ������minImageExtent��maxImageExtent֮��
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