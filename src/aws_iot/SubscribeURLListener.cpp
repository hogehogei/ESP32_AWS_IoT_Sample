
#include "SubscribeURLListener.hpp"

#include "esp_log.h"


SubscribeURLListener::SubscribeURLListener() 
{}

SubscribeURLListener::~SubscribeURLListener()
{}

void SubscribeURLListener::SubscribeHandler( const std::string& topic, const SubscribePayloadArray& payload )
{
    ESP_LOGI( sk_AWSSubTag, "Subscribe callback" );
    if( topic == "esp32/sub/url" ){
        // payload はNULL終端されていない可能性があるので、std::string にコピーしておく
        // 無駄かもしれないけど念のため
        std::string str;
        std::copy( payload.begin(), payload.end(), std::back_inserter(str) );
        ESP_LOGI( sk_AWSSubTag, "Received String: %s", str.c_str() );
    }
}
