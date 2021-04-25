
#include "AWS_IoTClientWrapper.hpp"

#include <cstring>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

AWS_IoT_ClientWrapper::AWS_IoT_ClientWrapper()
    : m_Initialized( false ),
      m_Connected( false ),
      m_NeedToRunTask( false ),
      m_PublishDataQueue()
{
    m_TaskMutex  = xSemaphoreCreateMutex();
    m_QueueMutex = xSemaphoreCreateMutex();

    xTaskCreate( AWS_IoTTask, "AWS_IoTTask", sk_TaskStackSize, this, sk_TaskPriority, &m_TaskHandle );
}

AWS_IoT_ClientWrapper::~AWS_IoT_ClientWrapper()
{}

AWS_IoT_ClientWrapper& AWS_IoT_ClientWrapper::Instance()
{
    static AWS_IoT_ClientWrapper s_Instance;
    return s_Instance;
}

bool AWS_IoT_ClientWrapper::Initialize( const ClientInitParam& param )
{
    AWS_IoT_ClientWrapper& instance = AWS_IoT_ClientWrapper::Instance();

    if( instance.m_Initialized ){
        return false;
    }

    bool result = instance.initializeMQTTClient( param );
    instance.m_Initialized = result;
    instance.m_HostURL  = param.HostURL;
    instance.m_HostPort = param.HostPort;

    return result;
}

bool AWS_IoT_ClientWrapper::Connect( const ConnectParam& param )
{
    if( !m_Initialized ){
        return false;
    }
    if( m_Connected ){
        return false;
    }

    bool result = false;
    if( initializeMQTTConnection( param ) ){
        m_Connected = true;
        result = true;
    }

    return result;
}

bool AWS_IoT_ClientWrapper::Disconnect()
{
    IoT_Error_t rc = FAILURE;

    rc = aws_iot_mqtt_autoreconnect_set_status( &m_Client, false );
    if( SUCCESS != rc ) {
        ESP_LOGE( sk_InfoTag, "Unable to set Auto Reconnect to false - %d", rc );
        //abort();
    }
    
    ESP_LOGI( sk_InfoTag, "Disconnecting to AWS..." );
    rc = aws_iot_mqtt_disconnect( &m_Client );

    if( rc == SUCCESS ){
        m_Connected = false;
        runAWSIoTEventLoop( false );
    }
    else {
        ESP_LOGE( sk_InfoTag, "Error(%d) disconnecting to %s:%d", rc, m_HostURL.c_str(), m_HostPort );
    }

    return rc == SUCCESS;
}

void AWS_IoT_ClientWrapper::StartPublish()
{
    runAWSIoTEventLoop( true );
}

void AWS_IoT_ClientWrapper::StopPublish()
{
    runAWSIoTEventLoop( false );
}

bool AWS_IoT_ClientWrapper::Subscribe( const SubscribeTopicParam& param )
{
    IoT_Error_t rc = FAILURE;
    
    ESP_LOGI( sk_InfoTag, "Subscribing..." );
    rc = aws_iot_mqtt_subscribe( &m_Client, param.Topic, std::strlen(param.Topic), param.QOS, SubscribeCallbackHandler, param.Listener );

    if( SUCCESS != rc ) {
        ESP_LOGE( sk_InfoTag, "Error subscribing : %d ", rc );
        abort();
    }

    return rc == SUCCESS;
}

bool AWS_IoT_ClientWrapper::Publish( const PublishTopicParam& txdata )
{
    return queuePublishData( txdata );
}


void AWS_IoT_ClientWrapper::DisconnectCallbackHandler( AWS_IoT_Client *client, void *data )
{
    ESP_LOGW( sk_InfoTag, "MQTT Disconnect" );
    IoT_Error_t rc = FAILURE;

    if( client == nullptr ) {
        return;
    }

    if( aws_iot_is_autoreconnect_enabled( client ) ){
        ESP_LOGI( sk_InfoTag, "Auto Reconnect is enabled, Reconnecting attempt will start now" );
    } else {
        ESP_LOGW( sk_InfoTag, "Auto Reconnect not enabled. Starting manual reconnect..." );
        rc = aws_iot_mqtt_attempt_reconnect( client );
        if( NETWORK_RECONNECTED == rc ) {
            ESP_LOGW( sk_InfoTag, "Manual Reconnect Successful" );
        } else {
            ESP_LOGW( sk_InfoTag, "Manual Reconnect Failed - %d", rc );
        }
    }
}

void AWS_IoT_ClientWrapper::SubscribeCallbackHandler( 
    AWS_IoT_Client *client, char *topic_name, uint16_t topic_name_len, IoT_Publish_Message_Params *params, void *data 
)
{
    ESP_LOGI( sk_InfoTag, "SubscribeCallbackHandler Invoked." );

    if( data ){
        I_SubscribeListener* listener = reinterpret_cast<I_SubscribeListener*>(data);

        // AWS IoT SDKからコールバックされる。
        // このコールバックを抜けた後、確保した変数領域そのまま維持されるか不明なので
        // 自前のメモリに内容をコピーしておく。
        std::string topic_name_string( topic_name, topic_name_len );
        uint8_t* payload_src = reinterpret_cast<uint8_t*>(params->payload);
        std::vector<uint8_t> payload_dst( params->payloadLen );

        std::copy( payload_src, payload_src + (params->payloadLen), payload_dst.begin() );
        
        listener->SubscribeHandler( topic_name_string, payload_dst );
    }
}

void AWS_IoT_ClientWrapper::AWS_IoTTask( void* param )
{
    if( param == nullptr ){
        return;
    }
    AWS_IoT_ClientWrapper* instance = reinterpret_cast<AWS_IoT_ClientWrapper*>(param);

    while(1)
    {
        bool need_running_task = instance->getNeedToRunAWSIoTEventLoop();
        if( need_running_task ){
            instance->AWS_IoTTaskImpl( param );
            instance->runAWSIoTEventLoop( false );
        }
        vTaskDelay( sk_TaskDelayMs );
    }
}

void AWS_IoT_ClientWrapper::AWS_IoTTaskImpl( void* param )
{
    ESP_LOGI( sk_InfoTag, "Running AWS_IoTTaskImpl()" );    
    if( param == nullptr ){
        return;
    }
    
    AWS_IoT_ClientWrapper* instance = reinterpret_cast<AWS_IoT_ClientWrapper*>(param);
    IoT_Error_t rc = SUCCESS;
    PublishTopicParam txdata;

    while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc)) {

        bool need_running_task = instance->getNeedToRunAWSIoTEventLoop();
        if( !need_running_task ){
            ESP_LOGI( sk_InfoTag, "Running Task received request stopping." );    
            break;
        }

        //Max time the yield function will wait for read messages
        rc = aws_iot_mqtt_yield( &(instance->m_Client), 100 );
        if(NETWORK_ATTEMPTING_RECONNECT == rc) {
            // If the client is attempting to reconnect we will skip the rest of the loop.
            continue;
        }

        while( 1 ){
            bool is_empty = true;
            bool result = instance->isEmptyPublishDataQueue( &is_empty );
            if( !result || is_empty ){
                break;
            }
            
            if( instance->getQueuedPublishData( &txdata ) ){
                instance->sendPublishData( txdata );
            }
        }

        vTaskDelay( sk_TaskDelayMs );
    }
    
    ESP_LOGI( sk_InfoTag, "Stop AWS_IoTTaskImpl()" );    
}

bool AWS_IoT_ClientWrapper::getNeedToRunAWSIoTEventLoop()
{
    bool result = false;
    if( xSemaphoreTake( m_TaskMutex, ( portTickType )sk_MutexTakeWaitPeriodMs ) ){
        result = m_NeedToRunTask;

        if( xSemaphoreGive( m_TaskMutex ) != pdTRUE ){
            // IT MUST BE BUG
            abort();
        }
    }

    return result;
}

void AWS_IoT_ClientWrapper::runAWSIoTEventLoop( bool run_start_or_stop )
{
    if( xSemaphoreTake( m_TaskMutex, ( portTickType )sk_MutexTakeWaitPeriodMs ) ){
        m_NeedToRunTask = run_start_or_stop;

        if( xSemaphoreGive( m_TaskMutex ) != pdTRUE ){
            // IT MUST BE BUG
            abort();
        }
    }
}

bool AWS_IoT_ClientWrapper::initializeMQTTClient( const ClientInitParam& param  )
{
    if( m_Initialized ){
        return false;
    }

    IoT_Error_t rc = FAILURE;

    IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;

    ESP_LOGI( sk_InfoTag, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG );

    mqttInitParams.enableAutoReconnect = false; // We enable this later below
    mqttInitParams.pHostURL = param.HostURL;
    mqttInitParams.port = param.HostPort;

    mqttInitParams.pRootCALocation = (const char *)param.AWS_RootCAPemStart;
    mqttInitParams.pDeviceCertLocation = (const char *)param.CertificatePemCrtStart;
    mqttInitParams.pDevicePrivateKeyLocation = (const char *)param.PrivatePemKeyStart;

    mqttInitParams.mqttCommandTimeout_ms  = param.MQTTCommandTimeoutMs;
    mqttInitParams.tlsHandshakeTimeout_ms = param.TLSHandshakeTimeoutMs;
    mqttInitParams.isSSLHostnameVerify    = true;
    mqttInitParams.disconnectHandler      = DisconnectCallbackHandler;
    mqttInitParams.disconnectHandlerData  = nullptr;

    rc = aws_iot_mqtt_init( &m_Client, &mqttInitParams );
    if(SUCCESS != rc) {
        ESP_LOGE( sk_InfoTag, "aws_iot_mqtt_init returned error : %d ", rc );
        //abort();
    }

    return rc == SUCCESS;
}

bool AWS_IoT_ClientWrapper::initializeMQTTConnection( const ConnectParam& param )
{
    IoT_Error_t rc = FAILURE;

    IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

    connectParams.keepAliveIntervalInSec = param.KeepAliveIntervalInSec;
    connectParams.isCleanSession   = true;
    connectParams.MQTTVersion      = MQTT_3_1_1;
    /* Client ID is set in the menuconfig of the example */
    connectParams.pClientID        = param.ClientID;
    connectParams.clientIDLen      = static_cast<uint16_t>(strlen( param.ClientID ));
    connectParams.isWillMsgPresent = false;

    ESP_LOGI( sk_InfoTag, "Connecting to AWS..." );
    do {
        rc = aws_iot_mqtt_connect( &m_Client, &connectParams );
        if( SUCCESS != rc ) {
            ESP_LOGE( sk_InfoTag, "Error(%d) connecting to %s:%d", rc, m_HostURL.c_str(), m_HostPort );
            vTaskDelay( 1000 / portTICK_RATE_MS );
        }
    } while( SUCCESS != rc );

    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    rc = aws_iot_mqtt_autoreconnect_set_status( &m_Client, true );
    if( SUCCESS != rc ) {
        ESP_LOGE( sk_InfoTag, "Unable to set Auto Reconnect to true - %d", rc );
        //abort();
    }

    return rc == SUCCESS;
}

bool AWS_IoT_ClientWrapper::isEmptyPublishDataQueue( bool* is_empty_result ) const
{
    if( is_empty_result == nullptr ){
        return false;
    }

    bool result = false;
    if( xSemaphoreTake( m_QueueMutex, ( portTickType )sk_MutexTakeWaitPeriodMs ) ){
        *is_empty_result = m_PublishDataQueue.empty();

        if( xSemaphoreGive( m_QueueMutex ) != pdTRUE ){
            // IT MUST BE BUG
            abort();
        }
        result = true;
    }

    return result;
}

bool AWS_IoT_ClientWrapper::queuePublishData( const PublishTopicParam& data )
{
    bool result = false;
    if( xSemaphoreTake( m_QueueMutex, ( portTickType )sk_MutexTakeWaitPeriodMs ) ){
        m_PublishDataQueue.push( data );

        if( xSemaphoreGive( m_QueueMutex ) != pdTRUE ){
            // IT MUST BE BUG
            abort();
        }
        result = true;
    }

    return result;
}

bool AWS_IoT_ClientWrapper::getQueuedPublishData( PublishTopicParam* queued_data )
{
    bool result = false;
    if( xSemaphoreTake( m_QueueMutex, ( portTickType )sk_MutexTakeWaitPeriodMs ) ){
        if( !m_PublishDataQueue.empty() ){
            const PublishTopicParam& data = m_PublishDataQueue.front();
            *queued_data = data;
            m_PublishDataQueue.pop();

            result = true;
        }

        if( xSemaphoreGive( m_QueueMutex ) != pdTRUE ){
            // IT MUST BE BUG
            abort();
        }
    }

    return result;
}

bool AWS_IoT_ClientWrapper::sendPublishData( const PublishTopicParam& txdata )
{
    IoT_Error_t rc = FAILURE;
    IoT_Publish_Message_Params msgparam;

    msgparam.qos        = txdata.QOS;
    msgparam.payload    = (void *)txdata.Payload.data();
    msgparam.isRetained = 0;
    msgparam.payloadLen = txdata.Payload.size();

    // ESP_LOGI(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
    rc = aws_iot_mqtt_publish( &m_Client, txdata.Topic, std::strlen(txdata.Topic), &msgparam );
    
    if( rc == SUCCESS ){
        ESP_LOGI( sk_InfoTag, "Publishing Data!" );
    }
    else {
        ESP_LOGW( sk_InfoTag, "Publishing Data failed. ErrorCode(%d)", rc );
    }
    if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
        ESP_LOGW( sk_InfoTag, "QOS0 publish ack not received." );
        rc = SUCCESS;
    }


    return rc == SUCCESS;
}