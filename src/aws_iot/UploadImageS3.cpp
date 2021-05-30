
#include "UploadImageS3.hpp"
#include "Camera.hpp"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_log.h"

const char sk_Tag[] = "UploadS3";

bool UploadImageS3( const std::string& webserver, const std::string& url )
{
    if( webserver.empty() || url.empty() ){
        return false;
    }

    CameraFrameBuffer fb = Camera::Instance().FrameBuffer();

    char s3Request[2048]= {0};
    sprintf( s3Request,
             "PUT /%s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n", 
              url.c_str(), webserver.c_str(), fb.Length() );

    struct addrinfo hints = {};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = nullptr;
    struct in_addr *addr = nullptr;
    int sock = -1;
    const char footer[] = "\r\n";
    
    int err = getaddrinfo( webserver.c_str(), "80", &hints, &res );

    if( err != 0 || res == NULL ) {
        ESP_LOGE( sk_Tag, "DNS lookup failed err=%d res=%p", err, res );
        return false;
    }

    /* Code to print the resolved IP.

        Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */

    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ESP_LOGI( sk_Tag, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr) );

    sock = socket( res->ai_family, res->ai_socktype, 0 );
    if( sock < 0 ) {
        ESP_LOGE( sk_Tag, "... Failed to allocate socket." );
        goto UploadImageS3_bailout;
    }
    ESP_LOGI( sk_Tag, "... allocated socket" );

    if( connect(sock, res->ai_addr, res->ai_addrlen) != 0 ){
        ESP_LOGE( sk_Tag, "... socket connect failed errno=%d", errno );
        goto UploadImageS3_bailout;
    }

    ESP_LOGI( sk_Tag, "... connected" );


    if( write(sock, s3Request, strlen(s3Request)) < 0 ){
        goto UploadImageS3_bailout;
        ESP_LOGE( sk_Tag, "... socket send failed #1" );
    }
    
    if( write(sock, (const char *)fb.Buffer(), fb.Length()) < 0 ){
        ESP_LOGE( sk_Tag, "... socket send failed #2" );
        goto UploadImageS3_bailout;
    }
    

    if( write(sock, footer, strlen(footer)) < 0 ){
        ESP_LOGE( sk_Tag, "... socket send failed #3" );
        goto UploadImageS3_bailout;
    }

    ESP_LOGI( sk_Tag, "... socket send success" );

    struct timeval receiving_timeout;
    receiving_timeout.tv_sec = 5;
    receiving_timeout.tv_usec = 0;
    if( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
            sizeof(receiving_timeout)) < 0 ){
        ESP_LOGE( sk_Tag, "... failed to set socket receiving timeout" );
        goto UploadImageS3_bailout;
    }
    ESP_LOGI( sk_Tag, "... set socket receiving timeout success" );

    freeaddrinfo( res );
    close( sock );

    return true;

UploadImageS3_bailout:
    if( sock >= 0 ){
        close( sock );
    }
    if( res ){
        freeaddrinfo( res );
    }

    return false;
}