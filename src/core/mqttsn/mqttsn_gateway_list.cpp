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

#include "mqttsn_gateway_list.hpp"
#include "common/timer.hpp"

namespace ot {

namespace Mqttsn {
	
template <typename Type>
StaticArrayList<Type>::StaticArrayList(StaticListEntry<Type> *aBuffer, uint16_t aSize)
    : mList()
    , mBuffer(aBuffer)
    , mMaxSize(aSize)
    , mSize(0)
{
    Clear();
}

template <typename Type>
StaticListEntry<Type> *StaticArrayList<Type>::GetHead(void)
{
    return mList.GetHead();
}

template <typename Type>
const StaticListEntry<Type> *StaticArrayList<Type>::GetHead(void) const
{
    return mList.GetHead();
}

template <typename Type>
otError StaticArrayList<Type>::Add(const Type &aEntry)
{
    otError error = OT_ERROR_NO_BUFS;
    if (mBuffer == NULL || mMaxSize == 0 || mSize >= mMaxSize)
    {
        return error;
    }
    for (uint16_t i = 0; i < mMaxSize; i++)
    {
        // Find free space in the buffer for new item
        if (mBuffer[i].IsRemoved())
        {
            mBuffer[i] = aEntry;
            mBuffer[i].SetIsRemoved(false);
            error = mList.Add(mBuffer[i]);
            if (error == OT_ERROR_NONE)
            {
                mSize++;
            }
            break;
        }
    }
    return error;
}

template <typename Type>
otError StaticArrayList<Type>::Remove(StaticListEntry<Type> &aEntry)
{
    otError error = OT_ERROR_NOT_FOUND;
    if (mBuffer == NULL || mList.IsEmpty())
    {
        return error;
    }
    SuccessOrExit(error = mList.Remove(aEntry));
    aEntry.SetIsRemoved(true);
    mSize--;
exit:
    return error;
}

template<typename Type>
bool StaticArrayList<Type>::IsEmpty(void) const
{
    return mList.IsEmpty();
}

template <typename Type>
void StaticArrayList<Type>::Clear(void)
{
    if (mBuffer == NULL)
    {
        return;
    }
    for (uint16_t i = 0; i < mMaxSize; i++)
    {
        mBuffer[i].SetIsRemoved(true);
    }
    mSize = 0;
    mList.Clear();
}

otError ActiveGatewayList::Add(GatewayId aGatewayId, const ot::Ip6::Address &aGatewayAddress, uint32_t aDuration)
{
    otError error = OT_ERROR_NONE;
    uint32_t millisNow = TimerMilli::GetNow().GetValue();

    GatewayInfo *gatewayInfo = Find(aGatewayId);
    if (gatewayInfo != NULL)
    {
        // If gateway exists in the list just update information
        gatewayInfo->mGatewayAddress = aGatewayAddress;
        gatewayInfo->mLastUpdatedTimestamp = millisNow;
        gatewayInfo->mDuration = aDuration;
    }
    else
    {
        GatewayInfo newGatewayInfo = GatewayInfo(aGatewayId, aGatewayAddress, millisNow, aDuration);
        SuccessOrExit(error = mGatewayInfoList.Add(newGatewayInfo));
    }

exit:
    return error;
}

void ActiveGatewayList::Clear()
{
    mGatewayInfoList.Clear();
}

otError ActiveGatewayList::HandleTimer(void)
{
	otError error = OT_ERROR_NONE;
	StaticListEntry<GatewayInfo> *entry = NULL;
	uint32_t millisNow;
    if (mGatewayInfoList.IsEmpty())
    {
        ExitNow(error = OT_ERROR_NONE);
    }
	millisNow = TimerMilli::GetNow().GetValue();
    entry = mGatewayInfoList.GetHead();
    // Find all expired gateways in the list and remove them
    do
    {
        StaticListEntry<GatewayInfo> *currentEntry = entry;
        GatewayInfo &info = currentEntry->GetValue();
        entry = currentEntry->GetNext();
        if (millisNow > info.mLastUpdatedTimestamp + info.mDuration)
        {
            SuccessOrExit(error = mGatewayInfoList.Remove(*currentEntry));
        }
    }
    while (entry != NULL);
exit:
	return error;
}

GatewayInfo *ActiveGatewayList::Find(GatewayId aGatewayId)
{
    StaticListEntry<GatewayInfo> *entry = mGatewayInfoList.GetHead();
    if (entry != NULL)
    {
        do
        {
            if (entry->GetValue().GetGatewayId() == aGatewayId)
            {
                return &entry->GetValue();
            }
            entry = entry->GetNext();
        } while (entry != NULL);
    }
    return NULL;
}

const StaticArrayList<GatewayInfo> &ActiveGatewayList::GetList(void) const
{
    return mGatewayInfoList;
}

}

}
