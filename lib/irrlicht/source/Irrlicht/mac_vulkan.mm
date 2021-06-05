#include "mac_vulkan.h"
#import <AppKit/NSApplication.h>
namespace MacVulkan
{
bool supportsVulkan()
{
    // MacOSX 10.11 or later supports Metal (MoltenVK)
    if (floor(NSAppKitVersionNumber) <= NSAppKitVersionNumber10_10_Max)
        return false;
    return true;
}   // supportsVulkan

}
