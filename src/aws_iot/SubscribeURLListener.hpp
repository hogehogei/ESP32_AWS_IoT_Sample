#ifndef     I_SUBSCRIBE_URL_LISTENNER_INCLUDED
#define     I_SUBSCRIBE_URL_LISTENNER_INCLUDED

#include <cstdint>
#include "I_SubscribeListener.hpp"

class SubscribeURLListener : public I_SubscribeListener
{
public:

    SubscribeURLListener();
    virtual ~SubscribeURLListener() noexcept;

    virtual void SubscribeHandler( const std::string& topic, const SubscribePayloadArray& payload );

    static inline constexpr char sk_AWSSubTag[] = "AWS_Sub";
};

#endif    // I_SUBSCRIBE_URL_LISTENNER_INCLUDED
