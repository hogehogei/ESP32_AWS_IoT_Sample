
#include "Camera.hpp"

#include "esp_log.h"

static camera_config_t s_CameraConfig = {
    .pin_pwdn       = Camera::sk_Pin_PWDN,
    .pin_reset      = Camera::sk_Pin_RESET,
    .pin_xclk       = Camera::sk_Pin_XCLK,
    .pin_sscb_sda   = Camera::sk_Pin_SIOD,
    .pin_sscb_scl   = Camera::sk_Pin_SIOC,

    .pin_d7         = Camera::sk_Pni_D7,
    .pin_d6         = Camera::sk_Pin_D6,
    .pin_d5         = Camera::sk_Pin_D5,
    .pin_d4         = Camera::sk_Pin_D4,
    .pin_d3         = Camera::sk_Pin_D3,
    .pin_d2         = Camera::sk_Pin_D2,
    .pin_d1         = Camera::sk_Pin_D1,
    .pin_d0         = Camera::sk_Pin_D0,

    .pin_vsync      = Camera::sk_Pin_VSYNC,
    .pin_href       = Camera::sk_Pin_HREF,
    .pin_pclk       = Camera::sk_Pin_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz   = Camera::sk_XCLK_FreqHz,
    .ledc_timer     = Camera::sk_LEDC_Timer,
    .ledc_channel   = Camera::sk_LEDC_Channel,

    .pixel_format   = Camera::sk_PixelFormat,
    .frame_size     = Camera::sk_FrameSize,
    .jpeg_quality   = Camera::sk_JpegQuality,
    .fb_count       = Camera::sk_FrameBuffCount
};



// 
// class CameraFrameBuffer implemantation
//

CameraFrameBuffer::CameraFrameBuffer()
    : m_FrameBuffer( nullptr )
{}

CameraFrameBuffer::CameraFrameBuffer( camera_fb_t* fb )
    : m_FrameBuffer( fb, 
        [](camera_fb_t* p) {
            if( p ){
                esp_camera_fb_return( p );
            }
        }
    )
{}

CameraFrameBuffer::~CameraFrameBuffer()
{}

bool CameraFrameBuffer::IsValid() const
{
    return m_FrameBuffer.get() != nullptr;
}

size_t CameraFrameBuffer::Width() const
{
    if( m_FrameBuffer ){
        return m_FrameBuffer->width;
    }

    return 0;
}

size_t CameraFrameBuffer::Height() const
{
    if( m_FrameBuffer ){
        return m_FrameBuffer->height;
    }

    return 0;
}

pixformat_t CameraFrameBuffer::Format() const
{
    if( m_FrameBuffer ){
        return m_FrameBuffer->format;
    }

    // とりあえず適当
    return PIXFORMAT_JPEG;
}

size_t CameraFrameBuffer::Length() const
{
    if( m_FrameBuffer ){
        return m_FrameBuffer->len;
    }

    return 0;
}

const uint8_t* CameraFrameBuffer::Buffer() const
{
    if( m_FrameBuffer ){
        return m_FrameBuffer->buf;
    }

    return nullptr;
}

camera_fb_t* CameraFrameBuffer::RawPtr()
{
    return m_FrameBuffer.get();
}


// 
// class Camera implemantation
//
bool Camera::Initialize()
{
    if( sk_Pin_PWDN != -1 ){
        pwdnPinPowerUp();
    }

    if( esp_camera_init(&s_CameraConfig) != ESP_OK ){
        ESP_LOGE( Camera::sk_CameraTag, "Camera Init Failed." );
        return false;
    }

    return true;
}

Camera& Camera::Instance()
{
    static Camera s_Instance;
    return s_Instance;
}

bool Camera::Capture()
{
    // すでに有効な画像があるならまず解放
    if( m_CapturedImage.IsValid() ){
        m_CapturedImage = CameraFrameBuffer();
    }

    CameraFrameBuffer fb( esp_camera_fb_get() );
    if( fb.IsValid() ){
        m_CapturedImage = fb;
    }

    return fb.IsValid();
}

CameraFrameBuffer Camera::FrameBuffer()
{
    return m_CapturedImage;
}

void Camera::pwdnPinPowerUp()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = (gpio_int_type_t)(GPIO_PIN_INTR_DISABLE);
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = (1ULL << sk_Pin_PWDN);
    //disable pull-down mode
    io_conf.pull_down_en = (gpio_pulldown_t)(0);
    //disable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)(0);
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    // set low level
    gpio_set_level( sk_Pin_PWDN, 0 );
}