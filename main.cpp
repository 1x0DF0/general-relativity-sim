#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>

#include <webgpu/webgpu_cpp.h>
#include <GLFW/glfw3.h>

#include "metric_tensor.hpp"
#include "surface_utils.h"

// -----------------------------------------------------------------------
// Camera state (modified by GLFW callbacks)
// -----------------------------------------------------------------------

static float  g_r_cam     = 15.0f;
static float  g_cam_theta = 3.14159265f * 0.5f - 0.48f;  // ~27° above disk
static float  g_cam_phi   = 0.0f;

static bool   g_mouse_down = false;
static double g_last_mx    = 0.0;
static double g_last_my    = 0.0;

static void scroll_callback(GLFWwindow*, double /*xoff*/, double yoff) {
    // Multiplicative zoom — feels natural at any distance
    g_r_cam *= std::pow(0.88f, static_cast<float>(yoff));
    g_r_cam  = std::clamp(g_r_cam, 2.6f, 200.0f);  // stay outside ISCO / not too far
}

static void mouse_button_callback(GLFWwindow* w, int button, int action, int /*mods*/) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        g_mouse_down = (action == GLFW_PRESS);
        if (g_mouse_down) glfwGetCursorPos(w, &g_last_mx, &g_last_my);
    }
}

static void cursor_callback(GLFWwindow*, double mx, double my) {
    if (!g_mouse_down) return;
    const float dx = static_cast<float>(mx - g_last_mx) * 0.006f;
    const float dy = static_cast<float>(my - g_last_my) * 0.006f;
    g_cam_phi   -= dx;
    g_cam_theta  = std::clamp(g_cam_theta + dy, 0.08f, 3.14159265f - 0.08f);
    g_last_mx = mx;
    g_last_my = my;
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Could not open file: " << path << std::endl;
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Must match the WGSL Uniforms struct exactly (8 × f32 = 32 bytes)
struct Uniforms {
    float time;
    float width;
    float height;
    float rs;
    float r_cam;
    float fov;
    float cam_theta;
    float cam_phi;   // was pad — now carries the camera azimuth
};

// -----------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    wgpu::Instance instance = wgpu::CreateInstance();
    if (!instance) {
        std::cerr << "Failed to create WebGPU instance." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Spacetime Simulator", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwSetScrollCallback(window,      scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window,   cursor_callback);

    wgpu::Surface surface = CreateSurfaceForWindow(instance, window);
    if (!surface) {
        std::cerr << "Failed to create WebGPU surface." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // --- Adapter ---
    wgpu::RequestAdapterOptions adapterOptions = {};
    adapterOptions.compatibleSurface = surface;

    wgpu::Adapter adapter;
    bool adapterDone = false;

    instance.RequestAdapter(
        &adapterOptions,
        wgpu::CallbackMode::AllowProcessEvents,
        [&adapter, &adapterDone](wgpu::RequestAdapterStatus status, wgpu::Adapter a, const char* msg) {
            if (status == wgpu::RequestAdapterStatus::Success) {
                adapter = std::move(a);
            } else {
                std::cerr << "RequestAdapter failed: " << (msg ? msg : "unknown") << std::endl;
            }
            adapterDone = true;
        }
    );
    while (!adapterDone) {
        instance.ProcessEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!adapter) {
        std::cerr << "Failed to get WebGPU adapter." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // --- Device ---
    wgpu::DeviceDescriptor deviceDesc = {};
    deviceDesc.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
            std::cerr << "WebGPU Error (" << static_cast<int>(type) << "): "
                      << std::string(message.data, message.length) << std::endl;
        }
    );

    wgpu::Device device;
    bool deviceDone = false;

    adapter.RequestDevice(
        &deviceDesc,
        wgpu::CallbackMode::AllowProcessEvents,
        [&device, &deviceDone](wgpu::RequestDeviceStatus status, wgpu::Device d, const char* msg) {
            if (status == wgpu::RequestDeviceStatus::Success) {
                device = std::move(d);
            } else {
                std::cerr << "RequestDevice failed: " << (msg ? msg : "unknown") << std::endl;
            }
            deviceDone = true;
        }
    );
    while (!deviceDone) {
        instance.ProcessEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!device) {
        std::cerr << "Failed to get WebGPU device." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // --- Surface format ---
    wgpu::SurfaceCapabilities caps;
    surface.GetCapabilities(adapter, &caps);
    wgpu::TextureFormat swapChainFormat = caps.formats[0];

    wgpu::SurfaceConfiguration surfaceConfig = {};
    surfaceConfig.device      = device;
    surfaceConfig.format      = swapChainFormat;
    surfaceConfig.usage       = wgpu::TextureUsage::RenderAttachment;
    surfaceConfig.width       = 1280;
    surfaceConfig.height      = 720;
    surfaceConfig.presentMode = wgpu::PresentMode::Fifo;
    surfaceConfig.alphaMode   = wgpu::CompositeAlphaMode::Opaque;
    surface.Configure(&surfaceConfig);

    // --- Shader ---
    std::string shaderSrc = readFile("shaders/raymarcher.wgsl");
    if (shaderSrc.empty()) return -1;

    wgpu::ShaderSourceWGSL wgslDesc;
    wgslDesc.code = { shaderSrc.c_str(), shaderSrc.size() };

    wgpu::ShaderModuleDescriptor shaderModuleDesc = {};
    shaderModuleDesc.nextInChain = &wgslDesc;
    wgpu::ShaderModule shaderModule = device.CreateShaderModule(&shaderModuleDesc);

    // --- Bind group layout (group 0, binding 0 = uniform buffer) ---
    wgpu::BindGroupLayoutEntry bglEntry = {};
    bglEntry.binding    = 0;
    bglEntry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bglEntry.buffer.type = wgpu::BufferBindingType::Uniform;
    bglEntry.buffer.minBindingSize = sizeof(Uniforms);

    wgpu::BindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries    = &bglEntry;
    wgpu::BindGroupLayout bindGroupLayout = device.CreateBindGroupLayout(&bglDesc);

    // --- Pipeline layout ---
    wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts     = &bindGroupLayout;
    wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

    // --- Render pipeline ---
    wgpu::ColorTargetState colorTarget = {};
    colorTarget.format = swapChainFormat;

    wgpu::FragmentState fragmentState = {};
    fragmentState.module      = shaderModule;
    fragmentState.entryPoint  = { "fs_main", WGPU_STRLEN };
    fragmentState.targetCount = 1;
    fragmentState.targets     = &colorTarget;

    wgpu::RenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.layout                  = pipelineLayout;
    pipelineDesc.vertex.module           = shaderModule;
    pipelineDesc.vertex.entryPoint       = { "vs_main", WGPU_STRLEN };
    pipelineDesc.primitive.topology      = wgpu::PrimitiveTopology::TriangleList;
    pipelineDesc.fragment                = &fragmentState;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pipelineDesc);

    // --- Uniform buffer ---
    wgpu::BufferDescriptor uniformBufferDesc = {};
    uniformBufferDesc.size  = sizeof(Uniforms);
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer uniformBuffer = device.CreateBuffer(&uniformBufferDesc);

    // --- Bind group ---
    wgpu::BindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer  = uniformBuffer;
    bgEntry.size    = sizeof(Uniforms);

    wgpu::BindGroupDescriptor bgDesc = {};
    bgDesc.layout     = bindGroupLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries    = &bgEntry;
    wgpu::BindGroup bindGroup = device.CreateBindGroup(&bgDesc);

    // --- Main render loop ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Upload per-frame uniforms
        static constexpr float kPi = 3.14159265f;
        Uniforms uniforms = {
            static_cast<float>(glfwGetTime()),
            1280.0f,
            720.0f,
            1.0f,           // rs (natural units)
            g_r_cam,
            kPi / 3.0f,    // 60° FOV
            g_cam_theta,
            g_cam_phi,
        };
        device.GetQueue().WriteBuffer(uniformBuffer, 0, &uniforms, sizeof(Uniforms));

        wgpu::SurfaceTexture surfaceTexture;
        surface.GetCurrentTexture(&surfaceTexture);

        if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
            surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
            std::cerr << "Failed to acquire swap chain texture (status "
                      << static_cast<int>(surfaceTexture.status) << ")" << std::endl;
            break;
        }

        wgpu::TextureView view = surfaceTexture.texture.CreateView();

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        wgpu::RenderPassColorAttachment colorAttachment = {};
        colorAttachment.view      = view;
        colorAttachment.loadOp    = wgpu::LoadOp::Clear;
        colorAttachment.storeOp   = wgpu::StoreOp::Store;
        colorAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};

        wgpu::RenderPassDescriptor renderPassDesc = {};
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments     = &colorAttachment;

        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.Draw(3);   // 3 vertices → full-screen triangle, no vertex buffer needed
        pass.End();

        wgpu::CommandBuffer commands = encoder.Finish();
        device.GetQueue().Submit(1, &commands);

        surface.Present();
        device.Tick();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
