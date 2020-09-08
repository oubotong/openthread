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

#ifndef MQTTSN_GATEWAY_LIST_HPP_
#define MQTTSN_GATEWAY_LIST_HPP_

#include <openthread/mqttsn.h>
#include "net/ip6_address.hpp"
#include "common/linked_list.hpp"

/**
 * @file
 *   This file includes definition of list for maintaining active MQTT-SN gateways.
 *
 */

namespace ot {

/**
 * @namespace ot::Mqttsn
 * @brief
 *   This namespace includes definitions for MQTT-SN.
 *
 */
namespace Mqttsn {

/**
 * @addtogroup core-mqttsn
 *
 * @brief
 *   This module includes definitions for MQTT-SN.
 *
 * @{
 *
 */

/**
 * MQTT-SN gateway ID.
 *
 */
typedef otMqttsnGatewayId GatewayId;

/**
 * Generic statically allocated linked list entry (node) class.
 *
 */
template <class Type>
class StaticListEntry : public LinkedListEntry<StaticListEntry<Type> >
{
    friend  LinkedListEntry<StaticListEntry<Type> >;

public:
    /**
     * This constructor initializes the object.
     *
     */
    StaticListEntry(void)
        : LinkedListEntry<StaticListEntry<Type> >()
        , mNext(NULL)
        , mValue()
        , mIsRemoved(false)
    {
        ;
    }

    /**
     * This constructor initializes the object.
     *
     * @param[in]  aValue  A reference to value of the list item.
     *
     */
    StaticListEntry(const Type &aValue)
        : LinkedListEntry<StaticListEntry<Type> >()
        , mNext(NULL)
        , mValue(aValue)
        , mIsRemoved(false)
    {
        ;
    }

    /**
     * Get value of the list item.
     *
     * @returns  A reference to the value of the list item.
     *
     */
    Type &GetValue(void) { return mValue; }

    /**
     * Get value of the list item.
     *
     * @returns  A reference to constant value of the list item.
     *
     */
    const Type &GetValue(void) const { return mValue; }

    /**
     * This method indicates whether the list entry is already removed from the buffer.
     *
     * @retval TRUE   If the entry is removed from the buffer.
     * @retval FALSE  If the entry is not removed from the buffer.
     *
     */
    bool IsRemoved(void) const { return mIsRemoved; }

    /**
     * Set the flag if the entry is removed from the buffer.
     *
     * @param[in]  aIsRemoved  True if the entry is removed from the buffer.
     *
     */
    void SetIsRemoved(bool aIsRemoved) { mIsRemoved = aIsRemoved; }

private:
    StaticListEntry<Type> *mNext;
    Type mValue;
    bool mIsRemoved;
};

/**
 * Generic linked list with statically allocated buffer for storing entries.
 * All entries are copied into the buffer.
 *
 */
template<class Type>
class StaticArrayList
{
public:
    /**
     * This constructor initializes the object.
     *
     * @param[in]  aBuffer  A Pointer to statically allocated buffer where list items will be stored.
     * @param[in]  aSize    Maximal number of items.
     *
     */
    StaticArrayList(StaticListEntry<Type> *aBuffer, uint16_t aSize);

    /**
     * This method returns the entry at the head of the list
     *
     * @returns Pointer to the entry at the head of the list, or NULL if list is empty.
     *
     */
    StaticListEntry<Type> *GetHead(void);

    /**
     * This method returns the entry at the head of the list.
     *
     * @returns Pointer to the entry at the head of the list, or NULL if list is empty.
     *
     */
    const StaticListEntry<Type> *GetHead(void) const;

    /**
     * Add new item to the list. The new item is copied to static buffer.
     *
     * @param[in]  aEntry  A reference to new item value.
     *
     * @retval OT_ERROR_NONE     New item was successfully added.
     * @retval OT_ERROR_NO_BUFS  There is not space left in list buffer.
     */
    otError Add(const Type &aEntry);

    /**
     * Remove existing item from the list.
     *
     * @param[in]  aEntry  A pointer to the item to be removed.
     *
     * @retval OT_ERROR_NONE       Item was successfully removed.
     * @retval OT_ERROR_NOT_FOUND  Item is not present in the list.
     */
    otError Remove(StaticListEntry<Type> &aEntry);

    /**
     * Get current number of items in the list.
     *
     * @returns  Number of items in the list.
     *
     */
    uint16_t Size(void) const { return mSize; };

    /**
     * This method indicates whether the list is empty or not.
     *
     * @retval TRUE   If the list is empty.
     * @retval FALSE  If the list is not empty.
     *
     */
    bool IsEmpty(void) const;

    /**
     * Remove all items from the list.
     *
     */
    void Clear(void);

private:

    LinkedList<StaticListEntry<Type> > mList;
    StaticListEntry<Type> *mBuffer;
    uint16_t mMaxSize;
    uint16_t mSize;
};

/**
 * This class represents list for maintaining information about active MQTT-SN gateways.
 *
 */
class ActiveGatewayList;

/**
 * This structure contains information about advertised MQTT-SN gateway.
 *
 */
class GatewayInfo : public otMqttsnGatewayInfo
{
    friend class ActiveGatewayList;

public:
    /**
     * This constructor initializes the object.
     *
     */
    GatewayInfo(void)
        : mLastUpdatedTimestamp()
        , mDuration()
    {
        mGatewayId = 0;
        mGatewayAddress = ot::Ip6::Address();
    }

    /**
     * This constructor initializes the object.
     *
     * @param[in]  aGatewayId             ID of the gateway.
     * @param[in]  aGatewayAddress        A reference to IPv6 address of the gateway.
     * @param[in]  aLastUpdatedTimestamp  Device timestamp in ms when was gateway state updated.
     * @param[in]  aDuration              Keepalive duration how long is gateway considered as active before next update.
     *
     */
    GatewayInfo(GatewayId aGatewayId, const ot::Ip6::Address &aGatewayAddress,
            uint32_t aLastUpdatedTimestamp, uint32_t aDuration)
        : mLastUpdatedTimestamp(aLastUpdatedTimestamp)
        , mDuration(aDuration)
    {
        mGatewayId = aGatewayId;
        mGatewayAddress = aGatewayAddress;
    }

    /**
     * Get ID of the gateway.
     *
     * @return  ID of the gateway.
     *
     */
    GatewayId GetGatewayId(void) const
    {
        return mGatewayId;
    }

    /**
     * IPv6 address of the gateway.
     *
     * @return  A reference to IPv6 address of the gateway.
     *
     */
    const ot::Ip6::Address &GetGatewayAddress(void) const
    {
        return *static_cast<const ot::Ip6::Address *>(&mGatewayAddress);
    }

    /**
     * This method evaluates whether or not the gateway info match.
     *
     * @param[in]  aOther  The gateway info to compare.
     *
     * @retval TRUE   If the gateway info match.
     * @retval FALSE  If the gateway info do not match.
     *
     */
    bool operator==(const GatewayInfo &aOther) const
    {
        return GetGatewayAddress() == aOther.GetGatewayAddress() && mGatewayId == aOther.mGatewayId
                && mLastUpdatedTimestamp == aOther.mLastUpdatedTimestamp
                && mDuration == aOther.mDuration;
    }

    /**
     * This method evaluates whether or not the gateway info differ.
     *
     * @param[in]  aOther  The gateway info to compare.
     *
     * @retval TRUE   If the gateway info differ.
     * @retval FALSE  If the gateway info do not differ.
     *
     */
    bool operator!=(const GatewayInfo &aOther) const { return !(*this == aOther); }

private:
    uint32_t mLastUpdatedTimestamp;
    uint32_t mDuration;
};

template class StaticArrayList<GatewayInfo>;

/**
 * This class represents list for maintaining information about active MQTT-SN gateways.
 * Each gateway has the duration for which is considered as active. If gateway is
 * not updated in this period it is removed from the list.
 *
 */
class ActiveGatewayList
{
public:
    /**
     * This constructor initializes the object.
     *
     */
    ActiveGatewayList(void)
        : mGatewayInfoListArray()
        , mGatewayInfoList(mGatewayInfoListArray, kMaxGatewayInfoCount)
    {

    }

    /**
     * Add new active gateway to the list.
     *
     * @param[in]  aGatewayId       A ID of the gateway.
     * @param[in]  aGatewayAddress  A reference to IPv6 address of the gateway.
     * @param[in]  aGatewayAddress  A duration in ms for which is gateway considered as active.
     *
     * @retval OT_ERROR_NONE     New item was successfully added.
     * @retval OT_ERROR_NO_BUFS  There is not space left in list buffer.
     */
    otError Add(GatewayId aGatewayId, const ot::Ip6::Address &aGatewayAddress, uint32_t aDuration);

    /**
     * Remove all gateways from the list.
     *
     */
    void Clear(void);

    /**
     * This method checks active gateways in the list and removes inactive gateways.
     *
     * @retval OT_ERROR_NONE  Operation was successfully executed.
     *
     */
    otError HandleTimer(void);

    /**
     * Get list of active gateways.
     *
     * @returns  A reference to the list of active gateways.
     *
     */
    const StaticArrayList<GatewayInfo> &GetList(void) const;

private:
    /**
     * Find a gateway in the list.
     *
     * @param[in]  aGatewayId  A ID of the gateway.
     *
     * @returns  A pointer to the gateway info or NULL if no gateway found in the list.
     *
     */
    GatewayInfo *Find(GatewayId aGatewayId);

    StaticListEntry<GatewayInfo> mGatewayInfoListArray[kMaxGatewayInfoCount];
    StaticArrayList<GatewayInfo> mGatewayInfoList;
};

/**
 * @}
 *
 */

} // namespace Mqttsn
} // namespace ot

#endif /* MQTTSN_GATEWAY_LIST_HPP_ */
