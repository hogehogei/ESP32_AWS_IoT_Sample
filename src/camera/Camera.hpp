#ifndef     I_CAMERA_HPP_INCLUDED
#define     I_CAMERA_HPP_INCLUDED

#include <memory>

#include "esp_camera.h"


class CameraFrameBuffer
{
public:

    ~CameraFrameBuffer() noexcept;

    bool IsValid() const;
    size_t Width() const;
    size_t Height() const;
    pixformat_t Format() const;
    size_t Length() const;
    const uint8_t* Buffer() const;
    camera_fb_t* RawPtr();
    
private:
    friend class Camera;

    using FrameBufferSharedPtr = std::shared_ptr<camera_fb_t>;

    CameraFrameBuffer();
    CameraFrameBuffer( camera_fb_t* fb );

    FrameBufferSharedPtr m_FrameBuffer;
};

class Camera
{
public:
    Camera() = default;
    ~Camera() noexcept {}

    // DO NOT COPY!
    Camera& operator=( const Camera& ) = delete;
    Camera( const Camera& ) = delete;

    static bool Initialize();
    static Camera& Instance();

    bool Capture();
    CameraFrameBuffer FrameBuffer();

    static constexpr gpio_num_t sk_Pin_PWDN    = static_cast<gpio_num_t>(26);
    static constexpr gpio_num_t sk_Pin_RESET   = static_cast<gpio_num_t>(-1);
    static constexpr gpio_num_t sk_Pin_XCLK    = static_cast<gpio_num_t>(32);
    static constexpr gpio_num_t sk_Pin_SIOD    = static_cast<gpio_num_t>(13);
    static constexpr gpio_num_t sk_Pin_SIOC    = static_cast<gpio_num_t>(12);

    static constexpr gpio_num_t sk_Pin_D0      = static_cast<gpio_num_t>(5);
    static constexpr gpio_num_t sk_Pin_D1      = static_cast<gpio_num_t>(14);
    static constexpr gpio_num_t sk_Pin_D2      = static_cast<gpio_num_t>(4);
    static constexpr gpio_num_t sk_Pin_D3      = static_cast<gpio_num_t>(15);
    static constexpr gpio_num_t sk_Pin_D4      = static_cast<gpio_num_t>(18);
    static constexpr gpio_num_t sk_Pin_D5      = static_cast<gpio_num_t>(23);
    static constexpr gpio_num_t sk_Pin_D6      = static_cast<gpio_num_t>(36);
    static constexpr gpio_num_t sk_Pni_D7      = static_cast<gpio_num_t>(39);
    static constexpr gpio_num_t sk_Pin_VSYNC   = static_cast<gpio_num_t>(27);
    static constexpr gpio_num_t sk_Pin_HREF    = static_cast<gpio_num_t>(25);
    static constexpr gpio_num_t sk_Pin_PCLK    = static_cast<gpio_num_t>(19);

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    static constexpr int sk_XCLK_FreqHz = 20000000;
    static constexpr ledc_timer_t sk_LEDC_Timer  = LEDC_TIMER_0;
    static constexpr ledc_channel_t sk_LEDC_Channel = LEDC_CHANNEL_0;
    
    static constexpr pixformat_t sk_PixelFormat = PIXFORMAT_JPEG;
    static constexpr framesize_t sk_FrameSize   = FRAMESIZE_UXGA;
    static constexpr int sk_JpegQuality = 12;
    static constexpr int sk_FrameBuffCount = 1;

    static constexpr char sk_CameraTag[] = "Camera";

private:

    static void pwdnPinPowerUp();

    CameraFrameBuffer m_CapturedImage;
};

#endif    // I_CAMERA_HPP_INCLUDED
