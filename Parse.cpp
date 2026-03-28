//Sullivan Irons-Camp
//CSCE 463 Spring 2026

#include "pch.h"
#include "Parse.h"
#include <cstring>
#include <cstdlib>

ParsedURL::ParsedURL(const std::string& inUrl) {
    original = inUrl;

    const std::string prefix = "http://";
    if (original.rfind(prefix, 0) != 0) {
        validHost = false;
        invalidReason = "invalid scheme";
        return;
    }

    std::string s = original.substr(prefix.size()); // after http://

    // 1) strip fragment
    size_t hash = s.find('#');
    if (hash != std::string::npos) s = s.substr(0, hash);

    // 2) extract query (keep leading '?')
    std::string queryStr;
    size_t q = s.find('?');
    if (q != std::string::npos) {
        queryStr = s.substr(q);   // includes '?'
        s = s.substr(0, q);
    }

    // 3) extract path (keep leading '/')
    std::string pathStr;
    size_t slash = s.find('/');
    if (slash != std::string::npos) {
        pathStr = s.substr(slash); // includes '/'
        s = s.substr(0, slash);
    }
    else {
        pathStr = "/";             // root if missing
    }

    // 4) extract port from host part only
    std::string hostPart = s;
    size_t colon = hostPart.find(':');
    if (colon != std::string::npos) {
        std::string portStr = hostPart.substr(colon + 1);
        hostPart = hostPart.substr(0, colon);

        if (portStr.empty()) { validHost = false; invalidReason = "invalid port"; return; }
        for (char c : portStr) if (c < '0' || c > '9') { validHost = false; invalidReason = "invalid port"; return; }

        int p = atoi(portStr.c_str());
        if (p <= 0 || p > 65535) { validHost = false; invalidReason = "invalid port"; return; }
        port = p;
    }
    else {
        port = 80;
    }

    if (hostPart.empty()) { validHost = false; invalidReason = "invalid host"; return; }

    host = hostPart;

    // Request is [/path][?query]
    request = pathStr + queryStr;

    validHost = true;
}

std::string ParsedURL::generateRequest(const std::string& method) const {
    std::string req;
    req += method + " " + request + " HTTP/1.0\r\n";
    req += "Host: " + host + "\r\n";
    req += "Connection: close\r\n";
    req += "\r\n";
    return req;
}

std::string ParsedURL::getBaseUrlString() const {
    if (port == 80) return "http://" + host;
    return "http://" + host + ":" + std::to_string(port);
}
