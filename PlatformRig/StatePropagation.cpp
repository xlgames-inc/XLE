// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StatePropagation.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/PtrUtils.h"

namespace PlatformRig
{
    void OnStateBundleCreate(std::shared_ptr<Network::StatePropagation::StateBundle>& newBundle);
    void OnStateBundleEvent(Network::StateBundleId bundleId, const char eventString[]);
}

namespace PlatformRig { namespace Network { namespace StatePropagation
{

        ////////////////////////////////////////////////////////////////////////

    VisibilityObject::VisibilityObject()
    {
        _transpondVisibilityArea = 0;
        _transpondPosition = std::make_pair(
            Float3(-FLT_MAX, -FLT_MAX, -FLT_MAX), Float3(FLT_MAX, FLT_MAX, FLT_MAX));
        _active = false;
    }

    VisibilityObject::VisibilityObject(const std::pair<Float3, Float3>& position, VisibilityArea area)
    : _transpondPosition(position)
    , _transpondVisibilityArea(area)
    {
        _active = true;
    }

    bool    VisibilityObject::VisibleToObserver(const Float3& observerPosition, VisibilityArea observerArea) const
    {
            //      Currently visible to everyone, so long as we're active!
        return _active;
    }

        ////////////////////////////////////////////////////////////////////////

    bool    SubscriptionList::HasSubscriber(MeshNodeId meshNode) const
    {
        auto i = std::lower_bound(_subscribers.cbegin(), _subscribers.cend(), meshNode);
        return i!=_subscribers.end() && *i == meshNode;
    }

    void    SubscriptionList::AddSubscriber(MeshNodeId meshNode)
    {
        auto i = std::lower_bound(_subscribers.cbegin(), _subscribers.cend(), meshNode);
        assert(i==_subscribers.end() || *i != meshNode);
        _subscribers.insert(i, meshNode);
    }

    bool    SubscriptionList::RemoveSubscriber(MeshNodeId meshNode)
    {
        auto i = std::lower_bound(_subscribers.cbegin(), _subscribers.cend(), meshNode);
        if (i!=_subscribers.end() && *i == meshNode) {
            _subscribers.erase(i);
            return true;
        }
        return false;
    }

        ////////////////////////////////////////////////////////////////////////

    StateBundle::Data::Data(    Threading::ReadWriteMutex& mutex, 
                                const void* data, size_t size, NetworkTime time)
    :       _lock(mutex, false), _data(data)
    ,       _time(time), _size(size)
    {
    }

        ////////////////////////////////////////////////////////////////////////

    void            StateBundle::UpdateData(const void* newData, size_t newDataSize, NetworkTime time)
    {
        if (time == 0) {
            time = _broadcaster->GetTimeNow();
        }

            //
            //      Push the new data into our currentData
            //      First, write into into the "_prevData" part,
            //      and then swap into _data.
            //      This allows us to minimize time spent locking "_data"
            //
        Threading::Mutex::scoped_lock lock(_prevDataLock);
        std::copy(
            (uint8*)newData, PtrAdd((uint8*)newData, std::min(newDataSize, _prevData.size())),
            _prevData.begin());

            //
            //      Note, double mutex locks opens door for deadlocks
            //      don't lock the _dataLock first and _prevDataLock second!
            //
        Threading::ReadWriteMutex::scoped_lock(_dataLock, true);
        std::swap(_prevData, _data);
        _currentDataTime = time;

            //
            //      Sometimes, we should push the updated data to
            //      our subscribers. We don't need to do this absolutely
            //      every time, for every packet. For some packets, it's
            //      ok for a little bit of latency. In these cases; 
            //      we should throttle the output, and only send updated
            //      packets infrequently.
            //
        if (_broadcaster && _authority == _broadcaster->LocalMeshNode()) {
            _broadcaster->OnUpdate(
                _packetId, _subscriptionList, 
                AsPointer(_data.begin()), _data.size(), 
                _bundleType, _currentDataTime);
        }
    }

    auto StateBundle::LockData() const -> Data
    {
        return Data(_dataLock, AsPointer(_data.begin()), _data.size(), _currentDataTime);
    }

    void            StateBundle::UpdateVisibility(const VisibilityObject& visibility)
    {
        _visibilityObject = visibility;
    }

    void            StateBundle::UpdateBroadcaster(IStatePacketBroadcaster* broadcaster)
    {
        _broadcaster = broadcaster;
    }

    void            StateBundle::UpdateAuthority(MeshNodeId authority)
    {
        _authority = authority;
    }

    void            StateBundle::Event(const char eventString[])
    {
        if (_broadcaster) {
            _broadcaster->OnEvent(_packetId, _subscriptionList, eventString);
        }
    }

    StateBundle::StateBundle(StateBundleId id, size_t size, StateBundleType type)
    {
        _data.resize(size, 0);
        _prevData.resize(size, 0);
        _packetId = id;
        _authority = 0;
        _currentDataTime = _prevDataTime = 0;
        _bundleType = type;
        _broadcaster = nullptr;
    }

        ////////////////////////////////////////////////////////////////////////

    std::shared_ptr<StateBundle>        StateWorld::CreateAuthoritativePacket(size_t size, StateBundleType type)
    {
        StateBundleId packetId = GenerateGuid64();
        auto newPacket = std::make_shared<StateBundle>(packetId, size, type);
        newPacket->UpdateBroadcaster(this);
        newPacket->UpdateAuthority(_localMeshNode);
        _authoriativePackets.insert(std::make_pair(packetId, newPacket));
        return newPacket;
    }

    std::vector<StateBundleId>  StateWorld::FindSubscriptionSuggestions(
        const Float3& observerPosition, VisibilityArea observerArea, MeshNodeId observer)
    {   
        std::vector<StateBundleId> result;

            //
            //      Find all packets that are visible to the given observer,
            //      but don't yet have that observer as a subscriber
            //
        for (auto i=_authoriativePackets.begin(); i!=_authoriativePackets.end(); ++i) {
            const bool visible = i->second->GetVisibility().VisibleToObserver(observerPosition, observerArea);
            if (visible && !i->second->GetSubscriptionList().HasSubscriber(observer)) {
                result.push_back(i->first);
            }
        }

        return result;
    }

    void    StateWorld::Subscribe       (MeshNodeId meshNode, StateBundleId statePacket)
    {
            //
            //      Find the given state packet, and add "meshNode" as a subscriber.
            //      We them have to immediately update that packet -- but only to
            //      the new subscriber
            //
        auto i=_authoriativePackets.find(statePacket);
        if (i!=_authoriativePackets.end()) {

                // (do we need to send recent events to the new subscriber, also?)
            i->second->GetSubscriptionList().AddSubscriber(meshNode);
            auto data = i->second->LockData();
            OnUpdate(statePacket, meshNode, data.Get(), data.GetSize(), i->second->GetType(), data.GetTime());

        } else {
            LogWarningF("Attempting to subscribe to a state packet that doesn't exist in our authoritative packet list!");
        }
    }

    void    StateWorld::Unsubscribe     (MeshNodeId meshNode, StateBundleId statePacket)
    {
        auto i=_authoriativePackets.find(statePacket);
        if (i!=_authoriativePackets.end()) {
            const bool success = i->second->GetSubscriptionList().RemoveSubscriber(meshNode);
            if (!success) {
                LogWarningF("Unsubscribe failed because the given mesh node wasn't in our original subscription list");
            }
        } else {
            LogWarningF("Unsubscribe failed because couldn't find the requested state packet. It may have already been destroyed");
        }
    }

    void    StateWorld::UnsubscribeAll  (MeshNodeId meshNode)
    {
        for (auto i=_authoriativePackets.begin(); i!=_authoriativePackets.end(); ++i) {
            i->second->GetSubscriptionList().RemoveSubscriber(meshNode);
        }
    }

    void    StateWorld::OnUpdate(   StateBundleId packet, const SubscriptionList& subscription, 
                                    const void* data, size_t size, StateBundleType packetType, 
                                    NetworkTime time)
    {
            //
            //      We have to push this packet data out as the new state of the given
            //      packet. Send it to all mesh nodes listed in "subscription"
            //
            //      Note -- not thread safe!
            //
        for (auto i=subscription.GetSubscribers().cbegin(); i!=subscription.GetSubscribers().cend(); ++i) {
                //  probably we should cache the XlConnection object, rather than finding
                //  it every time!
            OnUpdate(packet, *i, data, size, packetType, time);
        }
    }

    static void SendPacket(MeshNodeId destination, XlNetPacket* packet)
    {
        XlConnection* connection = LocalMeshNode::GetInstance().FindConnection(destination);
        if (connection) {
            connection->SendPacket(packet);
        }
    }

    void    StateWorld::OnUpdate(   StateBundleId packet, MeshNodeId singleDestination, 
                                    const void* data, size_t size, StateBundleType packetType, 
                                    NetworkTime time)
    {
        ContactPacket::StateBundleUpdate out;
        out.Init(packet, data, size, packetType, time);
        SendPacket(singleDestination, &out);
    }

    void    StateWorld::OnDestroy(StateBundleId packet, const SubscriptionList& subscription)
    {
        for (auto i=subscription.GetSubscribers().cbegin(); i!=subscription.GetSubscribers().cend(); ++i) {
            ContactPacket::StateBundleDestroy out;
            out.Init(packet);
            SendPacket(*i, &out);
        }
    }

    void    StateWorld::OnEvent(StateBundleId packet, const SubscriptionList& subscription, const char eventString[])
    {
        for (auto i=subscription.GetSubscribers().cbegin(); i!=subscription.GetSubscribers().cend(); ++i) {
            ContactPacket::StateBundleEvent out;
            out.Init(packet, eventString);
            SendPacket(*i, &out);
        }
    }

    MeshNodeId  StateWorld::LocalMeshNode() const   { return _localMeshNode; }
    MeshNodeId  StateWorld::GetTimeNow() const      { return LocalMeshNode::GetInstance().GetNetworkTime(); }

    void    StateWorld::ReceiveBundle   (StateBundleId id, const void* data, size_t size, StateBundleType type, 
                                         uint64 authority, NetworkTime updateTime)
    {
            //
            //      Try to find this bundle in our _nonAuthoriativePackets
            //      If it's already there -- then we can easily get at it.
            //      
        auto i = _nonAuthoriativePackets.find(id);
        if (i!=_nonAuthoriativePackets.end()) {
            i->second->UpdateAuthority(authority);
            i->second->UpdateData(data, size, updateTime);
        } else {
            auto newPacket = std::make_shared<StateBundle>(id, size, type);
            newPacket->UpdateBroadcaster(this);
            newPacket->UpdateAuthority(authority);
            newPacket->UpdateData(data, size, updateTime);
            _nonAuthoriativePackets.insert(std::make_pair(id, newPacket));

                //  
                //  hack --     simple callback for now
                //              We normally need to do something when
                //              a new bundle is first seen. Typically, this might
                //              involve creating a character, or at least attaching
                //              the data to an existing character
                //
            OnStateBundleCreate(newPacket);
        }
    }

    void    StateWorld::DestroyBundle   (StateBundleId bundle, MeshNodeId authority)
    {
        auto i = _nonAuthoriativePackets.find(bundle);
        if (i!=_nonAuthoriativePackets.end()) {
            _nonAuthoriativePackets.erase(i);
        }
    }

    void    StateWorld::ReceiveEvent    (StateBundleId bundle, const char eventString[], MeshNodeId authority)
    {
        OnStateBundleEvent(bundle, eventString);
    }

    void    StateWorld::Update()
    {
        const unsigned countdownStart = 60;

        static unsigned subscriptionCountdown = countdownStart;
        if (subscriptionCountdown == 0) {

                //
                //      Make list of subscription suggestions for each observer
                //
            for (auto i=_observers.begin(); i!=_observers.end(); ++i) {
                auto suggestions = FindSubscriptionSuggestions(Float3(0.f, 0.f, 0.f), 0, *i);
                if (!suggestions.empty()) {
                    ContactPacket::SubscriptionSuggestions out;
                    out.Init(AsPointer(suggestions.begin()), suggestions.size());
                    SendPacket(*i, &out);
                }
            }

            subscriptionCountdown = countdownStart-1;
        } else {
            --subscriptionCountdown;
        }

        static unsigned observerCountdown = countdownStart;
        if (observerCountdown == 0) {

            ContactPacket::MoveObserver out;
            out.Init(_localObserverPosition, _localObserverArea);
            LocalMeshNode::GetInstance().Broadcast(&out);

            observerCountdown = countdownStart-1;
        } else {
            --observerCountdown;
        }
    }

    void    StateWorld::MoveObserver    (const Float3& position, VisibilityArea observerArea, MeshNodeId observer)
    {
        if (observer == _localMeshNode) {
            _localObserverPosition  = position;
            _localObserverArea      = observerArea;
        } else {
            auto i=std::lower_bound(_observers.begin(), _observers.end(), observer);
            if (i==_observers.end() || *i != observer) {
                _observers.insert(i, observer);
            }
        }
    }

    StateWorld*  StateWorld::_instance = nullptr;

    StateWorld::StateWorld(MeshNodeId localMeshNode)
    : _localMeshNode(localMeshNode)
    {
        assert(!_instance);
        _instance = this;
        _localObserverPosition = Float3(0.f, 0.f, 0.f);
        _localObserverArea = 0;
    }

    StateWorld::~StateWorld() 
    {
        assert(_instance==this);
        _instance = nullptr;
    }

}}}



