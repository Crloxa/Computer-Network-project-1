#include"pic.h"


Mat preprocessImage(const Mat& src)
{
	Mat gray, blurred, edges;
	cvtColor(src, gray, COLOR_BGR2GRAY);// 转换为灰度图像，简化后续处理

	GaussianBlur(gray, blurred, Size(5, 5), 0);// 高斯模糊，减少噪声对边缘检测的影响

	Mat binary;
	adaptiveThreshold(gray, binary, 255, ADAPTIVE_THRESH_GAUSSIAN_C,
		THRESH_BINARY, 11, 2);

	//Canny(blurred, edges, 50, 150, 3);// Canny边缘检测，提取图像中的边缘信息
	return binary;
}

// 判断是否为QR码的特征点
bool IsQrPoint(vector<Point>& contour, Mat& img)
{
	double area = contourArea(contour);
	if (area < 30) return false;

	RotatedRect rect = minAreaRect(Mat(contour));
	double w = rect.size.width;
	double h = rect.size.height;
	double rate = min(w, h) / max(w, h);

	if (rate > 0.6)  // 放宽比例限制
	{
		// 这里可以添加更复杂的判断逻辑
		return true;
	}
	return false;
}

int QrParse()
{
	// 读取图片
	Mat src = imread("qrcode_4.jpeg");
	if (src.empty())
	{
		cout << "Could not read the image: " << endl;
		return 1;
	}

	float ratio = src.cols / 500.0f;// cols是宽，rows是高
	Mat orig = src.clone();// 备份原图
	int newHeight = static_cast<int>(src.rows / ratio);
	resize(src, src, Size(500, newHeight));

	vector<Point> center_all;

	Mat edges;
	edges = preprocessImage(src);
	imshow("Edges", edges);

	// 形态学操作
	Mat element = getStructuringElement(MORPH_RECT, Size(3, 3));
	morphologyEx(edges, edges, MORPH_CLOSE, element);
	imshow("Closed Edges", edges);

	vector<vector<Point>> contours;// 存储轮廓点的集合
	vector<Vec4i> hierarchy;// 存储轮廓层级信息
	findContours(edges, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);// 查找轮廓，RETR_EXTERNAL只检索外部轮廓
	cout << "Found " << contours.size() << " contours" << endl;
	int numOfRec = 0;

	int ic = 0;
	int parentIndex = -1;
	for (size_t i = 0; i < contours.size(); i++) {
		if (hierarchy[i][2] != -1 && ic == 0) {
			parentIndex = i;
			ic++;
		}
		else if (hierarchy[i][2] != -1) {
			ic++;
		}
		else if (hierarchy[i][2] == -1)
		{
			parentIndex = -1;
			ic = 0;
		}
		if (ic >= 2) {
			if (IsQrPoint(contours[parentIndex], src)) {
				RotatedRect rect = minAreaRect(contours[parentIndex]);

				Point2f vertices[4];
				rect.points(vertices);
				for (int j = 0; j < 4; j++) {
					line(src, vertices[j], vertices[(j + 1) % 4], Scalar(0, 255, 0), 2);
				}
				drawContours(src, contours, parentIndex, Scalar(255, 0, 0), 2);
				imshow("Detected QR Code", src);// 显示检测结果
				center_all.push_back(rect.center);
				numOfRec++;
			}
			ic = 0;
			parentIndex = -1;
		}
	}



	waitKey(0);
	return 0;
}


