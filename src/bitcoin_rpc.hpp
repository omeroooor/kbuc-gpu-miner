#pragma once

#include <string>
#include <memory>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <iomanip>

class BitcoinRPC {
public:
    BitcoinRPC(const std::string& host, int port, const std::string& user, const std::string& pass)
        : url_(host + ":" + std::to_string(port))
        , auth_(user + ":" + pass)
        , curl_(nullptr) {
        std::cout << "Initializing Bitcoin RPC client for " << url_ << std::endl;
        curl_ = curl_easy_init();
        if (!curl_) {
            throw std::runtime_error("Failed to initialize CURL");
        }
    }

    ~BitcoinRPC() {
        if (curl_) {
            curl_easy_cleanup(curl_);
        }
    }

    bool broadcastSupportTicket(const std::string& hexData) {
        std::cout << "\nCalling broadcastsupportticket RPC method" << std::endl;
        std::cout << "Hex data to broadcast: " << hexData << std::endl;
        
        nlohmann::json request;
        request["jsonrpc"] = "1.0";
        request["id"] = "curltest";
        request["method"] = "broadcastsupportticket";
        request["params"] = nlohmann::json::array({hexData});

        std::string requestStr = request.dump(2);
        std::cout << "Request JSON:\n" << requestStr << std::endl;

        std::string response = makeRequest(requestStr);
        std::cout << "Response from Bitcoin RPC:\n" << response << std::endl;
        
        try {
            auto json = nlohmann::json::parse(response);
            bool success = json["error"].is_null();
            std::cout << "RPC call " << (success ? "succeeded" : "failed") << std::endl;
            return success;
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse RPC response: " << e.what() << std::endl;
            return false;
        }
    }

private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    std::string makeRequest(const std::string& data) {
        std::string response;
        
        std::cout << "Making HTTP request to " << url_ << std::endl;
        
        curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());
        curl_easy_setopt(curl_, CURLOPT_USERPWD, auth_.c_str());
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
        
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl_);
        curl_slist_free_all(headers);
        
        if (res != CURLE_OK) {
            std::string error = "CURL error: ";
            error += curl_easy_strerror(res);
            std::cerr << error << std::endl;
            throw std::runtime_error(error);
        }
        
        return response;
    }

    std::string url_;
    std::string auth_;
    CURL* curl_;
};
