//Sullivan Irons-Camp
//CSCE 463 Spring 2026

#pragma once
#pragma once
#include <string>
#include <queue>
#include <vector>
#include <set>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <winsock2.h>

class Crawler {
public:
    Crawler(int numThreads);

    void PushAll(const std::vector<std::string>& urls);

    void Start();

    void JoinAndFinish();

private:
    void WorkerLoop(int tid);
    void StatsLoop();

    bool PopURL(std::string& out);

    static std::string FormatKilo(uint64_t x);
    static void PrintAlignedLine(int elapsedSec, int active,
        uint64_t Q, uint64_t E, uint64_t H, uint64_t D,
        uint64_t I, uint64_t R, uint64_t C, uint64_t Lk);

private:
    int nThreads;

    std::queue<std::string> q;
    std::mutex qMtx;
    std::condition_variable qCv;

    std::set<std::string> seenHosts;
    std::mutex hostMtx;

    std::set<DWORD> seenIPs;
    std::mutex ipMtx;

    std::vector<std::thread> workers;
    std::thread statsThread;
    std::atomic<int> activeThreads{ 0 };
    std::atomic<bool> stopStats{ false };

    std::atomic<uint64_t> cntE{ 0 };
    std::atomic<uint64_t> cntH{ 0 };
    std::atomic<uint64_t> cntD{ 0 };
    std::atomic<uint64_t> cntI{ 0 };
    std::atomic<uint64_t> cntR{ 0 };
    std::atomic<uint64_t> cntC{ 0 };
    std::atomic<uint64_t> linksTotal{ 0 };
    std::atomic<uint64_t> pages2xx{ 0 };
    std::atomic<uint64_t> pages2xxWithTamuLink{ 0 };
    std::atomic<uint64_t> pages2xxWithTamuLinkFromOutside{ 0 };

    std::atomic<uint64_t> bytesRobots{ 0 };
    std::atomic<uint64_t> bytesPages{ 0 };

    std::atomic<uint64_t> code2xx{ 0 }, code3xx{ 0 }, code4xx{ 0 }, code5xx{ 0 }, codeOther{ 0 };

    std::atomic<uint64_t> robotsAttempted{ 0 };
    std::atomic<uint64_t> dnsAttempted{ 0 };

    uint64_t startTickMs = 0;
};
