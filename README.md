# bfly_test - FLIR Blackfly GigE Camera Viewer

FLIR Blackfly GigE 카메라에서 영상을 실시간으로 수신하여 OpenCV 창에 표시하는 C++ 예제 프로그램입니다.

## 주요 기능

- Spinnaker SDK를 이용한 GigE Vision 카메라 자동 검색 및 연결
- 컬러(BGR8) / 흑백(Mono8) 카메라 자동 감지
- 실시간 FPS 측정 및 로깅
- Persistent IP 설정 기능 (`--set-ip`)
- 시리얼 번호로 특정 카메라 선택

## 요구사항

| 항목 | 버전 | 비고 |
|---|---|---|
| **OS** | Ubuntu 20.04+ (x86_64) | aarch64도 Spinnaker SDK 지원 시 가능 |
| **CMake** | 3.16+ | |
| **C++ 컴파일러** | C++17 지원 (GCC 7+) | |
| **Spinnaker SDK** | 2.x+ | FLIR 공식 사이트에서 다운로드 |
| **OpenCV** | 4.x | `apt install libopencv-dev` |
| **spdlog** | 1.x | 소스 빌드 또는 `apt install libspdlog-dev` |
| **fmt** | 7.x+ | `apt install libfmt-dev` |

## 1. 의존성 설치

### Spinnaker SDK

FLIR 공식 사이트에서 Spinnaker SDK for Linux를 다운로드합니다:
https://www.flir.com/products/spinnaker-sdk/

```bash
# 다운로드한 .deb 패키지 설치 (예시)
sudo dpkg -i spinnaker-<version>_amd64.deb
sudo apt-get install -f

# 또는 tar.gz를 /opt/spinnaker에 설치
# 기본 경로: /opt/spinnaker
```

> **참고**: Spinnaker SDK 설치 후 `/opt/spinnaker/include/Spinnaker.h`가 존재해야 합니다.

### OpenCV, spdlog, fmt

```bash
sudo apt update
sudo apt install -y libopencv-dev libfmt-dev libspdlog-dev
```

spdlog를 소스에서 빌드하는 경우:

```bash
git clone https://github.com/gabime/spdlog.git
cd spdlog && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/spdlog
make -j$(nproc) && sudo make install
```

### GigE 카메라 네트워크 설정

GigE Vision 카메라는 Jumbo Frame 설정이 권장됩니다:

```bash
# 네트워크 인터페이스 이름 확인
ip link show

# Jumbo Frame 활성화 (예: eno1)
sudo ip link set eno1 mtu 9000

# 영구 설정 (/etc/netplan/ 또는 /etc/network/interfaces에서 mtu 9000 추가)
```

수신 버퍼 크기도 늘려줍니다:

```bash
sudo sysctl -w net.core.rmem_max=10485760
sudo sysctl -w net.core.rmem_default=10485760

# 영구 설정
echo "net.core.rmem_max=10485760" | sudo tee -a /etc/sysctl.conf
echo "net.core.rmem_default=10485760" | sudo tee -a /etc/sysctl.conf
```

## 2. 빌드

```bash
git clone https://github.com/warhammer50K/bfly_test.git
cd bfly_test
mkdir build && cd build

# 기본 빌드 (Spinnaker: /opt/spinnaker, spdlog: /opt/spdlog)
cmake ..
make -j$(nproc)

# spdlog 경로를 직접 지정하는 경우
cmake -DSPDLOG_INCLUDE_DIR=/path/to/spdlog/include ..
make -j$(nproc)
```

## 3. 실행

```bash
# 첫 번째 발견된 카메라로 실행
./bfly_test

# 특정 시리얼 번호의 카메라 지정
./bfly_test 12345678

# 카메라에 Persistent IP 설정 (192.168.1.30)
./bfly_test --set-ip

# 특정 카메라에 Persistent IP 설정
./bfly_test --set-ip 12345678
```

### 조작법

| 키 | 동작 |
|---|---|
| `q` / `ESC` | 종료 |
| `Ctrl+C` | 종료 (시그널) |

## 4. Persistent IP 변경

기본 설정값은 `main.cpp`의 `set_persistent_ip()` 함수에서 수정할 수 있습니다:

```cpp
constexpr int64_t NEW_IP   = ip_to_int(192, 168, 1, 30);
constexpr int64_t NEW_MASK = ip_to_int(255, 255, 255, 0);
constexpr int64_t NEW_GW   = ip_to_int(192, 168, 1, 1);
```

## 프로젝트 구조

```
bfly_test/
├── CMakeLists.txt   # 빌드 설정
├── main.cpp         # 메인 소스 코드
└── README.md        # 이 문서
```

## 트러블슈팅

- **"카메라를 찾을 수 없습니다"**: 카메라와 PC가 같은 서브넷에 있는지 확인. `SpinView`로 먼저 연결 테스트 권장.
- **프레임 드롭 / Incomplete image**: Jumbo Frame(MTU 9000) 및 수신 버퍼 크기 설정 확인.
- **권한 오류**: `sudo` 없이 실행하려면 udev rule 설정 필요 (`/etc/udev/rules.d/`에 Spinnaker 설치 시 제공되는 규칙 파일 복사).

## License

MIT
