#include "ge_vulkan_driver.hpp"

#ifdef _IRR_COMPILE_WITH_VULKAN_
#include "SDL_vulkan.h"
#include <set>
#include <sstream>
#include <stdexcept>

#include "../source/Irrlicht/os.h"

#ifndef __APPLE__
extern "C" PFN_vkVoidFunction _vk_getInstanceProcAddr(VkInstance instance,
                                                      const char* name)
{
    PFN_vkGetInstanceProcAddr f =
        (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    // Workaround for android vulkan driver return NULL function pointer with
    // non-NULL instance
    // See https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetInstanceProcAddr.html
    // Also slient the warnings when loading with NULL instance in android
    if (strcmp(name, "vkGetInstanceProcAddr") == 0)
        return (PFN_vkVoidFunction)f;
    static std::set<std::string> instance_function =
    {{
        "vkEnumerateInstanceVersion",
        "vkEnumerateInstanceExtensionProperties",
        "vkEnumerateInstanceLayerProperties",
        "vkCreateInstance",
    }};
    if (instance_function.find(name) != instance_function.end())
        return f(NULL, name);
    else if (instance == NULL &&
        instance_function.find(name) == instance_function.end())
        return NULL;
    return f(instance, name);
}   // _vk_getInstanceProcAddr
#endif

namespace GE
{
GEVulkanDriver::GEVulkanDriver(const SIrrlichtCreationParameters& params,
                               io::IFileSystem* io, SDL_Window* window)
              : CNullDriver(io, core::dimension2d<u32>(0, 0))
{
    m_physical_device = VK_NULL_HANDLE;
    m_graphics_queue = VK_NULL_HANDLE;
    m_present_queue = VK_NULL_HANDLE;
    m_graphics_family = m_present_family = 0;
    m_properties = {};
    m_features = {};

    createInstance(window);

#ifndef __APPLE__
    if (gladLoadVulkanUserPtr(NULL,
        (GLADuserptrloadfunc)_vk_getInstanceProcAddr, m_vk.instance) == 0)
    {
        throw std::runtime_error("gladLoadVulkanUserPtr failed "
            "with non-NULL instance");
    }
#endif

    if (SDL_Vulkan_CreateSurface(window, m_vk.instance, &m_vk.surface) == SDL_FALSE)
        throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
    m_device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    findPhysicalDevice();
    createDevice();

#ifndef __APPLE__
    if (gladLoadVulkanUserPtr(m_physical_device,
        (GLADuserptrloadfunc)_vk_getInstanceProcAddr, m_vk.instance) == 0)
    {
        throw std::runtime_error("gladLoadVulkanUserPtr failed with "
            "non-NULL instance and non-NULL m_physical_device");
    }
#endif

    vkGetPhysicalDeviceProperties(m_physical_device, &m_properties);
    os::Printer::log("Vulkan version", getVulkanVersionString().c_str());
    os::Printer::log("Vulkan vendor", getVendorInfo().c_str());
    os::Printer::log("Vulkan renderer", m_properties.deviceName);
    os::Printer::log("Vulkan driver version", getDriverVersionString().c_str());
    for (const char* ext : m_device_extensions)
        os::Printer::log("Vulkan enabled extension", ext);
}   // GEVulkanDriver

// ----------------------------------------------------------------------------
GEVulkanDriver::~GEVulkanDriver()
{
}   // ~GEVulkanDriver

// ----------------------------------------------------------------------------
void GEVulkanDriver::createInstance(SDL_Window* window)
{
#ifndef __APPLE__
    if (gladLoadVulkanUserPtr(NULL,
        (GLADuserptrloadfunc)_vk_getInstanceProcAddr, NULL) == 0)
    {
        throw std::runtime_error("gladLoadVulkanUserPtr failed 1st time");
    }
#endif

    unsigned int count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, NULL))
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed with NULL extensions");
    std::vector<const char*> extensions(count, NULL);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data()))
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed with extensions vector");
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.enabledExtensionCount = extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();
    VkResult result = vkCreateInstance(&create_info, NULL, &m_vk.instance);
    if (result != VK_SUCCESS)
        throw std::runtime_error("vkCreateInstance failed");
}   // createInstance

// ----------------------------------------------------------------------------
void GEVulkanDriver::findPhysicalDevice()
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_vk.instance, &device_count, NULL);

    if (device_count < 1)
        throw std::runtime_error("findPhysicalDevice has < 1 device_count");

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_vk.instance, &device_count, &devices[0]);

    for (VkPhysicalDevice& device : devices)
    {
        uint32_t graphics_family = 0;
        uint32_t present_family = 0;

        bool success = findQueueFamilies(device, &graphics_family, &present_family);

        if (!success)
            continue;

        success = checkDeviceExtensions(device);

        if (!success)
            continue;

        VkSurfaceCapabilitiesKHR surface_capabilities;
        std::vector<VkSurfaceFormatKHR> surface_formats;
        std::vector<VkPresentModeKHR> present_modes;

        success = updateSurfaceInformation(device, &surface_capabilities,
            &surface_formats, &present_modes);

        if (!success)
            continue;

        vkGetPhysicalDeviceFeatures(device, &m_features);
        m_graphics_family = graphics_family;
        m_present_family = present_family;
        m_surface_capabilities = surface_capabilities;
        m_surface_formats = surface_formats;
        m_present_modes = present_modes;
        m_physical_device = device;
        break;
    }

    if (m_physical_device == VK_NULL_HANDLE)
        throw std::runtime_error("findPhysicalDevice m_physical_device is VK_NULL_HANDLE");
}   // findPhysicalDevice

// ----------------------------------------------------------------------------
bool GEVulkanDriver::checkDeviceExtensions(VkPhysicalDevice device)
{
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count,
        &extensions[0]);

    std::set<std::string> required_extensions(m_device_extensions.begin(),
        m_device_extensions.end());

    for (VkExtensionProperties& extension : extensions)
        required_extensions.erase(extension.extensionName);

    return required_extensions.empty();
}   // checkDeviceExtensions

// ----------------------------------------------------------------------------
bool GEVulkanDriver::updateSurfaceInformation(VkPhysicalDevice device,
                                              VkSurfaceCapabilitiesKHR* surface_capabilities,
                                              std::vector<VkSurfaceFormatKHR>* surface_formats,
                                              std::vector<VkPresentModeKHR>* present_modes)
{
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_vk.surface, &format_count, NULL);

    if (format_count < 1)
        return false;

    uint32_t mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_vk.surface, &mode_count, NULL);

    if (mode_count < 1)
        return false;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_vk.surface, surface_capabilities);

    (*surface_formats).resize(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_vk.surface, &format_count,
        &(*surface_formats)[0]);

    (*present_modes).resize(mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_vk.surface, &mode_count,
        &(*present_modes)[0]);

    return true;
}   // updateSurfaceInformation

// ----------------------------------------------------------------------------
bool GEVulkanDriver::findQueueFamilies(VkPhysicalDevice device,
                                       uint32_t* graphics_family,
                                       uint32_t* present_family)
{
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    if (queue_family_count == 0)
        return false;

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
        &queue_families[0]);

    bool found_graphics_family = false;
    bool found_present_family = false;

    for (unsigned int i = 0; i < queue_families.size(); i++)
    {
        if (queue_families[i].queueCount > 0 &&
            queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            *graphics_family = i;
            found_graphics_family = true;
            break;
        }
    }

    for (unsigned int i = 0; i < queue_families.size(); i++)
    {
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_vk.surface, &present_support);

        if (queue_families[i].queueCount > 0 && present_support)
        {
            *present_family = i;
            found_present_family = true;
            break;
        }
    }

    return found_graphics_family && found_present_family;
}   // findQueueFamilies

// ----------------------------------------------------------------------------
void GEVulkanDriver::createDevice()
{
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    float queue_priority = 1.0f;

    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = m_graphics_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(queue_create_info);

    queue_create_info.queueFamilyIndex = m_present_family;
    queue_create_infos.push_back(queue_create_info);

    VkPhysicalDeviceFeatures device_features = {};
    if (m_features.samplerAnisotropy == VK_TRUE)
        device_features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = queue_create_infos.size();
    create_info.pQueueCreateInfos = &queue_create_infos[0];
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = m_device_extensions.size();
    create_info.ppEnabledExtensionNames = &m_device_extensions[0];
    create_info.enabledLayerCount = 0;

    VkResult result = vkCreateDevice(m_physical_device, &create_info, NULL, &m_vk.device);

    if (result != VK_SUCCESS)
        throw std::runtime_error("vkCreateDevice failed");

    vkGetDeviceQueue(m_vk.device, m_graphics_family, 0, &m_graphics_queue);
    vkGetDeviceQueue(m_vk.device, m_present_family, 0, &m_present_queue);
}   // createDevice

// ----------------------------------------------------------------------------
std::string GEVulkanDriver::getVulkanVersionString() const
{
    std::stringstream vk_version;
    vk_version << VK_VERSION_MAJOR(m_properties.apiVersion) << "." <<
        VK_VERSION_MINOR(m_properties.apiVersion) << "." <<
        VK_VERSION_PATCH(m_properties.apiVersion);
    return vk_version.str();
}   // getVulkanVersionString

// ----------------------------------------------------------------------------
std::string GEVulkanDriver::getDriverVersionString() const
{
    std::stringstream driver_version;
    // Following https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/master/includes/functions.php
    if (m_properties.vendorID == 0x10DE)
    {
        // NVIDIA
        driver_version << ((m_properties.driverVersion >> 22) & 0x3ff) << "." <<
            ((m_properties.driverVersion >> 14) & 0x0ff) << "." <<
            ((m_properties.driverVersion >> 6) & 0x0ff) << "." <<
            (m_properties.driverVersion & 0x003f);
    }
#ifdef WIN32
    if (m_properties.vendorID == 0x8086)
    {
        // Intel on Windows
        driver_version << (m_properties.driverVersion >> 14) << "." <<
            (m_properties.driverVersion & 0x3fff);
    }
#endif
    // Use Vulkan version conventions if vendor mapping is not available
    driver_version << (m_properties.driverVersion >> 22) << "." <<
        ((m_properties.driverVersion >> 12) & 0x3ff) << "." <<
        (m_properties.driverVersion & 0xfff);
    return driver_version.str();
}   // getDriverVersionString

}

namespace irr
{
namespace video
{
    IVideoDriver* createVulkanDriver(const SIrrlichtCreationParameters& params,
                                     io::IFileSystem* io, SDL_Window* window)
    {
        return new GE::GEVulkanDriver(params, io, window);
    }   // createVulkanDriver
}
}
#endif
