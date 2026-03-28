//Sullivan Irons-Camp
//CSCE 463 Spring 2026

#include "pch.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Socket.h"
#include <cstdio>
#include <cstring>

static const int INITIAL_BUF_SIZE = 8192;

Socket::Socket() {
    buf = new char[INITIAL_BUF_SIZE];
    allocatedSize = INITIAL_BUF_SIZE;
    curPos = 0;
}

Socket::~Socket() {
    CloseSocket();
    delete[] buf;
    buf = nullptr;
}

void Socket::CloseSocket() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}

bool Socket::CreateSocket() {
    CloseSocket();
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf(" failed with %d\n", WSAGetLastError());
        return false;
    }
    return true;
}

void Socket::ResetBufferForReuse() {
    if (allocatedSize > 32 * 1024) {
        delete[] buf;
        buf = new char[INITIAL_BUF_SIZE];
        allocatedSize = INITIAL_BUF_SIZE;
    }
    curPos = 0;
}

bool Socket::EnsureSpace(int needBytes) {
    if (allocatedSize - curPos >= needBytes) return true;
    int newSize = allocatedSize;
    while (newSize - curPos < needBytes) newSize *= 2;
    char* nb = new char[newSize];
    memcpy(nb, buf, curPos);
    delete[] buf;
    buf = nb;
    allocatedSize = newSize;
    return true;
}

bool Socket::DoDNS(const std::string& host, sockaddr_in& serverOut, DWORD& ipOut, int& dnsMsOut) {
    serverOut = {};
    serverOut.sin_family = AF_INET;

    clock_t t0 = clock();

    DWORD ip = inet_addr(host.c_str());
    if (ip == INADDR_NONE) {
        hostent* remote = gethostbyname(host.c_str());
        if (!remote || !remote->h_addr_list || !remote->h_addr_list[0]) {
            return false;
        }
        memcpy(&serverOut.sin_addr, remote->h_addr_list[0], remote->h_length);
    }
    else {
        serverOut.sin_addr.S_un.S_addr = ip;
    }

    ipOut = serverOut.sin_addr.S_un.S_addr;
    dnsMsOut = (int)(1000.0 * (double)(clock() - t0) / (double)CLOCKS_PER_SEC);
    return true;
}

bool Socket::ConnectAndSend(const sockaddr_in& serverResolved, int port,
    const std::string& hostHeader,
    const std::string& method,
    const std::string& request,
    int& connectMsOut) {
    ResetBufferForReuse();
    if (!CreateSocket()) return false;

    sockaddr_in server = serverResolved;
    server.sin_port = htons((u_short)port);

    clock_t t0 = clock();
    if (connect(sock, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        printf(" failed with %d\n", WSAGetLastError());
        CloseSocket();
        return false;
    }
    connectMsOut = (int)(1000.0 * (double)(clock() - t0) / (double)CLOCKS_PER_SEC);

    std::string req;
    req += method + " " + request + " HTTP/1.0\r\n";
    req += "Host: " + hostHeader + "\r\n";
    req += "Connection: close\r\n";
    req += "\r\n";

    int total = 0, toSend = (int)req.size();
    while (total < toSend) {
        int snt = send(sock, req.c_str() + total, toSend - total, 0);
        if (snt == SOCKET_ERROR) {
            printf("\tLoading... failed with %d on send\n", WSAGetLastError());
            CloseSocket();
            return false;
        }
        total += snt;
    }
    return true;
}

Socket::ReadResult Socket::ReadWithLimits(int maxBytes, int slowSeconds, int& recvMsOut, int& bytesOut) {
    clock_t tStart = clock();
    clock_t tLastProgress = tStart;

    while (true) {
        double totalSec = (double)(clock() - tStart) / (double)CLOCKS_PER_SEC;
        if (totalSec > (double)slowSeconds) {
            CloseSocket();
            return ReadResult::SLOW;
        }

        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(sock, &readset);

        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ret = select(0, &readset, nullptr, nullptr, &timeout);
        if (ret > 0) {
            if (curPos >= maxBytes) {
                CloseSocket();
                return ReadResult::EXCEED_MAX;
            }

            int want = 8192;
            int remaining = maxBytes - curPos;
            if (remaining <= 0) {
                CloseSocket();
                return ReadResult::EXCEED_MAX;
            }
            if (want > remaining) want = remaining;

            EnsureSpace(want);

            int bytes = recv(sock, buf + curPos, want, 0);
            if (bytes == SOCKET_ERROR) {
                CloseSocket();
                return ReadResult::RECV_ERROR;
            }

            if (bytes == 0) {
                recvMsOut = (int)(1000.0 * (double)(clock() - tStart) / (double)CLOCKS_PER_SEC);
                bytesOut = curPos;

                if (curPos >= allocatedSize) EnsureSpace(1);
                buf[curPos] = '\0';

                CloseSocket();
                return ReadResult::OK;
            }

            curPos += bytes;
            tLastProgress = clock();

            if (curPos > maxBytes) {
                CloseSocket();
                return ReadResult::EXCEED_MAX;
            }
        }
        else if (ret == 0) {
            double idleSec = (double)(clock() - tLastProgress) / (double)CLOCKS_PER_SEC;
            if (idleSec >= 10.0) {
                CloseSocket();
                return ReadResult::TIMEOUT;
            }
        }
        else {
            CloseSocket();
            return ReadResult::RECV_ERROR;
        }
    }
}
