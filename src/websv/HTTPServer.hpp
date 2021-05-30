#ifndef     HTTP_SERVER_HPP_INCLUDED
#define     HTTP_SERVER_HPP_INCLUDED

#include "esp_http_server.h"


httpd_handle_t StartWebServer();
void StopWebServer( httpd_handle_t server );


#endif    // HTTP_SERVER_HPP_INCLUDED
