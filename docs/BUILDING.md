# Compiling, Testing & Documentation Guide {#building_guide}

This guide provides technical instructions for compiling the **NeoNect Desktop Client** from source code, running the automated test suites, benchmarking performance, and generating the documentation.

---

## 1. Toolchain & Prerequisites

To build NeoNect from source, ensure the following software packages are installed:

| Component | Minimum Version | Notes |
| :--- | :--- | :--- |
| **C++ Compiler** | C++17 compliant | GCC 11.0+, Clang 14.0+, or MSVC 2019/2022 (v142/v143) |
| **CMake** | 3.20 or newer | Build system generator |
| **Build Tool** | Ninja (recommended) | Fast multi-threaded builds |
| **Qt Framework** | Qt 6.5.0+ (6.7+ recommended) | Modules: `Gui`, `Qml`, `Quick`, `Concurrent`, `Network`, `Test`, `Sql`, `Multimedia` |
| **OpenSSL** | 3.0.0+ / 4.0.0 | Cryptographic engine for AES-256-GCM and PBKDF2 |
| **Doxygen** | 1.10.0+ (1.13.2 recommended) | For building API documentation |
| **Graphviz** | 9.0+ (`dot` executable) | For class hierarchies and dependency graphs |

---

## 2. Cloning the Repository

```bash
git clone https://github.com/NeoNect-devs/NeoNect-desktop.git
cd NeoNect-desktop
```

---

## 3. Configuring and Building with CMake

### A. Windows (MinGW Toolchain & Ninja)
```powershell
# Configure CMake build tree
cmake -B build -G "Ninja" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/mingw_64" `
    -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"

# Compile all targets in parallel
cmake --build build --config Release
```

### B. Windows (Microsoft Visual Studio MSVC)
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64" `
    -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"

cmake --build build --config Release
```

### C. Linux (Ubuntu / Debian / Arch)
```bash
cmake -B build -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="/opt/qt/6.7.3/gcc_64"

cmake --build build --config Release
```

---

## 4. CMake Build Targets

The CMake build definitions produce the following executables:

| Target Name | Description | Output Location |
| :--- | :--- | :--- |
| `NeoNectApp` | Primary Desktop GUI Application | `build/NeoNectApp.exe` (or `build/NeoNectApp`) |
| `NeoNectTests` | Automated Unit & Integration Test Suites | `build/NeoNectTests.exe` |
| `NeoNectBenchmarks` | High-throughput throughput and encryption benchmarks | `build/NeoNectBenchmarks.exe` |
| `NeoNectStressTests` | Concurrency and SQLite WAL stress simulators | `build/NeoNectStressTests.exe` |

To build an individual target:
```bash
cmake --build build --target NeoNectApp
```

---

## 5. Running the Test Suite

NeoNect includes a comprehensive suite of 34 unit and integration tests covering cryptographic invariants, SQLite WAL atomicity, mock server protocols, and QML viewmodels:

### Running via CTest
```powershell
cd build
ctest --output-on-failure --verbose
```

### Running the Direct Test Binary
```powershell
.\build\NeoNectTests.exe
```

Expected output:
```text
==========================================
  RUNNING NEONECT DESKTOP TEST SUITES
==========================================

[TestCrypto Result]: PASSED
[TestStorage Result]: PASSED
[TestModels Result]: PASSED
--> Testing testAuthServiceFlow... Result: PASSED
--> Testing testDeviceServiceFlow... Result: PASSED
--> Testing testRelayServiceFlowAndDeduplication... Result: PASSED
...
[TestMessages Result]: PASSED
==========================================
  ALL NEONECT TESTS PASSED SUCCESSFULLY! [100%]
==========================================
```

---

## 6. Building the Doxygen Documentation Locally

NeoNect features modern, responsive documentation augmented with `doxygen-awesome-css` and the interactive Mermaid graph viewer:

```powershell
# Ensure doxygen and dot are in your PATH
doxygen Doxyfile
```

The generated HTML documentation is located at:
```text
docs/doxygen/html/index.html
```

To view the generated documentation in your browser:
```powershell
Start-Process "docs/doxygen/html/index.html"
```

---

## 7. Next Steps

* 🐳 [Docker & Containerized Streaming Guide](DOCKER.md)
* 🌐 [Documentation Ecosystem & GitHub Pages Deployment](DOCUMENTATION.md)
* 🚀 [Getting Started Guide](GETTING_STARTED.md)
