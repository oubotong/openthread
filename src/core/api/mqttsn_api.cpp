/*
 *  Copyright (c) 2018, Vit Holasek
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <openthread/mqttsn.h>
#include "mqttsn/mqttsn_client.hpp"
#include "common/instance.hpp"

#if OPENTHREAD_CONFIG_MQTTSN_ENABLE

#define MQTTSN_DEFAULT_CLIENT_ID "openthread"

using namespace ot;

otMqttsnTopic otMqttsnCreateTopicName(const char *aTopicName)
{
    int32_t length = strlen(aTopicName);
    if (length <= 2)
    {
        return Mqttsn::Topic::FromShortTopicName(aTopicName);
    }
    else
    {
        return Mqttsn::Topic::FromTopicName(aTopicName);
    }
}

otMqttsnTopic otMqttsnCreateTopicId(otMqttsnTopicId aTopicId)
{
    return Mqttsn::Topic::FromTopicId(aTopicId);
}

otMqttsnTopic otMqttsnCreatePredefinedTopicId(otMqttsnTopicId aTopicId)
{
    return Mqttsn::Topic::FromPredefinedTopicId(aTopicId);
}

const char *otMqttsnGetTopicName(const otMqttsnTopic *aTopic)
{
    const Mqttsn::Topic &topic = *static_cast<const Mqttsn::Topic *>(aTopic);
    return topic.GetTopicName();
}

otMqttsnTopicId otMqttsnGetTopicId(const otMqttsnTopic *aTopic)
{
    const Mqttsn::Topic &topic = *static_cast<const Mqttsn::Topic *>(aTopic);
    return topic.GetTopicId();
}

otError otMqttsnStart(otInstance *aInstance, uint16_t aPort)
{
    Instance &instance = *static_cast<Instance *>(aInstance);

    return instance.Get<Mqttsn::MqttsnClient>().Start(aPort);
}

otError otMqttsnStop(otInstance *aInstance)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    return instance.Get<Mqttsn::MqttsnClient>().Stop();
}

otMqttsnClientState otMqttsnGetState(otInstance *aInstance)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    return instance.Get<Mqttsn::MqttsnClient>().GetState();
}

otError otMqttsnConnect(otInstance *aInstance, const otMqttsnConfig *aConfig)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    if (aConfig == NULL)
    {
        return OT_ERROR_INVALID_ARGS;
    }
    Mqttsn::MqttsnConfig config;
    config.SetAddress(*static_cast<Ip6::Address *>(aConfig->mAddress));
    config.SetCleanSession(aConfig->mCleanSession);
    config.SetClientId(aConfig->mClientId);
    config.SetKeepAlive(aConfig->mKeepAlive);
    config.SetPort(aConfig->mPort);
    config.SetRetransmissionCount(aConfig->mRetransmissionCount);
    config.SetRetransmissionTimeout(aConfig->mRetransmissionTimeout);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.Connect(config);
}

otError otMqttsnConnectDefault(otInstance *aInstance, const otIp6Address *aAddress, uint16_t mPort)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnConfig config;
    config.SetAddress(*static_cast<const Ip6::Address *>(aAddress));
    config.SetClientId(MQTTSN_DEFAULT_CLIENT_ID);
    config.SetPort(mPort);
    config.SetCleanSession(true);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.Connect(config);
}

otError otMqttsnReconnect(otInstance *aInstance)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.Reconnect();
}

otError otMqttsnSubscribe(otInstance *aInstance, const otMqttsnTopic *aTopic, otMqttsnQos aQos, otMqttsnSubscribedHandler aHandler, void *aContext)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    const Mqttsn::Topic &topic = *static_cast<const Mqttsn::Topic *>(aTopic);
    return client.Subscribe(topic, aQos, aHandler, aContext);
}

otError otMqttsnRegister(otInstance *aInstance, const char* aTopicName, otMqttsnRegisteredHandler aHandler, void* aContext)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.Register(aTopicName, aHandler, aContext);
}

otError otMqttsnPublish(otInstance *aInstance, const uint8_t* aData, int32_t aLength, otMqttsnQos aQos, bool aRetained, const otMqttsnTopic *aTopic, otMqttsnPublishedHandler aHandler, void* aContext)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    const Mqttsn::Topic topic = *static_cast<const Mqttsn::Topic *>(aTopic);
    return client.Publish(aData, aLength, aQos, aRetained, topic, aHandler, aContext);
}

otError otMqttsnPublishQosm1(otInstance *aInstance, const uint8_t* aData, int32_t aLength, bool aRetained, const otMqttsnTopic *aTopic, const otIp6Address* aAddress, uint16_t aPort)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    const Mqttsn::Topic topic = *static_cast<const Mqttsn::Topic *>(aTopic);
    return client.PublishQosm1(aData, aLength, aRetained, topic, *static_cast<const Ip6::Address *>(aAddress), aPort);
}

otError otMqttsnUnsubscribe(otInstance *aInstance, const otMqttsnTopic *aTopic, otMqttsnUnsubscribedHandler aHandler, void* aContext)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    const Mqttsn::Topic &topic = *static_cast<const Mqttsn::Topic *>(aTopic);
    return client.Unsubscribe(topic, aHandler, aContext);
}

otError otMqttsnDisconnect(otInstance *aInstance)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    return instance.Get<Mqttsn::MqttsnClient>().Disconnect();
}

otError otMqttsnSleep(otInstance *aInstance, uint16_t aDuration)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    return instance.Get<Mqttsn::MqttsnClient>().Sleep(aDuration);
}

otError otMqttsnAwake(otInstance *aInstance, uint32_t aTimeout)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    return instance.Get<Mqttsn::MqttsnClient>().Awake(aTimeout);
}

otError otMqttsnSearchGateway(otInstance *aInstance, const otIp6Address* aMulticastAddress, uint16_t aPort, uint8_t aRadius)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.SearchGateway(*static_cast<const Ip6::Address *>(aMulticastAddress), aPort, aRadius);
}

uint16_t otMqttsnGetActiveGatewaysCount(otInstance *aInstance)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.GetActiveGateways().Size();
}

uint16_t otMqttsnGetActiveGateways(otInstance *aInstance, otMqttsnGatewayInfo *aBuffer, uint16_t aBufferSize)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    const Mqttsn::StaticListEntry<Mqttsn::GatewayInfo> *entry = client.GetActiveGateways().GetHead();
    uint16_t i = 0;
    while (entry != NULL && i < aBufferSize)
    {
        aBuffer[i] = *static_cast<const otMqttsnGatewayInfo *>(&entry->GetValue());
        i++;
        entry = entry->GetNext();
    }
    return i;
}

otError otMqttsnSetConnectedHandler(otInstance *aInstance, otMqttsnConnectedHandler aHandler, void *aContext)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.SetConnectedCallback(aHandler, aContext);
}

otError otMqttsnSetPublishReceivedHandler(otInstance *aInstance, otMqttsnPublishReceivedHandler aHandler, void* aContext)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.SetPublishReceivedCallback(aHandler, aContext);
}

otError otMqttsnSetDisconnectedHandler(otInstance *aInstance, otMqttsnDisconnectedHandler aHandler, void* aContext)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.SetDisconnectedCallback(aHandler, aContext);
}

otError otMqttsnSetSearchgwHandler(otInstance *aInstance, otMqttsnSearchgwHandler aHandler, void* aContext)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.SetSearchGwCallback(aHandler, aContext);
}

otError otMqttsnSetAdvertiseHandler(otInstance *aInstance, otMqttsnAdvertiseHandler aHandler, void* aContext)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.SetAdvertiseCallback(aHandler, aContext);
}

otError otMqttsnSetRegisterReceivedHandler(otInstance *aInstance, otMqttsnRegisterReceivedHandler aHandler, void* aContext)
{
    Instance &instance = *static_cast<Instance *>(aInstance);
    Mqttsn::MqttsnClient &client = instance.Get<Mqttsn::MqttsnClient>();
    return client.SetRegisterReceivedCallback(aHandler, aContext);
}

otError otMqttsnReturnCodeToString(otMqttsnReturnCode aCode, const char** aCodeString)
{
    switch (aCode)
    {
    case kCodeAccepted:
        *aCodeString = "Accepted";
        break;
    case kCodeRejectedCongestion:
        *aCodeString = "RejectedCongestion";
        break;
    case kCodeRejectedNotSupported:
        *aCodeString = "RejectedNotSupported";
        break;
    case kCodeRejectedTopicId:
        *aCodeString = "RejectedTopicId";
        break;
    case kCodeTimeout:
        *aCodeString = "Timeout";
        break;
    default:
        return OT_ERROR_INVALID_ARGS;
    }
    return OT_ERROR_NONE;
}

otError otMqttsnStringToQos(const char* aQosString, otMqttsnQos *aQos)
{
    if (strcmp(aQosString, "0") == 0)
    {
        *aQos = kQos0;
    }
    else if (strcmp(aQosString, "1") == 0)
    {
        *aQos = kQos1;
    }
    else if (strcmp(aQosString, "2") == 0)
    {
        *aQos = kQos2;
    }
    else if (strcmp(aQosString, "-1") == 0)
    {
        *aQos = kQosm1;
    }
    else
    {
        return OT_ERROR_INVALID_ARGS;
    }
    return OT_ERROR_NONE;
}

otError otMqttsnClientStateToString(otMqttsnClientState aClientState, const char** aClientStateString)
{
    switch (aClientState)
    {
    case kStateDisconnected:
        *aClientStateString = "Disconnected";
        break;
    case kStateActive:
        *aClientStateString = "Active";
        break;
    case kStateAsleep:
        *aClientStateString = "Asleep";
        break;
    case kStateAwake:
        *aClientStateString = "Awake";
        break;
    case kStateLost:
        *aClientStateString = "Lost";
        break;
    default:
        return OT_ERROR_INVALID_ARGS;
    }
    return OT_ERROR_NONE;
}

otError otMqttsnDisconnectTypeToString(otMqttsnDisconnectType aDisconnectType, const char** aDisconnectTypeString)
{
    switch (aDisconnectType)
    {
    case kDisconnectServer:
        *aDisconnectTypeString = "Server";
        break;
    case kDisconnectClient:
        *aDisconnectTypeString = "Client";
        break;
    case kDisconnectAsleep:
        *aDisconnectTypeString = "Asleep";
        break;
    case kDisconnectTimeout:
        *aDisconnectTypeString = "Timeout";
        break;
    default:
        return OT_ERROR_INVALID_ARGS;
    }
    return OT_ERROR_NONE;
}

#endif

