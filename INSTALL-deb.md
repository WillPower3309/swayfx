# A guide to installing SwayFX on Debian 13 (Trixie/Sid)

> **Note:** This guide targets **SwayFX 0.5.3** (based on Sway 1.11). It manually builds the required dependencies (**wlroots 0.19.0** and **scenefx 0.4.1**) using Meson subprojects to ensure compatibility and stability.

## 1\. Install Dependencies

Debian Trixie requires a specific set of development headers, particularly for the newer `wlroots` 0.19 backend (which needs `liblcms2`, `libliftoff`, and `libdisplay-info`).

```bash
sudo apt update && sudo apt install -y \
  meson pkg-config cmake git scdoc \
  wayland-protocols libwayland-dev libpcre2-dev libjson-c-dev \
  libpango1.0-dev libcairo2-dev libgdk-pixbuf-2.0-dev \
  libdrm-dev libgbm-dev libinput-dev libseat-dev libxkbcommon-dev \
  libxcb-dri3-dev libxcb-present-dev libxcb-res0-dev \
  libxcb-render-util0-dev libxcb-ewmh-dev libxcb-icccm4-dev \
  libliftoff-dev libdisplay-info-dev liblcms2-dev libpixman-1-dev
```

## 2\. Setup Build Environment

We will use the **Meson Subprojects** method. This places the specific versions of `wlroots` and `scenefx` directly inside the build folder, preventing conflicts with system libraries.

```bash
# Create a clean build directory
mkdir -p ~/build
cd ~/build

# 1. Clone SwayFX 0.5.3 (The Window Manager)
git clone https://github.com/WillPower3309/swayfx.git
cd swayfx
git checkout 0.5.3

# 2. Setup Subprojects Directory
mkdir subprojects
cd subprojects

# 3. Clone SceneFX 0.4.1 (The Rendering FX Library)
# Note: 0.4.1 is required for wlroots 0.19 support
git clone https://github.com/wlrfx/scenefx.git
cd scenefx
git checkout 0.4.1
cd ..

# 4. Clone Wlroots 0.19.0 (The Wayland Compositor Backend)
git clone https://gitlab.freedesktop.org/wlroots/wlroots.git
cd wlroots
git checkout 0.19.0

# Return to source root
cd ../..
```

Your directory structure should now look like this:

```text
~/build/swayfx
└── subprojects
    ├── scenefx (v0.4.1)
    └── wlroots (v0.19.0)
```

## 3\. Compile and Install

Now we build everything in one go. Meson will detect the code in `subprojects/` and build it automatically.

```bash
# Configure the build
meson setup build/

# Compile
ninja -C build/

# Install
sudo ninja -C build/ install
```

## 4\. Finalize

Debian does not automatically link new libraries in `/usr/local/lib` immediately. You must update the linker cache to avoid "shared object not found" errors.

```bash
sudo ldconfig
```

## 5\. Verification

Check that the installation was successful and is using the correct version base.

```bash
sway --version
# Expected Output: 
# swayfx version 0.5.3... (based on sway 1.11.0)
```

  + Guide updated for Debian 13 / SwayFX 0.5.3 with ♥️
