#ifndef SEND_IMG_H
#define SEND_IMG_H
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <mqueue.h>
#include <cstring>
#include "json.hpp"

using json = nlohmann::json;

// 回调函数，用于处理响应数据
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* user_data) 
{
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(user_data);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

static void send_image(std::string filename) {
    
    // check file
    std::ifstream file(filename);
    if (!file.good())
    {
        spdlog::error("file name error, file {} not exist",filename);
        return;
    }
    // read json
    std::ifstream configFile("/home/pi/code/github/battery-detect/config.json");
    json configData;
    configFile >> configData;
    // 最大重试次数 
    int max_retries = configData["MAX_RETRIES"];
    // 重试延迟时间(秒)
    int retry_delay_seconds = configData["RETRY_DELAY_SECONDS"];
    // URL
    std::string url = configData["URL"];
    // TOKEN
    std::string token = configData["TOKEN"];

    // time
    std::string currentTimeStr;
    CURL *curl;
    CURLcode res;
	
    // 初始化 cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    // 创建 cURL 句柄
    curl = curl_easy_init();

    if (curl) 
    {
        std::string response; // response 响应字符串 
	// 设置 URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // 设置 POST 请求
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        // 设置请求头
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: multipart/form-data");
        headers = curl_slist_append(headers, token.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // 获取当前时间
        std::time_t currentTime = std::time(nullptr);
        currentTimeStr = std::to_string(currentTime);
	
	// 设置请求体（form-data 格式）
        curl_httppost *formpost = NULL;
        curl_httppost *lastptr = NULL;
        // add file
	curl_formadd(&formpost, &lastptr,
            CURLFORM_COPYNAME, "file",
            CURLFORM_FILE, filename.c_str(),
            CURLFORM_END);
	// 添加时间戳字段
        curl_formadd(&formpost, &lastptr,
            CURLFORM_COPYNAME, "timeStamp",
            CURLFORM_COPYCONTENTS, currentTimeStr.c_str(),
            CURLFORM_END);
	// 设置请求体 
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	// 设置回调函数来处理响应数据
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	// 循环发送
        for (int retries = 0; retries < max_retries; ++retries) 
	{
		// 执行请求
        	res = curl_easy_perform(curl);
		// 网络原因未发送
        	if (res != CURLE_OK) 
		{
			spdlog::error("curl_easy_perform() failed: {}",curl_easy_strerror(res));
			spdlog::error("network error and failed,file name = {}, time stamp = {}",filename,currentTimeStr);
			std::this_thread::sleep_for(std::chrono::seconds(retry_delay_seconds));
        	}
		else
		{
			// send success 发送成功
			if (response == "200")
			{
				spdlog::info("send file success, file name = {}, time stamp = {}",filename,currentTimeStr);
				break;
			}
			// send but unsuccess 网络正常，但是发送失败
			else
			{
				spdlog::error("network correct ,but failed,file name = {}, time stamp = {}",filename,currentTimeStr);
				std::this_thread::sleep_for(std::chrono::seconds(retry_delay_seconds));
			}
		}
	}
	// fail to send
	// after 30 times and 60 seconds ,if it is still failed
	if (response != "200")
	{
		spdlog::info("send message because send image failed,file name = {}, time stamp = {}",filename,currentTimeStr);
		
		const char* queue_name = "/my_queue"; // Unique queue name
    		unsigned int max_messages = 100; // 初始消息队列大小
    		unsigned int max_message_size = 1024; // 每条消息的最大大小

    		mqd_t mq;

    		struct mq_attr attr;
    		attr.mq_flags = 0;
    		attr.mq_maxmsg = max_messages;
    		attr.mq_msgsize = max_message_size;
    		attr.mq_curmsgs = 0;

    		// Open the message queue
    		mq = mq_open(queue_name, O_CREAT | O_RDWR, 0666, &attr);
    		if (mq == -1) 
		{
			perror("mq_open");
			spdlog::error("mq_open : open failed!");
        		return;
    		}
		std::string message_string = filename + ":" + currentTimeStr;
    		const char* message = message_string.c_str();
    		if (mq_send(mq, message, strlen(message) + 1, 0) == -1) 
		{
        		perror("mq_send");
			spdlog::error("mq_send : send failed! ");
        		return;
    		}

    		mq_close(mq);

	}

        // 清理请求头
        curl_slist_free_all(headers);

        // 清理 cURL 句柄
        curl_easy_cleanup(curl);
    }
    else
    {
    	spdlog::error("curl create error,file name = {}",filename);	
    }

    spdlog::info("send function over");
    // 清理 cURL
    curl_global_cleanup();
    // 关闭日志记录器
    // spdlog::shutdown();
}

#endif
