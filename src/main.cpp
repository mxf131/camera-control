#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <ctime>
#include <random> // 随机数库
#include <thread>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
// #include "send_img.h"
double calculateContrast(const cv::Mat& image) {
    cv::Mat grayImage;
    cv::cvtColor(image, grayImage, cv::COLOR_BGR2GRAY);

    cv::Scalar mean, stddev;
    cv::meanStdDev(grayImage, mean, stddev);

    return stddev[0] / mean[0];
}

double calculateMovingAverage(std::deque<double>& values, int window_size) {
    if (values.size() >= window_size) {
        values.pop_front();
    }
    double sum = 0;
    for (const double& value : values) {
        sum += value;
    }
    return sum / values.size();
}
int main()      
{   
    // 初始化文件日志记录器，将日志记录到 "my_log.txt" 文件中
    auto logger = spdlog::basic_logger_mt("basic_logger", "/home/pi/log/task-camera-check.txt");
    // set level
    logger->set_level(spdlog::level::debug);
    // flush on
    logger->flush_on(spdlog::level::debug);
    // set logger
    // spdlog::set_default_logger(logger);
    spdlog::info("camera self check task activation");
    // 初始化摄像头
    //cv::VideoCapture cap("http://admin:123456@192.168.31.234:8081/video");
    cv::VideoCapture cap(0, cv::CAP_V4L);
    
    if (!cap.isOpened()) 
    {
	    spdlog::error("can not open camera");
            return -1;
    }

    // capture settings
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    cap.set(cv::CAP_PROP_CONVERT_RGB, true); // 设置为 RGB 格式
    
    cap.set(cv::CAP_PROP_BACKLIGHT, 0);//补光 0不补光 1补光强度1 2补光强度2 (以相机为准)
    cap.set(cv::CAP_PROP_BRIGHTNESS, 0);//亮度
    cap.set(cv::CAP_PROP_CONTRAST,50);//对比度
    cap.set(cv::CAP_PROP_SATURATION,70);//饱和度
    cap.set(cv::CAP_PROP_HUE,0);//色相
    cap.set(cv::CAP_PROP_GAMMA,300);//伽马
    cap.set(cv::CAP_PROP_GAIN,60);//增益
    cap.set(cv::CAP_PROP_SHARPNESS,93);//锐度
    cap.set(cv::CAP_PROP_WB_TEMPERATURE,4600);//白平衡色温
    cap.set(cv::CAP_PROP_AUTO_WB,0);//启用/禁用自动白平衡 0 关闭 1 打开
    
    cap.set(cv::CAP_PROP_FOCUS,0);

    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));

    // 捕获图像帧
    cv::Mat frame;
    // 初始化PID参数
    double Kp = 0.1;
    double Ki = 0.01;
    double Kd = 0.01;
    double integral = 0;
    double prev_error = 0;

    // 存储对比度历史记录
    std::deque<double> contrast_history;

    while(1)
    {
// 先获取10帧contrast 得到最大值
//
    	cap >> frame;
/*
    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    std::time_t time_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // 格式化时间字符串
    char time_str[100];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d_%H-%M-%S", std::localtime(&time_now));
    std::string path = "/home/pi/saveimg/";
    std::string filename = path + std::string(time_str) + "_" + std::to_string(ms.count()) + ".jpg";
    // 保存图像
    // bool success = cv::imwrite(filename, frame);
*/
	// 动态计算目标对比度
        double target_contrast = calculateMovingAverage(contrast_history, 10);

        std::cout << "Target Contrast: " << target_contrast << std::endl;

        // 计算当前对比度
        double current_contrast = calculateContrast(frame);

        contrast_history.push_back(current_contrast);

        // 计算PID误差
        double error = target_contrast - current_contrast;

        // 计算积分部分
        integral += error;

        // 计算微分部分
        double derivative = error - prev_error;
        prev_error = error;

        // 计算PID输出
        double output = Kp * error + Ki * integral + Kd * derivative;

        // 获取当前焦距
        double current_focus = cap.get(cv::CAP_PROP_FOCUS);

        // 调整焦距
        double new_focus = current_focus + output;
        cap.set(cv::CAP_PROP_FOCUS, new_focus);

    }

    cap.release();
    cv::destroyAllWindows();

    return 0;
}       	
