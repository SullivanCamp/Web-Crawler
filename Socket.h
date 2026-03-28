//Sullivan Irons-Camp
//CSCE 463 Spring 2026

#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <ctime>

class Socket {
public:
    enum class ReadResult {
        OK,
        RECV_ERROR,
        TIMEOUT,
        SLOW,
        EXCEED_MAX
    };

    SOCKET sock = INVALID_SOCKET;
    char* buf = nullptr;
    int allocatedSize = 0;
    int curPos = 0;

    Socket();
    ~Socket();

    static bool DoDNS(const std::string& host, sockaddr_in& serverOut, DWORD& ipOut, int& dnsMsOut);

    bool ConnectAndSend(const sockaddr_in& serverResolved, int port,
        const std::string& hostHeader,
        const std::string& method,
        const std::string& request,
        int& connectMsOut);

    ReadResult ReadWithLimits(int maxBytes, int slowSeconds, int& recvMsOut, int& bytesOut);

private:
    void ResetBufferForReuse();
    bool EnsureSpace(int needBytes);
    bool CreateSocket();
    void CloseSocket();
};
