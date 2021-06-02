
#include "Tasks.hpp"

#include <sstream>

#include "sdkconfig.h"
#include "AWS_IotClientWrapper.hpp"
#include "SubscribeURLListener.hpp"

#if defined(CONFIG_EXAMPLE_EMBEDDED_CERTS)
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");
extern const uint8_t certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t certificate_pem_crt_end[] asm("_binary_certificate_pem_crt_end");
extern const uint8_t private_pem_key_start[] asm("_binary_private_pem_key_start");
extern const uint8_t private_pem_key_end[] asm("_binary_private_pem_key_end");
#endif

static char s_AWS_HostAddress[] = AWS_IOT_MQTT_HOST;
static uint32_t s_AWS_HostPort  = AWS_IOT_MQTT_PORT;
static SubscribeURLListener* s_SubscribeListener = nullptr;

void Initialize_AWS_IoT( void )
{
    AWS_IoT_ClientWrapper& instance = AWS_IoT_ClientWrapper::Instance();
    AWS_IoT_ClientWrapper::ClientInitParam initparam;

    initparam.HostURL                   = s_AWS_HostAddress;
    initparam.HostPort                  = s_AWS_HostPort;
    initparam.AWS_RootCAPemStart        = aws_root_ca_pem_start;
    initparam.CertificatePemCrtStart    = certificate_pem_crt_start;
    initparam.PrivatePemKeyStart        = private_pem_key_start;
    initparam.MQTTCommandTimeoutMs      = 20000;
    initparam.TLSHandshakeTimeoutMs     = 5000;

    if( !instance.Initialize( initparam ) ){
        ESP_LOGE( AWS_IoT_ClientWrapper::sk_InfoTag, "AWS_IoT_ClientWrapper initalize failed." );
        abort();
    }

    AWS_IoT_ClientWrapper::ConnectParam connparam;
    connparam.KeepAliveIntervalInSec    = 10;
    connparam.ClientID                  = CONFIG_AWS_EXAMPLE_CLIENT_ID;
    if( !instance.Connect( connparam ) ){
        ESP_LOGE( AWS_IoT_ClientWrapper::sk_InfoTag, "AWS_IoT_ClientWrapper start connection failed." );
        abort();
    }

    AWS_IoT_ClientWrapper::SubscribeTopicParam subparam;
    s_SubscribeListener = new SubscribeURLListener();
    subparam.Topic      = "esp32/sub/url";
    subparam.QOS        = QOS0;
    subparam.Listener   = s_SubscribeListener;
    instance.Subscribe( subparam );
    ESP_LOGI( AWS_IoT_ClientWrapper::sk_InfoTag, "Subscribe complete!" );

    instance.StartEventLoop();
}

void PublishHelloWorld( void )
{
    AWS_IoT_ClientWrapper::PublishTopicParam publish_data;
    
    publish_data.Topic   = "esp32/pub/url";
    publish_data.QOS     = QOS0;
    
    std::ostringstream os;
    os << "{\"id\": \"" << CONFIG_AWS_EXAMPLE_CLIENT_ID << "\"}";
    
    std::string message = os.str();
    AWS_IoT_ClientWrapper::PublishPayloadArray message_payload( message.length() );
    std::copy( message.begin(), message.end(), message_payload.begin() );
    publish_data.Payload = message_payload;

    AWS_IoT_ClientWrapper::Instance().Publish( publish_data );
}

