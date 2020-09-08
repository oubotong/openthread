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
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "mqttsn_client.hpp"
#include "mqttsn_serializer.hpp"
#include "common/locator-getters.hpp"
#include "common/timer.hpp"
#include "common/instance.hpp"
#include "common/logging.hpp"

#if OPENTHREAD_CONFIG_MQTTSN_ENABLE

/**
 * @file
 *   This file includes implementation of MQTT-SN protocol v1.2 client.
 *
 */

/**
 * Maximal supported MQTT-SN message size in bytes.
 *
 */
#define MAX_PACKET_SIZE 255
/**
 * Minimal MQTT-SN message size in bytes.
 *
 */
#define MQTTSN_MIN_PACKET_LENGTH 2

/**
 * Default expected ADVERTISE message interval is 15 min - with some timing
 * error avoidance 17 min.
 *
 */
#define MQTTSN_DEFAULT_ADVERTISE_DURATION 1020000

namespace ot {

namespace Mqttsn {

bool Topic::HasTopicName(void) const
{
    return mType == kTopicName || mType == kShortTopicName;
}

const char* Topic::GetTopicName(void) const
{
    if (mType == kTopicName)
        return mData.mTopicName;
    else if (mType == kShortTopicName)
        return mData.mShortTopicName;
    else
        return NULL;
}

bool Topic::HasTopicId(void) const
{
    return mType == kTopicId || mType == kPredefinedTopicId;
}

TopicId Topic::GetTopicId(void) const
{
    if (mType == kTopicId || mType == kPredefinedTopicId)
        return mData.mTopicId;
    return 0;
}

Topic Topic::FromTopicName(const char *aName)
{
    if (strlen(aName) <= 2)
    {
        return FromShortTopicName(aName);
    }
    Topic topic;
    topic.mType = kTopicName;
    topic.mData.mTopicName = const_cast<char *>(aName);
    return topic;
}

Topic Topic::FromShortTopicName(const char *aShortName)
{
    Topic topic;
    topic.mType = kShortTopicName;
    topic.mData.mShortTopicName[0] = aShortName[0];
    topic.mData.mShortTopicName[1] = aShortName[1];
    topic.mData.mShortTopicName[2] = '\0';
    return topic;
}

Topic Topic::FromTopicId(TopicId aTopicId)
{
    Topic topic;
    topic.mType = kTopicId;
    topic.mData.mTopicId = aTopicId;
    return topic;
}

Topic Topic::FromPredefinedTopicId(TopicId aTopicId)
{
    Topic topic;
    topic.mType = kPredefinedTopicId;
    topic.mData.mTopicId = aTopicId;
    return topic;
}

template <typename CallbackType>
MessageMetadata<CallbackType>::MessageMetadata()
    : mDestinationAddress()
    , mDestinationPort()
    , mMessageId()
    , mTimestamp()
    , mRetransmissionTimeout()
    , mRetransmissionCount()
    , mCallback()
    , mContext()
{
    ;
}

template <typename CallbackType>
MessageMetadata<CallbackType>::MessageMetadata(
        const Ip6::Address &aDestinationAddress, uint16_t aDestinationPort,
        uint16_t aMessageId, uint32_t aTimestamp,
        uint32_t aRetransmissionTimeout, uint8_t aRetransmissionCount,
        CallbackType aCallback, void* aContext)
    : mDestinationAddress(aDestinationAddress)
    , mDestinationPort(aDestinationPort)
    , mMessageId(aMessageId)
    , mTimestamp(aTimestamp)
    , mRetransmissionTimeout(aRetransmissionTimeout)
    , mRetransmissionCount(aRetransmissionCount)
    , mCallback(aCallback)
    , mContext(aContext)
{
    ;
}

template <typename CallbackType>
otError MessageMetadata<CallbackType>::AppendTo(Message &aMessage) const
{
    return aMessage.Append(this, sizeof(*this));
}

template <typename CallbackType>
otError MessageMetadata<CallbackType>::UpdateIn(Message &aMessage) const
{
    aMessage.Write(aMessage.GetLength() - sizeof(*this), sizeof(*this), this);
    return OT_ERROR_NONE;
}

template <typename CallbackType>
uint16_t MessageMetadata<CallbackType>::ReadFrom(Message &aMessage)
{
    return aMessage.Read(aMessage.GetLength() - sizeof(*this), sizeof(*this), this);
}

template <typename CallbackType>
Message* MessageMetadata<CallbackType>::GetRawMessage(const Message &aMessage) const
{
    return aMessage.Clone(aMessage.GetLength() - GetLength());
}

template <typename CallbackType>
uint16_t MessageMetadata<CallbackType>::GetLength() const
{
    return sizeof(*this);
}

template <typename CallbackType>
WaitingMessagesQueue<CallbackType>::WaitingMessagesQueue(TimeoutCallbackFunc aTimeoutCallback, void* aTimeoutContext, RetransmissionFunc aRetransmissionFunc, void* aRetransmissionContext)
    : mQueue()
    , mTimeoutCallback(aTimeoutCallback)
    , mTimeoutContext(aTimeoutContext)
    , mRetransmissionFunc(aRetransmissionFunc)
    , mRetransmissionContext(aRetransmissionContext)
{
    ;
}

template <typename CallbackType>
WaitingMessagesQueue<CallbackType>::~WaitingMessagesQueue(void)
{
    ForceTimeout();
}

template <typename CallbackType>
otError WaitingMessagesQueue<CallbackType>::EnqueueCopy(const Message &aMessage, uint16_t aLength, const MessageMetadata<CallbackType> &aMetadata)
{
    otError error = OT_ERROR_NONE;
    Message *messageCopy = NULL;

    VerifyOrExit((messageCopy = aMessage.Clone(aLength)) != NULL, error = OT_ERROR_NO_BUFS);
    SuccessOrExit(error = aMetadata.AppendTo(*messageCopy));
    //SuccessOrExit(error = mQueue.Enqueue(*messageCopy));
    mQueue.Enqueue(*messageCopy);

exit:
    return error;
}

template <typename CallbackType>
otError WaitingMessagesQueue<CallbackType>::Dequeue(Message &aMessage)
{
    //otError error = mQueue.Dequeue(aMessage);
    mQueue.Dequeue(aMessage);
    aMessage.Free();
    return OT_ERROR_NONE;
}

template <typename CallbackType>
Message* WaitingMessagesQueue<CallbackType>::Find(uint16_t aMessageId, MessageMetadata<CallbackType> &aMetadata)
{
    Message* message = mQueue.GetHead();
    while (message)
    {
        aMetadata.ReadFrom(*message);
        if (aMessageId == aMetadata.mMessageId)
        {
            return message;
        }
        message = message->GetNext();
    }
    return NULL;
}

template <typename CallbackType>
otError WaitingMessagesQueue<CallbackType>::HandleTimer()
{
    otError error = OT_ERROR_NONE;
    Message* message = mQueue.GetHead();
    Message* retransmissionMessage = NULL;
    while (message)
    {
        Message* current = message;
        message = message->GetNext();
        MessageMetadata<CallbackType> metadata;
        metadata.ReadFrom(*current);
        // check if message timed out
        if (metadata.mTimestamp + metadata.mRetransmissionTimeout <= TimerMilli::GetNow().GetValue())
        {
            if (metadata.mRetransmissionCount > 0)
            {
                // Invoke message retransmission and decrement retransmission counter
                if (mRetransmissionFunc)
                {
                    VerifyOrExit((retransmissionMessage = metadata.GetRawMessage(*current)), error = OT_ERROR_NO_BUFS);
                    mRetransmissionFunc(*retransmissionMessage, metadata.mDestinationAddress, metadata.mDestinationPort, mRetransmissionContext);
                    retransmissionMessage->Free();
                }
                metadata.mRetransmissionCount--;
                metadata.mTimestamp = TimerMilli::GetNow().GetValue();
                // Update message metadata
                metadata.UpdateIn(*current);
            }
            else
            {
                // Invoke timeout callback and dequeue message
                if (mTimeoutCallback)
                {
                    mTimeoutCallback(metadata, mTimeoutContext);
                }
                SuccessOrExit(error = Dequeue(*current));
            }
        }
    }
exit:
    return error;
}

template <typename CallbackType>
void WaitingMessagesQueue<CallbackType>::ForceTimeout()
{
    Message* message = mQueue.GetHead();
    while (message)
    {
        Message* current = message;
        message = message->GetNext();
        MessageMetadata<CallbackType> metadata;
        metadata.ReadFrom(*current);
        if (mTimeoutCallback)
        {
            mTimeoutCallback(metadata, mTimeoutContext);
        }
        Dequeue(*current);
    }
}

template <typename CallbackType>
bool WaitingMessagesQueue<CallbackType>::IsEmpty()
{
    return mQueue.GetHead() == NULL;
}

MqttsnClient::MqttsnClient(Instance& instance)
    : InstanceLocator(instance)
    , mSocket(instance)
    , mConfig()
    , mMessageId(0)
    , mPingReqTime(0)
    , mDisconnectRequested(false)
    , mSleepRequested(false)
    , mTimeoutRaised(false)
    , mClientState(kStateDisconnected)
    , mIsRunning(false)
    , mActiveGateways()
    , mProcessTask(instance, MqttsnClient::HandleProcessTask, this)
    , mSubscribeQueue(HandleSubscribeTimeout, this, HandleSubscribeRetransmission, this)
    , mRegisterQueue(HandleRegisterTimeout, this, HandleMessageRetransmission, this)
    , mUnsubscribeQueue(HandleUnsubscribeTimeout, this, HandleMessageRetransmission, this)
    , mPublishQos1Queue(HandlePublishQos1Timeout, this, HandlePublishRetransmission, this)
    , mPublishQos2PublishQueue(HandlePublishQos2PublishTimeout, this, HandlePublishRetransmission, this)
    , mPublishQos2PubrelQueue(HandlePublishQos2PubrelTimeout, this, HandleMessageRetransmission, this)
    , mPublishQos2PubrecQueue(HandlePublishQos2PubrecTimeout, this, HandleMessageRetransmission, this)
    , mConnectQueue(HandleConnectTimeout, this, HandleMessageRetransmission, this)
    , mDisconnectQueue(HandleDisconnectTimeout, this, HandleMessageRetransmission, this)
    , mPingreqQueue(HandlePingreqTimeout, this, HandlePingreqRetransmission, this)
    , mConnectedCallback(nullptr)
    , mConnectContext(nullptr)
    , mPublishReceivedCallback(nullptr)
    , mPublishReceivedContext(nullptr)
    , mAdvertiseCallback(nullptr)
    , mAdvertiseContext(nullptr)
    , mSearchGwCallback(nullptr)
    , mSearchGwContext(nullptr)
    , mDisconnectedCallback(nullptr)
    , mDisconnectedContext(nullptr)
    , mRegisterReceivedCallback(nullptr)
    , mRegisterReceivedContext(nullptr)
{
    ;
}

MqttsnClient::~MqttsnClient()
{
    mSocket.Close();
    OnDisconnected();
}

void MqttsnClient::HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    Message &message = *static_cast<Message *>(aMessage);
    const Ip6::MessageInfo &messageInfo = *static_cast<const Ip6::MessageInfo *>(aMessageInfo);

    // Read message content
    uint16_t offset = message.GetOffset();
    uint16_t length = message.GetLength() - message.GetOffset();

    unsigned char data[MAX_PACKET_SIZE];

    if (length > MAX_PACKET_SIZE)
    {
        return;
    }

    if (!data)
    {
        return;
    }
    message.Read(offset, length, data);

    //otLogDebgMqttsn("UDP message received:%");
    otDumpDebgMqttsn("received", data, length);

    // Determine message type
    MessageType messageType;
    if (MessageBase::DeserializeMessageType(data, length, &messageType))
    {
        return;
    }
    otLogDebgMqttsn("Message type: %d", messageType);

    // Handle received message type
    switch (messageType)
    {
    case kTypeConnack:
        client->ConnackReceived(messageInfo, data, length);
        break;
    case kTypeSuback:
        client->SubackReceived(messageInfo, data, length);
        break;
    case kTypePublish:
        client->PublishReceived(messageInfo, data, length);
        break;
    case kTypeAdvertise:
        client->AdvertiseReceived(messageInfo, data, length);
        break;
    case kTypeGwInfo:
        client->GwinfoReceived(messageInfo, data, length);
        break;
    case kTypeRegack:
        client->RegackReceived(messageInfo, data, length);
        break;
    case kTypeRegister:
        client->RegisterReceived(messageInfo, data, length);
        break;
    case kTypePuback:
        client->PubackReceived(messageInfo, data, length);
        break;
    case kTypePubrec:
        client->PubrecReceived(messageInfo, data, length);
        break;
    case kTypePubrel:
        client->PubrelReceived(messageInfo, data, length);
        break;
    case kTypePubcomp:
        client->PubcompReceived(messageInfo, data, length);
        break;
    case kTypeUnsuback:
        client->UnsubackReceived(messageInfo, data, length);
        break;
    case kTypePingreq:
        client->PingreqReceived(messageInfo, data, length);
        break;
    case kTypePingresp:
        client->PingrespReceived(messageInfo, data, length);
        break;
    case kTypeDisconnect:
        client->DisconnectReceived(messageInfo, data, length);
        break;
    case kTypeSearchGw:
        client->SearchGwReceived(messageInfo, data, length);
        break;
    default:
        break;
    }
}

void MqttsnClient::ConnackReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    // Check source IPv6 address
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }

    ConnackMessage connackMessage;
    if (connackMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }
    MessageMetadata<void*> metadata;

    // Check if any waiting connect message queued
    Message *connectMessage = mConnectQueue.Find(0, metadata);
    if (connectMessage)
    {
        mConnectQueue.Dequeue(*connectMessage);

        mClientState = kStateActive;

        if (mConnectedCallback)
        {
            mConnectedCallback(connackMessage.GetReturnCode(), mConnectContext);
        }
    }
}

void MqttsnClient::SubackReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    // Client must be in active state
    if (mClientState != kStateActive)
    {
        return;
    }
    // Check source IPv6 address
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }
    SubackMessage subackMessage;
    if (subackMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }

    // Find waiting message with corresponding ID
    MessageMetadata<otMqttsnSubscribedHandler> metadata;
    Message* subscribeMessage = mSubscribeQueue.Find(subackMessage.GetMessageId(), metadata);
    if (subscribeMessage)
    {
        // Invoke callback and dequeue message
        if (metadata.mCallback)
        {
            if (subackMessage.GetMessageId() != 0)
            {
                Topic topic = Topic::FromTopicId(subackMessage.GetTopicId());
                metadata.mCallback(subackMessage.GetReturnCode(), static_cast<otMqttsnTopic *>(&topic),
                        subackMessage.GetQos(), metadata.mContext);
            }
            else
            {
                metadata.mCallback(subackMessage.GetReturnCode(), NULL,
                        subackMessage.GetQos(), metadata.mContext);
            }
        }
        mSubscribeQueue.Dequeue(*subscribeMessage);
    }
}

void MqttsnClient::PublishReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    // Client must be in active or awake state to receive published messages
    if (mClientState != kStateActive && mClientState != kStateAwake)
    {
        return;
    }
    // Check source IPv6 address
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }
    PublishMessage publishMessage;
    if (publishMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }

    // Filter duplicate QoS level 2 messages
    if (publishMessage.GetQos() == kQos2)
    {
        MessageMetadata<void*> metadata;
        Message* pubrecMessage = mPublishQos2PubrecQueue.Find(publishMessage.GetMessageId(), metadata);
        if (pubrecMessage)
        {
            return;
        }
    }

    ReturnCode code = kCodeRejectedTopicId;
    if (mPublishReceivedCallback)
    {
        // Invoke callback
        code = mPublishReceivedCallback(publishMessage.GetPayload(), publishMessage.GetPayloadLength(),
            &publishMessage.GetTopic(), mPublishReceivedContext);
    }

    // Handle QoS
    if (publishMessage.GetQos() == kQos0 || publishMessage.GetQos() == kQosm1)
    {
        // On QoS level 0 or -1 do nothing
    }
    else if (publishMessage.GetQos() == kQos1)
    {
        // On QoS level 1  send PUBACK response
        int32_t packetLength = -1;
        Message* responseMessage = NULL;
        unsigned char buffer[MAX_PACKET_SIZE];
        PubackMessage pubackMessage(code, publishMessage.GetTopic(), publishMessage.GetMessageId());
        if (pubackMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
        {
            return;
        }
        if (NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE
            || SendMessage(*responseMessage) != OT_ERROR_NONE)
        {
            return;
        }
    }
    else if (publishMessage.GetQos() == kQos2)
    {
        // On QoS level 2 send PUBREC message and wait for PUBREL
        int32_t packetLength = -1;
        Message* responseMessage = NULL;
        unsigned char buffer[MAX_PACKET_SIZE];
        PubrecMessage pubrecMessage(publishMessage.GetMessageId());
        if (pubrecMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
        {
            return;
        }
        if (NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE ||
            SendMessageWithRetransmission<void*>(*responseMessage, mPublishQos2PubrecQueue,
                    publishMessage.GetMessageId(), NULL, NULL) != OT_ERROR_NONE)
        {
            return;
        }
    }
}

void MqttsnClient::AdvertiseReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    AdvertiseMessage advertiseMessage;
    if (advertiseMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }

    otLogDebgMqttsn("received advertise from %s[%u]: gateway_id=%u, keepalive=%u",
            messageInfo.GetPeerAddr().ToString().AsCString(), (uint32_t)messageInfo.GetPeerPort(),
            (uint32_t)advertiseMessage.GetGatewayId(), (uint32_t)advertiseMessage.GetDuration());
    // Multiply duration by 1.2 to minimize timing error probability and multiply by 1000
    // to milliseconds
    uint32_t duration = (advertiseMessage.GetDuration() * 12 * 1000) / 10;
    // Add advertised gateway to active gateways list
    mActiveGateways.Add(advertiseMessage.GetGatewayId(), messageInfo.GetPeerAddr(), duration);

    if (mAdvertiseCallback)
    {
        mAdvertiseCallback(&messageInfo.GetPeerAddr(), advertiseMessage.GetGatewayId(),
            advertiseMessage.GetDuration(), mAdvertiseContext);
    }
}

void MqttsnClient::GwinfoReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    GwInfoMessage gwInfoMessage;
    if (gwInfoMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }
    if (mSearchGwCallback)
    {
        // If received from gateway add to active gateways list with default duration
        if (!gwInfoMessage.GetHasAddress())
        {
            mActiveGateways.Add(gwInfoMessage.GetGatewayId(),
                    messageInfo.GetPeerAddr(), MQTTSN_DEFAULT_ADVERTISE_DURATION);
        }

        const Ip6::Address &address = (gwInfoMessage.GetHasAddress()) ? gwInfoMessage.GetAddress()
            : messageInfo.GetPeerAddr();

        mSearchGwCallback(&address, gwInfoMessage.GetGatewayId(), mSearchGwContext);
    }
}

void MqttsnClient::RegackReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    // Client state must be active
    if (mClientState != kStateActive)
    {
        return;
    }
    // Check source IPv6 address
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }

    RegackMessage regackMessage;
    if (regackMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }
    // Find waiting message with corresponding ID
    MessageMetadata<otMqttsnRegisteredHandler> metadata;
    Message* registerMessage = mRegisterQueue.Find(regackMessage.GetMessageId(), metadata);
    if (!registerMessage)
    {
        return;
    }
    // Invoke callback and dequeue message
    if (metadata.mCallback)
    {
        Topic topic = Topic::FromTopicId(regackMessage.GetTopicId());
        metadata.mCallback(regackMessage.GetReturnCode(), &topic, metadata.mContext);
    }
    mRegisterQueue.Dequeue(*registerMessage);
}

void MqttsnClient::RegisterReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    int32_t packetLength = -1;
    Message* responseMessage = NULL;
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        return;
    }
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }

    RegisterMessage registerMessage;
    if (registerMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }

    // Invoke register callback
    ReturnCode code = kCodeRejectedTopicId;
    if (mRegisterReceivedCallback)
    {
        code = mRegisterReceivedCallback(registerMessage.GetTopicId(), registerMessage.GetTopicName().AsCString(), mRegisterReceivedContext);
    }

    // Send REGACK response message
    RegackMessage regackMessage(code, registerMessage.GetTopicId(), registerMessage.GetMessageId());
    if (regackMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
    {
        return;
    }
    if (NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE ||
        SendMessage(*responseMessage) != OT_ERROR_NONE)
    {
        return;
    }
}

void MqttsnClient::PubackReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    // Client state must be active
    if (mClientState != kStateActive)
    {
        return;
    }
    // Check source IPv6 address.
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }
    PubackMessage pubackMessage;
    if (pubackMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }

    // Process QoS level 1 message
    // Find message waiting for acknowledge
    MessageMetadata<otMqttsnPublishedHandler> metadata;
    Message* publishMessage = mPublishQos1Queue.Find(pubackMessage.GetMessageId(), metadata);
    if (publishMessage)
    {
        // Invoke confirmation callback
        if (metadata.mCallback)
        {
            metadata.mCallback(pubackMessage.GetReturnCode(), metadata.mContext);
        }
        // Dequeue waiting message
        mPublishQos1Queue.Dequeue(*publishMessage);
        return;
    }
    // May be QoS level 2 message error response
    publishMessage = mPublishQos2PublishQueue.Find(pubackMessage.GetMessageId(), metadata);
    if (publishMessage)
    {
        // Invoke confirmation callback
        if (metadata.mCallback)
        {
            metadata.mCallback(pubackMessage.GetReturnCode(), metadata.mContext);
        }
        // Dequeue waiting message
        mPublishQos2PublishQueue.Dequeue(*publishMessage);
        return;
    }

    // May be QoS level 0 message error response - it is not handled
}

void MqttsnClient::PubrecReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    int32_t packetLength = -1;
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        return;
    }
    // Check source IPv6 address
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }
    PubrecMessage pubrecMessage;
    if (pubrecMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }

    // Process QoS level 2 message
    // Find message waiting for receive acknowledge
    MessageMetadata<otMqttsnPublishedHandler> metadata;
    Message* publishMessage = mPublishQos2PublishQueue.Find(pubrecMessage.GetMessageId(), metadata);
    if (!publishMessage)
    {
        return;
    }

    // Send PUBREL message
    PubrelMessage pubrelMessage(metadata.mMessageId);
    if (pubrelMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
    {
        return;
    }
    Message* responseMessage = NULL;
    if (NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE ||
        SendMessageWithRetransmission<otMqttsnPublishedHandler>(
                *responseMessage, mPublishQos2PubrelQueue, metadata.mMessageId,
                metadata.mCallback, metadata.mContext) != OT_ERROR_NONE)
    {
        return;
    }

    // Dequeue waiting PUBLISH message
    mPublishQos2PublishQueue.Dequeue(*publishMessage);
}

void MqttsnClient::PubrelReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    int32_t packetLength = -1;
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        return;
    }
    // Check source IPv6 address
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }
    PubrelMessage pubrelMessage;
    if (pubrelMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }

    // Process QoS level 2 PUBREL message
    // Find PUBREC message waiting for receive acknowledge
    MessageMetadata<void*> metadata;
    Message* pubrecMessage = mPublishQos2PubrecQueue.Find(pubrelMessage.GetMessageId(), metadata);
    if (!pubrecMessage)
    {
        return;
    }
    // Send PUBCOMP message
    PubcompMessage pubcompMessage(metadata.mMessageId);
    if (pubcompMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
    {
        return;
    }
    Message* responseMessage = NULL;
    if (NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE ||
        SendMessage(*responseMessage) != OT_ERROR_NONE)
    {
        return;
    }

    // Dequeue waiting message
    mPublishQos2PubrecQueue.Dequeue(*pubrecMessage);
}

void MqttsnClient::PubcompReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    // Client state must be active
    if (mClientState != kStateActive)
    {
        return;
    }
    // Check source IPv6 address
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }
    PubcompMessage pubcompMessage;
    if (pubcompMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }

    // Process QoS level 2 PUBCOMP message
    // Find PUBREL message waiting for receive acknowledge
    MessageMetadata<otMqttsnPublishedHandler> metadata;
    Message* pubrelMessage = mPublishQos2PubrelQueue.Find(pubcompMessage.GetMessageId(), metadata);
    if (!pubrelMessage)
    {
        return;
    }
    // Invoke confirmation callback
    if (metadata.mCallback)
    {
        metadata.mCallback(kCodeAccepted, metadata.mContext);
    }
    // Dequeue waiting message
    mPublishQos2PubrelQueue.Dequeue(*pubrelMessage);
}

void MqttsnClient::UnsubackReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    // Client state must be active
    if (mClientState != kStateActive)
    {
        return;
    }
    // Check source IPv6 address
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }

    UnsubackMessage unsubackMessage;
    if (unsubackMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }
    // Find unsubscription message waiting for confirmation
    MessageMetadata<otMqttsnUnsubscribedHandler> metadata;
    Message* unsubscribeMessage = mUnsubscribeQueue.Find(unsubackMessage.GetMessageId(), metadata);
    if (!unsubscribeMessage)
    {
        return;
    }
    // Invoke unsubscribe confirmation callback
    if (metadata.mCallback)
    {
        metadata.mCallback(kCodeAccepted, metadata.mContext);
    }
    // Dequeue waiting message
    mUnsubscribeQueue.Dequeue(*unsubscribeMessage);
}

void MqttsnClient::PingreqReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    Message* responseMessage = NULL;
    int32_t packetLength = -1;
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        return;
    }

    PingreqMessage pingreqMessage;
    if (pingreqMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }

    // Send PINGRESP message
    PingrespMessage pingrespMessage;
    if (pingrespMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
    {
        return;
    }
    if (NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE ||
        SendMessage(*responseMessage, messageInfo.GetPeerAddr(), mConfig.GetPort()) != OT_ERROR_NONE)
    {
        return;
    }
}

void MqttsnClient::PingrespReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    // Check source IPv6 address
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }
    PingrespMessage pingrespMessage;
    if (pingrespMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }

    // Check if any waiting connect message queued
    MessageMetadata<void*> metadata;
    Message *pingreqMessage = mPingreqQueue.Find(0, metadata);
    if (pingreqMessage == NULL)
    {
        return;
    }
    mPingreqQueue.Dequeue(*pingreqMessage);

    // If the client is awake PINRESP message put it into sleep again
    if (mClientState == kStateAwake)
    {
        mClientState = kStateAsleep;
        if (mDisconnectedCallback)
        {
            mDisconnectedCallback(kDisconnectAsleep, mDisconnectedContext);
        }
    }
}

void MqttsnClient::DisconnectReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
    DisconnectMessage disconnectMessage;
    if (disconnectMessage.Deserialize(data, length))
    {
        return;
    }

    // Check source IPv6 address
    if (!VerifyGatewayAddress(messageInfo))
    {
        return;
    }

    // Check if the waiting disconnect message is queued
    MessageMetadata<void*> metadata;
    Message *waitingMessage = mDisconnectQueue.Find(0, metadata);
    if (waitingMessage != NULL)
    {
        mDisconnectQueue.Dequeue(*waitingMessage);
    }

    // Handle disconnection behavior depending on client state
    DisconnectType reason = kDisconnectServer;
    switch (mClientState)
    {
    case kStateActive:
    case kStateAwake:
    case kStateAsleep:
        if (mDisconnectRequested)
        {
            // Regular disconnect
            mClientState = kStateDisconnected;
            reason = kDisconnectClient;
        }
        else if (mSleepRequested)
        {
            // Sleep state was requested - go asleep
            mClientState = kStateAsleep;
            reason = kDisconnectAsleep;
        }
        else
        {
            // Disconnected by gateway
            mClientState = kStateDisconnected;
            reason = kDisconnectServer;
        }
        break;
    default:
        break;
    }
    OnDisconnected();

    // Invoke disconnected callback
    if (mDisconnectedCallback)
    {
        mDisconnectedCallback(reason, mDisconnectedContext);
    }
}

void MqttsnClient::SearchGwReceived(const Ip6::MessageInfo &messageInfo, const unsigned char* data, uint16_t length)
{
#if OPENTHREAD_FTD
    SearchGwMessage searchGwMessage;
    // Do not respond to SEARCHGW messages when client not active
    if (GetState() != kStateActive)
    {
        return;
    }
    if (searchGwMessage.Deserialize(data, length) != OT_ERROR_NONE)
    {
        return;
    }

    // Respond with GWINFO message for each active gateway in cached list
    const StaticListEntry<GatewayInfo> *gatewayInfoEntry = GetActiveGateways().GetHead();
    do
    {
        const GatewayInfo &info = gatewayInfoEntry->GetValue();
        Message* responseMessage = NULL;
        int32_t packetLength = -1;
        unsigned char buffer[MAX_PACKET_SIZE];

        GwInfoMessage gwInfoMessage = GwInfoMessage(info.GetGatewayId(), true, info.GetGatewayAddress());
        if (gwInfoMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
        {
            return;
        }
        if (NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE
                || SendMessage(*responseMessage, messageInfo.GetPeerAddr(), mConfig.GetPort()) != OT_ERROR_NONE)
        {
            return;
        }
    }
    while ((gatewayInfoEntry = gatewayInfoEntry->GetNext()) != NULL);
#else
    OT_UNUSED_VARIABLE(messageInfo);
    OT_UNUSED_VARIABLE(data);
    OT_UNUSED_VARIABLE(length);
#endif
}

void MqttsnClient::HandleProcessTask(Tasklet &aTasklet)
{
    otError error = aTasklet.GetOwner<MqttsnClient>().Process();
    if (error != OT_ERROR_NONE)
    {
        otLogWarnMqttsn("Process task failed: %s", otThreadErrorToString(error));
    }
}

otError MqttsnClient::Start(uint16_t aPort)
{
    otError error = OT_ERROR_NONE;
    Ip6::SockAddr sockaddr;
    sockaddr.mPort = aPort;
    printf("prepare to start\n");
    VerifyOrExit(!mIsRunning, error = OT_ERROR_INVALID_STATE);
    // Open UDP socket
    SuccessOrExit(error = mSocket.Open(&MqttsnClient::HandleUdpReceive, this));
    // Start listening on configured port
    SuccessOrExit(error = mSocket.Bind(sockaddr));

    // Enqueue process task which will handle message queues etc.
    //SuccessOrExit(error = mProcessTask.Post());
    //mProcessTask.Post();
    mIsRunning = true;
    printf("MQTT is running\n");
exit:
    return error;
}

otError MqttsnClient::Stop()
{
    mIsRunning = false;
    otError error = mSocket.Close();
    // Clear active gateways list because ADVERTISE messages won't be received anymore
    mActiveGateways.Clear();
    // Disconnect client if it is not disconnected already
    mClientState = kStateDisconnected;
    if (mClientState != kStateDisconnected && mClientState != kStateLost)
    {
        OnDisconnected();
        if (mDisconnectedCallback)
        {
            mDisconnectedCallback(kDisconnectClient, mDisconnectedContext);
        }
    }
    return error;
}

otError MqttsnClient::Process()
{
    otError error = OT_ERROR_NONE;
    uint32_t now = TimerMilli::GetNow().GetValue();

    if (mIsRunning)
    {
        // Enqueue again if client running
        //SuccessOrExit(error = mProcessTask.Post());
        //mProcessTask.Post();
    }

    // Process keep alive and send periodical PINGREQ message
    if (mClientState == kStateActive && mPingReqTime != 0 && mPingReqTime <= now)
    {
        SuccessOrExit(error = PingGateway());
    }

    // Handle pending messages timeouts
    SuccessOrExit(error = mConnectQueue.HandleTimer());
    SuccessOrExit(error = mSubscribeQueue.HandleTimer());
    SuccessOrExit(error = mRegisterQueue.HandleTimer());
    SuccessOrExit(error = mUnsubscribeQueue.HandleTimer());
    SuccessOrExit(error = mPublishQos1Queue.HandleTimer());
    SuccessOrExit(error = mPublishQos2PublishQueue.HandleTimer());
    SuccessOrExit(error = mPublishQos2PubrelQueue.HandleTimer());
    SuccessOrExit(error = mPublishQos2PubrecQueue.HandleTimer());
    SuccessOrExit(error = mPingreqQueue.HandleTimer());
    SuccessOrExit(error = mDisconnectQueue.HandleTimer());

    // Handle active gateways
    SuccessOrExit(error = mActiveGateways.HandleTimer());

exit:
    // Handle communication timeout
    // Transition from Active, Asleep or Awake to Lost state
    // In other case the timeout is ignored
    if (mTimeoutRaised && (mClientState == kStateActive || mClientState == kStateAsleep
            || mClientState == kStateAwake))
    {
        mClientState = mDisconnectRequested ? kStateDisconnected : kStateLost;
        OnDisconnected();
        if (mDisconnectedCallback)
        {
            mDisconnectedCallback(kDisconnectTimeout, mDisconnectedContext);
        }
    }
    mTimeoutRaised = false;
    // Only enqueue process when client running and is not asleep
    if (mIsRunning && mClientState != kStateAsleep)
    {
        //mProcessTask.Post();
    }
    return error;
}

otError MqttsnClient::Connect(const MqttsnConfig &aConfig)
{
    printf("prepare to connect\n");
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = NULL;
    ConnectMessage connectMessage(aConfig.GetCleanSession(), false, aConfig.GetKeepAlive(), aConfig.GetClientId().AsCString());
    unsigned char buffer[MAX_PACKET_SIZE];

    // Previous Connect message is still pending
    if (!mConnectQueue.IsEmpty())
    {
        printf("Previous connect message is still pending. Wait for timeout.");
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }
    // MQTT-SN service is not running
    if (!mIsRunning)
    {
        printf("MQTT-SN service is not running.");
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }
    mConfig = aConfig;

    // Serialize and send CONNECT message
    SuccessOrExit(error = connectMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessageWithRetransmission<void*>(*message, mConnectQueue, 0, NULL, NULL));

    mDisconnectRequested = false;
    mSleepRequested = false;
    // Set next keepalive PINGREQ time
    ResetPingreqTime();
    WakeUp();
exit:
    return error;
}

otError MqttsnClient::Reconnect(void)
{
    return Connect(mConfig);
}

otError MqttsnClient::Subscribe(const Topic &aTopic, Qos aQos, otMqttsnSubscribedHandler aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Ip6::MessageInfo messageInfo;
    Message *message = NULL;
    uint16_t messageId = GetNextMessageId();
    SubscribeMessage subscribeMessage = SubscribeMessage(false, aQos, messageId, aTopic);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Topic subscription is possible only for QoS levels 1, 2, 3
    if (aQos != kQos0 && aQos != kQos1 && aQos != kQos2)
    {
        error = OT_ERROR_INVALID_ARGS;
        goto exit;
    }

    // Serialize and send SUBSCRIBE message
    SuccessOrExit(error = subscribeMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    // Enqueue message to waiting queue - waiting for SUBACK
    SuccessOrExit(error = SendMessageWithRetransmission<otMqttsnSubscribedHandler>(
                    *message, mSubscribeQueue, messageId, aCallback, aContext));

exit:
    return error;
}

otError MqttsnClient::Register(const char* aTopicName, otMqttsnRegisteredHandler aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = NULL;
    uint16_t messageId = GetNextMessageId();
    RegisterMessage registerMessage(0, messageId, aTopicName);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Serialize and send REGISTER message
    SuccessOrExit(error = registerMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    // Enqueue message to waiting queue - waiting for REGACK
    SuccessOrExit(error = SendMessageWithRetransmission<otMqttsnRegisteredHandler>(
            *message, mRegisterQueue, messageId, aCallback, aContext));

exit:
    return error;
}

otError MqttsnClient::Publish(const uint8_t* aData, int32_t aLength, Qos aQos, bool aRetained, const Topic &aTopic, otMqttsnPublishedHandler aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = NULL;
    uint16_t messageId = 0;
    if (aQos == kQos1 || aQos == kQos2)
        messageId = GetNextMessageId();
    unsigned char buffer[MAX_PACKET_SIZE];
    PublishMessage publishMessage(false, aRetained, aQos, messageId, aTopic, aData, aLength);

    // Client state must be active or sleeping
    if (mClientState != kStateActive && mClientState != kStateAsleep)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Serialize and send PUBLISH message
    SuccessOrExit(error = publishMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    if (aQos == kQos0 || aQos == kQosm1)
    {
        SuccessOrExit(error = SendMessage(*message));
    }
    if (aQos == kQos1)
    {
        // If QoS level 1 enqueue message to waiting queue - waiting for PUBACK
        SuccessOrExit(error = SendMessageWithRetransmission<otMqttsnPublishedHandler>(
                *message, mPublishQos1Queue, messageId, aCallback, aContext));
    }
    if (aQos == kQos2)
    {
        // If QoS level 1 enqueue message to waiting queue - waiting for PUBREC
        SuccessOrExit(error = SendMessageWithRetransmission<otMqttsnPublishedHandler>(
                *message, mPublishQos2PublishQueue, messageId, aCallback, aContext));
    }

exit:
    return error;
}

otError MqttsnClient::PublishQosm1(const uint8_t* aData, int32_t aLength, bool aRetained, const Topic &aTopic, const Ip6::Address &aAddress, uint16_t aPort)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = NULL;
    PublishMessage publishMessage;
    publishMessage = PublishMessage(false, aRetained, kQosm1, 0, aTopic, aData, aLength);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Serialize and send PUBLISH message
    SuccessOrExit(error = publishMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message, aAddress, aPort));

exit:
    return error;
}

otError MqttsnClient::Unsubscribe(const Topic &aTopic, otMqttsnUnsubscribedHandler aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = NULL;
    uint16_t messageId = GetNextMessageId();
    UnsubscribeMessage unsubscribeMessage(messageId, aTopic);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Serialize and send UNSUBSCRIBE message
    SuccessOrExit(error = unsubscribeMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    // Enqueue message to waiting queue - waiting for UNSUBACK
    SuccessOrExit(error = SendMessageWithRetransmission<otMqttsnUnsubscribedHandler>(
                *message, mUnsubscribeQueue, messageId, aCallback, aContext));

exit:
    return error;
}

otError MqttsnClient::Disconnect()
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = NULL;
    Message* messageCopy = NULL;
    DisconnectMessage disconnectMessage(0);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client must be connected
    if ((mClientState != kStateActive && mClientState != kStateAwake
        && mClientState != kStateAsleep) || !mDisconnectQueue.IsEmpty())
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Serialize and send DISCONNECT message
    SuccessOrExit(error = disconnectMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    messageCopy = message->Clone();
    VerifyOrExit(messageCopy != NULL, error = OT_ERROR_NO_BUFS);
    SuccessOrExit(error = SendMessage(*message));

    SuccessOrExit(error = mDisconnectQueue.EnqueueCopy(*messageCopy, messageCopy->GetLength(),
        MessageMetadata<void*>(mConfig.GetAddress(), mConfig.GetPort(), 0, TimerMilli::GetNow().GetValue(),
            mConfig.GetRetransmissionTimeout() * 1000, 0, NULL, NULL)));

    // Set flag for regular disconnect request and wait for DISCONNECT message from gateway
    mDisconnectRequested = true;
    WakeUp();

exit:
    if (messageCopy != NULL)
    {
        messageCopy->Free();
    }
    return error;
}

otError MqttsnClient::Sleep(uint16_t aDuration)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = NULL;
    DisconnectMessage disconnectMessage(aDuration);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client must be connected
    if ((mClientState != kStateActive && mClientState != kStateAwake && mClientState != kStateAsleep)
        || !mDisconnectQueue.IsEmpty())
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Serialize and send DISCONNECT message
    SuccessOrExit(error = disconnectMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessageWithRetransmission<void*>(*message,
            mDisconnectQueue, 0, NULL, NULL));

    // Set flag for sleep request and wait for DISCONNECT message from gateway
    mSleepRequested = true;
    WakeUp();

exit:
    return error;
}

otError MqttsnClient::Awake(uint32_t aTimeout)
{
	OT_UNUSED_VARIABLE(aTimeout);
    otError error = OT_ERROR_NONE;
    // Awake is possible only when the client is in
    if (mClientState != kStateAwake && mClientState != kStateAsleep)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Send PINGEQ message
    SuccessOrExit(error = PingGateway(aTimeout, mConfig.GetRetransmissionCount()));

    // Set awake state and wait for any PUBLISH messages
    mClientState = kStateAwake;
    // Enqueue process tasklet again
    //mProcessTask.Post();
exit:
    return error;
}

otError MqttsnClient::SearchGateway(const Ip6::Address &aMulticastAddress, uint16_t aPort, uint8_t aRadius)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = NULL;
    SearchGwMessage searchGwMessage(aRadius);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Serialize and send SEARCHGW message
    SuccessOrExit(error = searchGwMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message, aMulticastAddress, aPort, aRadius));

exit:
    return error;
}

ClientState MqttsnClient::GetState() const
{
    return mClientState;
}

const StaticArrayList<GatewayInfo> &MqttsnClient::GetActiveGateways() const
{
    return mActiveGateways.GetList();
}

otError MqttsnClient::SetConnectedCallback(otMqttsnConnectedHandler aCallback, void* aContext)
{
    mConnectedCallback = aCallback;
    mConnectContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetPublishReceivedCallback(otMqttsnPublishReceivedHandler aCallback, void* aContext)
{
    mPublishReceivedCallback = aCallback;
    mPublishReceivedContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetAdvertiseCallback(otMqttsnAdvertiseHandler aCallback, void* aContext)
{
    mAdvertiseCallback = aCallback;
    mAdvertiseContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetSearchGwCallback(otMqttsnSearchgwHandler aCallback, void* aContext)
{
    mSearchGwCallback = aCallback;
    mSearchGwContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetDisconnectedCallback(otMqttsnDisconnectedHandler aCallback, void* aContext)
{
    mDisconnectedCallback = aCallback;
    mDisconnectedContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetRegisterReceivedCallback(otMqttsnRegisterReceivedHandler aCallback, void* aContext)
{
    mRegisterReceivedCallback = aCallback;
    mRegisterReceivedContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::NewMessage(Message **aMessage, unsigned char* aBuffer, int32_t aLength)
{
    otError error = OT_ERROR_NONE;
    Message *message = NULL;

    VerifyOrExit((message = mSocket.NewMessage(0)) != NULL, error = OT_ERROR_NO_BUFS);
    SuccessOrExit(error = message->Append(aBuffer, aLength));
    *aMessage = message;

exit:
    if (error != OT_ERROR_NONE && message != NULL)
    {
        message->Free();
    }
    return error;
}


inline uint16_t Swap16(uint16_t v)
{
    return (((v & 0x00ffU) << 8) & 0xff00) | (((v & 0xff00U) >> 8) & 0x00ff);
}
otError MqttsnClient::SendMessage(Message &aMessage)
{
    //printf("|||||connect message: port=%i\n",mConfig.GetPort());
    otIp6Address address = mConfig.GetAddress();
    printf("address:%x:%x:%x:%x:%x:%x:%x:%x\n", Swap16(address.mFields.m16[0]), Swap16(address.mFields.m16[1]),
Swap16(address.mFields.m16[2]), Swap16(address.mFields.m16[3]), Swap16(address.mFields.m16[4]),
        Swap16(address.mFields.m16[5]), Swap16(address.mFields.m16[6]), Swap16(address.mFields.m16[7]));
    return SendMessage(aMessage, mConfig.GetAddress(), mConfig.GetPort());
}

template <typename CallbackType>
otError MqttsnClient::SendMessageWithRetransmission(Message &aMessage, WaitingMessagesQueue<CallbackType> &aQueue, uint16_t aMessageId, CallbackType aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    // Make copy of the message before send because send operation appends UDP header to the message
    MessageMetadata<CallbackType> metadata;
    Message *messageCopy = aMessage.Clone();
    VerifyOrExit(messageCopy != NULL, error = OT_ERROR_NO_BUFS);
    printf("sending connect message");
    SuccessOrExit(error = SendMessage(aMessage));
    metadata = MessageMetadata<CallbackType>(
            mConfig.GetAddress(), mConfig.GetPort(), aMessageId,
            TimerMilli::GetNow().GetValue(),
            mConfig.GetRetransmissionTimeout() * 1000,
            mConfig.GetRetransmissionCount(), aCallback, aContext);
    SuccessOrExit(error = aQueue.EnqueueCopy(*messageCopy, messageCopy->GetLength(), metadata));
exit:
    if (messageCopy)
    {
        messageCopy->Free();
    }
    return error;
}

otError MqttsnClient::SendMessage(Message &aMessage, const Ip6::Address &aAddress, uint16_t aPort)
{
    return SendMessage(aMessage, aAddress, aPort, 0);
}

otError MqttsnClient::SendMessage(Message &aMessage, const Ip6::Address &aAddress, uint16_t aPort, uint8_t aHopLimit)
{
    //otIp6Address address = aAddress;
    otError error = OT_ERROR_NONE;
    Ip6::MessageInfo messageInfo;

    messageInfo.SetHopLimit(aHopLimit);
    messageInfo.SetPeerAddr(aAddress);
    messageInfo.SetPeerPort(aPort);
    messageInfo.SetIsHostInterface(false);

    printf("Sending message to %s[:%u]\n", messageInfo.GetPeerAddr().ToString().AsCString(), messageInfo.GetPeerPort());
    SuccessOrExit(error = mSocket.SendTo(aMessage, messageInfo));

exit:
    if (error != OT_ERROR_NONE)
    {
        aMessage.Free();
    }
    return error;
}

otError MqttsnClient::PingGateway()
{
    return PingGateway(mConfig.GetRetransmissionTimeout() * 1000, mConfig.GetRetransmissionCount());
}

otError MqttsnClient::PingGateway(uint32_t aRetransmissionTimeout, uint8_t aRetransmissionCount)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = NULL;
    PingreqMessage pingreqMessage(mConfig.GetClientId().AsCString());
    unsigned char buffer[MAX_PACKET_SIZE];
    MessageMetadata<void*> metadata;
    Message *messageCopy = NULL;

    if (mClientState == kStateDisconnected && mClientState == kStateLost)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // There is already pingreq message waiting
    VerifyOrExit(mPingreqQueue.IsEmpty(), OT_NOOP);

    // Serialize and send PINGREQ message
    SuccessOrExit(error = pingreqMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    messageCopy = message->Clone();

    // Enqueue copy to the queue waiting for PINGRESP message
    VerifyOrExit(messageCopy != NULL, error = OT_ERROR_NO_BUFS);
    SuccessOrExit(error = SendMessage(*message));
    metadata = MessageMetadata<void*>(mConfig.GetAddress(), mConfig.GetPort(), 0,
            TimerMilli::GetNow().GetValue(), aRetransmissionTimeout,
            aRetransmissionCount, NULL, NULL);
    SuccessOrExit(error = mPingreqQueue.EnqueueCopy(*messageCopy,
            messageCopy->GetLength(), metadata));

    ResetPingreqTime();

exit:
 if (messageCopy)
    {
        messageCopy->Free();
    }
    return error;
}

void MqttsnClient::OnDisconnected()
{
    mDisconnectRequested = false;
    mSleepRequested = false;
    mTimeoutRaised = false;
    mPingReqTime = 0;

    mConnectQueue.ForceTimeout();
    mSubscribeQueue.ForceTimeout();
    mRegisterQueue.ForceTimeout();
    mUnsubscribeQueue.ForceTimeout();
    mPublishQos1Queue.ForceTimeout();
    mPublishQos2PublishQueue.ForceTimeout();
    mPublishQos2PubrelQueue.ForceTimeout();
    mPublishQos2PubrecQueue.ForceTimeout();
    mPingreqQueue.ForceTimeout();
    mDisconnectQueue.ForceTimeout();
}

bool MqttsnClient::VerifyGatewayAddress(const Ip6::MessageInfo &aMessageInfo)
{
    return aMessageInfo.GetPeerAddr() == mConfig.GetAddress()
        && aMessageInfo.GetPeerPort() == mConfig.GetPort();
}

uint16_t MqttsnClient::GetNextMessageId(void)
{
    return ++mMessageId;
}

void MqttsnClient::ResetPingreqTime(void)
{
    mPingReqTime = TimerMilli::GetNow().GetValue() + mConfig.GetKeepAlive() * 800;
}

void MqttsnClient::WakeUp(void)
{
    // Wake up client from sleeping mode
    // If in sleep mode then switch to active again
    if (mClientState == kStateAsleep || mClientState == kStateAwake)
    {
        mClientState = kStateActive;
    }
    // Enqueue process tasklet again
    //mProcessTask.Post();
}

void MqttsnClient::HandleSubscribeTimeout(const MessageMetadata<otMqttsnSubscribedHandler> &aMetadata, void* aContext)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, 0, kQos0, aMetadata.mContext);
}

void MqttsnClient::HandleRegisterTimeout(const MessageMetadata<otMqttsnRegisteredHandler> &aMetadata, void* aContext)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, NULL, aMetadata.mContext);
}

void MqttsnClient::HandleUnsubscribeTimeout(const MessageMetadata<otMqttsnUnsubscribedHandler> &aMetadata, void* aContext)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, aMetadata.mContext);
}

void MqttsnClient::HandlePublishQos1Timeout(const MessageMetadata<otMqttsnPublishedHandler> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);
    aMetadata.mCallback(kCodeTimeout, aMetadata.mContext);
}

void MqttsnClient::HandlePublishQos2PublishTimeout(const MessageMetadata<otMqttsnPublishedHandler> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);
    aMetadata.mCallback(kCodeTimeout, aMetadata.mContext);
}

void MqttsnClient::HandlePublishQos2PubrelTimeout(const MessageMetadata<otMqttsnPublishedHandler> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);
    aMetadata.mCallback(kCodeTimeout, aMetadata.mContext);
}

void MqttsnClient::HandlePublishQos2PubrecTimeout(const MessageMetadata<void*> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aMetadata);
    OT_UNUSED_VARIABLE(aContext);
}

void MqttsnClient::HandleConnectTimeout(const MessageMetadata<void*> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aMetadata);
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    if (client->mConnectedCallback)
    {
        client->mConnectedCallback(kCodeTimeout, client->mConnectContext);
    }
}

void MqttsnClient::HandleDisconnectTimeout(const MessageMetadata<void*> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aMetadata);
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
}

void MqttsnClient::HandlePingreqTimeout(const MessageMetadata<void*> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aMetadata);
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    //otLogInfoMqttsn("Ping timeout - gateway not responding");
    // If gateway not responding to ping client will be disconnected
    client->mTimeoutRaised = true;
}

void MqttsnClient::HandleMessageRetransmission(const Message &aMessage, const Ip6::Address &aAddress, uint16_t aPort, void* aContext)
{
    //otLogInfoMqttsn("Message retransmission");
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    Message* retransmissionMessage = aMessage.Clone(aMessage.GetLength());
    if (retransmissionMessage != NULL)
    {
        client->SendMessage(*retransmissionMessage, aAddress, aPort);
    }
}

void MqttsnClient::HandlePublishRetransmission(const Message &aMessage, const Ip6::Address &aAddress, uint16_t aPort, void* aContext)
{
    //otLogInfoMqttsn("Publish message retransmission");
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    unsigned char buffer[MAX_PACKET_SIZE];
    PublishMessage publishMessage;

    // Read message content
    uint16_t offset = aMessage.GetOffset();
    int32_t length = aMessage.GetLength() - aMessage.GetOffset();

    if (length > MAX_PACKET_SIZE || !buffer)
    {
        return;
    }
    aMessage.Read(offset, length, buffer);
    if (publishMessage.Deserialize(buffer, length) != OT_ERROR_NONE)
    {
        return;
    }
    // Set DUP flag
    publishMessage.SetDupFlag(true);
    if (publishMessage.Serialize(buffer, MAX_PACKET_SIZE, &length) != OT_ERROR_NONE)
    {
        return;
    }
    Message* retransmissionMessage = NULL;
    if (client->NewMessage(&retransmissionMessage, buffer, length) != OT_ERROR_NONE)
    {
        return;
    }
    client->SendMessage(*retransmissionMessage, aAddress, aPort);
}

void MqttsnClient::HandleSubscribeRetransmission(const Message &aMessage, const Ip6::Address &aAddress, uint16_t aPort, void* aContext)
{
    //otLogInfoMqttsn("Subscribe message retransmission");
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    unsigned char buffer[MAX_PACKET_SIZE];
    SubscribeMessage subscribeMessage;

    // Read message content
    uint16_t offset = aMessage.GetOffset();
    int32_t length = aMessage.GetLength() - aMessage.GetOffset();

    unsigned char data[MAX_PACKET_SIZE];

    if (length > MAX_PACKET_SIZE || !data)
    {
        return;
    }
    aMessage.Read(offset, length, data);
    if (subscribeMessage.Deserialize(buffer, length) != OT_ERROR_NONE)
    {
        return;
    }
    // Set DUP flag
    subscribeMessage.SetDupFlag(true);
    if (subscribeMessage.Serialize(buffer, MAX_PACKET_SIZE, &length) != OT_ERROR_NONE)
    {
        return;
    }
    Message* retransmissionMessage = NULL;
    if (client->NewMessage(&retransmissionMessage, buffer, length) != OT_ERROR_NONE)
    {
        return;
    }
    client->SendMessage(*retransmissionMessage, aAddress, aPort);
}

void MqttsnClient::HandlePingreqRetransmission(const Message &aMessage, const Ip6::Address &aAddress, uint16_t aPort, void* aContext)
{
    //otLogInfoMqttsn("Pingreq message retransmission");
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    Message* retransmissionMessage = aMessage.Clone(aMessage.GetLength());
    if (retransmissionMessage != NULL)
    {
        client->SendMessage(*retransmissionMessage, aAddress, aPort);
    }
    client->ResetPingreqTime();
}

}

}

#endif // OPENTHREAD_CONFIG_MQTTSN_ENABLE
