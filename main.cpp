//Sullivan Irons-Camp
//CSCE 463 Spring 2026

#include "pch.h"
#pragma comment(lib, "Ws2_32.lib")
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <string>

#include "Crawler.h"

static std::string TrimLine(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    return s.substr(i);
}

static void PrintUsage(const char* exe) {
    printf("Usage: %s <threads> <URL-input.txt>\n", exe);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        PrintUsage(argv[0]);
        return 0;
    }

    int threads = atoi(argv[1]);
    if (threads <= 0) {
        PrintUsage(argv[0]);
        return 0;
    }

    const char* filename = argv[2];
    std::ifstream f(filename, std::ios::binary);
    if (!f) {
        printf("Error: cannot open %s\n", filename);
        return 0;
    }

    f.seekg(0, std::ios::end);
    std::streamoff sz = f.tellg();
    f.seekg(0, std::ios::beg);

    std::vector<char> filebuf((size_t)sz);
    if (sz > 0) f.read(filebuf.data(), sz);
    f.close();

    printf("Opened %s with size %lld\n", filename, (long long)sz);

    std::vector<std::string> urls;
    {
        std::string cur;
        for (size_t i = 0; i < (size_t)sz; i++) {
            char c = filebuf[i];
            if (c == '\n') {
                std::string line = TrimLine(cur);
                if (!line.empty()) urls.push_back(line);
                cur.clear();
            }
            else {
                cur.push_back(c);
            }
        }
        std::string line = TrimLine(cur);
        if (!line.empty()) urls.push_back(line);
    }

    // Winsock init once (shared)
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
        printf("WSAStartup error %d\n", WSAGetLastError());
        return 0;
    }

    Crawler crawler(threads);
    crawler.PushAll(urls);
    crawler.Start();
    crawler.JoinAndFinish();

    WSACleanup();
    return 0;
}
