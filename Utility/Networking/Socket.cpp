// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Socket.h"

#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <exception>

#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
#endif

#include "../../ConsoleRig/Log.h"
#include "../../Utility/SystemUtils.h"
#include "../../Core/Exceptions.h"

namespace
{
    #if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
        // https://stackoverflow.com/a/20817001
        // TODO: move to apportable?
        static int inet_pton(int af, const char *src, void *dst)
        {
            struct sockaddr_storage ss;
            int size = sizeof(ss);
            char src_copy[INET6_ADDRSTRLEN+1];

            ZeroMemory(&ss, sizeof(ss));
            /* stupid non-const API */
            strncpy (src_copy, src, INET6_ADDRSTRLEN+1);
            src_copy[INET6_ADDRSTRLEN] = 0;

            if (WSAStringToAddress(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
                switch(af) {
                    case AF_INET:
                        *(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
                        return 1;
                    case AF_INET6:
                        *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
                        return 1;
                }
            }
            return 0;
        }
    #endif

    struct sockaddr_in CreateSocketAddress(const char *address, const uint16_t port)
    {
        struct sockaddr_in serverAddr;
        memset(&serverAddr, '0', sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        // Convert IPv4 and IPv6 addresses from text to binary form
        if (inet_pton(AF_INET, address, &serverAddr.sin_addr) <= 0) {
            Throw(Networking::SocketException(Networking::SocketException::ErrorCode::invalid_address));
        }
        return serverAddr;
    }

    void EnsureNetworkingInitialized()
    {
        #if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
            static bool s_WSAInitialized = false;
            if (!s_WSAInitialized) {
                WSADATA wsaData;
                int error = WSAStartup(MAKEWORD(2,2), &wsaData);
                if (error != 0) {
                    Throw(Networking::SocketException(Networking::SocketException::ErrorCode::bad_creation));
                }
                s_WSAInitialized = true;
            }
        #endif
    }
}


namespace Utility { namespace Networking
{
    SocketConnection::SocketConnection(const std::string &address, const uint16_t port)
    {
        EnsureNetworkingInitialized();

        struct sockaddr_in serverAddr = CreateSocketAddress(address.c_str(), port);

        if ((_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            Throw(SocketException(SocketException::ErrorCode::bad_creation));
        }

        if (connect(_fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            close(_fd);
            Throw(SocketException(SocketException::ErrorCode::bad_connection));
        }
    }

    SocketConnection::SocketConnection(int socket) : _fd(socket) {}

    SocketConnection::~SocketConnection() {
        close(_fd);
    }

    bytes SocketConnection::Read(const uint32_t count)
    {
        constexpr unsigned int blockSize {64 * 1024}; // 64k buffer
        char buffer[blockSize];

        uint32_t unread = count;
        bytes data(unread);
        char* iter = data.data();

        while (unread > 0) {
            uint32_t curBlockSize = std::min(unread, blockSize);
            long numOfRead = recv(_fd, buffer, curBlockSize, 0);
            if (numOfRead <= 0) {
                Throw(SocketException(SocketException::ErrorCode::disconnected));
            }

            std::copy(buffer, buffer + numOfRead, iter);
            unread -= numOfRead;
            iter += numOfRead;
            if (numOfRead == 0 && unread != 0) Throw(SocketException(SocketException::ErrorCode::incomplete));
        }

        return data;
    }

    void SocketConnection::Write(IteratorRange<const void*> data) const
    {
        int flags = 0;
        #ifdef MSG_NOSIGNAL
            flags |= MSG_NOSIGNAL;
        #endif
        send(_fd, (const char *)data.begin(), data.size(), flags);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    SocketServer::SocketServer(uint16_t port) : _port(port)
    {
        EnsureNetworkingInitialized();

        struct sockaddr_in serverAddr = CreateSocketAddress("127.0.0.1", port);

        if ((_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            Throw(SocketException(SocketException::ErrorCode::bad_creation));
        }

        { // socket flags
            int enable = 1;

            // When connections are interrupted, the addres:port pair might noy be available to be used right away.
            // This flag allows us to create another socket with the same address right away.
            if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(enable)) < 0) {
                close(_fd);
                Throw(SocketException(SocketException::ErrorCode::bad_creation));
            }

            #if PLATFORMOS_TARGET != PLATFORMOS_WINDOWS
                // SIGPIPE will be raised when connections are ended by the client. Apple suggests a couple of different
                // ways to avoid a crash, but I found out that only this solution worked. The global ignore alternative
                // worked well on desktop, however. To learn more, search for SIGPIPE in:
                // https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/NetworkingOverview/CommonPitfalls/CommonPitfalls.html#//apple_ref/doc/uid/TP40010220-CH4-SW11
                if (setsockopt(_fd, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable)) < 0) {
                    close(_fd);
                    Throw(SocketException(SocketException::ErrorCode::bad_creation));
                }
            #endif
        }

        if (bind(_fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            close(_fd);
            Throw(SocketException(SocketException::ErrorCode::bad_creation));
        }
        if (listen(_fd, 5) < 0) {
            close(_fd);
            Throw(SocketException(SocketException::ErrorCode::bad_creation));
        }
    }

    SocketServer::~SocketServer()
    {
        close(_fd);
    }

    std::unique_ptr<SocketConnection> SocketServer::Listen()
    {
        Log(Debug) << "Waiting for client connection on port " << _port << "..." << std::endl;
        struct sockaddr_in clientAddr;
        socklen_t clilen = sizeof(clientAddr);
        int acceptedFD = accept(_fd, (struct sockaddr *)&clientAddr, &clilen);
        if (acceptedFD < 0) {
            return nullptr;
        }
        Log(Debug) << "Connection established on port " << _port << std::endl;
        return std::make_unique<SocketConnection>(acceptedFD);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    SocketException::SocketException(const SocketException::ErrorCode code)
    : _message("socket exception: " + ErrorCodeMsg[(int) code]),
    _code(code) {}

    SocketException::SocketException(const std::string &message,
                                     const SocketException::ErrorCode code)
    : _message(message), _code(code) {}

    SocketException::~SocketException() {}

    SocketException::ErrorCode SocketException::GetErrorCode() const
    {
        return _code;
    }

    std::string SocketException::GetErrorMsg() const
    {
        return _message;
    }

    const char* SocketException::what() const noexcept
    {
        return _message.data();
    }

}}
