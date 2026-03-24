# bfly_test - FLIR Blackfly GigE Camera Viewer

A minimal C++ example that streams live video from a FLIR Blackfly GigE camera using the Spinnaker SDK and displays it with OpenCV.

## Features

- Auto-discovery and connection of GigE Vision cameras via Spinnaker SDK
- Automatic detection of color (BGR8) and mono (Mono8) cameras
- Real-time FPS measurement and logging
- Persistent IP configuration (`--set-ip`)
- Camera selection by serial number

## Requirements

| Dependency | Version | Notes |
|---|---|---|
| **OS** | Ubuntu 20.04+ (x86_64) | aarch64 possible if Spinnaker SDK supports it |
| **CMake** | 3.16+ | |
| **C++ Compiler** | C++17 support (GCC 7+) | |
| **Spinnaker SDK** | 2.x+ | Download from FLIR website |
| **OpenCV** | 4.x | `apt install libopencv-dev` |
| **spdlog** | 1.x | Source build or `apt install libspdlog-dev` |
| **fmt** | 7.x+ | `apt install libfmt-dev` |

## 1. Install Dependencies

### Spinnaker SDK

Download Spinnaker SDK for Linux from the FLIR website:
https://www.flir.com/products/spinnaker-sdk/

```bash
# Install downloaded .deb package (example)
sudo dpkg -i spinnaker-<version>_amd64.deb
sudo apt-get install -f

# Or extract tar.gz to /opt/spinnaker
# Default expected path: /opt/spinnaker
```

> **Note**: After installation, `/opt/spinnaker/include/Spinnaker.h` must exist.

### OpenCV, spdlog, fmt

```bash
sudo apt update
sudo apt install -y libopencv-dev libfmt-dev libspdlog-dev
```

If building spdlog from source:

```bash
git clone https://github.com/gabime/spdlog.git
cd spdlog && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/spdlog
make -j$(nproc) && sudo make install
```

### GigE Camera Network Setup

Jumbo frames are recommended for GigE Vision cameras:

```bash
# Check your network interface name
ip link show

# Enable jumbo frames (e.g., for eno1)
sudo ip link set eno1 mtu 9000

# Make it persistent (add mtu 9000 in /etc/netplan/ or /etc/network/interfaces)
```

Increase the receive buffer size:

```bash
sudo sysctl -w net.core.rmem_max=10485760
sudo sysctl -w net.core.rmem_default=10485760

# Make it persistent
echo "net.core.rmem_max=10485760" | sudo tee -a /etc/sysctl.conf
echo "net.core.rmem_default=10485760" | sudo tee -a /etc/sysctl.conf
```

## 2. Build

```bash
git clone https://github.com/warhammer50K/bfly_test.git
cd bfly_test
mkdir build && cd build

# Default build (Spinnaker: /opt/spinnaker, spdlog: /opt/spdlog)
cmake ..
make -j$(nproc)

# Specify custom spdlog path
cmake -DSPDLOG_INCLUDE_DIR=/path/to/spdlog/include ..
make -j$(nproc)
```

## 3. Run

```bash
# Use the first detected camera
./bfly_test

# Select a specific camera by serial number
./bfly_test 12345678

# Set persistent IP on the camera (192.168.1.30)
./bfly_test --set-ip

# Set persistent IP on a specific camera
./bfly_test --set-ip 12345678
```

### Controls

| Key | Action |
|---|---|
| `q` / `ESC` | Quit |
| `Ctrl+C` | Quit (signal) |

## 4. Changing the Persistent IP

Edit the default values in `set_persistent_ip()` in `main.cpp`:

```cpp
constexpr int64_t NEW_IP   = ip_to_int(192, 168, 1, 30);
constexpr int64_t NEW_MASK = ip_to_int(255, 255, 255, 0);
constexpr int64_t NEW_GW   = ip_to_int(192, 168, 1, 1);
```

## Project Structure

```
bfly_test/
├── CMakeLists.txt   # Build configuration
├── main.cpp         # Main source code
└── README.md        # This file
```

## Troubleshooting

- **"No cameras found"**: Ensure the camera and PC are on the same subnet. Test with `SpinView` first.
- **Frame drops / Incomplete image**: Check jumbo frame (MTU 9000) and receive buffer size settings.
- **Permission errors**: To run without `sudo`, copy the udev rules provided by the Spinnaker installer to `/etc/udev/rules.d/`.

## License

MIT
