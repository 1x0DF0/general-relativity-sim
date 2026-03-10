#pragma once

#include <webgpu/webgpu_cpp.h>

struct GLFWwindow;

// Creates a WebGPU surface backed by a Metal layer for the given GLFW window (macOS only).
wgpu::Surface CreateSurfaceForWindow(const wgpu::Instance& instance, GLFWwindow* window);
