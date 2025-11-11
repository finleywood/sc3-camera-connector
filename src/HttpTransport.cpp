#include "HttpTransport.h"
#include <curl/curl.h>
#include <json/json.hpp>
#include <aixlog/aixlog.hpp>

using json = nlohmann::json;

struct MemoryStruct {
    char *memory;
    size_t size;
};

// Callback to write response data into our buffer
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = (char *)realloc(mem->memory, mem->size + totalSize + 1);
    if(ptr == NULL) {
        LOG(ERROR) << "Realloc of http response buffer failed." << std::endl;
        return 0;
    }


    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, totalSize);
    mem->size += totalSize;
    mem->memory[mem->size] = 0;

    return totalSize;
}

struct UploadCtx {
    const uint8_t *data;
    size_t size;
    size_t pos;
};

size_t read_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    UploadCtx *ctx = (UploadCtx *)userdata;
    size_t max = size * nmemb;
    size_t bytes_left = ctx->size - ctx->pos;
    size_t to_copy = std::min(max, bytes_left);

    if (to_copy > 0) {
        memcpy(ptr, ctx->data + ctx->pos, to_copy);
        ctx->pos += to_copy;
    }

    return to_copy;
}

bool HttpTransport::UploadQueueItem(UploadItem item)
{
    bool rgb_success = false;
    bool depth_success = false;
    std::string uid = this->InitMetadata(item.qr_det.identifier, item.qr_det.event_time);
    if(uid.empty())
        return false;

    if(item.qr_det.rgb_img.valid()) {
        rgb_success = this->UploadRGB(uid, item.qr_det.rgb_img.data.get(), item.qr_det.rgb_img.size);
        if(!rgb_success)
            return false;
    }

    if(item.qr_det.depth_image.valid()) {
        depth_success = this->UploadDepth(uid, item.qr_det.depth_image.data.get(), item.qr_det.depth_image.size);
        if (!depth_success)
            return false;
    } else {
        depth_success = true;
    }

    return rgb_success;
}

std::string HttpTransport::InitMetadata(std::string identifier, uint64_t event_time)
{
    CURL *curl;
    CURLcode res;

    MemoryStruct response;
    response.memory = (char *)malloc(1);
    response.size = 0;

    curl = curl_easy_init();

    if(curl == nullptr)
        return "";
    
    json data_to_send;
    data_to_send["identifier"] = identifier.c_str();
    data_to_send["event_time"] = event_time;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Set URL
    int url_str_size = snprintf(nullptr, 0, "%s/event/new", this->server_url.c_str());
    std::unique_ptr<char, decltype(&free)> out_url = std::unique_ptr<char, decltype(&free)>{ static_cast<char*>(malloc(url_str_size)), &free };
    sprintf(out_url.get(), "%s/event/new", this->server_url.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, out_url.get());

    // Set headers
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set POST data
    std::string out_json_str = data_to_send.dump(4);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, out_json_str.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

    // Perform the request
    res = curl_easy_perform(curl);

    // Check for errors
    if(res != CURLE_OK) {
        LOG(ERROR) << "There was an error sending the request to " << out_url.get() << " due to " << curl_easy_strerror(res) << std::endl;
        return "";
    } else {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if(response_code != 201) {
            LOG(WARNING) << "Server responded with " << response_code << std::endl;
            return "";
        }
    }

    json resp_j;
    try {
        resp_j = json::parse(response.memory);
    } catch (std::exception& e) {
        LOG(ERROR) << "Could not parse server response, due to: " << e.what() << std::endl;
        return "";
    }
    std::string resp_uid = resp_j["uid"];

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(response.memory);

    return resp_uid;
}

bool HttpTransport::PutBin(std::string uid, bool is_depth, uint8_t* img_buf, size_t img_buf_size)
{
    CURL *curl = curl_easy_init();
    if(!curl) {
        return false;
    }

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // --- Core PUT setup ---
    const char* upload_path = is_depth ? "depth_upload" : "rgb_upload";
    int url_str_size = std::snprintf(nullptr, 0, "%s/event/%s/%s", this->server_url.c_str(), uid.c_str(), upload_path);
    std::unique_ptr<char, decltype(&free)> out_url = std::unique_ptr<char, decltype(&free)>{ static_cast<char*>(malloc(url_str_size)), &free};
    sprintf(out_url.get(), "%s/event/%s/%s", this->server_url.c_str(), uid.c_str(), is_depth ? "depth_upload" : "rgb_upload");
    curl_easy_setopt(curl, CURLOPT_URL, out_url.get());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);  // Use PUT

    UploadCtx upload_ctx = { img_buf, img_buf_size, 0 };
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback); // default read from provided buffer
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx); // data pointer
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)upload_ctx.size);

    // Perform request
    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        LOG(ERROR) << "There was an error sending the request to " << out_url.get() << " due to " << curl_easy_strerror(res) << std::endl;
        return false;
    } else {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if(response_code != 200) {
            LOG(WARNING) << "Server responded with " << response_code << std::endl;
            return "";
        }
    }

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return true;
}

bool HttpTransport::UploadRGB(std::string uid, uint8_t* img_buf, size_t img_buf_size)
{
    return this->PutBin(uid, false, img_buf, img_buf_size);
}

bool HttpTransport::UploadDepth(std::string uid, uint8_t* img_buf, size_t img_buf_size)
{
    return this->PutBin(uid, true, img_buf, img_buf_size);
}