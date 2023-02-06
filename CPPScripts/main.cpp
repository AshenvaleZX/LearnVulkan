#include <iostream>
#include <vector>
#include <set>
#include <fstream>
#include <string>
#include <array>
#include <chrono>
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
// OpenGL��depth��Χ��-1��1������Ӻ궨��תΪVulkan��0��1
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace glm;

const int WIDTH = 800;
const int HEIGHT = 600;

// ��GPU��Ⱦ�����ʱ��CPU���Դ����֡��
// ����Ϊ2��˼�Ǽ��統ǰ��һ֡GPU��û��Ⱦ�꣬CPU�����ȴ�����һ֡�������Ƿ�Ҫ��GPU����һ֡��Ⱦ��
const int MAX_FRAMES_IN_FLIGHT = 2;

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

struct UniformBufferObject {
    // ���struct��Ҫ�ŵ�uniform buffer����shader�õģ�������漰һ��C++������shader�������ڴ��϶��������
    // Vulkan�ľ���Ҫ���:https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap15.html#interfaces-resources-layout
    // ����Ϊ�˲�������������⣬����uniform����������ö���alignas��ʾ����һ�¶���ĸ�ʽ
    // ��16�ֽڶ���Ӧ���ܱ�֤��������(����������������ݸպ���16�ֽڵı���������ʾ����Ҳ�У������������ϰ��)
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        // ������VERTEX����INSTANCE�����ǲ���GPU Instance����VERTEX
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        // ���ǵĶ�����2�����ݣ�������һ������Ϊ2��VkVertexInputAttributeDescription��������������
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        // ���Ǻܶ����binding�ĺ��壬�����Instance�й�
        attributeDescriptions[0].binding = 0;
        // �����ֱ�Ӷ�Ӧvertex shader�����layout location
        attributeDescriptions[0].location = 0;
        // ���ݸ�ʽ�����ǵ�pos������һ��2ά�з��Ÿ����������Ծ����������Ӧshader���vec2
        // ���Ƶ�:
        // float: VK_FORMAT_R32_SFLOAT
        // vec2: VK_FORMAT_R32G32_SFLOAT
        // vec3: VK_FORMAT_R32G32B32_SFLOAT
        // vec4: VK_FORMAT_R32G32B32A32_SFLOAT
        // ivec2: VK_FORMAT_R32G32_SINT
        // uvec4: VK_FORMAT_R32G32B32A32_UINT
        // double: VK_FORMAT_R64_SFLOAT
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        // ��ǰ������Ե�ƫ����
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // ͬ��
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
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
    // ���ڽ����������ͼ����Ϊ�������Ӧ��ͬһʱ�̺���ֻ��һ��fragment shader��д�뽻����������һ��depth�͹��ˣ����ö��
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;


    // VkCommandPool����������洢command buffer���ڴ�ͷ���command buffer��
    VkCommandPool commandPool;
    // ��CommandPool�����ģ�CommandPool���ٵ�ʱ����Զ�����
    std::vector<VkCommandBuffer> commandBuffers;
    // ��Ŷ������ݵ�buffer��Vulkan�е�buffer�����ڴ洢�����Կ���ȡ���������ݵ��ڴ�����
    VkBuffer vertexBuffer;
    // ��Ŷ������ݵ�buffer��ʹ�õ��ڴ�
    VkDeviceMemory vertexBufferMemory;
    // ��Ŷ���������buffer
    // ʵ�������buffer�������vertexBuffer���Ժϲ���һ����ʹ�õ�ʱ��Vulkan�ӿ������һ��offset������ָ����һ������Ӧ�ô��Ŀ�ʼ��
    // �������Լ�����ȫ��ͬ����Դʹ��ͬһ��VkBuffer��ֻҪ���ܱ�֤�⼸����Դ������һ��������ͬʱʹ�ã�����Ҫ�õ�ʱ�����VkBuffer����
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    // ��shader��uniform���ݵ�buffer����Ϊ���ǿ��ܻ�ͬʱ���ƶ�֡��������Щ���ݶ��ǳ���ΪMAX_FRAMES_IN_FLIGHT��vector
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    // ���VkImage��ʵ��VkBuffer��࣬VkImageֻ��ר��Ϊ�洢ͼ�������Ż���VkBuffer
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    // VkImage�޷�ֱ�ӷ��ʣ���Ҫͨ��VkImageView��ӷ���
    VkImageView textureImageView;
    // ���������
    VkSampler textureSampler;
    
    VkRenderPass renderPass;
    // �������glsl�����ڿ�ͷд���Ǹ�layout
    VkPipelineLayout pipelineLayout;
    // ������Ⱦͼ�εĹ���(����Ϸ�����϶���ͼ�ι��ߣ�����Vulkan����Ƴɿ����������������ҪGPU�Ĺ�����)
    VkPipeline graphicsPipeline;
    // ����������shader��MVP����֮������ݵ��������ṹ
    VkDescriptorSetLayout descriptorSetLayout;
    // ����ʵ�ʷ�����������pool
    VkDescriptorPool descriptorPool;
    // ʵ�ʷ�����������������������飬��uniform buffer�����Ӧ
    std::vector<VkDescriptorSet> descriptorSets;

    // �Ѵӽ�������ȡͼ�β��ҿ���������Ⱦ��ͬ���ź�
    std::vector<VkSemaphore> imageAvailableSemaphores;
    // ��Ⱦ����ɵ�ͬ���ź�
    std::vector<VkSemaphore> renderFinishedSemaphores;
    // һ��ֻ��Ⱦһ֡�����ͬ���ź�
    std::vector<VkFence> inFlightFences;
    // ��ǰCPU�ڴ�����һ֡(��������CPU���ȴ�GPU��ɵ�ǰ��һ֡�Ļ��ƣ��ȿ�ʼ������һ֡������ͬ���õ��ź�����command���ж�ݣ���Ҫȷ�ϵ�ǰ�õ���һ��)
    uint32_t currentFrame = 0;

    bool framebufferResized = false;

    void initWindow() {
        glfwInit();

        // ���GLFW��ΪOpenGL���������ģ�����������������Ҫ��������Ҫ����OpenGL��صĳ�ʼ������
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        // ��������ӿڴ���һ���Զ���ָ���window������ص������ȡthis
        glfwSetWindowUserPointer(window, this);
        // ���ô��ڴ�С�仯�Ļص�
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        // ͨ���ո����õ�thisָ���ȡ�����ǵ�demoʵ��
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        // ��ϵͳ�в��Ҳ�ѡ��֧���������蹦�ܵ��Կ�
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createSwapChainImageViews();
        createRenderPass();
        // ������������uniforms�Ľṹ��
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createCommandPool();
        createDepthResources();
        createFramebuffers();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }

        // ��ΪdrawFrame��������õĻ���ͼ�εĽӿڣ��ܶ඼���첽ִ�еģ�����whileѭ�������󣬿�����Щ���뻹��ִ��
        // ���ʱ�����ֱ�ӽ�����ѭ������ʼ����cleanup���ٸ���Vulkan���󣬺��п������ٵ�����ʹ�õĶ��󣬾ͻ�Crash
        // ���Ե�������ӿڵȴ��߼��豸�ϵ�����command����ִ����ϣ��ٽ�����ѭ��
        vkDeviceWaitIdle(device);
    }

    void cleanupSwapChain() {
        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

        for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
            vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
        }
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        }
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    void cleanup() {
        cleanupSwapChain();

        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyImageView(device, textureImageView, nullptr);
        vkDestroyImage(device, textureImage, nullptr);
        vkFreeMemory(device, textureImageMemory, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        // buffer���ٺ������ֶ��ͷŶ�Ӧ���ڴ�
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }
        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        
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

        // ��Ҫ֧�ָ������Բ���
        if (!deviceFeatures.samplerAnisotropy)
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

        // ��ȷ�豸Ҫʹ�õĹ�������
        VkPhysicalDeviceFeatures deviceFeatures = {};
        // ���öԸ������Բ�����֧��
        deviceFeatures.samplerAnisotropy = VK_TRUE;

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

    void createSwapChainImageViews() {
        // size��swapChainImagesһ��
        swapChainImageViews.resize(swapChainImages.size());

        // ѭ������swapChainImages������swapChainImageViews
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        // ��Ӧ���ĸ�VkImage
        createInfo.image = image;
        // ������1D��2D��3D����CubeMap����
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        // ͼƬ��ʽ
        createInfo.format = format;

        // components�ֶ����������ɫͨ�������յ�ӳ���߼�
        // ���磬���ǿ��Խ�������ɫͨ��ӳ��Ϊ��ɫͨ������ʵ�ֵ�ɫ��������Ҳ���Խ�ͨ��ӳ�����ĳ�����ֵ0��1
        // ������Ĭ�ϵ�
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // subresourceRangle�ֶ���������ͼ���ʹ��Ŀ����ʲô���Լ����Ա����ʵ���Ч����
        // ���ͼ���������color����depth stencil��
        createInfo.subresourceRange.aspectMask = aspectFlags;
        // û��mipmap
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        // û��multiple layer (����ڱ�д����ʽ��3DӦ�ó��򣬱���VR������Ҫ����֧�ֶ��Ľ�����������ͨ����ͬ�Ĳ�Ϊÿһ��ͼ�񴴽������ͼ�������㲻ͬ���ͼ������������Ⱦʱ����ͼ����Ҫ)
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        // ����һ��VkImageView
        VkImageView imageView;
        if (vkCreateImageView(device, &createInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image views!");
        }

        return imageView;
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

        // �ٴ���һ��depth attachment������ע�Ϳ������color attachment
        VkAttachmentDescription depthAttachment{};
        // ʹ�ú����Ǵ���depthImageʱһ����format
        depthAttachment.format = findDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // ��Ϊ������ɺ����ǲ�����ʹ��depth buffer�ˣ��������ﲻ����
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


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

        // depth attachment������
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // ����subpass�Ľṹ��
        VkSubpassDescription subpass{};
        // ��ͼ����Ⱦ��������������ΪVulkanҲ��֧��һЩ��ͼ�εĹ���
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        // ָ��color buffer�����ã����Ӧ���Լ���������(�����attachment = 0)����ֱ�Ӷ�Ӧ��ƬԪ��ɫ�����layout(location = 0) out vec4 outColor��
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.colorAttachmentCount = 1;
        // һ��subpassֻ����һ��depth stencil attachment���������ﲻ��color attachmentһ����Ҫ�����鳤��
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        // ����color��depth stencil��������Щ��������
        // pInputAttachments: ��shader�ж�ȡ
        // pResolveAttachments: ������ɫ�����Ķ��ز���
        // pPreserveAttachments: �������subpassʹ�ã���������Ҫ����

        // ���pass������attachment
        std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
        // ����render pass����Ϣ
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        // ǰ�洴��VkAttachmentReference��ʱ���Ǹ�����attachmentָ�ľ��������pAttachments�����������
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        // ����Subpass֮���������ϵ
        VkSubpassDependency dependency{};
        // ���嵱ǰ��������Ǵ�srcSubpass��dstSubpass�ģ�������pSubpasses���������
        // ���仰˵����ǰ��������������ϵ�����ǵ�pSubpasses[srcSubpass]������Ҫ����pSubpasses[dstSubpass]ʱ���
        // �����VK_SUBPASS_EXTERNAL���ʹ����ǵ�ǰ���Pass�ⲿ��
        // ��������srcSubpass��VK_SUBPASS_EXTERNAL��dstSubpass��0���ͱ�ʾ���ǽ��뵱ǰPass�ĵ�һ��Subpassʱ����Ҫ��һЩ������ϵ
        // ע��Render Pass���Subpass�ǰ���pSubpasses�����˳��ִ�еģ����������srcSubpassӦ��С��dstSubpass(�����VK_SUBPASS_EXTERNAL������ν)
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;

        // ���ﶨ�����������ϵ��Ҫ�ǽ��image��layoutת������
        // �����ڴ���Render Pass��VkRenderPassCreateInfo��pAttachments��������������Render Pass���õ�������attachment
        // Subpass��ͨ��VkAttachmentReference��������Щattachment
        // ����Щattachment�������˳�ʼ������layout�ģ���initialLayout��finalLayout
        // 
        // ��Subpass����Render Pass��attachment��ʱ�򣬶�����һ��layout����VkAttachmentReference���layout����
        // ���layout����ǰSubpass��ʼ��ʱ�򣬽����Զ������image��layoutת����ʲô��ʽ
        // �����������Subpassû��ʹ��ͬһ��attachment����ô����֮�䲻����layoutת������
        // �����������Subpassʹ����ͬһ��attachment����ô���attachment��ǰһ��Subpassʹ����֮����һ��Subpassʹ��֮ǰ����Ҫ��һ��layout��ת������
        // ���ת����Vulkan����������Subpass��attachment���������õ�layout���Զ�ת���ģ�����Ҫ�����ֶ�ת��
        // ����������Ҫ��ȷ�������ת��������ʱ��������ǳ���Vulkanϣ��������������ִ�У���GPU��Ҫ���õ���������������:
        // ��������û�в�������ȫ������������ִ����ЩSubpass����ô����ͺܼ��ˣ�attachment�ڵ�һ��Subpassִ����֮�󣬸ı�layout��ִ����һ��Subpass
        // �������layoutʵ����ֻӰ��image�Ķ�д����image����������Ⱦ���̵ĺ��ڲŻ��õ���ǰ���vertex shader�Ƚ׶ε�ִ�к�image�޹�
        // Ҳ����˵�����ǿ����ڵ�һ��Subpass��ûִ�����ʱ�򣬾��ȿ�ʼ����ִ�еڶ���Subpass
        // ����ͬһ��attachment�϶���������Subpassͬʱ��д��Ҳ����˵��Ȼ�ڶ���Subpass���Բ��õȵ�һ��Subpass��ȫ�������ٿ�ʼִ��
        // ���ǵڶ���Subpass�����Ҫ�õ��͵�һ��Subpass��ͬ��attachmentʱ��������Ҫ�ȴ���һ��Subpass�������attachment���ڶ���Subpass���ܶ�����з���
        // ���Զ������������ϵ��Ҫ����ľ����������:
        // 1��ǰһ��Subpass����ʲôʱ���������attachment
        // 2����һ��Subpass����ʲôʱ��ʼ��Ҫʹ�����attachment
        // ��ôһ��attachment��layout��ת�����ͻᷢ����ǰһ��Subpassʹ������֮����һ��Subpass��Ҫʹ����֮ǰ�����Ǿ���Ҫ��ȷ����������ʱ��������ʲô
        // ���ǰһ��Subpass����ʹ���У���һ��Subpass���Ѿ����е���Ҫʹ�����Ľ׶��ˣ���ô��һ��Subpass��ִ�оͻ����ȴ�
        // �����һ��Subpassִ�е�����Ҫʹ����ͬattachment��ʱ��ǰһ��Subpass�Ѿ�ʹ������(���߲���ʹ����ͬattachment)����ô�Ͳ�����ֵȴ�������ֱ�Ӳ���ִ��
        // 
        // ǰ��˵��Render Pass��attachment�������˳�ʼ������layout�ģ���initialLayout��finalLayout
        // ��ôSubpass֮���layoutת����ϵ���¿��Է�Ϊ3��:
        // 1��External��Subpass
        // 2��Subpass��External
        // 3��Subpass֮��
        // �ֱ��Ӧ3��layoutת����ϵ:
        // 1��initialLayout���׸�ʹ�ø�attachment��subpass����layout��һ��
        // 2��finalLayout�����ʹ�ø�attachment��subpass����layout��һ��
        // 3������subpass��ȡͬһ��attachement��������layout��ͬ��������ʵ�������ֶԴ�����layout
        // ���Բο�:https://zhuanlan.zhihu.com/p/350483554

        // ��������������ϵ�У���Ҫ�ȴ���һ��Subpass�������ʲô����
        // ���ȴ�srcStageMask�׶ε�srcAccessMask������ɺ�
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        // ����0��������Ҫ�ȴ�����׶����в�����ȫ������
        dependency.srcAccessMask = 0;

        // ��������������ϵ�У���һ��Subpass������ʲô����ſ�ʼ��Ҫ�ȴ���һ��Subpass���õĲ������
        // ����dstStageMask�׶ε�dstAccessMask����ִ��֮ǰ����Ҫ���ǰ��srcXXX���õľ��岽��
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // ���������ϵ����
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void createDescriptorSetLayout() {
        // ��������uniform buffer
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        // �����Ӧshader���������layout(binding = 0)
        uboLayoutBinding.binding = 0;
        // shader��uniform���ݾ������type
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // ���Դ���uniform���飬��������鳤��
        // ���磬�������Ϊ���������ĹǼ��е�ÿ������ָ��һ��trasform
        uboLayoutBinding.descriptorCount = 1;
        // �����������ֻ����vertex shader
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // ������ͼ�ģ���ʱ�ò���
        uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // ��������texture sampler��
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.descriptorCount = 1;
        // ���sampler����fragment shader���õ�
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerLayoutBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
        // ���������봴�����������Ľṹ��
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        // �Ѵ������������ݽṹ��ŵ�descriptorSetLayout��
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void createDescriptorPool() {
        // ��ȷ��������Ҫ�����������ͺ�����
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // ��Ϊ�����Ǹ�uniform buffer�õģ�����������uniform buffer����һ��
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        // sampler��������
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        // ���ÿ��ܷ����descriptor sets���������
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        // ��������ΪVK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT����ʾ�����ͷŵ�����������(descriptor set)
        // ������ʱ����
        poolInfo.flags = 0;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createDescriptorSets() {
        // ��Ϊ�����ж��uniform buffer������Vulkan�ӿ���Ҫ����ƥ������ݣ����԰�������layout����Ū�ɳ���һ�������飬��ʹ������һ����
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        // ����������������Ϣ
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        // ���ĸ�pool����
        allocInfo.descriptorPool = descriptorPool;
        // ����Ҫ����ʲô��������������������ʽ����
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        // ����һ��vector����
        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        // ���ýӿ�ʵ�ʷ�����������
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        // ʵ���������������
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            // ������������buffer��������������ṹ�������Ϣ
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            // ���������Ҫ��������buffer�����ݣ�Ҳ����ֱ����VK_WHOLE_SIZE����ʾ����buffer�Ĵ�С
            bufferInfo.range = sizeof(UniformBufferObject);

            // ��������sampler�Ľṹ��
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textureImageView;
            imageInfo.sampler = textureSampler;

            // ��������������Ҫ�õĽṹ�壬��ʵ�ʸ����������Ľӿ�vkUpdateDescriptorSets�Ĳ���
            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            // ����Ҫ���µ���������
            descriptorWrites[0].dstSet = descriptorSets[i];
            // ������������ĺ���
            descriptorWrites[0].dstBinding = 0;
            // ���������������������飬���ָ���������Ҫ�ӵڼ�����������ʼ����
            descriptorWrites[0].dstArrayElement = 0;
            // ����Ҫ���¶��ٸ�������
            descriptorWrites[0].descriptorCount = 1;
            // ����Ҫ���µ�����������
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            // ����3���������ǳ���ΪdescriptorCount������
            // ����������������ʲô�������ݵģ��������һ������
            // ����Ҫ���µ������������������buffer����Ϣ
            descriptorWrites[0].pBufferInfo = &bufferInfo;
            // ����image�õ�
            descriptorWrites[0].pImageInfo = nullptr; // Optional
            // ���������Ҳ��֪����ʲô�����õ�
            descriptorWrites[0].pTexelBufferView = nullptr; // Optional

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            // ������������������ӿڽ����������飬��һ������������Ҫ���µ������������飬�ڶ������������ڸ��������������ݵ�����
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
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
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        // Bindings:�������ݵļ�϶��ȷ��������ÿ�����������ÿ��instance(instancing)
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        // Attribute:������Ҫ���а󶨼��������ԵĶ�����ɫ���е������������
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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
        // �������ǵĶ���˳��Ӧ����˳ʱ��Ϊ���棬������Ϊ�����õ�GLM����͸��ͶӰ���󣬶�GLM�Ǹ�OpenGL��Ƶģ��ü��ռ��Y���Vulkan�Ƿ���
        // ���Ի�����Ⱦ���������µߵ��ģ����ǾͰ�͸��ͶӰ�����Y��ȡ����һ�£��ѻ���ת����
        // ������ᵼ�¶����˳��ʱ�뷽��Ҳ���ˣ�������Ȼ���ǵĶ���˳��ԭ����˳ʱ��Ϊ���棬��������Ҫ���ó���ʱ��
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
        // ���Ҳ������ɫ�ʻ�ϵ�һ�ַ�ʽ��������һ��ȫ�ֵ����ã����������ˣ�������Ǹ����þͻ�ʧЧ���൱�������blendEnable = VK_FALSE
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

        // shader��ͷд���Ǹ�layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        // shader���uniform��Ϣ
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        // 
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
        // ����PipelineLayout�������浽��Ա��������������
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // ����depth��stencil test��ز���
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        // ������Ȳ��Ժ�д��
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        // ����depth��С��fragment��Ҳ������ʾ������
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        // ���ڿ���ֻ��һ��depth��Χ�ڻ���fragment��һ�㲻�����
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
        // ����stencil�����Ĳ���
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional
        depthStencil.back = {}; // Optional


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
        pipelineInfo.pDepthStencilState = &depthStencil;
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
            std::array<VkImageView, 2> attachments = {
                swapChainImageViews[i],
                depthImageView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            // ָ�����Լ��ݵ�render pass(���¾������frame buffer��ָ����render pass��attachment��������������Ҫһ��)
            framebufferInfo.renderPass = renderPass;
            // ָ��attach��ȥ��VkImageView���������(���鳤��)
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
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

    void createDepthResources() {
        VkFormat depthFormat = findDepthFormat();
        // �ò鵽��format����depth image
        createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
        // ����depth image view
        depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
        // ��layoutת����depth stencilר�õ�(���ﲻ���������Ҳ�У���Ϊ��render pass��Ҳ�ᴦ��)
        transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }
    
    VkFormat findDepthFormat() {
        return findSupportedFormat(
            // ��������׼���õĸ�ʽ��Խǰ������ȼ�Խ��
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            // ��Ϊ���ͼ����Ҫ��CPU���ʣ�����TILING��OPTIMAL
            VK_IMAGE_TILING_OPTIMAL,
            // ��Ҫ֧�ֵ�Feature��֧��depth��stencil attachment
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        // ����Щ��ʽ����һ��Ӳ��֧�ֵĸ�ʽ����
        // �����ǰ�˳����ҵģ����Ӳ��֧�־����̷��أ��������������ʱ��Ӧ�ð�����ϣ����ȡ�ĸ�ʽд��ǰ��
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void createTextureImage() {
        // ��stb_image���Ӳ���ϵ�ͼƬ�ļ�
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("../../Textures/awesomeface.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        // �½�һ����ʱ��buffer��CPUд�����ݣ��������createVertexBufferһ��
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        // �����ݿ�������ʱbuffer
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);
        // ���ݿ�����Ϳ����ͷ���
        stbi_image_free(pixels);

        // usage����:
        // ��������Ҫ��һ��stagingBuffer�������ݣ�����Ҫдһ��VK_IMAGE_USAGE_TRANSFER_DST_BIT
        // Ȼ����дһ��VK_IMAGE_USAGE_SAMPLED_BIT��ʾ������shader�����������
        // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT����˼�����image���ڴ�ֻ��GPU�ɼ���CPU����ֱ�ӷ��ʣ�������bufferһ��
        createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

        // ����createImage��ʱ��initialLayout��VK_IMAGE_LAYOUT_UNDEFINED������ת���ɽ������ݵ�VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        // �����ݴ�stagingBuffer���Ƶ�image
        copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        // ��Ϊ���ǵ����image�Ǹ�shader�����õģ������ٰ�layoutת��VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
        // ��ʱbuffer��������
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
        // ����VkImage�Ľṹ��(VkImage��ʵ����ר��Ϊͼ��洢�Ż�����VkBuffer)
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        // ���Ǵ���2D����
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        // �߿�
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        // depth���Դ������Ϊ��ֱ���ϵ������������������3D����Ӧ�ö���1��
        imageInfo.extent.depth = 1;
        // mipmap����
        imageInfo.mipLevels = 1;
        // ͼƬ���鳤��?
        imageInfo.arrayLayers = 1;
        // ͼƬ��ʽ����Ҫ��stagingBuffer�ϵ����ݸ�ʽ��Ҳ����stbi_load�ĸ�ʽһ�£�����д�����ݵ�ʱ��Ҫ������
        imageInfo.format = format;
        // ������2������
        // VK_IMAGE_TILING_LINEAR: texel����Ϊ��������Ϊ����
        // VK_IMAGE_TILING_OPTIMAL: texel����Vulkan�ľ���ʵ���������һ��˳�����У���ʵ����ѷ���
        // �����layout��һ����һ������֮���ǹ̶��Ĳ��ܸģ����CPU��Ҫ��ȡ������ݣ�������ΪVK_IMAGE_TILING_LINEAR
        // ���ֻ��GPUʹ�ã�������ΪVK_IMAGE_TILING_OPTIMAL���ܸ���
        imageInfo.tiling = tiling;
        // ����ֻ����VK_IMAGE_LAYOUT_UNDEFINED����VK_IMAGE_LAYOUT_PREINITIALIZED
        // VK_IMAGE_LAYOUT_UNDEFINED��ζ�ŵ�һ��transition���ݵ�ʱ�����ݻᱻ����
        // VK_IMAGE_LAYOUT_PREINITIALIZED�ǵ�һ��transition���ݵ�ʱ�����ݻᱻ����
        // ���Ǻܶ����ʲô��˼�������һ��������CPUд�����ݣ�Ȼ��transfer������VkImage��stagingImage����Ҫ��VK_IMAGE_LAYOUT_PREINITIALIZED
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // ���VkImage����;���ʹ���VkBuffer�Ĳ���һ��
        imageInfo.usage = usage;
        // ֻ��һ�����д�ʹ�ã�������
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // ���ֻӰ�쵱��attachmentsʹ�õ�VkImage������������1_Bit
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        // ���Լ�һЩ��־����������;��ͼ�����Ż�������3D��ϡ��(sparse)ͼ��
        imageInfo.flags = 0; // Optional

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        // �����ڴ沢�󶨣���createBuffer��һģһ���ģ����ﲻ����ע����
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }
        vkBindImageMemory(device, image, imageMemory, 0);
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        // Vulkan�����Barrier���ڶ�����дؿ��ܻ�ͬʱʹ��һ����Դ��ʱ�򣬿�������ȷ����Դ��ͬ����Ҳ����һ����Դ������д����ûд��֮ǰ��ֹ�����ط���д
        // �����VK_SHARING_MODE_EXCLUSIVEģʽ��Ҳ����˵�����ж�����д�ͬʱ���ʵ��������������ת��image��layout��Ҳ��������ת�����дص�����Ȩ
        // ������Ϊ��ת��image layout�������õ���VkImageMemoryBarrier������һ��VkBufferMemoryBarrier�����ƵĶ���
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        // ת��ǰ��layout����������Ŀ�����VK_IMAGE_LAYOUT_UNDEFINED
        barrier.oldLayout = oldLayout;
        // ת�����layout
        barrier.newLayout = newLayout;
        // ����������������ת�����д�����Ȩ�ģ�������ǲ������ת����һ��Ҫ��ȷ����VK_QUEUE_FAMILY_IGNORED
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        // ����Ҫת����ͼ��
        barrier.image = image;
        // ���imageû��mipmap
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        // ���imageҲ��������
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        // image����;
        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            // ����µ�layout��������ͱ�ʾ������depth��stencil��
            // mask����depth
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            // ��ʱ����Ȼֻ��depth������Ҳ�������depth stencil layout�����Ի��ÿ�������format�ǲ�����stencil���ټ�stencil mask
            if (hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else {
            // ����Ĭ�Ͼ�������color��
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }


        // Barrier����Ҫ���������ڿ���ͬ����
        // �����ָ��ʲô�����Ǳ��������Barrier֮ǰ��ɵ�
        // barrier.srcAccessMask = 0;
        // �����ָ��ʲô��������Ҫ�ȵ����Barrier֮����ܿ�ʼ��
        // barrier.dstAccessMask = 0;

        // ָ��Ӧ����Barrier֮ǰ��ɵĲ������ڹ�������ĸ�stage
        VkPipelineStageFlags sourceStage;
        // ָ��Ӧ�õȴ�Barrier�Ĳ������ڹ�������ĸ�stage
        VkPipelineStageFlags destinationStage;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            // ����ǰ�һ��initialLayoutΪVK_IMAGE_LAYOUT_UNDEFINED����image��ת��ΪVK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            // ��Ϊ����Ҫ�����ݴ�stagingBuffer���Ƶ��µ�image������image��layout��Ҫת��ΪVK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL����������

            // ����Ҫ�κεȴ������ƣ��������̿�ʼ����
            barrier.srcAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // �����һ�������stage����˼�������stage

            // transfer���ݵ�д���������Ҫ�����Barrier֮��
            // ��������һ��layoutת��ָ������൱�ڴ�buffer�������ݵ�image�Ĳ�������Ҫ�����layoutת�����
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // ���stage��ʵ������ʵ��������Ⱦ�����ϵģ���һ����ʾtransfer���ݲ�����α�׶�
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            // �����ϵ���˼�ǣ����ݸոտ����꣬���ǽ��ܿ������ݵ�layout������ת���ɺ���Ҫ�õģ���shader�����ĸ�ʽ

            // ���layoutת������Ҫ��transfer���ݵĲ�����ɺ��ٽ���
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // ���stage��ʵ������ʵ��������Ⱦ�����ϵģ���һ����ʾtransfer���ݲ�����α�׶�

            // fragment shader����������������Ҫ�����layoutת������
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            // �½�depth image��ʱ�򣬰�layoutת��depth stencilר�õ�layout
            
            // ��Ϊ������depth image������ǰ��û�в�����ɶ�����õ�
            barrier.srcAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

            // ���ͼ�Ķ�д��Ҫ�����layoutת�����
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            // depth���Է�����VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
            // depthд�뷢����VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
            // ������һ������Ľ׶Σ���ȷ���õ�ʱ����׼�����˵�
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            // ָ��Ӧ����Barrier֮ǰ��ɵĲ������ڹ�������ĸ�stage
            sourceStage,
            // ָ��Ӧ�õȴ�Barrier�Ĳ������ڹ�������ĸ�stage�������������������stage��:https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap7.html#synchronization-access-types-supported
            // ע�������stage��Ҫ��VkImageCreateInfo��usageƥ�䣬�㲻�ܶ�һ������shader��image��һ����ͼ�ε�stage
            destinationStage,
            // ���������0����VK_DEPENDENCY_BY_REGION_BIT��������ζ�������ȡ��ĿǰΪֹ��д�����Դ����(��˼Ӧ��������д����;ȥ�����о���ɧ����)
            0,
            // VkMemoryBarrier����
            0, nullptr,
            // VkBufferMemoryBarrier����
            0, nullptr,
            // VkImageMemoryBarrier����
            1, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        // �������Ҫ��һ��buffer�ϸ������ݵ�image�ϣ�������Ҫָ������buffer�ϵ���һ�������ݣ���image����һ����
        VkBufferImageCopy region{};
        // ��buffer��ȡ���ݵ���ʼƫ����
        region.bufferOffset = 0;
        // ������������ȷ�������ڴ���Ĳ��ַ�ʽ���������ֻ�Ǽ򵥵Ľ����������ݣ�����0
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        // ����4��������������������Ҫ�����ݿ�����image����һ����
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        // ���Ҳ������������Ҫ��ͼ�񿽱�����һ����
        // ���������ͼƬ��offset��ȫ��0��extent��ֱ����ͼ��߿�
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            // image��ǰ��layout�����Ǽ����Ѿ�������Ϊר�Ž������ݵ�layout��
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            // ����VkBufferImageCopy���飬������һ��ָ����д�����ͬ�����ݿ�������
            1, &region
        );

        endSingleTimeCommands(commandBuffer);
    }

    void createTextureImageView() {
        textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createTextureSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        // ������������ܶȴ��ں�С��texel�ܶȵ�ʱ��filterͼ��ķ�ʽ
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        // ���ò�����������Χʱ�Ĳ�����ʽ
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        // ������������filter�������������ʵ�ڲ��У�һ�㶼�Ὺ
        samplerInfo.anisotropyEnable = VK_TRUE;
        // ��һ��Ӳ��֧�ֶ��ٱ��ĸ������ԣ��������(���õĻ���1)
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        // ����addressMode����ΪCLAMP_TO_BORDER��ʱ��border������ʲô��ɫ������ֻ����һЩ�̶������������Լ���������
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        // ������false������������귶Χ����������[0, 1)�������true���ͻ���[0, texWidth)��[0, texHeight)�����󲿷�����¶�����[0, 1)
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        // һ���ò��������ĳЩ����������shadow map��percentage-closer filtering���õ�
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        // mipmap����
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        // �½�һ����ʱ��buffer��CPUд������
        // ��ΪVulkan��buffer�ڴ�������ıȽ�ϸ�£���������CPU�Ƿ���Է��ʵģ���Щbuffer�����ݣ�һ�������˾�ֻ�ᱻGPU���ʣ�CPU��߲�����Ҫ����
        // �������ݾͿ�����ȷ����һ��CPU����ֱ�ӷ��ʵ�buffer����ţ��ڴ������Ǹ��õ�
        // ��������buffer�����ݿ϶�������ҪCPU�ṩ������������Ҫ������һ��CPU���Է��ʵ�buffer������ΪstagingBuffer����CPU������д��stagingBuffer
        // Ȼ�����½���һ��vertexBuffer��Ϊ����������ݵ�buffer��ͨ��һ��command����GPU�Ǳ߰����ݴ�stagingBuffer���Ƶ�vertexBuffer��
        // ����������vertexBuffer�Ͳ���Ҫ��CPU���ʣ�Vulkan��������ڴ��Ż�
        // stagingBuffer��Ҫ����ΪVK_BUFFER_USAGE_TRANSFER_SRC_BIT����ʾΪ���������ڴ�transfer������Դbuffer
        // vertexBuffer��Ϊ�������ݵ�һ������Ҫ����ΪVK_BUFFER_USAGE_TRANSFER_DST_BIT����ʾ���������ڴ�transfer������Ŀ��buffer
        // ���һ���Ҫͬʱ����ΪVK_BUFFER_USAGE_VERTEX_BUFFER_BIT��ʾ��������Ŷ������ݵ�
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        // ���������VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT��һ���̶����
        // ����ָCPU���Է��ʣ����ҽ������������memcpy�������ݵ�ʱ�򣬿��ܻ�������һЩ���⣬���忴����
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        // ������Ҫ�Ѷ��������������Ǹո�������ڴ���
        // �½�һ����ʱָ������ȡ���ǵ�buffer�ڴ��ַ
        void* data;
        // ��stagingBufferMemory�ĵ�ַӳ�䵽�����ʱָ���ϣ������ڶ���������ǰVulkan�汾��û��ʵ�֣��ȹ̶���0
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        // �������ǵĶ������ݵ�ָ����ַ��
        // ����Vulkan���ܲ������������ݸ��Ƶ�buffer��Ӧ���ڴ��У�������Ϊcache�����⣬����Ҳ�п�������Ϊ��buffer��д���mapped memory���ɼ�
        // �����ѯ�����������ڴ�ʱ�����VK_MEMORY_PROPERTY_HOST_COHERENT_BIT���ǽ����Щ�����
        // ����ʵ���ϻ������ܸ��õ��Ǹ��鷳�Ľ����������:https://vulkan-tutorial.com/en/Vertex_buffers/Vertex_buffer_creation
        memcpy(data, vertices.data(), (size_t)bufferSize);
        // ���ӳ��
        vkUnmapMemory(device, stagingBufferMemory);
        // ������Vulkan�������������������д�룬����ʵ��������Vulkan���첽��ƣ���ʱ���ݲ������̴���GPU
        // �������ǲ��ù����������ϸ�ڣ�Vulkan�ᱣ֤�������´ε���vkQueueSubmit֮ǰ(Ҳ����ʵ���ύ��Ⱦ����֮ǰ)GPU�ǿ�����ȷ��ȡ��Щ���ݵ�

        // �����������ڴ�Ŷ������ݵ�buffer��VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT�����Ѿ����͹���
        // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT����˼�����buffer���ڴ�ֻ��GPU�ɼ���CPU����ֱ�ӷ���
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

        // �����Լ�ʵ�ֵĺ�������buffer����
        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

        // ��ʱbuffer������������ٲ��ͷ��ڴ�
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void createIndexBuffer() {
        // ���������createVertexBuffer������һ���ģ��Ͳ�����ע����
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t)bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        // ������VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        // ��Щ���ڴ���uniform���ݵ�buffer����Ϊÿһ֡��������д�룬ֱ������ΪCPU�ɼ���ģʽ����
        // ����Ҳû��Ҫ��stagingBuffer��transfer������
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

            // ������ڴ�ӳ�䵽ָ������uniformBuffersMapped����֮�������
            // ���������������ݣ���Ϊ����ÿһ֡��Ⱦ��ʱ����ȥ������
            // ����Ҳ����ҪvkUnmapMemory����Ϊ����ÿһ֡����Ҫ�����ָ��ȥ�����ݣ��������Unmap�ˣ���ÿһ֡��������map
            // �������persistent mapping
            // ���ӳ��Ӧ�ú�uniformBuffers��������������һ��
            vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
        }
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        // buffer���ڴ�ռ�ô�С
        bufferInfo.size = size;
        // ˵��buffer����;
        bufferInfo.usage = usage;
        // ��ȷ���buffer��ֻ��һ�������Ķ��д�ʹ�û��Ƕ�����д�֮��Ṳ��
        // VK_SHARING_MODE_EXCLUSIVE����ֻ��һ�����д�ʹ����
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // ��������sparse�ڴ棬��ʱ����
        bufferInfo.flags = 0;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vertex buffer!");
        }

        // ��ѯ���buffer����Ҫ���ڴ���Ϣ������ڴ���Ϣ����3������
        // size: ��bytes��ʾ�������ڴ��С, ���ܺ�����ǰ�����õ�bufferInfo.size��һ��
        // alignment: The offset in bytes where the buffer begins in the allocated region of memory, depends on bufferInfo.usageand bufferInfo.flags.
        // memoryTypeBits : �ʺ����buffer���ڴ�����(��bits��ʽ���)
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        // �����ڴ��������Ϣ
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        // ������ڴ��С
        allocInfo.allocationSize = memRequirements.size;
        // �������ǲ��ҵ��ķ���Ҫ����ڴ�Type����
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        // �����ڴ棬�ŵ�bufferMemory���������������������ڴ���Ҫ�ֶ���vkFreeMemory
        // ע�����vkAllocateMemory������ʵ�ǲ��Ƽ�ʹ�õģ���������Ϊ�е��˷��ڴ�ɣ�����������ӿ�������ڴ�����������ͬʱ���ڳ���maxMemoryAllocationCount��
        // ��ʹ��GTX 1080���ֱȽϸ߶˵��Կ��ϣ��������Ҳֻ��4096
        // ����ʵ����������VulkanӦ�ã���Ҫ�Լ�ʵ��allocator������Ҳ�п�Դ�Ŀ�:https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vertex buffer memory!");
        }

        // ��buffer�͸���������ڴ������
        // ���ĸ��������ڴ��ϵ�ƫ������û�о���0
        // ���ƫ��������0������Ҫ��memRequirements.alignment�ı���
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    VkCommandBuffer beginSingleTimeCommands() {
        // ��Ҫ����һ����ʱ��command�������buffer�ĸ���
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        // ������ʱcommand������ר�Ž�һ�����Ը����ʵ�commandPool����������ֱ�������poolҲ��
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        // ���������ʱcommand buffer
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        // ֱ�ӿ�ʼrecord command
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // ��ȷ����Ϊһ���Ե�command
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        // ����Vulkan��ʼrecord
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        // ����record��
        vkEndCommandBuffer(commandBuffer);

        // ֱ�ӿ�ʼ׼���ύcommand
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        // ��ָ���ύ�����ǵ�graphicsQueueִ��
        // �������ύ��VkQueue����Ҫ֧��VK_QUEUE_TRANSFER_BIT�ģ��������ǵ�graphicsQueue��֧��VK_QUEUE_GRAPHICS_BIT��
        // ���֧��VK_QUEUE_GRAPHICS_BIT�Ļ���Ҳһ��֧��VK_QUEUE_TRANSFER_BIT������ֱ����graphicsQueue����
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        // ��סVulkan���첽��ƣ�vkQueueSubmit�ύ��ָ��󣬲���ȵ�GPUִ����ͻ����̷���
        // �����������vkQueueWaitIdle��GPUִ����command�ټ���ִ�к���Ĵ���
        // ��Ϊ����ֻ��һ��ָ�����ֱ�������ֱȽϱ�׾�ķ�ʽ�ȴ�����������vkQueueSubmit�����һ����������VkFence
        // ����кܶ�ָ���Ҫ�ֶ�����ִ�У������Ǹ�VkFence�Ļ��ƶ��������Wait
        vkQueueWaitIdle(graphicsQueue);

        // ִ���������������һ����command
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        // ׼��CopyBufferָ������Ҫ����Ϣ
        VkBufferCopy copyRegion{};
        // ƫ����
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        // ���ݴ�С
        copyRegion.size = size;
        // ��¼CopyBufferָ��
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        // �Ȳ�ѯ��ǰӲ��֧�ֵ��ڴ�����
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        // ���ҷ����������ڴ�Type����
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        // �Ҳ����Ļ��׳��쳣
        throw std::runtime_error("failed to find suitable memory type!");
    }

    void createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        // VK_COMMAND_BUFFER_LEVEL_PRIMARY: �����ύ������ִ�У������ܴ������������������
        // VK_COMMAND_BUFFER_LEVEL_SECONDARY: �޷�ֱ���ύ�����ǿ��Դ��������������
        // �����һЩCommand Buffer�ǲ��ģ�����ͨ��SECONDARY��ʵ��һЩͨ�õĲ���
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        // ��������ֻ����һ��command buffer
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
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

        // ����ͼ���ʹ�� vkCmdBeginRenderPass ����Render Pass��ʼ
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        // ʹ�ò���ָ���Ľ������е�һ��Frame Buffer(���Ӧ����fragment shaderҪд���buffer)
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        // ���render area������shader��Ҫ���غʹ洢��λ��(û����)
        renderPassInfo.renderArea.offset = { 0, 0 };
        // һ����˵��С(extend)�Ǻ�framebuffer��attachmentһ�µģ����С�˻��˷ѣ����˳���ȥ�Ĳ�����һЩδ������ֵ
        renderPassInfo.renderArea.extent = swapChainExtent;
        // ��VK_ATTACHMENT_LOAD_OP_CLEAR��������clear color
        // ע�����clearValues�����˳��Ӧ�ú�attachment��˳�򱣳�һ��
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        // Vulkan����depth�ķ�Χ��0��1��1������Զ
        clearValues[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        // ���ڿ��Կ�ʼrender pass�ˣ�����record commands�Ľӿڶ�����vkCmdΪǰ׺��
        // ��Щ��������void����Ҫ�ȵ����ǽ���command��record��Ż᷵�ش���
        // �������������������
        // VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed.
        // VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: The render pass commands will be executed from secondary command buffers.
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);


        // ������֮ǰ����ͼ����Ⱦ�Ĺ���
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        // ����֮ǰ�ڴ���Pipeline��ʱ�򣬰�viewport��scissor���óɶ�̬���ˣ�������Ҫ����������һ��
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

        VkBuffer vertexBuffers[] = { vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        // �󶨶���buffer����23������ָ��������Ҫָ����vertex buffer��binding��ƫ����������
        // ��4������ָ��������Ҫ�󶨵�vertex buffer����
        // ��5������ָ����Ҫ��ȡ��Ӧ��vertex buffer����ʱ����byteΪ��λ��ƫ����
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        // �󶨶�������
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        // ����������uniform����������
        // ����2��ָ�����ǰ󶨵���ͼ�ι��ߣ���Ϊ������Ҳ�������ڷ�ͼ�ι���
        // ����3��ָ���ǰ󶨵�������ߵ�layout(�������ߵ�ʱ�������layout������������������Ϣ)
        // ����4��ָ�Ѳ���6������������ĵڼ�����������������һ��
        // ����5��Ҫ�󶨶��ٸ���������
        // ����6��������������
        // ����78��һ��ƫ�������飬���ڶ�̬����������
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

        // DrawCallָ�����4�������ֱ���
        // vertexCount: Ҫ���ƵĶ�������
        // instanceCount : GPU Instance������������õĻ�����1
        // firstVertex : ��vertex buffer�ϵ�ƫ����, ������gl_VertexIndex����Сֵ
        // firstInstance : GPU Instance��ƫ����, ������gl_InstanceIndex����Сֵ
        // vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

        // IndexedDrawCallָ�����5�������ֱ���
        // vertexCount: Ҫ���ƵĶ�������
        // instanceCount : GPU Instance������������õĻ�����1
        // firstIndex : ��һ��������±꣬������1�Ļ�GPU�ͻ�������еڶ������㿪ʼ��
        // vertexOffset : ����ƫ����������index����������ֶ�����������ֵ
        // firstInstance : GPU Instance��ƫ����, ������gl_InstanceIndex����Сֵ
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        // ����render pass
        vkCmdEndRenderPass(commandBuffer);

        // ����command�Ĵ���
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        // ����Semaphore�Ľṹ�壬Ŀǰ��ʱû���κβ�����Ҫ���ã�Vulkan��δ���������
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        // ����Fence�Ľṹ��
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // ����ʱ��������Ϊsignaled״̬(�����һ֡��vkWaitForFences��Զ�Ȳ������)
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    void drawFrame() {
        // ����һ֡�������
        // ����2��3����fense���飬����4Ϊtrue��ʾ�ȴ�����fense���Ϊ����ɣ�false��ʾ������һ����ɼ��ɣ�����5�ǵȴ������ʱ��(��64λ����޷���int����ʾ�ر����ȴ�ʱ��)
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        
        // �ӽ������л�ȡһ�����õ�image����������õ�image��VkFrameBuffer����±�д��imageIndex�У�ͨ������±�ȥʵ�ʻ�ȡimage
        // ��3�������ǵȴ������ʱ�䣬����64λ����޷���int����ʾ�ر�����ȴ�
        // ��4��5��������ʾ��Ҫ�ȴ���Ϊsignaled״̬��Semaphore��Fence
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        // ��������ʾ��������Surface�Ѿ��������ˣ����ܼ������ˣ�һ���Ǵ��ڴ�С�仯���µ�
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // ���ʱ����Ҫ���´����������������µ�Surface
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // �ȴ���һ֡���ƽ���������ȷ������Ҫ������һ֡�Ļ��ƺ��ֶ�����Щfense����Ϊunsignaled
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        // commandBuffer��ÿ֡��֮ǰ������һ��
        // �ڶ���������VkCommandBufferResetFlagBits���������ﲻ��Ҫ�����κα�־��������0
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        // ��������д�ĺ�����recordһ��ʵ�ʻ���ͼ���commandBuffer
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        // ÿ֡����һ��uniform buffer�������
        updateUniformBuffer(currentFrame);

        // �����ύcommand����Ҫ����Ϣ
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        // ������Ҫ�ȴ���Semaphore����������ֻ��Ҫ��image����
        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        // ��������Ⱦ���ߵ��ĸ��׶εȴ�Semaphore�����pWaitDstStageMask�����飬��pWaitSemaphores��һһ��Ӧ��
        // ����˵pWaitDstStageMask[i]��Ҫ�ȴ�pWaitSemaphores[i]
        // ��Ϊ���ǵȴ����ǿ��õ�image�������image�����color��ʱ��Ż��õ���������������ΪVK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        // ��˼�ǣ�ǰ�����Щvertex��geometry shader���GPU�п��п���������
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.pWaitDstStageMask = waitStages;
        // ���õ�ǰ���VkSubmitInfo��������Щcommand buffer��
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        // ����������Щcommand buffersִ�н�������ЩSemaphore��Ҫ�����ź�(���Ǳ��signaled״̬)
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // ��VkSubmitInfo�����ύ��ָ����VkQueue��
        // ��4��������ʾ����ύ������command buffers��ִ����֮����Ҫ���signaled״̬��VkFence
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }


        // �����Ҫ��ִ�н�������ύ���������ϣ���������չʾ����Ļ��
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        // ������Ҫ�ȴ���ЩSemaphore���signaled״̬�󣬲ſ�������ִ��presentation
        // �������Ǿ�����Ϊ������Щcommandsִ����󼴿�
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        // ������Ҫ��imageչʾ����Щ���������ĸ�image�ϣ���������image���Ǵ��ݵ�����
        // ��˼�ǰ�image�ύ��pSwapchains[i][pImageIndices[i]]�ϣ���������������ĳ�����һ���ģ���һ��swapchainCount����
        // �������󲿷�����£�swapchainCount����1
        VkSwapchainKHR swapChains[] = { swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        // ����һ����ѡ�Ĳ�����������������˶���������������������������һ��VkResult����������ÿ����������presentationִ�н��
        // ����ֻ��һ���������Ļ����Բ����������
        presentInfo.pResults = nullptr;
        // ���հ�image present����������
        result = vkQueuePresentKHR(presentQueue, &presentInfo);
        // VK_ERROR_OUT_OF_DATE_KHR��ʾ��������Surface�Ѿ��������ˣ����ܼ������ˣ��������´���������
        // VK_SUBOPTIMAL_KHR��ʾ���������ǿ��Լ����ã����Ǻ�Surface��ĳЩ����ƥ��ò��Ǻܺã������´���Ҳ��
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        // CPU��ǰ������һ֡
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void recreateSwapChain() {
        int width = 0, height = 0;
        // ��ȡһ�µ�ǰ�Ĵ��ڴ�С
        glfwGetFramebufferSize(window, &width, &height);
        // ������ڴ�СΪ0(����С����)����ô�����������ȴ���ֱ���������µ���
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        // �ȵ��߼��豸ִ���굱ǰ���������ռ����Դ
        vkDeviceWaitIdle(device);

        // ��ֱ�������ԭ���Ľ����������Դ
        cleanupSwapChain();

        // ���µ��ô����������Ľӿ�
        createSwapChain();
        // ImageView��FrameBuffer��������������Image�ģ�����Ҳ��Ҫ���´���һ��
        createSwapChainImageViews();
        // depth bufferҲҪ���´���һ��
        createDepthResources();
        createFramebuffers();
    }

    void updateUniformBuffer(uint32_t currentImage) {
        // ����һ�µ�ǰ���е�ʱ��
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        // ����MVP����
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
        // GLMԭ����ΪOpenGL��Ƶģ�Vulkan�Ĳü�����Y���OpenGL�Ƿ��ģ���������͸��ͶӰ�����Y��ȡ��һ��
        ubo.proj[1][1] *= -1;

        // �����ݸ��Ƶ�ָ��uniform buffer�ڴ��ַ��ָ��
        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
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