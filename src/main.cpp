#include <opencv2/opencv.hpp>
#include <iostream>
#include <consoleapi2.h>

using namespace cv;
using namespace std;

int main() {
    // 1. 创建一个 400x400 的黑色画布 (8位3通道)
    Mat image = Mat::zeros(400, 400, CV_8UC3);

    // 2. 在图像中心画一个圆
    // 参数：原图, 中心点, 半径, 颜色(BGR), 粗细
    circle(image, Point(200, 200), 100, Scalar(0, 255, 255), 3);

    // 3. 在图像上写字
    putText(image, "OpenCV Config Success!", Point(50, 50),
        FONT_HERSHEY_SIMPLEX, 0.8, Scalar(255, 255, 255), 2);

    // 4. 创建窗口并显示
    namedWindow("OpenCV Test", WINDOW_AUTOSIZE);
    imshow("OpenCV Test", image);

    cout << "OpenCV 版本: " << CV_VERSION << endl;
    cout << "按下任意键退出程序..." << endl;

    // 5. 等待按键，否则窗口会一闪而过
    waitKey(0);
    destroyAllWindows();

    return 0;
}