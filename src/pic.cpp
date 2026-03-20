// Optimized code implementing morphological operations, adaptive thresholding, and robust fallback mechanism

#include <opencv2/opencv.hpp>
#include <vector>

void processFrame(const cv::Mat &frame) {
    cv::Mat processedFrame;
    cv::Mat grayFrame;

    // Convert to gray scale
    cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);

    // Adaptive Thresholding
    cv::adaptiveThreshold(grayFrame, processedFrame, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 11, 2);

    // Morphological operations to remove noise
    cv::Mat morphedFrame;
    cv::morphologyEx(processedFrame, morphedFrame, cv::MORPH_CLOSE, cv::Mat());

    // Fallback mechanism to read bounding box of blurry frames
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(morphedFrame, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto &contour : contours) {
        cv::Rect boundingBox = cv::boundingRect(contour);
        // Check if the bounding box is valid
        if (boundingBox.area() > 0) {
            // Process bounding box
            cv::rectangle(frame, boundingBox, cv::Scalar(0, 255, 0), 2);
        }
    }
}

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        return -1; // Fallback if camera cannot be opened
    }

    cv::Mat frame;
    while (cap.read(frame)) {
        processFrame(frame);
        cv::imshow("Processed Frame", frame);
        if (cv::waitKey(30) >= 0) break;
    }

    return 0;
}