#include "surface_utils.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <webgpu/webgpu_cpp.h>

wgpu::Surface CreateSurfaceForWindow(const wgpu::Instance& instance, GLFWwindow* window) {
    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    NSView* view = [nsWindow contentView];

    [view setWantsLayer:YES];
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    [view setLayer:metalLayer];

    // Renamed in newer WebGPU spec: SurfaceDescriptorFromMetalLayer → SurfaceSourceMetalLayer
    WGPUSurfaceSourceMetalLayer metalDesc = {};
    metalDesc.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
    metalDesc.layer = metalLayer;

    WGPUSurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&metalDesc);

    return wgpu::Surface::Acquire(wgpuInstanceCreateSurface(instance.Get(), &surfaceDesc));
}
