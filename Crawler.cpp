//Sullivan Irons-Camp
//CSCE 463 Spring 2026

#include "pch.h"
#include "Crawler.h"

#include <cstdio>
#include <cstring>
#include <chrono>

#include "HTMLParserBase.h"
#include "Parse.h"
#include "Socket.h"

using namespace std;

static bool ExtractHeaderBody(char* resp, int respLen, std::string& headerOut, char*& bodyPtr, int& bodyLen) {
    for (int i = 0; i + 3 < respLen; i++) {
        if (resp[i] == '\r' && resp[i + 1] == '\n' && resp[i + 2] == '\r' && resp[i + 3] == '\n') {
            headerOut.assign(resp, resp + i);
            bodyPtr = resp + i + 4;
            bodyLen = respLen - (i + 4);
            return true;
        }
    }
    return false;
}

static int ParseStatusCode(const std::string& header) {
    if (header.size() < 12) return -1;
    if (header.rfind("HTTP/", 0) != 0) return -1;
    size_t sp = header.find(' ');
    if (sp == std::string::npos || sp + 4 > header.size()) return -1;
    return atoi(header.substr(sp + 1, 3).c_str());
}

static bool StatusInRange(int code, int lo, int hi) {
    return code >= lo && code <= hi;
}

static uint64_t NowMs() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

Crawler::Crawler(int numThreads) : nThreads(numThreads) {}

void Crawler::PushAll(const vector<string>& urls) {
    {
        lock_guard<mutex> lk(qMtx);
        for (const auto& u : urls) q.push(u);
    }
    qCv.notify_all();
}

bool Crawler::PopURL(string& out) {
    unique_lock<mutex> lk(qMtx);
    if (q.empty()) return false;
    out = std::move(q.front());
    q.pop();
    return true;
}

void Crawler::Start() {
    startTickMs = NowMs();
    activeThreads.store(nThreads);
    statsThread = std::thread(&Crawler::StatsLoop, this);

    workers.reserve(nThreads);
    for (int i = 0; i < nThreads; i++) {
        workers.emplace_back(&Crawler::WorkerLoop, this, i);
    }
}

static void AddHttpBucket(std::atomic<uint64_t>& a2, std::atomic<uint64_t>& a3,
    std::atomic<uint64_t>& a4, std::atomic<uint64_t>& a5,
    std::atomic<uint64_t>& ao, int code) {
    if (code >= 200 && code <= 299) a2++;
    else if (code >= 300 && code <= 399) a3++;
    else if (code >= 400 && code <= 499) a4++;
    else if (code >= 500 && code <= 599) a5++;
    else ao++;
}
static bool IsTamuDomainHost(const std::string & host) {
        if (host == "tamu.edu") return true;
        if (host.size() > 9 && host.compare(host.size() - 9, 9, ".tamu.edu") == 0) return true;
        return false;
    }
void Crawler::WorkerLoop(int /*tid*/) {
    HTMLParserBase parser;
    

    while (true) {
        std::string u;
        {
            unique_lock<mutex> lk(qMtx);
            if (q.empty()) break;
            u = std::move(q.front());
            q.pop();
        }

        cntE++;

        ParsedURL url(u);
        if (!url.validHost) {
            continue;
        }


        {
            lock_guard<mutex> lk(hostMtx);
            auto ins = seenHosts.insert(url.host);
            if (!ins.second) continue;
        }
        cntH++;

        sockaddr_in server{};
        DWORD ipOut = 0;
        int dnsMs = 0;
        dnsAttempted++;
        if (!Socket::DoDNS(url.host, server, ipOut, dnsMs)) {
            continue;
        }
        cntD++;

        {
            lock_guard<mutex> lk(ipMtx);
            auto ins = seenIPs.insert(ipOut);
            if (!ins.second) continue;
        }
        cntI++;

        robotsAttempted++;
        bool robotsAllowed = false;
        {
            Socket s;
            int connMs = 0, loadMs = 0, bytes = 0;

            if (!s.ConnectAndSend(server, url.port, url.host, "HEAD", "/robots.txt", connMs)) {
                robotsAllowed = false;
            }
            else {
                Socket::ReadResult rr = s.ReadWithLimits(16 * 1024, 10, loadMs, bytes);
                if (rr != Socket::ReadResult::OK) {
                    robotsAllowed = false;
                }
                else {
                    bytesRobots += (uint64_t)bytes;

                    std::string header;
                    char* body = nullptr;
                    int bodyLen = 0;

                    if (!ExtractHeaderBody(s.buf, s.curPos, header, body, bodyLen)) {
                        robotsAllowed = false;
                    }
                    else {
                        int code = ParseStatusCode(header);
                        if (code < 100) robotsAllowed = false;
                        else robotsAllowed = StatusInRange(code, 400, 499);
                    }
                }
            }
        }

        if (!robotsAllowed) continue;
        cntR++;

        {
            Socket s;
            int connMs = 0, loadMs = 0, bytes = 0;

            if (!s.ConnectAndSend(server, url.port, url.host, "GET", url.request, connMs)) {
                continue;
            }

            Socket::ReadResult rr = s.ReadWithLimits(2 * 1024 * 1024, 10, loadMs, bytes);
            if (rr != Socket::ReadResult::OK) {
                continue;
            }

            bytesPages += (uint64_t)bytes;

            std::string header;
            char* body = nullptr;
            int bodyLen = 0;

            if (!ExtractHeaderBody(s.buf, s.curPos, header, body, bodyLen)) {
                continue;
            }

            int code = ParseStatusCode(header);
            if (code < 100) {
                AddHttpBucket(code2xx, code3xx, code4xx, code5xx, codeOther, 0);
                continue;
            }

            AddHttpBucket(code2xx, code3xx, code4xx, code5xx, codeOther, code);

            cntC++;

            if (code >= 200 && code <= 299) {
                pages2xx++;

                int nLinks = 0;
                std::string base = url.getBaseUrlString();
                char* linkBuf = parser.Parse(body, bodyLen, (char*)base.c_str(), (int)base.size(), &nLinks);

                if (nLinks > 0) linksTotal += (uint64_t)nLinks;

                bool hasTamu = false;
                if (linkBuf && nLinks > 0) {
                    char* p = linkBuf;
                    for (int i = 0; i < nLinks; i++) {
                        if (!p || *p == '\0') break;

                        std::string link(p);

                        ParsedURL lu(link);
                        if (lu.validHost && IsTamuDomainHost(lu.host)) {
                            hasTamu = true;
                            break;
                        }

                        p += (int)strlen(p) + 1;
                    }
                }

                if (hasTamu) {
                    pages2xxWithTamuLink++;

                    if (!IsTamuDomainHost(url.host)) {
                        pages2xxWithTamuLinkFromOutside++;
                    }
                }
            }


        }
    }

    activeThreads--;
}

std::string Crawler::FormatKilo(uint64_t x) {
    uint64_t k = x / 1000;
    return std::to_string(k) + "K";
}

void Crawler::PrintAlignedLine(int elapsedSec, int active,
    uint64_t Q, uint64_t E, uint64_t H, uint64_t D,
    uint64_t I, uint64_t R, uint64_t C, uint64_t Lk) {
    printf("[ %3d] %d Q %6llu E %7llu H %6llu D %6llu I %5llu R %5llu C %5llu L %4s\n",
        elapsedSec, active,
        (unsigned long long)Q,
        (unsigned long long)E,
        (unsigned long long)H,
        (unsigned long long)D,
        (unsigned long long)I,
        (unsigned long long)R,
        (unsigned long long)C,
        FormatKilo(Lk).c_str()
    );
}

void Crawler::StatsLoop() {
    uint64_t lastTick = NowMs();
    uint64_t lastC = 0;
    uint64_t lastBytes = 0;

    while (!stopStats.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        uint64_t now = NowMs();
        int elapsedSec = (int)((now - startTickMs) / 1000);

        int active = activeThreads.load();

        uint64_t qSize = 0;
        {
            lock_guard<mutex> lk(qMtx);
            qSize = (uint64_t)q.size();
        }

        uint64_t E = cntE.load();
        uint64_t H = cntH.load();
        uint64_t D = cntD.load();
        uint64_t I = cntI.load();
        uint64_t R = cntR.load();
        uint64_t C = cntC.load();
        uint64_t L = linksTotal.load();

        PrintAlignedLine(elapsedSec, active, qSize, E, H, D, I, R, C, L);

        double dt = (double)(now - lastTick) / 1000.0;
        uint64_t bytesNow = bytesRobots.load() + bytesPages.load();
        uint64_t dBytes = bytesNow - lastBytes;
        uint64_t dC = C - lastC;

        double pps = (dt > 0.0) ? (double)dC / dt : 0.0;
        double mbps = (dt > 0.0) ? (double)dBytes * 8.0 / (dt * 1000.0 * 1000.0) : 0.0;

        printf("*** crawling %.1f pps @ %.1f Mbps\n", pps, mbps);

        lastTick = now;
        lastBytes = bytesNow;
        lastC = C;

        if (active <= 0) {
            if (qSize == 0) break;
        }
    }
}

void Crawler::JoinAndFinish() {
    for (auto& t : workers) t.join();

    stopStats.store(true);
    if (statsThread.joinable()) statsThread.join();

    double totalSec = (double)(NowMs() - startTickMs) / 1000.0;
    if (totalSec < 1e-6) totalSec = 1e-6;

    uint64_t E = cntE.load();
    uint64_t D = cntD.load();
    uint64_t R = robotsAttempted.load();
    uint64_t C = cntC.load();
    uint64_t L = linksTotal.load();
    uint64_t pageBytes = bytesPages.load();

    printf("Extracted %llu URLs @ %.0f/s\n", (unsigned long long)E, (double)E / totalSec);
    printf("Looked up %llu DNS names @ %.0f/s\n", (unsigned long long)dnsAttempted.load(), (double)dnsAttempted.load() / totalSec);
    printf("Attempted %llu robots @ %.0f/s\n", (unsigned long long)R, (double)R / totalSec);
    printf("Crawled %llu pages @ %.0f/s (%.2f MB)\n",
        (unsigned long long)C, (double)C / totalSec, (double)pageBytes / (1024.0 * 1024.0));
    printf("Parsed %llu links @ %.0f/s\n", (unsigned long long)L, (double)L / totalSec);

    printf("HTTP codes: 2xx = %llu, 3xx = %llu, 4xx = %llu, 5xx = %llu, other = %llu\n",

        (unsigned long long)code2xx.load(),
        (unsigned long long)code3xx.load(),
        (unsigned long long)code4xx.load(),
        (unsigned long long)code5xx.load(),
        (unsigned long long)codeOther.load()
    );
    printf("2xx pages with tamu.edu link: %llu\n",(unsigned long long)pages2xxWithTamuLink.load());

    printf("2xx pages with tamu.edu link from outside TAMU: %llu\n",(unsigned long long)pages2xxWithTamuLinkFromOutside.load());

}
