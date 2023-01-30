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

// ���������ļ�
static std::vector<char> readFile(const std::string& filename) {
    // ate:���ļ�ĩβ��ʼ��ȡ�����ļ�ĩβ��ʼ��ȡ���ŵ������ǿ���ʹ�ö�ȡλ����ȷ���ļ��Ĵ�С�����仺����
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("failed to open file!");
    
    // ʹ�ö�ȡλ����ȷ���ļ��Ĵ�С�����仺����
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    // �����ļ���ͷ��������ȡ����
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    // ��������������
    file.close();
    return buffer;
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
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    // ʹ���κε�VkImage�������ڽ�����������Ⱦ�����еģ����Ƕ���Ҫ����VkImageView����
    // �����������������һ�����ͼ�����ͼ��������ͨ�����������Ⱦ���߲��ܹ���д��Ⱦ���ݣ����仰˵VkImage��������Ⱦ���߽��н���
    // �����ͬ��VkImage�����ֶ������ģ�������Ҫ�ֶ�����
    std::vector<VkImageView> swapChainImageViews;
    // ����������ÿһ��VkImage����һ��֡������
    std::vector<VkFramebuffer> swapChainFramebuffers;
    // VkCommandPool����������洢command buffer���ڴ�ͷ���command buffer��
    VkCommandPool commandPool;
    // ��CommandPool�����ģ�CommandPool���ٵ�ʱ����Զ�����
    VkCommandBuffer commandBuffer;
    
    VkRenderPass renderPass;
    // �������glsl�����ڿ�ͷд���Ǹ�layout
    VkPipelineLayout pipelineLayout;

    VkPipeline graphicsPipeline;

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
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffer();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
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

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
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

    void createImageViews() {
        // size��swapChainImagesһ��
        swapChainImageViews.resize(swapChainImages.size());

        // ѭ������swapChainImages������swapChainImageViews
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];

            // ������1D��2D��3D����CubeMap����
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            // ��֮ǰ����swapChainImagesʱ��ĸ�ʽ
            createInfo.format = swapChainImageFormat;

            // components�ֶ����������ɫͨ�������յ�ӳ���߼�
            // ���磬���ǿ��Խ�������ɫͨ��ӳ��Ϊ��ɫͨ������ʵ�ֵ�ɫ��������Ҳ���Խ�ͨ��ӳ�����ĳ�����ֵ0��1
            // ������Ĭ�ϵ�
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            // subresourceRangle�ֶ���������ͼ���ʹ��Ŀ����ʲô���Լ����Ա����ʵ���Ч����
            // ���ͼ���������color(��������ȣ�stencil��)
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            // û��mipmap
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            // û��multiple layer (����ڱ�д����ʽ��3DӦ�ó��򣬱���VR������Ҫ����֧�ֶ��Ľ�����������ͨ����ͬ�Ĳ�Ϊÿһ��ͼ�񴴽������ͼ�������㲻ͬ���ͼ������������Ⱦʱ����ͼ����Ҫ)
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            // ����һ��VkImageView
            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    void createRenderPass() {
        // ����һ��color buffer
        // ʵ�������VkAttachmentDescription�������κ�buffer�Ĵ�����ֻ���������Ǵ�������һ��������color buffer
        VkAttachmentDescription colorAttachment = {};
        // �ͽ��������ͼƬ��ʽ����һ��
        colorAttachment.format = swapChainImageFormat;
        // �����ö��ز����Ļ��������1bit
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // loadOp����Ⱦǰ�Ĳ���
        // VK_ATTACHMENT_LOAD_OP_LOAD: �����Ѿ������ڵ�ǰbuffer������
        // VK_ATTACHMENT_LOAD_OP_CLEAR: ��ʼ�׶���һ��������������
        // VK_ATTACHMENT_LOAD_OP_DONT_CARE: ���ڵ�����δ���壬��������
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // storeOp����Ⱦ֮����ô����
        // VK_ATTACHMENT_STORE_OP_STORE: ��Ⱦ���ݴ洢����������֮����ж�ȡ����
        // VK_ATTACHMENT_STORE_OP_DONT_CARE: ֡����������������Ⱦ������Ϻ�����Ϊδ����(���Ǻܶ�����������Ǹ�һЩɧ�����İ�)
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // �����Ǹ�����������color��depth�ģ�stencil�ĵ���һ��
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // Vulkan���textures��framebuffers������һ��ȷ����format��VkImage����ʾ������layout�ǿ��Զ�̬�ĵ�
        // ���layout��Ҫ��Ӱ���ڴ�Ķ�д��ʽ�����Ը��������Ҫ��ʲô��ѡ����ʵ�layout
        // ��3��:
        // VK_IMAGE_LAYOUT_COLOR_ATTACHMET_OPTIMAL: ͼ����Ϊ��ɫ����
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: ͼ���ڽ������б�����
        // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : ͼ����ΪĿ�꣬�����ڴ�COPY����
        // initialLayoutָ�����ڽ��뵱ǰ���render pass֮ǰ��layout���������ǲ����ģ�������ΪUNDEFINED
        // ���������������ΪUNDEFINED���ǲ��ܱ�֤���ݿ��Ա��������ģ�������������loadOpҲ���ó���clear����������ν
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // finalLayoutָ���ǣ���ǰrender pass��������֮�󣬻����������Ϊʲôlayout
        // ��������ΪPRESENT_SRC_KHR����Ϊ����׼��ֱ�Ӱ����ݸ�������
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


        // һ����������Ⱦpass�����ɶ��subpass��ɡ�subpass��һϵ��ȡ����ǰһ��pass����������Ⱦ��������
        // ����˵����Ч��������ͨ��ÿһ��������֮ǰ�Ĳ���
        // �������Щ��Ⱦ�������鵽һ����Ⱦpass�У���Vulkan�ܹ�����������Щ��������ʡ�ڴ�����Ի�ÿ��ܸ��õ�����
        // ��������Ҫ���Ƶ������Σ�����ֻ��Ҫһ��subpass

        // ÿ��subpass����Ҫ����һ������attachment����Щattachment������֮ǰ�õĵĽṹ��������
        // ��Щ���ñ�������VkAttachmentReference����ʾ��
        VkAttachmentReference colorAttachmentRef{};
        // ���õ�attachment��attachment descriptions array�������������ֻ��һ��VkAttachmentDescription��������0
        colorAttachmentRef.attachment = 0;
        // ���ָ���˵�subpass��ʼ��ʱ���������õ�attachment�ᱻVulkan�Զ�ת��������layout
        // ��Ϊ�������attachment������color buffer�ģ��������������COLOR_ATTACHMENT_OPTIMAL���ﵽ�������
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // ����subpass�Ľṹ��
        VkSubpassDescription subpass{};
        // ��ͼ����Ⱦ��������������ΪVulkanҲ��֧��һЩ��ͼ�εĹ���
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        // ָ��color buffer�����ã����Ӧ���Լ���������(�����attachment = 0)����ֱ�Ӷ�Ӧ��ƬԪ��ɫ�����layout(location = 0) out vec4 outColor��
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.colorAttachmentCount = 1;
        // ����colorAttachment��������Щ��������
        // pInputAttachments: ��shader�ж�ȡ
        // pResolveAttachments: ������ɫ�����Ķ��ز���
        // pDepthStencilAttachment: ����depth��stencil����
        // pPreserveAttachments: �������subpassʹ�ã���������Ҫ����

        // ����render pass����Ϣ
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        // ǰ�洴��VkAttachmentReference��ʱ���Ǹ�����attachmentָ�ľ��������pAttachments�����������
        // �����ǿ��Դ�����ģ���������ֻ������һ��������Ҳֻ����һ��
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

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
        // ָ��Ϊ������ɫ��
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        // ָ�������������ɫ��ģ���shader�����������
        // ���Խ����Ƭ����ɫ����ϵ�������ɫ��ģ���У���ʹ�ò�ͬ����ڵ����������ǵ���Ϊ
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
        // ������Ը���ɫ��ָ������ֵ��ʹ�õ�����ɫ��ģ�飬ͨ��Ϊ����ʹ�ò�ͬ�ĳ���ֵ����������ˮ�ߴ���ʱ����Ϊ��������
        // �������Ⱦʱʹ�ñ���������ɫ������Ч�ʣ���Ϊ���������Խ����Ż�����������ifֵ�жϵ����
        vertShaderStageInfo.pSpecializationInfo = nullptr;

        // ƬԪ��ɫ�����������һ�����Ͳ���һ������
        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // �ṹ�������˶������ݵĸ�ʽ���ýṹ�����ݴ��ݵ�vertex shader��
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        // Bindings:�������ݵļ�϶��ȷ��������ÿ�����������ÿ��instance(instancing)
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        // Attribute:������Ҫ���а󶨼��������ԵĶ�����ɫ���е������������
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

        // ����ṹ��������������:����������ʲô���͵ļ���ͼԪ���˽��л��Ƽ��Ƿ����ö��������¿�ʼͼԪ
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        // VK_PRIMITIVE_TOPOLOGY_POINT_LIST: ��ͼԪ
        // VK_PRIMITIVE_TOPOLOGY_LINE_LIST: ��ͼԪ�����㲻����
        // VK_PRIMITIVE_TOPOLOGY_LINE_STRIP : ��ͼԪ��ÿ���߶εĽ���������Ϊ��һ���߶εĿ�ʼ����
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : ������ͼԪ�����㲻����
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : ������ͼԪ��ÿ�������εĵڶ��������������㶼��Ϊ��һ�������ε�ǰ��������
        // �о�LIST����OpenGL���DrawArray��STRIP����DrawElement
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // ���������_STRIP���͵�ͼԪ���������ΪVK_TRUE�󣬿�����0xFFFF����0xFFFFFFFF����element buffer����±����Ͽ�����һ��ͼԪ������ظ�ʹ��
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapChainExtent.width;
        viewport.height = (float)swapChainExtent.height;
        // �ֶ�ָ����ȷ�Χ��������Ҫ������0-1֮�䣬�̳���˵minDepth may be higher than maxDepth? 
        // ���Ǻܶ�������һ�������Ϊ0��1����
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // �ü�����һ����˵�����κβü��Ļ���offset�͸߿�(Ҳ����extent)��viewport�ı���һ�¾���
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;

        // viewport��scissorҲ���Բ��ڴ���Pipeline��ʱ��ȷ����������Ϊһ����̬�������ں������Ƶ�ʱ��ָ��
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // VkViewport��scissor��Ϣһ����䵽����
        // ��������֮��viewport��scissor�൱�ڱ�ɹ̶����ߵ�һ�����ˣ�Ҳ����Ū�ɶ�̬�ķŵ�command buffer��ȥ����
        // ��:https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions#page_Viewports-and-scissors
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        // ���ҪŪ�ɶ�̬�ģ�����Ͳ����ã��������ֱ�������ˣ������pipeline��viewport��scissor֮��Ͳ����ٸ���
        // viewportState.pViewports = &viewport;
        // viewportState.pScissors = &scissor;

        // ��դ���׶���Ϣ
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        // ���depthClampEnable����ΪVK_TRUE������Զ���ü����ƬԪ����������������Ƕ�������
        // �������������±Ƚ����ã�����Ӱ��ͼ��ʹ�øù�����Ҫ�õ�GPU��֧��
        rasterizer.depthClampEnable = VK_FALSE;
        // ���rasterizerDiscardEnable����ΪVK_TRUE����ô����ͼԪ��Զ���ᴫ�ݵ���դ���׶�
        // ���ǽ�ֹ�κ����������framebuffer�ķ���
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        // ���ָ��ƬԪ��δӼ���ģ���в�����һ�������������FILL��������ǵĻ�����Ҫ����GPU feature
        // VK_POLYGON_MODE_FILL: ������������
        // VK_POLYGON_MODE_LINE: ����α�Ե�߿����
        // VK_POLYGON_MODE_POINT : ����ζ�����Ϊ������
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        // �����������˼�ǣ��ö���ƬԪ�������������ߵĿ�ȣ�һ�㶼��1�����Ҫ����1����Ҫ����wideLines���GPU feature
        rasterizer.lineWidth = 1.0f;
        // �������FaceCull�������ñ���ü�
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        // ������ô�ж�����
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // ��Щ����һ������Ⱦ��Ӱ��ͼ�õģ���ʱ�ò���
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional
        // ���ö��ز���
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional
        // ����ɫ�ʻ�Ϸ�ʽ
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        // ���Ҳ������ɫ�ʻغϵ�һ�ַ�ʽ��������һ��ȫ�ֵ����ã����������ˣ�������Ǹ����þͻ�ʧЧ���൱�������blendEnable = VK_FALSE
        // ��������Ǻܶ����Ȳ������
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

        // shader��ͷд���Ǹ�layout��������ʱ������
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
        // ����PipelineLayout�������浽��Ա��������������
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }


        // ���ڿ�ʼ������һ��Ѷ�������pipeLineInfo��......
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        // ��Ϊ����ֻд��vert��frag shader��������2��shader stage
        pipelineInfo.stageCount = 2;
        // ��������shader
        pipelineInfo.pStages = shaderStages;
        // ���������ǰ��д�ĸ��̶ֹ�����������Ϣ
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
        // ��������õ�renderPass
        // ����������Ϊֻ��һ�����ߺ�һ��pass�����Ծ���һһ��Ӧ�ģ���ʵ���Դ���һ��passȻ���õ��ܶ��������
        // ֻ������Ҫ��Щpass����һЩ��������:https://registry.khronos.org/vulkan/specs/1.3-extensions/html/chap8.html#renderpass-compatibility
        pipelineInfo.renderPass = renderPass;
        // �����õ��ĸ�subpass
        pipelineInfo.subpass = 0;

        // �����������ǿ���ʵ��ͨ��������һ�����еĹ��ߣ���������ǰ����
        // �����������֮��Ĳ��Ƚ�С���Ϳ���ͨ�������̳��������������л���ʱ��Ҳ�Ƚϸ�Ч
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional
        // ���Ҫ����������������ָ���̳еĻ�����Ҫ�����������flags
        // pipelineInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;

        // ������Ⱦ���ߣ�����������Խ���VkGraphicsPipelineCreateInfo����Ȼ��һ���Դ����������
        // �ڶ���������VkPipelineCache��������Դ洢���ߴ��������ݣ�Ȼ���ڶ��vkCreateGraphicsPipelines������ʹ��
        // �������Դ浽�ļ���ڲ�ͬ��Vulkan������ʹ��
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        // ���ߴ����ú��û���ˣ���������ɾ��
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    // VkShaderModule����ֻ���ֽ��뻺������һ����װ��������ɫ����û�б˴����ӣ�����û�и���Ŀ��
    // ������Ҫͨ��VkPipelineShaderStageCreateInfo�ṹ����ɫ��ģ����䵽�����еĶ������Ƭ����ɫ���׶�
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        // ����ò����Ҫȷ����������uint32_t�Ķ���Ҫ��
        // �������ݴ洢��std::vector�У�Ĭ�Ϸ������Ѿ�ȷ�����������������µĶ���Ҫ��
        createInfo.codeSize = code.size();
        // �ֽ���Ĵ�С�����ֽ�ָ���ģ������ֽ���ָ����һ��uint32_t���͵�ָ�룬������һ��charָ��
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        // ����VkShaderModule
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
            // ָ�����Լ��ݵ�render pass(���¾������frame buffer��ָ����render pass��attachment��������������Ҫһ��)
            framebufferInfo.renderPass = renderPass;
            // ָ��attach��ȥ��VkImageView���������(���鳤��)
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            // layer��ָ��ͼ�������еĲ��������ǵĽ�����ͼ���ǵ���ͼ����˲���Ϊ1(û����)
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
        // ���flags��Ҫ�ɲ�Ҫ�����Ҫ���flags�Ļ���|����
        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: ��ʾ��������ǳ�Ƶ�������¼�¼������(���ܻ�ı��ڴ������Ϊ)
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: ������������������¼�¼��û�������־�����е��������������һ������
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        // command buffer��ͨ����һ���豸�������ύ������ִ�еģ�ÿ�������ֻ�ܷ����ڵ�һ���Ͷ������ύ���������
        // ���ǽ���¼���ڻ�ͼ�����������graphicsFamily
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        // VK_COMMAND_BUFFER_LEVEL_PRIMARY: �����ύ������ִ�У������ܴ������������������
        // VK_COMMAND_BUFFER_LEVEL_SECONDARY: �޷�ֱ���ύ�����ǿ��Դ��������������
        // �����һЩCommand Buffer�ǲ��ģ�����ͨ��SECONDARY��ʵ��һЩͨ�õĲ���
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        // ��������ֻ����һ��command buffer
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    // ʵ�����һ��command buffer��Ϣ
    // imageIndex�ǵ�ǰ��������ͼ�������
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        // ͨ��vkBeginCommandBuffer��������������ļ�¼����
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // flags��־λ��������ָ�����ʹ�������������ѡ�Ĳ�����������
        // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: �����������ִ��һ�κ��������¼�¼
        // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: ����һ��LEVEL_SECONDARY��command buffer����ֻ����һ��render pass��ʹ��
        // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution.(û����)
        beginInfo.flags = 0; // Optional
        // ����LEVEL_SECONDARYʹ��
        beginInfo.pInheritanceInfo = nullptr; // Optional

        // ����command buffer
        // ��������command buffer���Ѿ��������ģ�������û�����command buffer
        // It's not possible to append commands to a buffer at a later time.(����ô�о�����Ҳû��append��ȥ��)
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
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