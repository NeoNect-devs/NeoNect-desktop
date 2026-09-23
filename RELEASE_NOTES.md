# NeoNect v1.0.0 Release Notes & Changelog

**Release Date**: 2026-09-23  
**Commit**: `2501220`  
**Supported Platforms**: Windows x86_64, Linux x86_64  
**Documentation**: [https://neonect-devs.github.io/NeoNect-desktop/](https://neonect-devs.github.io/NeoNect-desktop/)

## 🌟 Release Summary & Highlights

NeoNect `v1.0.0` delivers production-ready decentralized, end-to-end encrypted (E2EE) real-time messaging with high-performance desktop client streaming.

### 🔒 Security & Cryptography (E2EE)
- ensure end-to-end encryption for all media transmissions (`017baea`)

### 🚀 New Features & Capabilities
- implement automated semantic versioning, patch notes engine, and cross-platform production packaging (`0219f30`)

### ⚡ Performance & Memory Optimizations
- fix Windows test runner & windeployqt packaging, optimize scripts (`2ca5e2d`)

### 🐛 Bug Fixes & Stability
- standard RFC 7230 Host/Origin headers, TLS SNI peer verification, and exponential backoff (`2501220`)
- prevent windows.h min/max macro collision on MSVC with NOMINMAX (`30a54e5`)
- make Windows post-build DLL copy dynamic and remove hardcoded paths (`a565360`)
- resolve missing closing brace syntax error in modern-graph-viewer.js (`4c54975`)
- resolve namespace duplication and add descriptions for all namespaces (`2d25222`)
- register friendship via POST /api/v1/friends and prevent false 401 session resets on 403 forbidden relay errors (`f2e7ba1`)
- include required protocol_version in relay payload and fix unit test setup (`c68a6f3`)
- synchronize friend retrieval with backend api and fix settings logout flow (`ce44ccf`)

### 🛠️ General Improvements & Toolchain
- automate changelog and patch notes synchronization for docs site and GitHub draft release (`8990b12`)
- switch Windows release pipeline to native MSVC 2022 and win64_msvc2022_64 (`b7d0de9`)
- install matching MinGW 13.1.0 toolchain to prevent 0xC0000139 entry point mismatch (`a01a8af`)
- enable unbuffered Out-Host test console streaming and stderr logging (`ac28054`)
- stage OpenSSL and minimal QPA platform plugins for Windows test runner (`6a9a1c1`)
- remove unnecessary windeployqt from test step and add error resilience to packager (`6345b13`)
- deploy test dependencies with windeployqt and OpenSSL DLLs before running tests on Windows (`e5d9934`)
- configure PATH and QT_QPA_PLATFORM for headless test execution on CI runners (`5384688`)
- chain docs deployment after builds and improve windows openssl discovery (`c60b186`)
- fix windres space parsing with 8.3 paths and set linux qt arch to linux_gcc_64 (`ebba39b`)
- fix windows openssl detection and update linux qt6 installation in release workflow (`f2f4796`)
- add Apache-2.0 license, changelog and patch notes, and release-triggered docs deployment workflow (`b30eb7e`)
- create comprehensive modular documentation, readme showcase, and github pages workflow (`4e3da1c`)
- modernize architectural layers graph viewer and fix light theme styling (`873995b`)
- improve layout responsiveness and enlarge documentation logo (`5685142`)
- modernize Doxygen UI, remove anonymous namespace, and add technical constraints (`daa0cf2`)
- fix header logo scaling with custom CSS and render 7-layer architecture via Graphviz Dot (`3e13d70`)
- implement enterprise-grade Doxygen documentation and architecture specification (`2d5b6f9`)
- configure GitHub Actions to save releases as drafts for manual review (`89ffa33`)

### 📜 Detailed Commit Changelog
- `2501220` fix(websocket): standard RFC 7230 Host/Origin headers, TLS SNI peer verification, and exponential backoff - *M-U-T-E*
- `8990b12` docs: automate changelog and patch notes synchronization for docs site and GitHub draft release - *M-U-T-E*
- `30a54e5` fix(windows): prevent windows.h min/max macro collision on MSVC with NOMINMAX - *M-U-T-E*
- `b7d0de9` ci: switch Windows release pipeline to native MSVC 2022 and win64_msvc2022_64 - *M-U-T-E*
- `a01a8af` ci: install matching MinGW 13.1.0 toolchain to prevent 0xC0000139 entry point mismatch - *M-U-T-E*
- `ac28054` ci: enable unbuffered Out-Host test console streaming and stderr logging - *M-U-T-E*
- `6a9a1c1` ci: stage OpenSSL and minimal QPA platform plugins for Windows test runner - *M-U-T-E*
- `6345b13` ci: remove unnecessary windeployqt from test step and add error resilience to packager - *M-U-T-E*
- `2ca5e2d` ci: fix Windows test runner & windeployqt packaging, optimize scripts - *M-U-T-E*
- `e5d9934` ci: deploy test dependencies with windeployqt and OpenSSL DLLs before running tests on Windows - *M-U-T-E*
- `5384688` ci: configure PATH and QT_QPA_PLATFORM for headless test execution on CI runners - *M-U-T-E*
- `a565360` fix(build): make Windows post-build DLL copy dynamic and remove hardcoded paths - *M-U-T-E*
- `c60b186` ci: chain docs deployment after builds and improve windows openssl discovery - *M-U-T-E*
- `ebba39b` ci: fix windres space parsing with 8.3 paths and set linux qt arch to linux_gcc_64 - *M-U-T-E*
- `f2f4796` ci: fix windows openssl detection and update linux qt6 installation in release workflow - *M-U-T-E*
- `b30eb7e` docs: add Apache-2.0 license, changelog and patch notes, and release-triggered docs deployment workflow - *M-U-T-E*
- `4e3da1c` docs: create comprehensive modular documentation, readme showcase, and github pages workflow - *M-U-T-E*
- `4c54975` fix(docs): resolve missing closing brace syntax error in modern-graph-viewer.js - *M-U-T-E*
- `873995b` docs: modernize architectural layers graph viewer and fix light theme styling - *M-U-T-E*
- `5685142` docs: improve layout responsiveness and enlarge documentation logo - *M-U-T-E*
- `2d25222` docs: resolve namespace duplication and add descriptions for all namespaces - *M-U-T-E*
- `daa0cf2` docs: modernize Doxygen UI, remove anonymous namespace, and add technical constraints - *M-U-T-E*
- `3e13d70` docs: fix header logo scaling with custom CSS and render 7-layer architecture via Graphviz Dot - *M-U-T-E*
- `2d5b6f9` docs: implement enterprise-grade Doxygen documentation and architecture specification - **
- `f2e7ba1` fix(friends): register friendship via POST /api/v1/friends and prevent false 401 session resets on 403 forbidden relay errors - *M-U-T-E*
- `c68a6f3` fix(relay): include required protocol_version in relay payload and fix unit test setup - *M-U-T-E*
- `ce44ccf` fix(client): synchronize friend retrieval with backend api and fix settings logout flow - *M-U-T-E*
- `89ffa33` ci(release): configure GitHub Actions to save releases as drafts for manual review - **
- `0219f30` feat(release): implement automated semantic versioning, patch notes engine, and cross-platform production packaging - **
- `017baea` feat(security): ensure end-to-end encryption for all media transmissions - **

---
### 📦 Asset Checksums & Verification
- All Windows `.zip` and Linux `.tar.gz` distribution packages contain full standalone runtimes and OpenSSL cryptographic libraries.
- For comprehensive API references and architecture diagrams, see the [Online Documentation](https://neonect-devs.github.io/NeoNect-desktop/).
