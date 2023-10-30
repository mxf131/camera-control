#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <ctime>
#include <random> // 随机数库
#include <cmath>
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

std::string getImageFileNameByCurrentTime(std::string path)
{
    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    std::time_t time_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    // 格式化时间字符串
    char time_str[100];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d_%H-%M-%S", std::localtime(&time_now));
    std::string file_name = path + std::string(time_str) + "_" + std::to_string(ms.count()) + ".jpg";
    return file_name;
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
    
    cap.set(cv::CAP_PROP_FOCUS,205);

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
    std::deque<double> contrasts = {};
    // 摄像头状态
    // 测焦状态
    // 调焦状态
    // 平稳运行状态
    int camera_state = 0;
    double contrast_threshold = 0.05;  // 对比度调整的阈值
    double focus_plus_contrast = 0; // 增大焦距后的对比度
    double focus_minus_contrast = 0; // 减小焦距后的对比度
    double current_contrast = 0; //当前对比度
    double target_contrast = 0; // 目标对比度
    int current_focus = 512; // 当前焦距
    int new_focus = 0; // 新焦距
    int focus_step = 0; //调焦间距
    double max_contrast; // 最大对比度
    int right_focus = 0; //正确的焦距
    int old_focus = 0;
    int count = 0; // 求最合适焦点的计数值
    int frame_count = 0; //几帧图片后拍摄
    double dy,k,l;
    int dx;
    double last_contrast;
    int last_focus;
    k = 500;
    while(1)
    {
    	cap >> frame;
	frame_count++;
	// 更改镜头或者说当焦距需要变化的时候
	// 先调整百分之十的焦距 减小百分之十 增大百分之十 确定调焦方向
	// 然后依次调整百分之十获取到最大对比度
	// 依据什么触发更改焦距呢----对比度较大更改
	// 动态计算目标对比度
	
	// 定义裁剪的区域（矩形）
    	int x = 540; // 左上角 x 坐标
    	int y = 285; // 左上角 y 坐标
    	int width = 200; // 裁剪的宽度
    	int height = 150; // 裁剪的高度

    	// 创建一个矩形区域
    	cv::Rect roi(x, y, width, height);

    	// 使用矩形区域裁取图像
    	cv::Mat target_img = frame(roi);	
	// 输出当前焦距
	current_focus = cap.get(cv::CAP_PROP_FOCUS);
        spdlog::info("focus : {}",current_focus);

	// 计算当前对比度
        current_contrast = calculateContrast(target_img);
	spdlog::info("contrast : {}", current_contrast);

	// 计算历史平均对比度
	target_contrast = calculateMovingAverage(contrast_history, 10);
	if (camera_state == 0)
	{	
		// 计算相机与之前保存的对比度平均值的差值
		// 小于阈值继续工作
		if (std::abs(current_contrast-target_contrast)<contrast_threshold)
		{
	    		// 把当前对比度放入对比度历史队列
            		contrast_history.push_back(current_contrast);
			if(frame_count%10==0)
			{
				// 拍摄图片样例
    				// 保存图像
    				//bool success = cv::imwrite(filename, frame);
    				// 定义绝对路径
    				std::string path = "/home/pi/saveimg/";
				std::string filename = getImageFileNameByCurrentTime(path);
				cv::imwrite(filename, target_img);
				spdlog::info("save image {}", filename);
			}
		}
		else
		{
	        	camera_state = -1; // 调整焦距
	    	}
	}
	// 需要调焦的准备工作
	else if(camera_state==-1)
	{
		// 清空无效数据
		contrast_history.clear();
		// 计算所处的位置log2
		int level = static_cast<int>(log2(old_focus));
		if(level<6)
		{
			focus_step=5;
		}
		else
		{
			focus_step=10;
		}
		// 开始调焦
		camera_state = 1;  // 调整焦距变大
		new_focus = current_focus + focus_step;  // 新的焦距
                cap.set(cv::CAP_PROP_FOCUS, new_focus);  // 设置焦距
		last_focus = current_focus;  // 上次的焦距
		last_contrast = current_contrast;  // 上次的对比度
	}
	else if(camera_state==1)
	{
		dx = current_focus - last_focus;
		if(dx==0)
		{
			camera_state = 0;
			continue;
		}
		dy = current_contrast - last_contrast;
		
		last_focus = current_focus;  // 上次的焦距
		last_contrast = current_contrast;  // 上次的对比度
		
		l = dy/dx;
		spdlog::info("dy/dx : {}", l);
		focus_step = (int) (k * l);
		if(focus_step > 10)
		{
			focus_step = 10;
		}
		else if(focus_step < -10)
		{
			focus_step = -10;
		}
		spdlog::info("focus step : {}", focus_step);
		if(std::abs(dy) > 0.01)
		{
			spdlog::info("camera state = 1 ==========");
			
			new_focus = current_focus + focus_step;
			cap.set(cv::CAP_PROP_FOCUS, new_focus);
	    	}	
	    	else
		{
			spdlog::info("camera state 1 end");
			// 开启pid调焦？最佳焦点在[right_focus - step, right_focus + step]
			contrast_history.clear();
			for (int i = 0; i < 10; i++) 
			{
				contrast_history.push_back(current_contrast);
			}
			camera_state=0;
		}
	}
    }

    cap.release();
    cv::destroyAllWindows();

    return 0;
}       	
