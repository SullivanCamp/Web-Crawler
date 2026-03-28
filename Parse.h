//Sullivan Irons-Camp
//CSCE 463 Spring 2026

#pragma once
#include <string>

class ParsedURL {
public:
    std::string original;
    std::string host;
    int port = 80;

    std::string request;

    bool validHost = false;
    std::string invalidReason;

    ParsedURL() = default;
    ParsedURL(const std::string& url);

    std::string generateRequest(const std::string& method = "GET") const;

    std::string getBaseUrlString() const;
};
#pragma once
