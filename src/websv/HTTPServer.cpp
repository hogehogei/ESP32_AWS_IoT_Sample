
#include "HTTPServer.hpp"
#include "Camera.hpp"

#include "esp_log.h"


struct jpg_chunking_t
{
    httpd_req_t* req;
    size_t len;
};

static esp_err_t CaptureGetHandler( httpd_req_t* req );
static size_t JpgEncodeStream( void * arg, size_t index, const void* data, size_t len );

static httpd_uri_t s_URI_CapturedImagePage = {
    .uri        = "/capture",
    .method     = HTTP_GET,
    .handler    = CaptureGetHandler,
    .user_ctx   = nullptr 
};


httpd_handle_t StartWebServer()
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    /* Start the httpd server */
    if( httpd_start(&server, &config) == ESP_OK ){
        /* Register URI handlers */
        httpd_register_uri_handler( server, &s_URI_CapturedImagePage );
    }
    /* If server failed to start, handle will be NULL */
    return server;
}

static esp_err_t CaptureGetHandler( httpd_req_t* req )
{
    CameraFrameBuffer fb = Camera::Instance().FrameBuffer();
    esp_err_t res = ESP_OK;

    if( !fb.IsValid() ){
        ESP_LOGE( "CAMServer", "Camera capture failed." );
        httpd_resp_send_500( req );
        return ESP_FAIL;
    }

    res = httpd_resp_set_type( req, "image/jpeg" );
    if( res == ESP_OK ){
        res = httpd_resp_set_hdr( req, "Content-Disposition", "inline; filename=capture.jpg" );
    }

    if( res == ESP_OK ){
        if( fb.Format() == PIXFORMAT_JPEG ){
            res = httpd_resp_send(req, (const char *)fb.Buffer(), fb.Length() );
        } else {
            jpg_chunking_t jchunk = { req, 0 };
            res = frame2jpg_cb( fb.RawPtr(), 80, JpgEncodeStream, &jchunk ) ? ESP_OK : ESP_FAIL;
            httpd_resp_send_chunk( req, NULL, 0 );
        }
    }

    return res;
}

static size_t JpgEncodeStream( void * arg, size_t index, const void* data, size_t len )
{
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}

