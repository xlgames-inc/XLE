// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <string>
typedef std::string string;

#include "../xlnet/net.h"
#include "../xlnet/p_base.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Math/Vector.h"
#include <memory>

namespace PlatformRig { namespace Network
{
    typedef uint64      MeshNodeId;
    typedef uint64      NetworkTime;
    typedef uint64      StateBundleId;
    typedef uint64      StateBundleType;
    typedef unsigned    VisibilityArea;

    template <int EnumValue, typename EnumType>
        class BasicPacket : public XlNetPacket
    {
    public:
        EnumType        type;
        BasicPacket() : type(EnumType(EnumValue))           {}
        void            SerializeType(XlNetSerializer& ser) { ser.EnumValue("type", type, 'ui16'); }
    };

    namespace ContactPacket
    {
        namespace Type
        {
            enum Enum
            {
                TestPacket,
                SendMeshNodeId,

                StateBundleUpdate,
                StateBundleDestroy,
                StateBundleEvent,

                RequestSubscribe,
                RequestUnsubscribe,
                SubscriptionSuggestions,

                MoveObserver,

                Max
            };
        }

        class TestPacket : public BasicPacket<  ContactPacket::Type::TestPacket, 
                                                ContactPacket::Type::Enum>
        {
        public:
            void    SerializeBody(XlNetSerializer& ser);
        };

        class SendMeshNodeId : public BasicPacket<  ContactPacket::Type::SendMeshNodeId, 
                                                ContactPacket::Type::Enum>
        {
        public:
            void    SerializeBody(XlNetSerializer& ser);
            void    Init(MeshNodeId meshNodeId) { _meshNodeId = meshNodeId; }

            MeshNodeId      _meshNodeId;
        };

        class StateBundleUpdate : public BasicPacket<   ContactPacket::Type::StateBundleUpdate, 
                                                        ContactPacket::Type::Enum>
        {
        public:
            void    SerializeBody(XlNetSerializer& ser);
            void    Init(StateBundleId id, const void* data, size_t size, StateBundleType type, NetworkTime time);

            StateBundleId       _stateBundle;
            size_t              _size;
            StateBundleType     _type;
            NetworkTime         _time;
            uint8               _buffer[2048];
        };

        class StateBundleDestroy : public BasicPacket<ContactPacket::Type::StateBundleDestroy, ContactPacket::Type::Enum>
        {
        public:
            void    SerializeBody(XlNetSerializer& ser);
            void    Init(StateBundleId id) { _stateBundle = id; }

            StateBundleId       _stateBundle;
        };

        class StateBundleEvent : public BasicPacket<ContactPacket::Type::StateBundleEvent, ContactPacket::Type::Enum>
        {
        public:
            void    SerializeBody(XlNetSerializer& ser);
            void    Init(StateBundleId id, const char eventString[]);

            StateBundleId       _stateBundle;
            size_t              _size;
            char                _buffer[256];
        };

        class RequestSubscribe : public BasicPacket<ContactPacket::Type::RequestSubscribe, ContactPacket::Type::Enum>
        {
        public:
            void    SerializeBody(XlNetSerializer& ser);
            void    Init(const StateBundleId stateBundles[], size_t count);

            size_t              _count;
            StateBundleId       _stateBundles[256];
        };
        
        class SubscriptionSuggestions : public BasicPacket<ContactPacket::Type::SubscriptionSuggestions, ContactPacket::Type::Enum>
        {
        public:
            void    SerializeBody(XlNetSerializer& ser);
            void    Init(const StateBundleId stateBundles[], size_t count);

            size_t              _count;
            StateBundleId       _stateBundles[256];
        };

        class MoveObserver : public BasicPacket<ContactPacket::Type::MoveObserver, ContactPacket::Type::Enum>
        {
        public:
            void    SerializeBody(XlNetSerializer& ser);
            void    Init(const Float3& newPosition, VisibilityArea observerArea);

            Float3          _newPosition;
            VisibilityArea  _observerArea;
        };
    }

    class ContactSession
    {
    public:
        ContactSession(MeshNodeId localMeshNodeId);
        ~ContactSession();

        MeshNodeId      GetRemoteMeshNodeId() const     { return _remoteMeshNodeId; }
        MeshNodeId      GetLocalMeshNodeId() const      { return _localMeshNodeId; }
        void            SetRemoteMeshNodeId(MeshNodeId newId);

        void            OnDisconnect();
        
        void            AttachTransport(XlConnection* connection);
        XlConnection*   GetTransport() { return _transport.get(); }

        int             HandlePacket(IoBuffer& buffer);
        typedef         PacketHandler<  ContactSession, ContactPacket::Type::Enum, 
                                        ContactPacket::Type::Max>
                        HandlerImpl;

    protected:
        static HandlerImpl          _impl;
        MeshNodeId                  _remoteMeshNodeId;
        MeshNodeId                  _localMeshNodeId;
        intrusive_ptr<XlConnection>    _transport;
    };

    class MeshContact
    {
    public:
        NetAddr         _remoteAddress;
        std::string     _name;
        std::shared_ptr<ContactSession> _session;
    };

    class MeshContactList : noncopyable
    {
    public:
        std::shared_ptr<ContactSession> AddContact(const NetAddr& remoteAddr);
        ContactSession*                 FindContact(MeshNodeId);
        
        size_t          GetContactCount() const;
        ContactSession* GetContact(size_t index);

        MeshContactList(MeshNodeId localMeshNodeId);
        ~MeshContactList();
    private:
        Threading::Mutex            _contactsLock;
        std::vector<MeshContact>    _contacts;
        MeshNodeId                  _localMeshNodeId;
    };

    namespace ConnectionServer
    {
        class Listener : public XlListener
        {
        public:
            Listener(MeshContactList& contactList, const ListenerConfig& config);

        protected:
            virtual void            OnDisconnect(ConnCloser closer, const char* desc);
            virtual void            OnListen();
            virtual XlConnection*   OnAccept(int sock, const NetAddr& localAddr, const NetAddr& remoteAddr);

            MeshContactList*        _contactList;
        };
    }

    namespace TwoWayConnectors
    {
        class Connection : public XlConnection
        {
        public:
            Connection(std::shared_ptr<ContactSession> session);

        protected:
            virtual void        OnConnect();
            virtual void        OnDisconnect(ConnCloser closer, const char* desc);
            virtual int         OnRecv();
            virtual void        OnSend();

            std::shared_ptr<ContactSession>     _session;
        };

        class Connector : public XlConnector
        {
        public:
            Connector(  std::shared_ptr<ContactSession> session, 
                        int recvBufferSize, int sendBufferSize);

        protected:
            virtual void    OnConnect();
            virtual void    OnDisconnect(ConnCloser closer, const char* desc);
            virtual int     OnRecv();
            virtual void    OnSend();
    
            std::shared_ptr<ContactSession>     _session;
        };
    }

    class LocalMeshNode
    {
    public:
        MeshNodeId              GetMeshNodeId() const   { return _meshNodeId; }
        NetworkTime             GetNetworkTime() const  { return Microsecond_Now(); }

        XlConnection*           FindConnection(MeshNodeId);
        void                    Broadcast(XlNetPacket* packet);

        static LocalMeshNode&   GetInstance()           { return *_instance; }
        LocalMeshNode();
        ~LocalMeshNode();
    private:
        std::unique_ptr<MeshContactList>    _contactList;
        XlHandle                            _globalSemaphore;
        MeshNodeId                          _meshNodeId;

        intrusive_ptr<TwoWayConnectors::Connector>    _connectorToExistingServer;
        intrusive_ptr<ConnectionServer::Listener>     _connectionListener;

        static LocalMeshNode*               _instance;
    };

    static uint64 GenerateGuid64()
    {
            //      
            //      Just generate some random number
            //      This should ideally use a better
            //      method to try to minimize the chances of a
            //      collision as much as possible!
            //
        uint64 result = 0;
        for (unsigned c=0; c<=(64/15); ++c) {
            result |= uint64(rand())<<uint64(c*15);
        }
        return result;
    }

}}



