#ifndef     AWS_IOT_CLIENT_WRAPPTER_HPP_INCLUDED
#define     AWS_IOT_CLIENT_WRAPPTER_HPP_INCLUDED

#include <cstdint>
#include <vector>
#include <queue>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "aws_iot_mqtt_client_interface.h"

#include "I_SubscribeListener.hpp"

class AWS_IoT_ClientWrapper
{
public:

    struct ClientInitParam
    {
        char* HostURL;
        uint32_t    HostPort;
        const uint8_t* AWS_RootCAPemStart;
        const uint8_t* CertificatePemCrtStart;
        const uint8_t* PrivatePemKeyStart;
        uint32_t MQTTCommandTimeoutMs;
        uint32_t TLSHandshakeTimeoutMs;
    };

    struct ConnectParam
    {
        uint32_t KeepAliveIntervalInSec;
        const char* ClientID;
    };

    struct SubscribeTopicParam 
    {
        const char* Topic;
        QoS QOS;
        I_SubscribeListener* Listener;
    };

    using PublishPayloadArray = std::vector<uint8_t>;
    struct PublishTopicParam
    {
        const char* Topic;
        QoS QOS;
        PublishPayloadArray Payload;
    };

    static inline constexpr char sk_InfoTag[] = "AWS_IoTWrap";

public:

    // DO NOT COPY
    AWS_IoT_ClientWrapper( const AWS_IoT_ClientWrapper& ) = delete;
    AWS_IoT_ClientWrapper& operator=( const AWS_IoT_ClientWrapper& ) = delete;

    static AWS_IoT_ClientWrapper& Instance();
    static bool Initialize( const ClientInitParam& param );

    bool Connect( const ConnectParam& param );
    bool Disconnect();
    bool IsConnected() const;

    void StartPublish();
    void StopPublish();
    bool Subscribe( const SubscribeTopicParam& param );
    bool Publish( const PublishTopicParam& txdata );

private:

    AWS_IoT_ClientWrapper();
    ~AWS_IoT_ClientWrapper() noexcept;

    static const portTickType sk_TaskDelayMs = (50 / portTICK_PERIOD_MS);
    static const portTickType sk_MutexTakeWaitPeriodMs = (100 / portTICK_PERIOD_MS);
    static const int sk_TaskStackSize = 1024 * 8;
    static const int sk_TaskPriority  = configMAX_PRIORITIES - 3;

    static void DisconnectCallbackHandler( AWS_IoT_Client *client, void *data ); 
    static void SubscribeCallbackHandler( AWS_IoT_Client *client, char *topic_name, uint16_t topic_name_len, IoT_Publish_Message_Params *params, void *data );
    static void AWS_IoTTask( void* param );
    static void AWS_IoTTaskImpl( void* param );

    bool getNeedToRunAWSIoTEventLoop();
    void runAWSIoTEventLoop( bool run_start_or_stop );

    bool initializeMQTTClient( const ClientInitParam& param );
    bool initializeMQTTConnection( const ConnectParam& param );

    bool isEmptyPublishDataQueue( bool* is_empty_result ) const;
    bool queuePublishData( const PublishTopicParam& data );
    bool getQueuedPublishData( PublishTopicParam* queued_data );
    bool sendPublishData( const PublishTopicParam& txdata );


    AWS_IoT_Client   m_Client;

    bool             m_Initialized;
    bool             m_Connected;

    xTaskHandle      m_TaskHandle;
    xSemaphoreHandle m_TaskMutex;
    bool             m_NeedToRunTask;

    std::string      m_HostURL;
    uint32_t         m_HostPort;

    xSemaphoreHandle m_QueueMutex;
    std::queue<PublishTopicParam> m_PublishDataQueue;
};

#endif      // AWS_IOT_CLIENT_WRAPPTER_HPP_INCLUDED
