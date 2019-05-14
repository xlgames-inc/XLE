// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Socket.h"

#include <stdlib.h>
#include <exception>

#pragma clang diagnostic ignored "-Winvalid-token-paste"

#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
    #include "../../Core/WinAPI/IncludeWindows.h"
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef long suseconds_t;
#else
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/socket.h>
#endif

#include "../../ConsoleRig/Log.h"
#include "../../Utility/SystemUtils.h"
#include "../../Core/Exceptions.h"

namespace
{
    struct sockaddr_in CreateSocketAddress(const char *address, const uint16_t port)
    {
        struct sockaddr_in serverAddr;
        memset(&serverAddr, '0', sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        // Convert IPv4 and IPv6 addresses from text to binary form
        if (inet_pton(AF_INET, address, &serverAddr.sin_addr) <= 0) {
            int errnoValue = errno;
            Throw(Networking::SocketException(Networking::SocketException::ErrorCode::invalid_address, errnoValue));
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
                    int errnoValue = errno;
                    Throw(Networking::SocketException(Networking::SocketException::ErrorCode::bad_creation, errnoValue));
                }
                s_WSAInitialized = true;
            }
        #endif
    }

    void HandleTimeout(int fd, uint16_t port, const std::chrono::milliseconds timeout)
    {
        using namespace std::chrono_literals;
        if (timeout > 0ms) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);

            struct timeval timeout_value;
            timeout_value.tv_sec = (suseconds_t)timeout.count() / 1000; // milliseconds to seconds
            timeout_value.tv_usec = ((suseconds_t)timeout.count() % 1000) * 1000; // milliseconds (minus whole seconds) to microseconds

            int timeoutResult = select(fd + 1, &rfds, nullptr, nullptr, &timeout_value);
            if (timeoutResult < 0) {
                int errnoValue = errno;
                Throw(Networking::SocketException(Networking::SocketException::ErrorCode::bad_connection, errnoValue));
            }
            if (timeoutResult == 0) {
                Log(Debug) << "Connection timed out on port " << port << std::endl;
                int errnoValue = errno;
                Throw(Networking::SocketException(Networking::SocketException::ErrorCode::timeout, errnoValue));
            }
        }
    }
}


namespace Utility { namespace Networking
{
    #if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
        void closesocket(int fd)
        {
            ::closesocket(fd);
        }
    #else
        void closesocket(int fd)
        {
            ::close(fd);
        }
    #endif

    SocketConnection::SocketConnection(const std::string &address, const uint16_t port, const std::chrono::milliseconds timeout)
    {
        EnsureNetworkingInitialized();

        struct sockaddr_in serverAddr = CreateSocketAddress(address.c_str(), port);

        if ((_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            int errnoValue = errno;
            Throw(SocketException(SocketException::ErrorCode::bad_creation, errnoValue));
        }

        HandleTimeout(_fd, port, timeout);

        if (connect(_fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            int errnoValue = errno;
            closesocket(_fd);
            Throw(SocketException(SocketException::ErrorCode::bad_connection, errnoValue));
        }
    }

    SocketConnection::SocketConnection(int socket) : _fd(socket) {}

    SocketConnection::~SocketConnection() {
        closesocket(_fd);
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
                int errnoValue = errno;
                Throw(SocketException(SocketException::ErrorCode::disconnected, errnoValue));
            }

            std::copy(buffer, buffer + numOfRead, iter);
            unread -= numOfRead;
            iter += numOfRead;
            if (numOfRead == 0 && unread != 0) {
                int errnoValue = errno;
                Throw(SocketException(SocketException::ErrorCode::incomplete, errnoValue));
            }
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
            int errnoValue = errno;
            Throw(SocketException(SocketException::ErrorCode::bad_creation, errnoValue));
        }

        { // socket flags
            int enable = 1;

            // When connections are interrupted, the addres:port pair might noy be available to be used right away.
            // This flag allows us to create another socket with the same address right away.
            if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(enable)) < 0) {
                int errnoValue = errno;
                closesocket(_fd);
                Throw(SocketException(SocketException::ErrorCode::bad_creation, errnoValue));
            }

            #if PLATFORMOS_TARGET != PLATFORMOS_WINDOWS
                // SIGPIPE will be raised when connections are ended by the client. Apple suggests a couple of different
                // ways to avoid a crash, but I found out that only this solution worked. The global ignore alternative
                // worked well on desktop, however. To learn more, search for SIGPIPE in:
                // https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/NetworkingOverview/CommonPitfalls/CommonPitfalls.html#//apple_ref/doc/uid/TP40010220-CH4-SW11
                if (setsockopt(_fd, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable)) < 0) {
                    int errnoValue = errno;
                    closesocket(_fd);
                    Throw(SocketException(SocketException::ErrorCode::bad_creation, errnoValue));
                }
            #endif
        }

        if (bind(_fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            int errnoValue = errno;
            closesocket(_fd);
            Throw(SocketException(SocketException::ErrorCode::bad_creation, errnoValue));
        }
        if (listen(_fd, 5) < 0) {
            int errnoValue = errno;
            closesocket(_fd);
            Throw(SocketException(SocketException::ErrorCode::bad_creation, errnoValue));
        }
    }

    SocketServer::~SocketServer()
    {
        closesocket(_fd);
    }

    std::unique_ptr<SocketConnection> SocketServer::Listen(const std::chrono::milliseconds timeout)
    {
        Log(Debug) << "Waiting for client connection on port " << _port << "..." << std::endl;

        HandleTimeout(_fd, _port, timeout);

        struct sockaddr_in clientAddr;
        socklen_t clilen = sizeof(clientAddr);
        int acceptedFD = accept(_fd, (struct sockaddr *)&clientAddr, &clilen);
        if (acceptedFD < 0) {
            int errnoValue = errno;
            Throw(SocketException(SocketException::ErrorCode::bad_connection, errnoValue));
        }
        Log(Debug) << "Connection established on port " << _port << std::endl;
        return std::make_unique<SocketConnection>(acceptedFD);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    SocketException::SocketException(const SocketException::ErrorCode code, const int errnoValue)
    : _message("socket exception: " + ErrorCodeMsg[(int) code]),
    _code(code), _errno(errnoValue) {}

    SocketException::SocketException(const std::string &message,
                                     const SocketException::ErrorCode code,
                                     const int errnoValue)
    : _message(message), _code(code), _errno(errnoValue) {}

    SocketException::~SocketException() {}

    SocketException::ErrorCode SocketException::GetErrorCode() const
    {
        return _code;
    }

    std::string SocketException::GetErrorMsg() const
    {
        return _message;
    }

    int SocketException::GetErrno() const
    {
        return _errno;
    }

    const char* SocketException::what() const noexcept
    {
        return _message.data();
    }

}}
