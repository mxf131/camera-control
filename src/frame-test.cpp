#include <opencv2/opencv.hpp>
#include <chrono>

int main()
{
	cv::VideoCapture cap;
	cap.open(0, cv::CAP_V4L);
	// cap.open("http://admin:123456@192.168.31.233:8081/video"); // 打开默认的摄像头（编号为0）
	if (!cap.isOpened()) {
    		std::cerr << "Error: Could not open camera." << std::endl;
    		return -1;
	}
	cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
	cap.set(cv::CAP_PROP_FPS, 30);
	cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
  	cap.set(cv::CAP_PROP_FRAME_HEIGHT,720);
	
	double fps = cap.get(cv::CAP_PROP_FPS);
  	double width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
  	double height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
  	
	std::cout << "fps : "<< fps << ", width : " << width << ", height : " << height << std::endl;;

	cv::Mat frame;
	
	int frame_count = 0;
    	auto start_time = std::chrono::high_resolution_clock::now();
	
	while (true) 
	{
    		if (!cap.read(frame)) 
		{
        		break;
    		}

    		// 在这里可以对帧进行处理

    		if (cv::waitKey(1) == 'q') 
		{
        		break;
    		}
		frame_count++;

        	auto current_time = std::chrono::high_resolution_clock::now();
        	auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

        	if (elapsed_time >= 1) {
            		std::cout << "Frames read in 1 second: " << frame_count << std::endl;
            		start_time = current_time;
            		frame_count = 0;
        	}
	}

	cap.release();
	cv::destroyAllWindows();

	return 0;

}
