// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MeshNode.h"
#include "StatePropagation.h"       // todo -- work on the physical design here!
#include "../xlnet/xlnet.h"
#include "../xlnet/io_buffer.h"
#include "../ConsoleRig/Log.h"
#include "../Core/Exceptions.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StringUtils.h"
#include <functional>

namespace PlatformRig { namespace Network
{

        /// ///      /// ///      /// ///      /// ///      /// ///      /// ///

    ContactSession::HandlerImpl  ContactSession::_impl;

    int ContactSession::HandlePacket(IoBuffer& buffer)
    {
        int packets = 0;
        PacketHeader header;
        while (IsPacketAvailable(&header, &buffer)) {
            ++packets;
            XlNetInputSerializer<IoBuffer> ser(&buffer);
            if (!_impl.Process(ser, this)) {
                // Disconnect(CC_US_PACKET_HANDLER, "OnRecv");
                return packets;
            }
        }
        return packets;
    }


    

    namespace ContactPacket
    {
        void    TestPacket::SerializeBody(XlNetSerializer& ser) {}

        void    SendMeshNodeId::SerializeBody(XlNetSerializer& ser)
        {
            ser.Value("MeshNodeId", _meshNodeId);
        }

        void    StateBundleUpdate::SerializeBody(XlNetSerializer& ser)
        {
            ser.Value("Bundle", _stateBundle);
            ser.Value("Size", _size);
            ser.Value("BundleType", _type);
            ser.Value("Time", _time);
            if (ser.IsWriting()) {
                ser.WriteBinaryArray("Data", (const char*)_buffer, (int)std::min(dimof(_buffer),_size));
            } else {
                ser.ReadBinaryArray("Data", (char*)_buffer, (int)std::min(dimof(_buffer),_size));
            }
        }

        void    StateBundleUpdate::Init(StateBundleId id, const void* data, 
                                        size_t size, StateBundleType type, NetworkTime time)
        {
            _stateBundle = id;
            XlCopyMemory(_buffer, data, std::min(dimof(_buffer), size));
            _size = size;
            _type = type;
            _time = time;
        }

        void    StateBundleDestroy::SerializeBody(XlNetSerializer& ser)
        {
            ser.Value("Bundle", _stateBundle);
        }

        void    StateBundleEvent::SerializeBody(XlNetSerializer& ser)
        {
            ser.Value("Bundle", _stateBundle);
            ser.Value("Size", _size);
            if (ser.IsWriting()) {
                ser.WriteBinaryArray("Data", _buffer, (int)std::min(dimof(_buffer),_size));
            } else {
                ser.ReadBinaryArray("Data", _buffer, (int)std::min(dimof(_buffer),_size));
                _buffer[std::min(dimof(_buffer)-1,_size)] = '\0';
            }
        }
        void    StateBundleEvent::Init(StateBundleId id, const char eventString[])
        {
            _stateBundle = id;
            _size = std::min((size_t)XlStringLen(eventString), dimof(_buffer)-1);
            XlCopyMemory(_buffer, eventString, _size);
            _buffer[_size] = '\0';
        }

        void    RequestSubscribe::SerializeBody(XlNetSerializer& ser)
        {
            ser.Value("Count", _count);
            _count = std::min(dimof(_stateBundles), size_t(_count));
            if (ser.IsWriting()) {
                ser.WriteBinaryArray("Data", (const char*)_stateBundles, int(sizeof(StateBundleId)*_count));
            } else {
                ser.ReadBinaryArray("Data", (char*)_stateBundles, int(sizeof(StateBundleId)*_count));
            }
        }

        void    RequestSubscribe::Init(const StateBundleId stateBundles[], size_t count)
        {
            _count = std::min(dimof(_stateBundles), size_t(count));
            XlCopyMemory(_stateBundles, stateBundles, count*sizeof(StateBundleId));
        }

        void    SubscriptionSuggestions::SerializeBody(XlNetSerializer& ser)
        {
            ser.Value("Count", _count);
            _count = std::min(dimof(_stateBundles), size_t(_count));
            if (ser.IsWriting()) {
                ser.WriteBinaryArray("Data", (const char*)_stateBundles, int(sizeof(StateBundleId)*_count));
            } else {
                ser.ReadBinaryArray("Data", (char*)_stateBundles, int(sizeof(StateBundleId)*_count));
            }
        }

        void    SubscriptionSuggestions::Init(const StateBundleId stateBundles[], size_t count)
        {
            _count = std::min(dimof(_stateBundles), size_t(count));
            XlCopyMemory(_stateBundles, stateBundles, count*sizeof(StateBundleId));
        }

        void    MoveObserver::SerializeBody(XlNetSerializer& ser)
        {
            ser.Value("Position", _newPosition[0]);
            ser.Value("Position", _newPosition[1]);
            ser.Value("Position", _newPosition[2]);
            ser.Value("Area", _observerArea);
        }

        void    MoveObserver::Init(const Float3& newPosition, VisibilityArea observerArea)
        {
            _newPosition = newPosition;
            _observerArea = observerArea;
        }
    }
    
    static bool OnTestPacket(ContactSession* session, const ContactPacket::TestPacket& packet) 
    {
        LogAlwaysInfoF("Received test packet!");
        return true;
    }

    static bool OnSendMeshNodeId(ContactSession* session, const ContactPacket::SendMeshNodeId& packet)
    {
        session->SetRemoteMeshNodeId(packet._meshNodeId);
        return true;
    }

    static bool OnStateBundleUpdate(ContactSession* session, const ContactPacket::StateBundleUpdate& packet)
    {
        StatePropagation::StateWorld::GetInstance().ReceiveBundle(
            packet._stateBundle, packet._buffer, packet._size, packet._type,
            session->GetRemoteMeshNodeId(), packet._time);
        return true;
    }

    static bool OnStateBundleDestroy(ContactSession* session, const ContactPacket::StateBundleDestroy& packet)
    {
        StatePropagation::StateWorld::GetInstance().DestroyBundle(
            packet._stateBundle, session->GetRemoteMeshNodeId());
        return true;
    }

    static bool OnStateBundleEvent(ContactSession* session, const ContactPacket::StateBundleEvent& packet)
    {
        StatePropagation::StateWorld::GetInstance().ReceiveEvent(
            packet._stateBundle, packet._buffer, session->GetRemoteMeshNodeId());
        return true;
    }

    static bool OnRequestSubscribe(ContactSession* session, const ContactPacket::RequestSubscribe& packet)
    {
        MeshNodeId sourceNode = session->GetRemoteMeshNodeId();
        auto& world = StatePropagation::StateWorld::GetInstance();
        for (size_t c=0; c<packet._count; ++c) {
            world.Subscribe(sourceNode, packet._stateBundles[c]);
        }
        return true;
    }
    
    static bool OnSubscriptionSuggestions(ContactSession* session, const ContactPacket::SubscriptionSuggestions& packet)
    {
        ContactPacket::RequestSubscribe out;
        out.Init(packet._stateBundles, packet._count);
        session->GetTransport()->SendPacket(&out);
        return true;
    }

    static bool OnMoveObserver(ContactSession* session, const ContactPacket::MoveObserver& packet)
    {
        StatePropagation::StateWorld::GetInstance().MoveObserver(
            packet._newPosition, packet._observerArea, session->GetRemoteMeshNodeId());
        return true;
    }

    void RegisterPacketHandlers(ContactSession::HandlerImpl& impl)
    {
        impl.Register(&OnTestPacket);
        impl.Register(&OnSendMeshNodeId);
        impl.Register(&OnStateBundleUpdate);
        impl.Register(&OnStateBundleDestroy);
        impl.Register(&OnStateBundleEvent);
        impl.Register(&OnRequestSubscribe);
        impl.Register(&OnSubscriptionSuggestions);
        impl.Register(&OnMoveObserver);
    }

    void        ContactSession::SetRemoteMeshNodeId(MeshNodeId newId)
    {
        _remoteMeshNodeId = newId;
    }

    void        ContactSession::OnDisconnect()
    {
                //      We got a disconnect... So make sure we unsubscribe from
                //      everything!
        StatePropagation::StateWorld::GetInstance().UnsubscribeAll(_remoteMeshNodeId);
    }

    void            ContactSession::AttachTransport(XlConnection* connection)
    {
        _transport = connection;
    }

    ContactSession::ContactSession(MeshNodeId localMeshNodeId) : _localMeshNodeId(localMeshNodeId), _remoteMeshNodeId(0), _transport(nullptr) {}
    ContactSession::~ContactSession() {}
    


        /// ///      /// ///      /// ///      /// ///      /// ///      /// ///

    std::shared_ptr<ContactSession> MeshContactList::AddContact(const NetAddr& remoteAddr)
    {
        MeshContact newContact;
        newContact._remoteAddress = remoteAddr;
        newContact._name = "Mesh Contact";
        newContact._session = std::make_shared<ContactSession>(_localMeshNodeId);
        {
            Threading::Mutex::scoped_lock lock(_contactsLock);
            _contacts.push_back(newContact);
        }
        return std::move(newContact._session);
    }

    ContactSession* MeshContactList::FindContact(MeshNodeId nodeId)
    {
        for (auto i=_contacts.begin(); i!=_contacts.end(); ++i) {
            if (i->_session->GetRemoteMeshNodeId() == nodeId) {
                return i->_session.get();
            }
        }
        return nullptr;
    }

    size_t          MeshContactList::GetContactCount() const
    {
        return _contacts.size();
    }

    ContactSession* MeshContactList::GetContact(size_t index)
    {
        assert(index < _contacts.size());
        return _contacts[index]._session.get();
    }

    MeshContactList::MeshContactList(MeshNodeId localMeshNodeId)
    : _localMeshNodeId(localMeshNodeId)
    {}

    MeshContactList::~MeshContactList()
    {}



        /// ///      /// ///      /// ///      /// ///      /// ///      /// ///

    namespace ConnectionServer
    {
        Listener::Listener(MeshContactList& contactList, const ListenerConfig& config)
        :   XlListener(config)
        ,   _contactList(&contactList)
        {
        }

        void            Listener::OnDisconnect(ConnCloser closer, const char* desc)
        {}

        void            Listener::OnListen()
        {}

        XlConnection*   Listener::OnAccept(int sock,  const NetAddr& localAddr, 
                                                                const NetAddr& remoteAddr)
        {
            return new TwoWayConnectors::Connection(_contactList->AddContact(remoteAddr));
        }
    }


    namespace TwoWayConnectors
    {

            /// ///      /// ///      /// ///      /// ///      /// ///      /// ///

        Connection::Connection(std::shared_ptr<ContactSession> session)
        : _session(session)
        {
            _session->AttachTransport(this);
        }

        void    Connection::OnSend()        {}
        void    Connection::OnConnect()     
        {
            ContactPacket::TestPacket out;
            SendPacket(&out);

            ContactPacket::SendMeshNodeId sendId;
            sendId.Init(_session->GetLocalMeshNodeId());
            SendPacket(&sendId);
        }
        void    Connection::OnDisconnect(ConnCloser closer, const char* desc)   
        {
            _session->OnDisconnect();
        }
        int     Connection::OnRecv()
        {
            return _session->HandlePacket(*_recvBuffer);
        }

            /// ///      /// ///      /// ///      /// ///      /// ///      /// ///

        Connector::Connector(   std::shared_ptr<ContactSession> session, 
                                int recvBufferSize, int sendBufferSize)
        :   XlConnector(recvBufferSize, sendBufferSize)
        ,   _session(session)
        {
            _session->AttachTransport(this);
        }

        void    Connector::OnSend()         {}
        void    Connector::OnConnect()      
        {
            ContactPacket::TestPacket out;
            SendPacket(&out);

            ContactPacket::SendMeshNodeId sendId;
            sendId.Init(_session->GetLocalMeshNodeId());
            SendPacket(&sendId);
        }
        void    Connector::OnDisconnect(ConnCloser closer, const char* desc)    
        {
            _session->OnDisconnect();
        }
        int     Connector::OnRecv() 
        {
            return _session->HandlePacket(*_recvBuffer);
        }

            /// ///      /// ///      /// ///      /// ///      /// ///      /// ///

    }



        /// ///      /// ///      /// ///      /// ///      /// ///      /// ///

    XlConnection*           LocalMeshNode::FindConnection(MeshNodeId meshNode)
    {
        auto i = _contactList->FindContact(meshNode);
        if (i) {
            return i->GetTransport();
        }
        return nullptr;
    }

    void                    LocalMeshNode::Broadcast(XlNetPacket* packet)
    {
        for (size_t c=0; c<_contactList->GetContactCount(); ++c) {
            auto contact = _contactList->GetContact(c);
            contact->GetTransport()->SendPacket(packet);
        }
    }
    
    static const char GlobalSemaphoreName[] = "Global\\MeshNodeConnectionServer";
    LocalMeshNode*      LocalMeshNode::_instance = nullptr;

    LocalMeshNode::LocalMeshNode()
    {
        intrusive_ptr<TwoWayConnectors::Connector>    connectorToExistingServer;
        intrusive_ptr<ConnectionServer::Listener>     connectionListener;
        XlHandle globalSemaphore = INVALID_HANDLE_VALUE;

        _meshNodeId = GenerateGuid64();
        auto contactList = std::make_unique<MeshContactList>(_meshNodeId);

            //      First; check if any existing copies of this application already exist...
        globalSemaphore = CreateSemaphore(nullptr, 1, 1, GlobalSemaphoreName);
        bool connectionServerAlreadyThere = false;
        if (WaitForSingleObject(globalSemaphore, 0) == WAIT_TIMEOUT) {
            connectionServerAlreadyThere = true;
        }

        TRY {

            const char configFilename[] = "network.cfg";
            Data config;
            if (!CommonProcServerConfig(configFilename, &config)) {
                Throw(Exceptions::BasicLabel("Failed loading network.cfg"));
            }

            ListenerConfig connectionListenerConfig;
            const bool loadSuccess = connectionListenerConfig.Load(config.ChildWithValue("connection_listener"));
            if (!loadSuccess) {
                Throw(Exceptions::BasicLabel("Failed reading connection_listener part of network.cfg"));
            }

            if (connectionServerAlreadyThere) {

                    //
                    //      Attempt to connect to a server that is already running!
                    //          We need to create the "session" object first
                    //
                    //      This is not very robust, because we don't know for sure
                    //      that the server we're connecting to will respond.
                    //
                    //      But we need some "first point of contact" in order to get
                    //      the list of all the other nodes in the network.
                    //
                auto session = contactList->AddContact(connectionListenerConfig.GetBindAddr());

                connectorToExistingServer = intrusive_ptr<TwoWayConnectors::Connector>(
                    new TwoWayConnectors::Connector(
                        session, connectionListenerConfig.GetRecvBufferSize(), connectionListenerConfig.GetSendBufferSize()));
                GetNet()->Connect(connectionListenerConfig.GetBindAddr(), connectorToExistingServer.get());

            } else {

                    //
                    //  Launch a new connection server, and listen for everything out there
                    //
                connectionListener = intrusive_ptr<ConnectionServer::Listener>(new ConnectionServer::Listener(*contactList, connectionListenerConfig));
                GetNet()->Listen(connectionListenerConfig.GetBindAddr(), connectionListener.get());


                    // DavidJ -- hack... This is leaking. Compensating for AddRef() in XlNet::Accept here..
                connectionListener->Release();

            }

        } CATCH(...) {

                //
                //      No smart pointer like behaviour for this semaphore object...
                //          so we have to clean things up manually
                //
            if (globalSemaphore!=INVALID_HANDLE_VALUE) {
                ReleaseSemaphore(globalSemaphore, 1, nullptr);
                CloseHandle(globalSemaphore);
            }
            RETHROW;

        } CATCH_END

        _contactList = std::move(contactList);
        _connectionListener = std::move(connectionListener);
        _connectorToExistingServer = std::move(connectorToExistingServer);
        _globalSemaphore = globalSemaphore;

        assert(!_instance);
        _instance = this;
    }

    LocalMeshNode::~LocalMeshNode()
    {
        assert(_instance == this);
        _instance = nullptr;

        ReleaseSemaphore(_globalSemaphore, 1, nullptr);
        CloseHandle(_globalSemaphore);
    }

}}




