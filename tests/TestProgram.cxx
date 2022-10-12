/*
 * Example program using the early Vulkray engine API.
 *
 * VULKRAY ENGINE SOFTWARE
 * Copyright (c) 2022, Max Rodriguez. All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license. You should have received a copy of this license along
 * with this source code in a file named "COPYING."
 */

#include "../include/Vulkray/ShowBase.h"
#include <iostream>

int main() {
    // Prepare Vulkray engine configuration
    EngineConfig configuration;
    configuration.windowTitle = (char*) "Vulkray Test";
    configuration.graphicsInput.vertexData = {
            {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}}, // 0
            {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}, // 1
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}, // 2
            {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}}, // 3
            {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}}, // 4
            {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}}, // 5
            {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}}, // 6
            {{-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}} // 7
    };
    configuration.graphicsInput.indexData = {
            0, 1, 2, 2, 3, 0, // top face
            4, 7, 6, 6, 5, 4, // bottom face
            0, 4, 5, 5, 1, 0, // back face
            1, 5, 6, 6, 2, 1, // right face
            4, 0, 3, 3, 7, 4, // left face
            3, 2, 6, 6, 7, 3 // front face
    };

    // Instantiate the engine base class using a smart pointer
    std::unique_ptr<ShowBase> base = std::make_unique<ShowBase>(configuration);

    // Initialize the engine vulkan renderer
    try {
        base->initialize();
    } catch (const std::exception& exception) {
        std::cout << "An exception was thrown by the engine:\n" << exception.what();
        return 1;
    }
    return 0;
}
