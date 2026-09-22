# Documentation Ecosystem & Static Hosting {#documentation_guide}

The **NeoNect Desktop Client** documentation system is engineered with **Doxygen 1.13.2**, **doxygen-awesome-css**, and a modern **interactive Mermaid.js graph engine**. This guide explains how the documentation is structured, how to generate it locally, and how automated deployment to **GitHub Pages** operates.

---

## 1. Documentation Architecture & Features

The documentation site provides an authentic software engineering reference:

```
docs/
├── mainpage.md                 # Primary architecture specification & system topology
├── GETTING_STARTED.md          # First-steps manual & account provisioning guide
├── INSTALLATION.md             # System requirements, dependencies, and packages
├── BUILDING.md                 # CMake toolchain, compilation, and unit test guide
├── DOCKER.md                   # Containerized virtual display & noVNC web streaming
├── DOCUMENTATION.md            # Documentation ecosystem & GitHub Pages workflow
├── doxygen-awesome.css         # Modern responsive base stylesheet
├── custom.css                  # Dual-theme tokens (Light & Dark) & visual styling
├── mermaid.min.js              # Standalone offline Mermaid.js diagram engine
└── modern-graph-viewer.js      # Pan, zoom, touch, and fullscreen controller
```

### Key Capabilities
1. **Interactive Architectural Topology Viewer**:
   - Replaces static images with an interactive, vectorized diagram supporting smooth mouse-wheel zoom, drag-to-pan, 2-finger touch pinch on mobile, and fullscreen expansion.
2. **Seamless Dual-Theme Palette (Light & Dark)**:
   - High-contrast slate light mode (`#ffffff` / `#0f172a` text / `#e2e8f0` borders) and deep GitHub dark mode (`#0d1117` / `#e6edf3` text / `#30363d` borders).
   - Diagram automatically re-renders dynamically when theme switching occurs.
3. **Rigorous Design Pattern Catalog & Invariants**:
   - Explicit specifications for mathematical bounds, zero nonce-reuse rules, SQLite WAL concurrency, and OpenSSL RAII safety wrappers.
4. **Clean Namespace Architecture**:
   - Zero anonymous namespaces or duplicated entries; full descriptions across all 8 project namespaces (`NeoNect`, `Core`, `Crypto`, `Domain`, `Services`, `Storage`, `Transport`, `Constants`).

---

## 2. Generating Documentation Locally

Ensure [Doxygen](https://www.doxygen.nl/) (1.10+) and [Graphviz](https://graphviz.org/) are installed:

```powershell
# Windows
choco install doxygen.install graphviz

# Linux (Debian/Ubuntu)
sudo apt-get install doxygen graphviz
```

Build the documentation from the project root:
```bash
doxygen Doxyfile
```

Open the generated site:
```powershell
Start-Process "docs/doxygen/html/index.html"
```

---

## 3. Automated GitHub Pages Static Hosting Workflow

NeoNect includes an automated GitHub Actions workflow (`.github/workflows/docs.yml`) that compiles and publishes the documentation to GitHub Pages whenever changes are pushed to `dev` or `main`.

### Workflow Definition (`.github/workflows/docs.yml`)

```yaml
name: Deploy Documentation to GitHub Pages

on:
  push:
    branches:
      - dev
      - main
    paths:
      - 'docs/**'
      - 'src/**'
      - 'Doxyfile'
      - '.github/workflows/docs.yml'
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: 'pages'
  cancel-in-progress: true

jobs:
  build-docs:
    name: Build Doxygen Documentation
    runs-on: ubuntu-24.04
    steps:
      - name: Checkout Repository
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Install Doxygen & Graphviz
        run: |
          sudo apt-get update
          sudo apt-get install -y doxygen graphviz

      - name: Build Doxygen Documentation
        run: |
          doxygen Doxyfile

      - name: Setup GitHub Pages
        uses: actions/configure-pages@v5

      - name: Upload Documentation Artifact
        uses: actions/upload-pages-artifact@v3
        with:
          path: docs/doxygen/html

  deploy-pages:
    name: Deploy to GitHub Pages
    needs: build-docs
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
```

---

## 4. Enabling GitHub Pages on Your Repository

To host the documentation site on GitHub's static CDN:
1. Navigate to your repository on GitHub: `https://github.com/<org>/NeoNect-desktop`.
2. Go to **Settings** ➔ **Pages**.
3. Under **Build and deployment** ➔ **Source**, select **GitHub Actions**.
4. Push a commit or trigger the `Deploy Documentation to GitHub Pages` workflow manually under the **Actions** tab.
5. Your live documentation site will be accessible at:
   ```text
   https://<org>.github.io/NeoNect-desktop/
   ```

---

## 5. Next Steps

* 🚀 [Getting Started Guide](GETTING_STARTED.md)
* 📦 [Installation & System Requirements](INSTALLATION.md)
* 🛠️ [Building from Source & Running Tests](BUILDING.md)
