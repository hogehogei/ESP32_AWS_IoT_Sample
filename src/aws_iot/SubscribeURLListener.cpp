
#include "SubscribeURLListener.hpp"
#include "UploadImageS3.hpp"

#include "esp_log.h"

#include <vector>

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

        cameraCaptureToUploadS3( str );
    }
}

void SubscribeURLListener::cameraCaptureToUploadS3( const std::string& str )
{
    const std::string delim = "/";
    std::vector<std::string> params;
    
    std::string::size_type before_pos = 0;
    std::string::size_type find_pos = std::string::npos;

    while( 1 ){
        find_pos = str.find( delim, before_pos );
        if( find_pos == std::string::npos ){
            params.push_back( str.substr( before_pos ) );
            break;
        }

        params.push_back( str.substr( before_pos, find_pos - before_pos ) );
        before_pos = find_pos + 1;
    }

    if( params.size() != 3 ){
        ESP_LOGE( sk_AWSSubTag, "Failed to Parse URL" );
        return;
    }

    const std::string& filename   = params[0];
    const std::string& webserver  = params[1];
    const std::string& url_params = params[2];

    ESP_LOGI( sk_AWSSubTag, "Upload Params: FileName=%s", filename.c_str() );
    ESP_LOGI( sk_AWSSubTag, "Upload Params: WebServer=%s", webserver.c_str() );
    ESP_LOGI( sk_AWSSubTag, "Upload Params: URLParams=%s", url_params.c_str() );

    if( !UploadImageS3( webserver, url_params ) ){
        ESP_LOGE( sk_AWSSubTag, "Failed to Upload Image to AWS S3." );
    }
}