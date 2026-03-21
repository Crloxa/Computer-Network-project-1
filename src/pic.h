#pragma once

#include <opencv2/opencv.hpp>

namespace ImgParse {

    /**
     * @brief 自定义二维码图像预处理与定位校正
     *
     * @param srcImg 输入原始视频帧，可能是任意分辨率，包��背景干扰、透视变形以及视频压缩伪影。
     * @param disImg 输出标准化后的二维码图像。严格规范：133x133，坐标零对齐，二值纯色。
     * @return bool 返回 0 (false) 表示成功，1 (true) 表示失败。
     */
    bool Main(const cv::Mat& srcImg, cv::Mat& disImg);

} // namespace ImgParse