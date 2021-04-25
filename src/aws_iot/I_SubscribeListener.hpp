#ifndef     I_SUBSCRIBE_LISTENER_HPP_INCLUDED
#define     I_SUBSCRIBE_LISTENER_HPP_INCLUDED

#include <cstdint>
#include <vector>
#include <string>

class I_SubscribeListener
{
public:
    using SubscribePayloadArray = std::vector<uint8_t>;

public:

    I_SubscribeListener() {}
    virtual ~I_SubscribeListener() noexcept {}

    virtual void SubscribeHandler( const std::string& topic, const SubscribePayloadArray& payload ) = 0;
};

#endif    // I_SUBSCRIBE_LISTENER_HPP_INCLUDED
