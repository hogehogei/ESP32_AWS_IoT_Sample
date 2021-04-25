/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "Tasks.hpp"

//
// static variables
//
static const int sk_Button_IONum = GPIO_NUM_34;
static const char WifiLogInfoTag[] = "Wifi";
static const char AppInfoTag[] = "App";

static const uint8_t sk_IPAddr[]  = { 192, 168, 24, 10 };
static const uint8_t sk_Gateway[] = { 192, 168, 24, 1 };
static const uint8_t sk_NetMask[] = { 255, 255, 255, 0 };
static const uint8_t sk_DNS_Server[] = { 192, 168, 24, 1};

static esp_netif_t* s_NetIf = NULL;
static EventGroupHandle_t s_WifiEventGroup;
static uint8_t m_BaseMacAddr[6] = {0};
static int s_ButtonPressedDown = 0;
static int s_ButtonTrigger = 0;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;

//
// static functions
//

static void Initialize_App( void );
static void Initialize_Wifi( void );
static void WifiEventHandler( void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data );
static void Initialize_AWS_IoTClient( void );

void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU cores, WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);
    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    printf("Free heap: %d\n", esp_get_free_heap_size());

    Initialize_App();
    Initialize_Wifi();
    Initialize_AWS_IoTClient();

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits( s_WifiEventGroup, CONNECTED_BIT, false, true, portMAX_DELAY );

    while( 1 )
    {
        if( gpio_get_level(sk_Button_IONum) && !s_ButtonPressedDown ){
            s_ButtonPressedDown = true;
            s_ButtonTrigger = false;
        }
        if( !gpio_get_level(sk_Button_IONum) && s_ButtonPressedDown ){
            s_ButtonPressedDown = false;
            s_ButtonTrigger = true;
        }
        if( s_ButtonTrigger ){
            PublishHelloWorld();
            s_ButtonTrigger = false;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

static void Initialize_App( void )
{
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = (gpio_int_type_t)(GPIO_PIN_INTR_DISABLE);
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = (1ULL << sk_Button_IONum);
    //disable pull-down mode
    io_conf.pull_down_en = (gpio_pulldown_t)(0);
    //disable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)(0);
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    esp_err_t ret = ESP_OK;
    esp_efuse_mac_get_default( m_BaseMacAddr );

    ret = esp_efuse_mac_get_default( m_BaseMacAddr );
    if (ret != ESP_OK) {
        ESP_LOGE( AppInfoTag, "Failed to get base MAC address from EFUSE BLK0. (%s)", esp_err_to_name(ret) );
        ESP_LOGE( AppInfoTag, "Aborting" );
        abort();
    } else {
        ESP_LOGI( AppInfoTag, "Base MAC Address read from EFUSE BLK0." );
        esp_log_buffer_hexdump_internal( AppInfoTag, m_BaseMacAddr, 6, ESP_LOG_INFO );
    }    
}

static void Initialize_Wifi( void )
{
    s_WifiEventGroup = xEventGroupCreate();

    //
    // ネットワークインターフェース初期化
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_NetIf = esp_netif_create_default_wifi_sta();
    // 先にDHCPを停止する
    ESP_ERROR_CHECK( esp_netif_dhcpc_stop( s_NetIf ) );
    // static IP 設定
    esp_netif_ip_info_t ip_info;
    esp_netif_set_ip4_addr( &ip_info.ip, sk_IPAddr[0], sk_IPAddr[1], sk_IPAddr[2], sk_IPAddr[3] );
    esp_netif_set_ip4_addr( &ip_info.gw, sk_Gateway[0], sk_Gateway[1], sk_Gateway[2], sk_Gateway[3] );
    esp_netif_set_ip4_addr( &ip_info.netmask, sk_NetMask[0], sk_NetMask[1], sk_NetMask[2], sk_NetMask[3] );
    ESP_ERROR_CHECK( esp_netif_set_ip_info( s_NetIf, &ip_info ) );
    // DNSサーバ設定
    esp_netif_dns_info_t dns_server_ip;
    IP_ADDR4( &dns_server_ip.ip, sk_DNS_Server[0], sk_DNS_Server[1], sk_DNS_Server[2], sk_DNS_Server[3] );
    ESP_ERROR_CHECK( esp_netif_set_dns_info( s_NetIf, ESP_NETIF_DNS_MAIN, &dns_server_ip ) );

    //
    // Wifi 初期化
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &WifiEventHandler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &WifiEventHandler,
                                                        NULL,
                                                        &instance_got_ip));
                
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD
        },
    };

    ESP_LOGI(AppInfoTag, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void WifiEventHandler( void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data )
{
    if( event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START ){
        esp_wifi_connect();
    } 
    else if( event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED ){
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        xEventGroupClearBits( s_WifiEventGroup, CONNECTED_BIT );
        xEventGroupSetBits( s_WifiEventGroup, DISCONNECTED_BIT );
        esp_wifi_connect();
        ESP_LOGI( WifiLogInfoTag, "retry to connect to the AP" );
    } 
    else if( event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP ){
        xEventGroupClearBits( s_WifiEventGroup, DISCONNECTED_BIT );
        xEventGroupSetBits( s_WifiEventGroup, CONNECTED_BIT );

        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI( WifiLogInfoTag, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void Initialize_AWS_IoTClient( void )
{
    Initialize_AWS_IoT();
}
