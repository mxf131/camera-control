#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <ctime>
#include <random> // 随机数库
#include <thread>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

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
    if (values.size()>0)
    {
    	return sum / values.size();
    }
    else
    {
    	return 0;
    }
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
    
    cap.set(cv::CAP_PROP_FOCUS,500);

    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));

    // 捕获图像帧
    cv::Mat frame;
    // 初始化PID参数
    double Kp = 10;
    double Ki = 1;
    double Kd = 1;
    double integral = 0;
    double prev_error = 0;

    // 存储对比度历史记录
    std::deque<double> contrast_history = {};
    // 摄像头状态
    // 测焦状态
    // 调焦状态
    // 平稳运行状态
    int camera_state = 0;
    double contrast_threshold = 0.1;  // 对比度调整的阈值
    double focus_plus_contrast = 0; // 增大焦距后的对比度
    double focus_minus_contrast = 0; // 减小焦距后的对比度
    double current_contrast = 0; //当前对比度
    double target_contrast = 0; // 目标对比度
    int current_focus = 512; // 当前焦距
    int new_focus = 0; // 新焦距
    int focus_step = 0; //调焦间距
    double max_contrast; // 最大对比度
    int right_focus = 0; //正确的焦距
    int count = 0; // 求最合适焦点的计数值
    while(1)
    {
    	cap >> frame;
	// 更改镜头或者说当焦距需要变化的时候
	// 先调整百分之十的焦距 减小百分之十 增大百分之十 确定调焦方向
	// 然后依次调整百分之十获取到最大对比度
	// 依据什么触发更改焦距呢----对比度较大更改
	// 动态计算目标对比度
	
	// 输出当前焦距
	current_focus = cap.get(cv::CAP_PROP_FOCUS);
        spdlog::info("focus : {}",current_focus);

	// 计算相机与之前保存的对比度平均值的差值
	// 计算当前对比度
        current_contrast = calculateContrast(frame);
        spdlog::info("current contrast : {}", current_contrast);
	// 计算历史平均对比度
	target_contrast = calculateMovingAverage(contrast_history, 10);
	spdlog::info("Target Contrast: {}", target_contrast);
	if (std::abs(current_contrast-target_contrast)<contrast_threshold)
	{
	    // 把当前对比度放入对比度历史队列
            contrast_history.push_back(current_contrast);
	}
	else
	{
	    if (camera_state == 0)
	    {
	        camera_state = -1; // 调整焦距
	    }
	    else if (camera_state < 0)
	    {
	    	camera_state--;
	    }
	    else if (camera_state == 2)
	    {
	    	camera_state = 2;
	    }
	    else if (camera_state == 3)
	    {
	    	camera_state = 3;
	    }
	}
	// 小于阈值继续运行
	if (camera_state==0)
	{
		spdlog::info("camera state = 0 ");
	}
	// 大于阈值调整焦距
	else if (camera_state==-1)
	{
		// 清空无效数据
		contrast_history.clear();
		// 获取当前焦距
                current_focus = cap.get(cv::CAP_PROP_FOCUS);
		if (current_focus>101 && current_focus < 923)
		{
			// 调整焦距
                	new_focus = current_focus + 100;
                	cap.set(cv::CAP_PROP_FOCUS, new_focus);
		}
		else
		{
			new_focus = current_focus + 10;
                	cap.set(cv::CAP_PROP_FOCUS, new_focus);
		}
	}
	else if (camera_state==-2)
	{
		focus_plus_contrast = current_contrast;
		// 获取当前焦距
                current_focus = cap.get(cv::CAP_PROP_FOCUS);
		if (current_focus>=201)
		{
			// 调整焦距
                	new_focus = current_focus-200;
                	cap.set(cv::CAP_PROP_FOCUS, new_focus);
		}
		else
		{
			// 调整焦距
                	new_focus = current_focus-20;
                	cap.set(cv::CAP_PROP_FOCUS, new_focus);
		}
	}
	else if (camera_state==-3)
	{
		focus_minus_contrast = current_contrast;
		// 获取当前焦距
                current_focus = cap.get(cv::CAP_PROP_FOCUS);
		if(focus_plus_contrast>focus_minus_contrast)
		{
			camera_state = 1; // 焦距增大调焦
			focus_step = (int)(1023-(current_focus+100))/10;
		}
		else
		{
			camera_state = 2; // 焦距减小调焦
			focus_step = (int)(current_focus+100-1)/10;
		}
		// 设置为原来的焦距 在按照比例调整
		cap.set(cv::CAP_PROP_FOCUS, current_focus+100);
	}
	else if(camera_state==1)
	{
		spdlog::info("camera state = 1 ==========");
		if(count<10)
		{
			current_focus = cap.get(cv::CAP_PROP_FOCUS);
			if(max_contrast<current_contrast)
			{
				max_contrast=current_contrast;
				right_focus=current_focus;
			}
			new_focus = current_focus + focus_step;
			cap.set(cv::CAP_PROP_FOCUS, new_focus);
			count++;
		}
		else
		{
			spdlog::info("camera state = 1 and end");
			// 开启pid调焦？最佳焦点在[right_focus - step, right_focus + step]
			cap.set(cv::CAP_PROP_FOCUS, right_focus);
			contrast_history.clear();
			for (int i = 0; i < 10; i++) 
			{
				contrast_history.push_back(max_contrast);
			}
			camera_state=0;
			count=0;
			max_contrast=0;
		}
	}
	else if(camera_state==2)
	{
		spdlog::info("camera state = 2 ==========");
		if(count<10)
		{
			// 获取当前焦距 设置焦距 获取对比度
			current_focus = cap.get(cv::CAP_PROP_FOCUS);
			if(max_contrast<current_contrast)
			{
				max_contrast=current_contrast;
				right_focus=current_focus;
			}
			new_focus = current_focus - focus_step;
			cap.set(cv::CAP_PROP_FOCUS, new_focus);
			count++;
		}
		else
		{
			spdlog::info("camera state = 1 and end");
			// 开启pid调焦？最佳焦点在[right_focus - step, right_focus + step]
			cap.set(cv::CAP_PROP_FOCUS, right_focus);
			contrast_history.clear();
			for (int i = 0; i < 10; i++) 
			{
				contrast_history.push_back(max_contrast);
			}
			camera_state=0;
			count=0;
			max_contrast=0;
		}
	}
/*
	// 拍摄图片样例
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
    	bool success = cv::imwrite(filename, frame);
	spdlog::info("save image {}", filename);
*/
    }

    cap.release();
    cv::destroyAllWindows();

    return 0;
}       	
