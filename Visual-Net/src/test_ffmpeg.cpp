#include "ffmpeg.h"
#include <iostream>

using namespace std;

void testFFmpegFunctions()
{
    cout << "===== 开始测试 FFmpeg 功能 ======" << endl;
    
    // 测试1: 图片转视频（使用指定的frames目录）
    cout << "\n--- 测试1: 图片转视频 ---" << endl;
    const char* inputImagePath = "..\\..\\artifacts\\vn-test\\frames";  // 输入图片目录（相对路径）
    const char* imageFormat = "jpg";           // 图片格式
    const char* outputVideoPath = "output_video2.mp4";  // 输出视频文件
    unsigned rawFrameRates = 30;           // 输入帧率
    unsigned outputFrameRates = 30;       // 输出帧率
    unsigned kbps = 2000;                  // 码率 (kbps)
    
    int result1 = FFMPEG::ImagetoVideo(inputImagePath, imageFormat, outputVideoPath, 
                                        rawFrameRates, outputFrameRates, kbps);
    if (result1 == 0) {
        cout << "图片转视频成功！视频保存在: " << outputVideoPath << endl;
    } else {
        cout << "图片转视频失败，错误码: " << result1 << endl;
    }
    
    // 测试2: 视频转图片（使用指定的encoded.mp4，转成1064*1064）
    cout << "\n--- 测试2: 视频转图片（1064*1064） ---" << endl;
    const char* videoPath = "..\\..\\artifacts\\vn-test\\encoded.mp4";  // 输入视频文件（相对路径）
    const char* videoToImagePath = "video_frames_1064x1064";   // 输出图片目录
    int resizeWidth = 1064;     // 目标宽度
    int resizeHeight = 1064;    // 目标高度
    
    int result2 = FFMPEG::VideotoImage(videoPath, videoToImagePath, imageFormat, resizeWidth, resizeHeight);
    if (result2 == 0) {
        cout << "视频转图片成功！图片保存在: " << videoToImagePath << endl;
    } else {
        cout << "视频转图片失败，错误码: " << result2 << endl;
    }
    
    // 测试3: 把拆出来的第一张图片修改为600*800大小
    cout << "\n--- 测试3: 图片调整为600*800 ---" << endl;
    const char* inputImage = "video_frames_1064x1064\\00001.jpg";  // 输入图片（视频转图片后的第一张）
    const char* outputScaledImage = "scaled_image_600x800.jpg";   // 输出缩放后的图片
    int targetWidth = 800;     // 目标宽度
    int targetHeight = 600;    // 目标高度
    
    int result3 = FFMPEG::ScaleImage(inputImage, outputScaledImage, targetWidth, targetHeight);
    if (result3 == 0) {
        cout << "图片缩放成功！缩放后的图片保存在: " << outputScaledImage << endl;
    } else {
        cout << "图片缩放失败，错误码: " << result3 << endl;
    }
    
    cout << "\n===== FFmpeg 功能测试完成 =====" << endl;
}

int main()
{
    testFFmpegFunctions();
    return 0;
}