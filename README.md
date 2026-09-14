# Thulium

A fast, lightweight, and cross-platform raster image editor written in native C++17 with Qt 6 and a hardware-accelerated Vulkan rendering pipeline.

---

## Overview

**Thulium** is designed to provide an intuitive, responsive, and distraction-free raster editing experience. Bridging the gap between overly simplistic paint tools and heavy, complex digital creation suites, Thulium prioritizes speed, clarity, and instant startup.

Inspired by the accessible workflows of classic raster editors like Paint.NET, Thulium is a clean-room reimplementation engineered from scratch to run natively across modern operating systems.

---

## Architecture & Technology

Thulium is architected with performance, modularity, and cross-platform longevity at its core:

* **Native C++17 Engine:** Zero managed runtime overhead, tight memory control, and optimal CPU cache utilization for responsive brush strokes and layer operations.
* **Dual Rendering Pipeline:**
  * **Hardware-Accelerated Vulkan Backend:** Direct GPU rendering for canvas blitting, zoom, pan, and real-time viewport transformations.
  * **Optimized Software Fallback:** CPU-based rasterization engine ensuring compatibility on any display server or headless virtual environment.
* **Qt 6 Desktop Architecture:** Clean, modular UI utilizing modern Qt 6 widgets, dockable tool palettes, non-blocking asynchronous operations, and high-DPI scaling.
* **Clean-Room Interoperability:** Native reader and writer for the Paint.NET (`.pdn` / PDN3) document format, alongside full support for industry-standard formats including PNG, JPEG, WebP, BMP, and GIF.
* **Non-Destructive Layer System:** Full support for multi-layer composition, opacity controls, standard layer blend modes, and an undo/redo command history stack.

---

## Building from Source

### Prerequisites

* C++17 compatible compiler (GCC 9+, Clang 10+, or MSVC 2019+)
* CMake (>= 3.16)
* Qt 6 (`Core`, `Gui`, `Widgets`, `Svg`)
* Vulkan SDK & Loader
* ZLIB & GIF libraries

### Build Instructions

```bash
# Configure the build directory
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Compile using all available CPU cores
cmake --build build -j$(nproc)

# Run Thulium
./build/thulium
```

### Running Tests

```bash
ctest --test-dir build --output-on-failure
```

---

## License & Trademarks

### License
Thulium is open-source software licensed under the **[MIT License](LICENSE)**.  
Copyright &copy; 2026 Finn Freitag.

### Trademarks & Disclaimers
* **Paint.NET** is a registered trademark of Rick Brewster and dotPDN LLC. Thulium is an independent, clean-room project and is not affiliated with, endorsed by, or sponsored by dotPDN LLC.
* **Qt** is a registered trademark of The Qt Company Ltd. and its subsidiaries.
* **Vulkan** and the Vulkan logo are registered trademarks of the Khronos Group Inc.
