#include <csignal>
#include <atomic>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>

#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"

static std::atomic<bool> g_running{true};
static void sig_handler(int) { g_running = false; }

// ── IP Utilities ────────────────────────────────
static constexpr int64_t ip_to_int(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return (int64_t(a) << 24) | (int64_t(b) << 16) | (int64_t(c) << 8) | d;
}

static std::string ip_to_str(int64_t ip)
{
    return fmt::format("{}.{}.{}.{}",
        (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
        (ip >> 8) & 0xFF, ip & 0xFF);
}

// ── Persistent IP Configuration ─────────────────
static bool set_persistent_ip(Spinnaker::CameraPtr pCam)
{
    using namespace Spinnaker::GenApi;

    constexpr int64_t NEW_IP   = ip_to_int(192, 168, 1, 30);
    constexpr int64_t NEW_MASK = ip_to_int(255, 255, 255, 0);
    constexpr int64_t NEW_GW   = ip_to_int(192, 168, 1, 1);

    try
    {
        pCam->Init();
        INodeMap& nm = pCam->GetNodeMap();

        // Enable persistent IP mode
        CBooleanPtr persistEn = nm.GetNode("GevCurrentIPConfigurationPersistentIP");
        if (!IsWritable(persistEn))
        {
            spdlog::error("GevCurrentIPConfigurationPersistentIP node is not writable");
            pCam->DeInit();
            return false;
        }
        persistEn->SetValue(true);

        // Set persistent IP / Mask / Gateway
        CIntegerPtr persistIP   = nm.GetNode("GevPersistentIPAddress");
        CIntegerPtr persistMask = nm.GetNode("GevPersistentSubnetMask");
        CIntegerPtr persistGW   = nm.GetNode("GevPersistentDefaultGateway");

        if (!IsWritable(persistIP) || !IsWritable(persistMask) || !IsWritable(persistGW))
        {
            spdlog::error("Persistent IP nodes are not writable");
            pCam->DeInit();
            return false;
        }

        persistIP->SetValue(NEW_IP);
        persistMask->SetValue(NEW_MASK);
        persistGW->SetValue(NEW_GW);

        spdlog::info("Persistent IP configured: {} (mask: {}, gw: {})",
                     ip_to_str(NEW_IP), ip_to_str(NEW_MASK), ip_to_str(NEW_GW));

        pCam->DeInit();
        spdlog::info("Changes will take effect after camera reboot or reconnect");
        return true;
    }
    catch (Spinnaker::Exception& e)
    {
        spdlog::error("Failed to set persistent IP: {}", e.what());
        try { pCam->DeInit(); } catch(...) {}
        return false;
    }
}


// ── Print GigE Network Info ─────────────────────
static void print_gige_info(Spinnaker::CameraPtr pCam)
{
    Spinnaker::GenApi::INodeMap& tldev = pCam->GetTLDeviceNodeMap();

    Spinnaker::GenApi::CIntegerPtr cam_ip_node = tldev.GetNode("GevDeviceIPAddress");
    Spinnaker::GenApi::CIntegerPtr cam_mask_node = tldev.GetNode("GevDeviceSubnetMask");

    if(Spinnaker::GenApi::IsReadable(cam_ip_node) &&
       Spinnaker::GenApi::IsReadable(cam_mask_node))
    {
        spdlog::info("Camera IP: {} (mask: {})",
                     ip_to_str(cam_ip_node->GetValue()),
                     ip_to_str(cam_mask_node->GetValue()));
    }
}

int main(int argc, char** argv)
{
    std::signal(SIGINT, sig_handler);

    // ── Configuration ────────────────────────────
    constexpr int    TARGET_FPS = 30;
    std::string      target_serial;   // empty = use first camera
    bool             do_set_ip = false;

    for (int i = 1; i < argc; i++)
    {
        if (std::string(argv[i]) == "--set-ip")
            do_set_ip = true;
        else
            target_serial = argv[i];
    }

    // ── Initialize Spinnaker ────────────────────
    Spinnaker::SystemPtr system = Spinnaker::System::GetInstance();
    Spinnaker::CameraList cam_list = system->GetCameras();

    if(cam_list.GetSize() == 0)
    {
        spdlog::error("No cameras found");
        cam_list.Clear();
        system->ReleaseInstance();
        return 1;
    }

    spdlog::info("Cameras found: {}", cam_list.GetSize());

    // List detected cameras
    for(unsigned int i = 0; i < cam_list.GetSize(); i++)
    {
        auto cam_i = cam_list.GetByIndex(i);
        auto& tldev = cam_i->GetTLDeviceNodeMap();
        Spinnaker::GenApi::CStringPtr sn = tldev.GetNode("DeviceSerialNumber");
        Spinnaker::GenApi::CStringPtr mn = tldev.GetNode("DeviceModelName");

        // Print GigE IP info
        Spinnaker::GenApi::CIntegerPtr ip_node = tldev.GetNode("GevDeviceIPAddress");
        std::string ip_str = (Spinnaker::GenApi::IsReadable(ip_node)) ?
                             ip_to_str(ip_node->GetValue()) : "N/A";

        if(Spinnaker::GenApi::IsReadable(sn))
            spdlog::info("  [{}] serial={}, model={}, ip={}",
                         i,
                         std::string(sn->GetValue().c_str()),
                         Spinnaker::GenApi::IsReadable(mn) ? std::string(mn->GetValue().c_str()) : "?",
                         ip_str);
    }

    // ── Select Camera ────────────────────────────
    Spinnaker::CameraPtr pCam = nullptr;

    if(target_serial.empty())
    {
        pCam = cam_list.GetByIndex(0);
        spdlog::info("No serial specified, using first camera");
    }
    else
    {
        for(unsigned int i = 0; i < cam_list.GetSize(); i++)
        {
            auto cam_i = cam_list.GetByIndex(i);
            auto& tldev = cam_i->GetTLDeviceNodeMap();
            Spinnaker::GenApi::CStringPtr sn = tldev.GetNode("DeviceSerialNumber");
            if(Spinnaker::GenApi::IsReadable(sn) &&
               std::string(sn->GetValue().c_str()) == target_serial)
            {
                pCam = cam_i;
                break;
            }
        }
        if(!pCam)
        {
            spdlog::error("Camera with serial {} not found", target_serial);
            cam_list.Clear();
            system->ReleaseInstance();
            return 1;
        }
    }

    // ── GigE Network Info ────────────────────────
    print_gige_info(pCam);

    // ── Persistent IP Setup Mode ─────────────────
    if (do_set_ip)
    {
        bool ok = set_persistent_ip(pCam);
        pCam = nullptr;
        cam_list.Clear();
        system->ReleaseInstance();
        return ok ? 0 : 1;
    }

    // ── Camera Setup ────────────────────────────
    try
    {
        pCam->Init();
        Spinnaker::GenApi::INodeMap& nm = pCam->GetNodeMap();

        // Continuous mode
        Spinnaker::GenApi::CEnumerationPtr acq_mode = nm.GetNode("AcquisitionMode");
        Spinnaker::GenApi::CEnumEntryPtr acq_cont = acq_mode->GetEntryByName("Continuous");
        acq_mode->SetIntValue(acq_cont->GetValue());

        // Frame rate
        Spinnaker::GenApi::CBooleanPtr fr_en = nm.GetNode("AcquisitionFrameRateEnable");
        if(Spinnaker::GenApi::IsWritable(fr_en))
            fr_en->SetValue(true);

        Spinnaker::GenApi::CFloatPtr fr = nm.GetNode("AcquisitionFrameRate");
        if(Spinnaker::GenApi::IsWritable(fr))
            fr->SetValue(TARGET_FPS);

        // Pixel format — mono cameras use Mono8, color cameras use BGR8
        Spinnaker::GenApi::CEnumerationPtr pf = nm.GetNode("PixelFormat");
        Spinnaker::GenApi::CEnumEntryPtr pf_bgr = pf->GetEntryByName("BGR8");
        Spinnaker::GenApi::CEnumEntryPtr pf_mono = pf->GetEntryByName("Mono8");

        bool is_mono = false;
        if(Spinnaker::GenApi::IsReadable(pf_bgr))
        {
            pf->SetIntValue(pf_bgr->GetValue());
        }
        else if(Spinnaker::GenApi::IsReadable(pf_mono))
        {
            pf->SetIntValue(pf_mono->GetValue());
            is_mono = true;
            spdlog::info("Mono camera detected, using Mono8");
        }

        // Print resolution
        Spinnaker::GenApi::CIntegerPtr w_node = nm.GetNode("Width");
        Spinnaker::GenApi::CIntegerPtr h_node = nm.GetNode("Height");
        int w = Spinnaker::GenApi::IsReadable(w_node) ? static_cast<int>(w_node->GetValue()) : 0;
        int h = Spinnaker::GenApi::IsReadable(h_node) ? static_cast<int>(h_node->GetValue()) : 0;

        pCam->BeginAcquisition();
        spdlog::info("Acquisition started ({}x{}, {}fps, {})",
                     w, h, TARGET_FPS, is_mono ? "Mono8" : "BGR8");
    }
    catch(Spinnaker::Exception& e)
    {
        spdlog::error("Camera initialization failed: {}", e.what());
        pCam = nullptr;
        cam_list.Clear();
        system->ReleaseInstance();
        return 1;
    }

    // ── Grab loop + imshow ────────────────────────
    Spinnaker::ImageProcessor img_proc;
    img_proc.SetColorProcessing(Spinnaker::SPINNAKER_COLOR_PROCESSING_ALGORITHM_HQ_LINEAR);

    const std::string win_name = "BFLY Test";
    cv::namedWindow(win_name, cv::WINDOW_AUTOSIZE);

    int frame_count = 0;
    auto t_start = std::chrono::steady_clock::now();

    while(g_running)
    {
        try
        {
            Spinnaker::ImagePtr raw = pCam->GetNextImage(1000);

            if(raw->IsIncomplete())
            {
                spdlog::warn("Incomplete image: {}",
                    Spinnaker::Image::GetImageStatusDescription(raw->GetImageStatus()));
                raw->Release();
                continue;
            }

            unsigned int rows = raw->GetHeight();
            unsigned int cols = raw->GetWidth();

            cv::Mat frame;
            if(raw->GetPixelFormat() == Spinnaker::PixelFormat_Mono8)
            {
                frame = cv::Mat(rows, cols, CV_8UC1, raw->GetData()).clone();
            }
            else
            {
                Spinnaker::ImagePtr converted = img_proc.Convert(raw, Spinnaker::PixelFormat_BGR8);
                frame = cv::Mat(rows, cols, CV_8UC3, converted->GetData()).clone();
            }
            raw->Release();

            // Calculate FPS
            frame_count++;
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - t_start).count();
            if(elapsed >= 1.0)
            {
                double fps = frame_count / elapsed;
                spdlog::info("FPS: {:.1f}", fps);
                frame_count = 0;
                t_start = now;
            }

            cv::imshow(win_name, frame);
        }
        catch(Spinnaker::Exception& e)
        {
            spdlog::error("Grab error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        int key = cv::waitKey(1);
        if(key == 27 || key == 'q')  // ESC or q
            break;
    }

    // ── Cleanup ─────────────────────────────────
    cv::destroyAllWindows();

    try
    {
        pCam->EndAcquisition();
        pCam->DeInit();
    }
    catch(Spinnaker::Exception& e)
    {
        spdlog::error("Cleanup error: {}", e.what());
    }

    pCam = nullptr;
    cam_list.Clear();
    system->ReleaseInstance();

    spdlog::info("Done.");
    return 0;
}
