#include "network.hpp"
#include <curl/curl.h>
#include <stdexcept>

namespace
{
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
    {
        size_t total_size = size * nmemb;
        auto* str = static_cast<std::string*>(userp);
        str->append(static_cast<char*>(contents), total_size);
        return total_size;
    }
}

namespace hypecc::utils
{
    std::string Network::fetch_recipe(const std::string& package_name, const std::string& script_name)
    {
        CURL* curl = curl_easy_init();
        if (!curl)
        {
            throw std::runtime_error("Critical: Failed to initialize network subsystem.");
        }

        std::string read_buffer;
        std::string url = "https://raw.githubusercontent.com/hypecc-pm/Signature/main/recipes/" 
                          + package_name + "/" + script_name;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK || response_code != 200)
        {
            throw std::runtime_error("Error: Failed to resolve registry pipeline for payload target.");
        }

        return read_buffer;
    }
}
