#!/usr/bin/env bash
set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}VibeNote Setup Script for Arch Linux${NC}"
echo "======================================"

# Check if running on Arch
if [ ! -f /etc/arch-release ]; then
    echo -e "${RED}Error: This script is for Arch Linux only${NC}"
    exit 1
fi

# Check for Zen kernel
if ! uname -r | grep -q "zen"; then
    echo -e "${YELLOW}Warning: Zen kernel not detected. Installing...${NC}"
    sudo pacman -S --needed linux-zen linux-zen-headers
    echo "Please reboot with Zen kernel and run this script again"
    exit 1
fi

# Install system dependencies
echo -e "${GREEN}Installing system packages...${NC}"
sudo pacman -S --needed --noconfirm \
    base-devel cmake ninja git wget \
    qt6-base qt6-declarative qt6-svg qt6-tools qt6-multimedia qt6-networkauth \
    qt6-httpserver qt6-websockets qt6-imageformats \
    kconfig kcoreaddons ki18n kirigami kglobalaccel kio kguiaddons kiconthemes qqc2-desktop-style \
    qtkeychain-qt6 \
    sqlite tesseract tesseract-data-eng \
    pipewire libpipewire \
    libportal libportal-gtk3 libportal-qt6 \
    opencv nlohmann-json fmt spdlog \
    nvidia-open-dkms nvidia-utils lib32-nvidia-utils \
    cuda cudnn \
    python python-pip \
    gtest gmock \
    clang llvm lld

# Ensure NVIDIA modules are loaded
echo -e "${GREEN}Loading NVIDIA modules...${NC}"
sudo modprobe nvidia
sudo modprobe nvidia_drm modeset=1
sudo modprobe nvidia_uvm

# Verify NVIDIA driver
if ! lsmod | grep -q nvidia; then
    echo -e "${RED}Error: NVIDIA modules not loaded${NC}"
    exit 1
fi

# Clone repository if not already in it
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${GREEN}Cloning VibeNote repository...${NC}"
    git clone https://github.com/Tim-Spurlin/VibeNote.git
    cd VibeNote
fi

# Initialize submodules
echo -e "${GREEN}Initializing submodules...${NC}"
git submodule update --init --recursive

# Build llama.cpp with CUDA support
echo -e "${GREEN}Building llama.cpp with CUDA...${NC}"
cd third_party/llama.cpp
make clean
make -j$(nproc) LLAMA_CUBLAS=1 LLAMA_CUDA_NVCC=/opt/cuda/bin/nvcc
cd ../..

# Create models directory
mkdir -p models

# Download Qwen 2.5 3B model
echo -e "${GREEN}Downloading Qwen 2.5 3B model...${NC}"
MODEL_URL="https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF/resolve/main/qwen2.5-3b-instruct-q4_K_M.gguf"
MODEL_PATH="models/qwen2.5-3b-instruct-q4_K_M.gguf"

if [ ! -f "$MODEL_PATH" ]; then
    wget -c "$MODEL_URL" -O "$MODEL_PATH"
else
    echo "Model already downloaded"
fi

# Build VibeNote
echo -e "${GREEN}Building VibeNote...${NC}"
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_PADDLE_OCR=OFF \
    -DVIBENOTE_ENABLE_TESTS=ON

cmake --build build -j$(nproc)

# Install binaries
echo -e "${GREEN}Installing VibeNote...${NC}"
sudo cmake --install build

# Create config directory
mkdir -p ~/.config/VibeNote

# Generate default config
cat > ~/.config/VibeNote/config.yml << 'CONFIGEOF'
watch:
  enabled: false
  fps: 1.0
  ocrBackend: tesseract
  excludeApps:
    - org.kde.kwalletd
    - keepassxc

model:
  server:
    host: 127.0.0.1
    port: 11434
    modelPath: "${HOME}/VibeNote/models/qwen2.5-3b-instruct-q4_K_M.gguf"

gpu:
  utilThreshold: 85
  vramHeadroomMB: 800
  ngl: 20

queue:
  capacity: 256

security:
  localhostOnly: true
CONFIGEOF

# Install systemd services
echo -e "${GREEN}Installing systemd services...${NC}"
mkdir -p ~/.config/systemd/user

# Install and enable services
cp packaging/systemd/*.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable vibemodel.service
systemctl --user enable vibed.service

# Install polkit policy
echo -e "${GREEN}Installing polkit policy...${NC}"
sudo cp packaging/polkit/org.saphyre.vibenote.policy /usr/share/polkit-1/actions/

# Install desktop entry
echo -e "${GREEN}Installing desktop entry...${NC}"
sudo cp app/org.saphyre.VibeNote.desktop /usr/share/applications/

# Update desktop database
sudo update-desktop-database

# Create data directory
mkdir -p ~/.local/share/VibeNote

# Setup complete
echo -e "${GREEN}Setup complete!${NC}"
echo ""
echo "To start VibeNote:"
echo "  systemctl --user start vibemodel.service"
echo "  systemctl --user start vibed.service"
echo "  vibenote_app"
echo ""
echo "Or for development:"
echo "  ./scripts/run-dev.sh"
echo ""
echo "Press Ctrl+Alt+Space to open the overlay"

