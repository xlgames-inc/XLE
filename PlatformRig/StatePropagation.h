// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "MeshNode.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Math/Vector.h"
#include "../Core/Types.h"
#include <map>

namespace PlatformRig { namespace Network { namespace StatePropagation
{
        ////////////////////////////////////////////////////////////////////////

    class VisibilityObject
    {
    public:
        bool    VisibleToObserver(const Float3& observerPosition, VisibilityArea observerArea) const;

        VisibilityObject();
        VisibilityObject(const std::pair<Float3, Float3>& position, VisibilityArea area);
    private:
        std::pair<Float3, Float3>   _transpondPosition;
        VisibilityArea              _transpondVisibilityArea;
        bool                        _active;
    };

        ////////////////////////////////////////////////////////////////////////

    class SubscriptionList
    {
    public:
        bool    HasSubscriber(MeshNodeId) const;
        void    AddSubscriber(MeshNodeId);
        bool    RemoveSubscriber(MeshNodeId);

        const std::vector<MeshNodeId>&      GetSubscribers() const  { return _subscribers; }
    private:
        std::vector<MeshNodeId>     _subscribers;
    };

        ////////////////////////////////////////////////////////////////////////

    class IStatePacketBroadcaster
    {
    public:
        virtual void            OnUpdate(   StateBundleId packet, const SubscriptionList& subscription, 
                                            const void* data, size_t size, StateBundleType packetType, 
                                            NetworkTime time) = 0;
        virtual void            OnDestroy(  StateBundleId packet, const SubscriptionList& subscription) = 0;
        virtual void            OnEvent(    StateBundleId packet, const SubscriptionList& subscription, 
                                            const char eventString[]) = 0;
        virtual MeshNodeId      LocalMeshNode() const = 0;
        virtual NetworkTime     GetTimeNow() const = 0;
    };

        ////////////////////////////////////////////////////////////////////////

    class StateBundle : noncopyable
    {
    public:
        struct State
        {
            enum Enum
            {
                Active,
                Disconnected,
                Unknown
            };
        };

        class Data : noncopyable
        {
        public:
            const void*     Get() const         { return _data; }
            NetworkTime     GetTime() const     { return _time; }
            size_t          GetSize() const     { return _size; }

            Data(   Threading::ReadWriteMutex& mutex, 
                    const void* data, size_t size, NetworkTime time);
        private:
            const void*                             _data;
            NetworkTime                             _time;
            size_t                                  _size;
            Threading::ReadWriteMutex::scoped_lock  _lock;
        };

        State::Enum                 GetState() const                { return State::Active; }
        MeshNodeId                  GetAuthority() const            { return _authority; }
        StateBundleType             GetType() const                 { return _bundleType; }
        StateBundleId               GetId() const                   { return _packetId; }

        Data                        LockData() const;
        void                        UpdateData(const void* newData, size_t newDataSize, NetworkTime time=0);
        void                        UpdateVisibility(const VisibilityObject& visibility);
        void                        UpdateBroadcaster(IStatePacketBroadcaster* broadcaster);
        void                        UpdateAuthority(MeshNodeId authority);
        void                        Event(const char eventString[]);

        const VisibilityObject&     GetVisibility() const           { return _visibilityObject; }
        const SubscriptionList&     GetSubscriptionList() const     { return _subscriptionList; }
        SubscriptionList&           GetSubscriptionList()           { return _subscriptionList; }

        StateBundle(StateBundleId id, size_t size, StateBundleType type);

    private:
        mutable Threading::ReadWriteMutex   _dataLock;
        mutable Threading::Mutex            _prevDataLock;

        std::vector<uint8>          _data;
        std::vector<uint8>          _prevData;

        StateBundleId               _packetId;
        MeshNodeId                  _authority;
        NetworkTime                 _currentDataTime, _prevDataTime;
        VisibilityObject            _visibilityObject;
        StateBundleType             _bundleType;

        IStatePacketBroadcaster*    _broadcaster;       // (broadcaster might own this object, so can't keep a reference)
        SubscriptionList            _subscriptionList;
    };

        ////////////////////////////////////////////////////////////////////////

    class StateWorld : IStatePacketBroadcaster
    {
    public:
        void    Subscribe       (MeshNodeId meshNode, StateBundleId statePacket);
        void    Unsubscribe     (MeshNodeId meshNode, StateBundleId statePacket);
        void    UnsubscribeAll  (MeshNodeId meshNode);

        void    ReceiveBundle   (StateBundleId id, const void* data, size_t size, StateBundleType type, 
                                 uint64 authority, NetworkTime updateTime);
        void    DestroyBundle   (StateBundleId bundle, MeshNodeId authority);
        void    ReceiveEvent    (StateBundleId bundle, const char eventString[], MeshNodeId authority);

        void    Update();
        void    MoveObserver    (const Float3& position, VisibilityArea observerArea, MeshNodeId observer);

        std::shared_ptr<StateBundle>    CreateAuthoritativePacket(size_t size, StateBundleType type);

        static StateWorld&              GetInstance() { return *_instance; }

        StateWorld(MeshNodeId localMeshNode);
        ~StateWorld();

    protected:
        std::map<StateBundleId, std::shared_ptr<StateBundle>>       _authoriativePackets;
        std::map<StateBundleId, std::shared_ptr<StateBundle>>       _nonAuthoriativePackets;

        std::vector<MeshNodeId>     _observers;
        MeshNodeId                  _localMeshNode;
        Float3                      _localObserverPosition;
        VisibilityArea              _localObserverArea;

        void            OnUpdate(   StateBundleId packet, const SubscriptionList& subscription, 
                                    const void* data, size_t size, StateBundleType packetType, 
                                    NetworkTime time);
        void            OnUpdate(   StateBundleId packet, MeshNodeId singleDestination, 
                                    const void* data, size_t size, StateBundleType packetType, 
                                    NetworkTime time);
        void            OnDestroy(  StateBundleId packet, const SubscriptionList& subscription);
        void            OnEvent(    StateBundleId packet, const SubscriptionList& subscription,
                                    const char eventString[]);
        MeshNodeId      LocalMeshNode() const;
        NetworkTime     GetTimeNow() const;

        std::vector<StateBundleId>      FindSubscriptionSuggestions(
            const Float3& observerPosition, VisibilityArea observerArea, MeshNodeId observer);

        static StateWorld*          _instance;
    };

        ////////////////////////////////////////////////////////////////////////

}}}


