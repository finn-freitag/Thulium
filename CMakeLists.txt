cmake_minimum_required(VERSION 3.16)
project(PaintNetClone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)
find_package(ZLIB REQUIRED)
find_package(GIF REQUIRED)

# Vulkan setup
set(VULKAN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan/include")
find_library(VULKAN_LIB NAMES vulkan)

include_directories(
    ${VULKAN_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(PDN_SOURCES
    src/core/BlendModes.h
    src/core/BlendModes.cpp
    src/core/Layer.h
    src/core/Layer.cpp
    src/core/Selection.h
    src/core/Selection.cpp
    src/core/Document.h
    src/core/Document.cpp
    src/core/Metadata.h
    src/core/History.h
    src/core/History.cpp
    src/core/Resampling.h
    src/core/Resampling.cpp

    src/io/PdnFormat.h
    src/io/PdnFormat.cpp
    src/io/StandardFormats.h
    src/io/StandardFormats.cpp
    src/io/ImageIO.h
    src/io/ImageIO.cpp

    src/rendering/IRenderer.h
    src/rendering/SoftwareRenderer.h
    src/rendering/SoftwareRenderer.cpp
    src/rendering/VulkanRenderer.h
    src/rendering/VulkanRenderer.cpp

    src/tools/ITool.h
    src/tools/BrushTools.h
    src/tools/BrushTools.cpp
    src/tools/SelectionTools.h
    src/tools/SelectionTools.cpp
    src/tools/MoveTools.h
    src/tools/MoveTools.cpp
    src/tools/FillTools.h
    src/tools/FillTools.cpp
    src/tools/ShapeTools.h
    src/tools/ShapeTools.cpp
    src/tools/TextTool.h
    src/tools/TextTool.cpp
    src/tools/ViewTools.h
    src/tools/ViewTools.cpp
    src/tools/ToolManager.h
    src/tools/ToolManager.cpp

    src/effects/IEffect.h
    src/effects/ICanvasInteraction.h
    src/effects/EffectDialog.h
    src/effects/EffectDialog.cpp
    src/effects/BrightnessContrast.h
    src/effects/BrightnessContrast.cpp
    src/effects/GaussianBlur.h
    src/effects/GaussianBlur.cpp
    src/effects/Adjustments.h
    src/effects/Adjustments.cpp
    src/effects/BlurEffects.h
    src/effects/BlurEffects.cpp
    src/effects/DistortEffects.h
    src/effects/DistortEffects.cpp
    src/effects/NoiseEffects.h
    src/effects/NoiseEffects.cpp
    src/effects/PhotoEffects.h
    src/effects/PhotoEffects.cpp
    src/effects/StylizeEffects.h
    src/effects/StylizeEffects.cpp
    src/effects/PluginManager.h
    src/effects/PluginManager.cpp

    src/ui/CanvasView.h
    src/ui/CanvasView.cpp
    src/ui/DocumentStrip.h
    src/ui/DocumentStrip.cpp
    src/ui/ToolOptionsBar.h
    src/ui/ToolOptionsBar.cpp
    src/ui/ToolsDock.h
    src/ui/ToolsDock.cpp
    src/ui/HistoryDock.h
    src/ui/HistoryDock.cpp
    src/ui/LayersDock.h
    src/ui/LayersDock.cpp
    src/ui/ColorsDock.h
    src/ui/ColorsDock.cpp
    src/ui/Dialogs.h
    src/ui/Dialogs.cpp
    src/ui/StatusWidget.h
    src/ui/StatusWidget.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp

    resources/resources.qrc
)

# Core library
add_library(pdn_core STATIC ${PDN_SOURCES})
target_link_libraries(pdn_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    ZLIB::ZLIB
    GIF::GIF
    ${VULKAN_LIB}
)
target_include_directories(pdn_core PUBLIC
    ${VULKAN_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Main executable
add_executable(paintnet-clone src/main.cpp)
target_link_libraries(paintnet-clone PRIVATE pdn_core)

# Tests
enable_testing()

add_executable(test_pdn tests/test_pdn.cpp)
target_link_libraries(test_pdn PRIVATE pdn_core)
add_test(NAME test_pdn COMMAND test_pdn "${CMAKE_CURRENT_SOURCE_DIR}/Card.pdn")

add_executable(test_blend tests/test_blend.cpp)
target_link_libraries(test_blend PRIVATE pdn_core)
add_test(NAME test_blend COMMAND test_blend)

add_executable(test_effects tests/test_effects.cpp)
target_link_libraries(test_effects PRIVATE pdn_core)
add_test(NAME test_effects COMMAND test_effects)

add_executable(test_formats tests/test_formats.cpp)
target_link_libraries(test_formats PRIVATE pdn_core)
add_test(NAME test_formats COMMAND test_formats)

add_executable(test_fixes tests/test_fixes.cpp)
target_link_libraries(test_fixes PRIVATE pdn_core)
add_test(NAME test_fixes COMMAND test_fixes)

add_executable(test_image_operations tests/test_image_operations.cpp)
target_link_libraries(test_image_operations PRIVATE pdn_core)
add_test(NAME test_image_operations COMMAND test_image_operations)

add_executable(test_multi_document tests/test_multi_document.cpp)
target_link_libraries(test_multi_document PRIVATE pdn_core)
add_test(NAME test_multi_document COMMAND test_multi_document)

add_executable(test_metadata tests/test_metadata.cpp)
target_link_libraries(test_metadata PRIVATE pdn_core)
add_test(NAME test_metadata COMMAND test_metadata "${CMAKE_CURRENT_SOURCE_DIR}/Card.pdn")

add_executable(test_clipboard_and_drop tests/test_clipboard_and_drop.cpp)
target_link_libraries(test_clipboard_and_drop PRIVATE pdn_core)
add_test(NAME test_clipboard_and_drop COMMAND test_clipboard_and_drop "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(test_icons tests/test_icons.cpp)
target_link_libraries(test_icons PRIVATE pdn_core)
add_test(NAME test_icons COMMAND test_icons)


