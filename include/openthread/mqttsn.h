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

#ifndef OPENTHREAD_MQTTSN_H_
#define OPENTHREAD_MQTTSN_H_

#include <openthread/instance.h>
#include <openthread/ip6.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OT_MQTTSN_DEFAULT_PORT 10000

#ifndef OPENTHREAD_CONFIG_MQTTSN_ENABLE
#define OPENTHREAD_CONFIG_MQTTSN_ENABLE 0
#endif

/**
 * @addtogroup api-mqttsn
 *
 * @brief
 *  This module provides API for MQTT-SN client library. Client provides functionality
 *  for MQTT-SN connection established and full messaging capabilities over UDP.
 *
 * @{
 *
 */

enum
{
    /**
     * Maximum number of active gateways to be maintained. Static buffer is allocated
     * for the gateway information.
     *
     */
    kMaxGatewayInfoCount = 5
};

/**
 * MQTT-SN message return code.
 *
 */
typedef enum
{
    kCodeAccepted = 0,
    kCodeRejectedCongestion = 1,
    kCodeRejectedTopicId = 2,
    kCodeRejectedNotSupported = 3,
    /**
     * Pending message timed out. this value is not returned by gateway.
     */
    kCodeTimeout = -1,
} otMqttsnReturnCode;

/**
 * MQTT-SN quality of service level.
 *
 */
typedef enum
{
    kQos0 = 0x0,
    kQos1 = 0x1,
    kQos2 = 0x2,
    kQosm1 = 0x3
} otMqttsnQos;

/**
 * Client lifecycle states.
 *
 */
typedef enum
{
    /**
     * Client is not connected to gateway.
     */
    kStateDisconnected,
    /**
     * Client is connected to gateway and currently alive.
     */
    kStateActive,
    /**
     * Client is in sleeping state.
     */
    kStateAsleep,
    /**
     * Client is awaken from sleep.
     */
    kStateAwake,
    /**
     * Client connection is lost due to communication error.
     */
    kStateLost,
} otMqttsnClientState;

/**
 * MQTT-SN topic identificator type.
 */
typedef enum
{
    /**
     * Two character short topic name.
     *
     */
    kShortTopicName,
    /**
     * Long topic name.
     *
     */
    kTopicName,
    /**
     * Registered topic ID.
     *
     */
    kTopicId,
    /**
     * Predefined topic ID.
     *
     */
    kPredefinedTopicId
} otMqttsnTopicIdType;

/**
 * Numeric topic ID.
 *
 */
typedef uint16_t otMqttsnTopicId;

/**
 * Topic ID structure.
 *
 * @note This structure aggregates all possible topic id types.
 *
 */
typedef struct otMqttsnTopic
{
    otMqttsnTopicIdType mType;
    union
    {
        const char *mTopicName;
        char mShortTopicName[3];
        otMqttsnTopicId mTopicId;
    } mData;
} otMqttsnTopic;

/**
 * Disconnected state reason.
 *
 */
typedef enum
{
    /**
     * Client was disconnected by gateway/broker.
     */
    kDisconnectServer,
    /**
     * Disconnection was invoked by client.
     */
    kDisconnectClient,
    /**
     * Client changed state to asleep
     */
    kDisconnectAsleep,
    /**
     * Communication timeout.
     */
    kDisconnectTimeout
} otMqttsnDisconnectType;

/**
 * This structure contains MQTT-SN connection parameters.
 *
 */
typedef struct otMqttsnConfig {
    /**
    * Gateway IPv6 address.
    */
    otIp6Address *mAddress;
    /**
    * Gateway interface port number.
    */
    uint16_t mPort;
    /**
     * Client id string.
     */
    char *mClientId;
    /**
     * Keepalive period in seconds.
     */
    uint16_t mKeepAlive;
    /**
     * Clean session flag.
     */
    bool mCleanSession;
    /**
     * Retransmission timeout in seconds.
     */
    uint32_t mRetransmissionTimeout;
    /**
     * Retransmission count.
     */
    uint8_t mRetransmissionCount;
} otMqttsnConfig;

/**
 * MQTT-SN gateway ID.
 *
 */
typedef uint8_t otMqttsnGatewayId;

/**
 * This structure contains information about advertised MQTT-SN gateway.
 */
typedef struct otMqttsnGatewayInfo {
    /**
     * ID of the gateway.
     */
    otMqttsnGatewayId mGatewayId;
    /**
     * IPv6 address of the gateway.
     */
    otIp6Address mGatewayAddress;
} otMqttsnGatewayInfo;

/**
 * Declaration of function for connection callback.
 *
 * @param[in]  aCode     CONNACK return code value or -1 when connection establishment timed out.
 * @param[in]  aContext  A pointer to connection callback context object.
 *
 */
typedef void (*otMqttsnConnectedHandler)(otMqttsnReturnCode aCode, void *aContext);
/**
 * Declaration of function for subscribe callback.
 *
 * @param[in]  aCode     SUBACK return code or -1 when subscription timed out.
 * @param[in]  aTopic    Subscribed topic. The value is NULL when timed out or short topic name was subscribed.
 * @param[in]  aQos      Subscribed quality of service level.
 * @param[in]  aContext  A pointer to subscription callback context object.
 *
 */
typedef void (*otMqttsnSubscribedHandler)(otMqttsnReturnCode aCode, const otMqttsnTopic* aTopic, otMqttsnQos aQos, void* aContext);

/**
 * Declaration of function for register callback.
 *
 * @param[in]  aCode     REGACK return code or -1 when subscription timed out.
 * @param[in]  aTopic    Registered topic. Only topic ID type can be registered.
 * @param[in]  aContext  A pointer to register callback context object.
 *
 */
typedef void (*otMqttsnRegisteredHandler)(otMqttsnReturnCode aCode, const otMqttsnTopic* aTopic, void* aContext);

/**
 * Declaration of function for publish callback. It is invoked only when quality of service level is 1 or 2.
 *
 * @param[in]  aCode     Publish response code or -1 when publish timed out.
 * @param[in]  aTopicId  Topic ID of published message. It is set to 0 when timed out or short topic name is used.
 * @param[in]  aContext  A pointer to publish callback context object.
 *
 */
typedef void (*otMqttsnPublishedHandler)(otMqttsnReturnCode aCode, void* aContext);

/**
 * Declaration of function for unsubscribe callback.
 *
 * @param[in]  aCode     UNSUBACK response code or -1 when publish timed out.
 * @param[in]  aContext  A pointer to unsubscribe callback context object.
 *
 */
typedef void (*otMqttsnUnsubscribedHandler)(otMqttsnReturnCode aCode, void* aContext);

/**
 * Declaration of function for callback invoked when publish message received.
 *
 * @param[in]  aPayload         A pointer to PUBLISH message payload byte array.
 * @param[in]  aPayloadLength   PUBLISH message payload length.
 * @param[in]  aTopic           Published topic. Can be only short topic name, topic ID and predefined topic ID.
 * @param[in]  aContext         A pointer to publish received callback context object.
 *
 * @returns  Code to be send in response PUBACK message. Timeout value is not relevant.
 */
typedef otMqttsnReturnCode (*otMqttsnPublishReceivedHandler)(const uint8_t* aPayload, int32_t aPayloadLength, const otMqttsnTopic* aTopic, void* aContext);

/**
 * Declaration of function for disconnection callback.
 *
 * @param[in]  aType     Disconnection reason.
 * @param[in]  aContext  A pointer to disconnection callback context object.
 *
 */
typedef void (*otMqttsnDisconnectedHandler)(otMqttsnDisconnectType aType, void* aContext);

/**
 * Declaration of function for search gateway callback.
 *
 * @param[in]  aAddress  A pointer to IPv6 address of discovered gateway.
 * @param[in]  aAddress  Discovered gateway ID.
 * @param[in]  aContext  A pointer to search gateway context object.
 *
 */
typedef void (*otMqttsnSearchgwHandler)(const otIp6Address* aAddress, uint8_t aGatewayId, void* aContext);

/**
 * Declaration of function for advertise callback.
 *
 * @param[in]  aAddress    A pointer to advertised gateway IPv6 address.
 * @param[in]  aGatewayId  Advertised gateway ID.
 * @param[in]  aDuration   Advertise message duration parameter.
 * @param[in]  aContext    A pointer to advertise callback context object.
 *
 */
typedef void (*otMqttsnAdvertiseHandler)(const otIp6Address* aAddress, uint8_t aGatewayId, uint32_t aDuration, void* aContext);

/**
 * Declaration of function for callback invoked when register message received.
 *
 * @param[in]  aTopicId    Registered topic ID.
 * @param[in]  aTopicName  Long topic name string mapped to registered topic.
 * @param[in]  aContext    A pointer to register received callback context object.
 *
 * @returns  Code to be send in response REGACK message. Timeout value is not relevant.
 *
 */
typedef otMqttsnReturnCode (*otMqttsnRegisterReceivedHandler)(otMqttsnTopicId aTopicId, const char *aTopicName, void* aContext);

/**
 * Create and initialize topic structure with topic name string. If is of length 1 or 2 then short topic name is sent.
 *
 * @note It can be used only for MQTT-SN subscribe or unsubscribe.
 *
 * @param[in]  aTopicName        A pointer full topic name string.
 *
 * @return Topic of type kTopicName initialized with topic name string.
 *
 */
otMqttsnTopic otMqttsnCreateTopicName(const char *aTopicName);

/**
 * Create and initialize topic structure with numeric topic ID.
 *
 * @note It should be used only for MQTT-SN publish.
 *
 * @param[in]  aTopicId          Numeric topic ID. Must not be 0.
 *
 * @return Topic of type kTopicId initialized with numeric topic ID.
 *
 */
otMqttsnTopic otMqttsnCreateTopicId(otMqttsnTopicId aTopicId);

/**
 * Create and initialize topic structure with predefined topic ID.
 *
 * @note It can be used only for MQTT-SN subscribe, unsubscribe and publish.
 *
 * @param[in]  aTopicId          Numeric topic ID. Must not be 0.
 *
 * @return Topic of type kTopicId initialized with numeric topic ID.
 *
 */
otMqttsnTopic otMqttsnCreatePredefinedTopicId(otMqttsnTopicId aTopicId);

/**
 * Get MQTT-SN topic name from topic structure. Topic must be of type kTopicName or kShortTopicName.
 *
 * @param[in]  aTopic            A pointer to topic.
 *
 * @return A pointer to full or short topic name string.
 *
 */
const char *otMqttsnGetTopicName(const otMqttsnTopic *aTopic);

/**
 * Get MQTT-SN numeric topic ID from topic structure. Topic must be of type kTopicId.
 *
 * @param[in]  aTopic            A pointer to topic.
 *
 * @return Numeric MQTT-SN topic ID.
 *
 */
otMqttsnTopicId otMqttsnGetTopicId(const otMqttsnTopic *aTopic);

/**
 * Start MQTT-SN service and start connection and listening.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 * @param[in]  aPort      MQTT-SN client listening port.
 *
 * @retval OT_ERROR_NONE           Successfully started the service.
 * @retval OT_ERROR_INVALID_STATE  MQTT-SN client is already running.
 *
 */
otError otMqttsnStart(otInstance *aInstance, uint16_t aPort);

/**
 * Stop MQTT-SN service.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 *
 * @retval OT_ERROR_NONE  Successfully stopped the service.
 *
 */
otError otMqttsnStop(otInstance *aInstance);

/**
 * Get current MQTT-SN client state.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 *
 * @return Returns current MQTT-SN client state.
 *
 */
otMqttsnClientState otMqttsnGetState(otInstance *aInstance);

/**
 * Establish MQTT-SN connection with gateway.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 * @param[in]  aConfig    A pointer to configuration object with connection parameters.
 *
 * @retval OT_ERROR_NONE           Connection message successfully queued.
 * @retval OT_ERROR_INVALID_ARGS   Invalid connection parameters.
 * @retval OT_ERROR_INVALID_STATE  Previous connection is still pending.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to process.
 *
 */
otError otMqttsnConnect(otInstance *aInstance, const otMqttsnConfig *aConfig);

/**
 * Establish MQTT-SN connection with gateway with default configuration.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 * @param[in]  mAddress   A pointer to a gateway IPv6 address.
 * @param[in]  mPort      Gateway interface port number.
 *
 * @retval OT_ERROR_NONE           Connection message successfully queued.
 * @retval OT_ERROR_INVALID_ARGS   Invalid connection parameters.
 * @retval OT_ERROR_INVALID_STATE  The client is in invalid state. It must be disconnected before new connection establishment.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to process.
 *
 */
otError otMqttsnConnectDefault(otInstance *aInstance, const otIp6Address* aAddress, uint16_t mPort);

/**
 * Reconnect MQTT-SN client with current connection settings. This is method is useful
 * e.g. for returning from sleep mode to active mode.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 *
 * @retval OT_ERROR_NONE           Connection message successfully queued.
 * @retval OT_ERROR_INVALID_STATE  The client is in invalid state. It must be disconnected before new connection establishment.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to process.
 *
 */
otError otMqttsnReconnect(otInstance *aInstance);

/**
 * Subscribe to the topic.
 *
 * @param[in]  aInstance          A pointer to an OpenThread instance.
 * @param[in]  aTopic             A pointer to the topic to be subscribed. Topic name, short topic name and topic ID are supported.
 * @param[in]  aQos               Quality of service level to be subscribed.
 * @param[in]  aHandler           A function pointer to handler which is invoked when subscription is acknowledged.
 * @param[in]  aContext           A pointer to context object passed to handler.
 *
 * @retval OT_ERROR_NONE           Subscription message successfully queued.
 * @retval OT_ERROR_INVALID_ARGS   Invalid subscription parameters.
 * @retval OT_ERROR_INVALID_STATE  The client cannot connect in active state.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to process.
 *
 */
otError otMqttsnSubscribe(otInstance *aInstance, const otMqttsnTopic* aTopic, otMqttsnQos aQos, otMqttsnSubscribedHandler aHandler, void *aContext);

/**
 * Register to topic with long topic name and obtain related topic ID.
 *
 * @param[in]  aInstance           A pointer to an OpenThread instance.
 * @param[in]  aTopicName          A pointer to long topic name string.
 * @param[in]  aHandler            A function pointer to callback invoked when registration is acknowledged.
 * @param[in]  aContext            A pointer to context object passed to callback.
 *
 * @retval OT_ERROR_NONE           Registration message successfully queued.
 * @retval OT_ERROR_INVALID_STATE  The client is not in active state.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to process.
 *
 */
otError otMqttsnRegister(otInstance *aInstance, const char* aTopicName, otMqttsnRegisteredHandler aHandler, void* aContext);

/**
 * Publish message to the topic.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 * @param[in]  aData      A pointer to byte array to be send as message payload.
 * @param[in]  aLength    Length of message payload data.
 * @param[in]  aQos       Message quality of service level.
 * @param[in]  aRetained  Set retained flag of MQTT-SN publish message.
 * @param[in]  aTopic     A pointer to the topic to publish to. Only short topic name, topic ID and predefined topic ID are allowed.
 * @param[in]  aHandler   A function pointer to callback invoked when publish is acknowledged.
 * @param[in]  aContext   A pointer to context object passed to callback.
 *
 * @retval OT_ERROR_NONE           Publish message successfully queued.
 * @retval OT_ERROR_INVALID_ARGS   Invalid publish parameters. Short topic name must have one or two characters.
 * @retval OT_ERROR_INVALID_STATE  The client is not in active state.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to process.
 *
 */
otError otMqttsnPublish(otInstance *aInstance, const uint8_t* aData, int32_t aLength, otMqttsnQos aQos, bool aRetained, const otMqttsnTopic* aTopic, otMqttsnPublishedHandler aHandler, void* aContext);

/**
 * Publish message to the topic with QoS level -1. No connection or subscription is required.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 * @param[in]  aData      A pointer to byte array to be send as message payload.
 * @param[in]  aLength    Length of message payload data.
 * @param[in]  aRetained  Set retained flag of MQTT-SN publish message.
 * @param[in]  aTopic     A pointer to the topic to publish to. Only short topic name and predefined topic ID are allowed.
 * @param[in]  aAddress   Gateway address.
 * @param[in]  aPort      Gateway port.
 *
 * @retval OT_ERROR_NONE           Publish message successfully queued.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to process.
 *
 */
otError otMqttsnPublishQosm1(otInstance *aInstance, const uint8_t* aData, int32_t aLength, bool aRetained, const otMqttsnTopic *aTopic, const otIp6Address* aAddress, uint16_t aPort);

/**
 * Unsubscribe from the topic by topic name string.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 * @param[in]  aTopic     A pointer to the topic to be unsubscribed. Long topic name, short topic name and topic ID are supported.
 * @param[in]  aHandler   A function pointer to callback invoked when unsubscription is acknowledged.
 * @param[in]  aContext   A pointer to context object passed to callback.
 *
 * @retval OT_ERROR_NONE           Unsubscribe message successfully queued.
 * @retval OT_ERROR_INVALID_STATE  The client is not in active state.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to unsubscribe.
 *
 */
otError otMqttsnUnsubscribe(otInstance *aInstance, const otMqttsnTopic* aTopic, otMqttsnUnsubscribedHandler aHandler, void* aContext);

/**
 * Disconnect MQTT-SN client from gateway.
 *
 * @param[in]  aInstance        A pointer to an OpenThread instance.
 *
 * @retval OT_ERROR_NONE           Disconnection message successfully queued.
 * @retval OT_ERROR_INVALID_STATE  The client is not in relevant state. It must be asleep, awake or active.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to process.
 *
 */
otError otMqttsnDisconnect(otInstance *aInstance);

/**
 * Put the client into asleep state or change sleep duration. Client must be awaken or reconnected before duration time passes.
 *
 * @param[in]  aInstance        A pointer to an OpenThread instance.
 * @param[in]  aDuration        Duration time in seconds for which will the client stay in asleep state.
 *
 * @retval OT_ERROR_NONE           Sleep request successfully queued.
 * @retval OT_ERROR_INVALID_STATE  The client is not in relevant state. It must be asleep, awake or active.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to process.
 *
 */
otError otMqttsnSleep(otInstance *aInstance, uint16_t aDuration);

/**
 * Awake the client and receive pending messages.
 *
 * @note  Configuration retransmission count is still applied.
 *
 * @param[in]  aInstance        A pointer to an OpenThread instance.
 * @param[in]  aTimeout  Timeout in milliseconds for staying in awake state. PINGRESP message must be received before timeout time passes.
 *
 * @retval OT_ERROR_NONE           Awake request successfully queued.
 * @retval OT_ERROR_INVALID_STATE  The client is not in relevant state. It must be asleep or awake.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to process.
 *
 */
otError otMqttsnAwake(otInstance *aInstance, uint32_t aTimeout);

/**
 * Search for gateway with multicast message.
 *
 * @param[in]  aInstance        A pointer to an OpenThread instance.
 * @param[in]  aMulticastAddress  A pointer to multicast IPv6 address.
 * @param[in]  aPort              Gateway port number.
 * @param[in]  aRadius            Message hop limit (time to live)
 *
 * @retval OT_ERROR_NONE           Search gateway request successfully queued.
 * @retval OT_ERROR_NO_BUFS        Insufficient available buffers to process.
 *
 */
otError otMqttsnSearchGateway(otInstance *aInstance, const otIp6Address *aMulticastAddress, uint16_t aPort, uint8_t aRadius);

/**
 * Get number of active gateways. Gateways are periodically advertised or obtained
 * with gwinfo message.
 *
 * @param[in]  aInstance   A pointer to an OpenThread instance.
 *
 * @returns  Number of active gateways cached in MQTT-SN client.
 *
 */
uint16_t otMqttsnGetActiveGatewaysCount(otInstance *aInstance);

/**
 * Get informations about active gateways. Gateways are periodically advertised or obtained
 * with gwinfo message.
 *
 * @param[in]  aInstance    A pointer to an OpenThread instance.
 * @param[in]  aBuffer      A pointer to statically allocated buffer for storing gateway informations.
 * @param[in]  aBufferSize  Maximal number of items which can be stored in the buffer.
 *
 * @returns  Number of active gateways written in the buffer.
 *
 */
uint16_t otMqttsnGetActiveGateways(otInstance *aInstance, otMqttsnGatewayInfo *aBuffer, uint16_t aBufferSize);

/**
 * Set handler which is invoked when connection is acknowledged.
 *
 * @param[in]  aInstance          A pointer to an OpenThread instance.
 * @param[in]  aHandler           A function pointer to handler which is invoked when connection is acknowledged.
 * @param[in]  aContext           A pointer to context object passed to handler.
 *
 * @retval OT_ERROR_NONE          Handler correctly set.
 *
 */
otError otMqttsnSetConnectedHandler(otInstance *aInstance, otMqttsnConnectedHandler aHandler, void *aContext);

/**
 * Set callback function invoked when publish message received from the topic.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 * @param[in]  aHandler   A function pointer to publish received callback function.
 * @param[in]  aContext   A pointer to context object passed to callback.
 *
 * @retval OT_ERROR_NONE  Callback function successfully set.
 *
 */
otError otMqttsnSetPublishReceivedHandler(otInstance *aInstance, otMqttsnPublishReceivedHandler aHandler, void* aContext);

/**
 * Set callback function invoked when disconnect acknowledged or timed out.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 * @param[in]  aHandler   A function pointer to disconnect callback function.
 * @param[in]  aContext   A pointer to context object passed to callback.
 *
 * @retval OT_ERROR_NONE  Callback function successfully set.
 *
 */
otError otMqttsnSetDisconnectedHandler(otInstance *aInstance, otMqttsnDisconnectedHandler aHandler, void* aContext);

/**
 * Set callback function invoked when gateway info received from gateway.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 * @param[in]  aHandler   A function pointer to gateway info received callback function.
 * @param[in]  aContext   A pointer to context object passed to callback.
 *
 * @retval OT_ERROR_NONE  Callback function successfully set.
 *
 */
otError otMqttsnSetSearchgwHandler(otInstance *aInstance, otMqttsnSearchgwHandler aHandler, void* aContext);

/**
 * Set callback function invoked when advertise message received from the gateway.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 * @param[in]  aHandler   A function pointer to advertise callback function.
 * @param[in]  aContext   A pointer to context object passed to callback.
 *
 * @retval OT_ERROR_NONE  Callback function successfully set.
 *
 */
otError otMqttsnSetAdvertiseHandler(otInstance *aInstance, otMqttsnAdvertiseHandler aHandler, void* aContext);

/**
 * Set callback function invoked when register message received.
 *
 * @param[in]  aInstance  A pointer to an OpenThread instance.
 * @param[in]  aHandler   A function pointer to register callback function.
 * @param[in]  aContext   A pointer to context object passed to callback.
 *
 * @retval OT_ERROR_NONE  Callback function successfully set.
 *
 */
otError otMqttsnSetRegisterReceivedHandler(otInstance *aInstance, otMqttsnRegisterReceivedHandler aHandler, void* aContext);

/**
 * Get string value of given return code.
 *
 * @param[in]  aCode              MQTT-SN message return code.
 * @param[out] aCodeString        A pointer to string pointer which will contain return code string value.
 *
 * @retval OT_ERROR_NONE          String value was obtained.
 * @retval OT_ERROR_INVALID_ARGS  Invalid return code value.
 *
 */
otError otMqttsnReturnCodeToString(otMqttsnReturnCode aCode, const char** aCodeString);

/**
 * Get MQTT-SN quality of service level from string value. Only values '0', '1', '2' and '-1' are allowed.
 *
 * @param[in]   aQosString        A pointer to string with MQTT-SN QoS level value
 * @param[out]  aQos              A pointer to MQTT-SN QoS level which will be set.
 *
 * @retval OT_ERROR_NONE          MQTT-SN QoS level was obtained..
 * @retval OT_ERROR_INVALID_ARGS  Invalid QoS string value.
 *
 */
otError otMqttsnStringToQos(const char* aQosString, otMqttsnQos *aQos);

/**
 * Get string value of given MQTT-SN client state.
 *
 * @param[in]  aClientState        MQTT-SN client state.
 * @param[out] aClientStateString  A pointer to string pointer which will contain client state string value.
 *
 * @retval OT_ERROR_NONE           String value was obtained.
 * @retval OT_ERROR_INVALID_ARGS   Invalid client state value.
 *
 */
otError otMqttsnClientStateToString(otMqttsnClientState aClientState, const char** aClientStateString);

/**
 * Get string value of given MQTT-SN disconnect type.
 *
 * @param[in]  aDisconnectType        MQTT-SN disconnect type.
 * @param[out] aDisconnectTypeString  A pointer to string pointer which will contain disconnect type string value.
 *
 * @retval OT_ERROR_NONE              String value was obtained.
 * @retval OT_ERROR_INVALID_ARGS      Invalid disconnect type value.
 *
 */
otError otMqttsnDisconnectTypeToString(otMqttsnDisconnectType aDisconnectType, const char** aDisconnectTypeString);

/**
 * @}
 *
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* OPENTHREAD_MQTTSN_H_ */
