// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>
#include <string>

#include "../IteratorUtils.h"

namespace Utility { namespace Networking
{
    using bytes = std::vector<char>;

    /// <summary>An established socket connection</summary>
    class SocketConnection
    {
    public:
        bytes Read(const uint32_t count);
        void Write(IteratorRange<const void*> data) const;

        SocketConnection(const std::string &address, const uint16_t port);
        SocketConnection(int socket);
        ~SocketConnection();
    private:
        int _fd;
    };

    /// <summary>Socket server that listens to incoming connections on a specific port</summary>
    class SocketServer
    {
    public:
        std::unique_ptr<SocketConnection> Listen();

        SocketServer(uint16_t port);
        ~SocketServer();
    private:
        int _fd;
        uint16_t _port;
    };

    /// <summary>Socket Exception: Error happening in network request</summary>
    class SocketException : public std::exception
    {
    public:
        enum class ErrorCode: int {
            bad_creation,
            invalid_address,
            bad_connection,
            incomplete,
            disconnected,
        };

        const std::string ErrorCodeMsg[5] = {
            "cannot create the socket",
            "invalid address",
            "bad connection",
            "data received is incomplete",
            "the socket is disconnected",
        };

        SocketException(const ErrorCode code);
        SocketException(const std::string& message, const ErrorCode code);

        ~SocketException();

        ErrorCode GetErrorCode() const;
        std::string GetErrorMsg() const;

        const char* what() const noexcept override;
    private:
        const std::string _message;
        const ErrorCode _code;
    };
}}
using namespace Utility;
