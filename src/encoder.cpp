#include "encoder.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace demo_encoder {
namespace {

struct CarrierLayout {
    int canvas_pixels = 1440;
    int marker_margin = 0;
    int marker_size = 0;
    int qr_size = 0;
    int qr_x = 0;
    int qr_y = 0;
};

struct DecodeAttemptResult {
    bool success = false;
    std::string method;
    std::vector<uint8_t> frame_bytes;
    cv::Mat warped_frame;
    cv::Mat qr_crop;
    std::string message;
};

struct EncodeProfileKey {
    protocol_iso::ProfileId profile_id = protocol_iso::ProfileId::kIso133;
    protocol_iso::ErrorCorrection error_correction = protocol_iso::ErrorCorrection::kQ;

    bool operator<(const EncodeProfileKey& other) const {
        if (profile_id != other.profile_id) {
            return profile_id < other.profile_id;
        }
        return error_correction < other.error_correction;
    }
};

struct CarrierCacheKey {
    int canvas_pixels = 1440;
    bool enable_markers = true;

    bool operator<(const CarrierCacheKey& other) const {
        if (canvas_pixels != other.canvas_pixels) {
            return canvas_pixels < other.canvas_pixels;
        }
        return enable_markers < other.enable_markers;
    }
};

struct CarrierAssets {
    CarrierLayout layout;
    cv::Mat base_frame;
    cv::Mat marker;
    cv::Mat marker_br;
};

struct DecodeContext {
    cv::QRCodeDetector detector;
    cv::QRCodeDetectorAruco aruco_detector;

    DecodeContext() {
        detector.setUseAlignmentMarkers(true);
    }
};

std::string FrameFileName(std::size_t index) {
    std::ostringstream stream;
    stream << "frame_" << std::setw(5) << std::setfill('0') << index << ".png";
    return stream.str();
}

bool EnsureDirectory(const std::filesystem::path& path, std::string* error_message) {
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        return true;
    }
    if (std::filesystem::create_directories(path, error)) {
        return true;
    }
    if (error_message != nullptr) {
        *error_message = "Failed to create directory: " + path.string() + " (" + error.message() + ")";
    }
    return false;
}

CarrierLayout BuildCarrierLayout(const protocol_iso::EncoderOptions& options) {
    CarrierLayout layout;
    layout.canvas_pixels = options.canvas_pixels;
    layout.marker_margin = std::max(24, options.canvas_pixels / 28);
    layout.marker_size = std::max(72, options.canvas_pixels / 9);
    layout.qr_size = std::max(720, static_cast<int>(options.canvas_pixels * 0.78));
    layout.qr_x = (options.canvas_pixels - layout.qr_size) / 2;
    layout.qr_y = layout.qr_x;
    return layout;
}

cv::QRCodeEncoder::CorrectionLevel ToOpenCvCorrection(protocol_iso::ErrorCorrection error_correction) {
    switch (error_correction) {
        case protocol_iso::ErrorCorrection::kM:
            return cv::QRCodeEncoder::CorrectionLevel::CORRECT_LEVEL_M;
        case protocol_iso::ErrorCorrection::kQ:
            return cv::QRCodeEncoder::CorrectionLevel::CORRECT_LEVEL_Q;
        case protocol_iso::ErrorCorrection::kH:
            return cv::QRCodeEncoder::CorrectionLevel::CORRECT_LEVEL_H;
    }
    return cv::QRCodeEncoder::CorrectionLevel::CORRECT_LEVEL_Q;
}

EncodeProfileKey MakeEncodeProfileKey(const protocol_iso::EncoderOptions& options) {
    return {options.profile_id, options.error_correction};
}

CarrierCacheKey MakeCarrierCacheKey(const protocol_iso::EncoderOptions& options) {
    return {options.canvas_pixels, options.enable_carrier_markers};
}

cv::Mat RenderMarker(int size_pixels, bool invert_center);

cv::QRCodeEncoder::Params MakeQrParams(const protocol_iso::EncoderOptions& options) {
    const protocol_iso::Profile profile = protocol_iso::ProfileFromId(options.profile_id);
    cv::QRCodeEncoder::Params params;
    params.version = profile.version;
    params.correction_level = ToOpenCvCorrection(options.error_correction);
    params.mode = cv::QRCodeEncoder::EncodeMode::MODE_BYTE;
    params.structure_number = 1;
    return params;
}

cv::Ptr<cv::QRCodeEncoder> GetOrCreateEncoder(const protocol_iso::EncoderOptions& options) {
    static std::map<EncodeProfileKey, cv::Ptr<cv::QRCodeEncoder>> encoders;

    const EncodeProfileKey key = MakeEncodeProfileKey(options);
    const auto it = encoders.find(key);
    if (it != encoders.end()) {
        return it->second;
    }

    const cv::Ptr<cv::QRCodeEncoder> encoder = cv::QRCodeEncoder::create(MakeQrParams(options));
    encoders.emplace(key, encoder);
    return encoder;
}

CarrierAssets BuildCarrierAssets(const protocol_iso::EncoderOptions& options) {
    CarrierAssets assets;
    assets.layout = BuildCarrierLayout(options);
    assets.base_frame = cv::Mat(assets.layout.canvas_pixels,
                                assets.layout.canvas_pixels,
                                CV_8UC3,
                                cv::Scalar(255, 255, 255));

    if (options.enable_carrier_markers) {
        assets.marker = RenderMarker(assets.layout.marker_size, false);
        assets.marker_br = RenderMarker(assets.layout.marker_size, true);
        assets.marker.copyTo(assets.base_frame(cv::Rect(assets.layout.marker_margin,
                                                        assets.layout.marker_margin,
                                                        assets.layout.marker_size,
                                                        assets.layout.marker_size)));
        assets.marker.copyTo(assets.base_frame(cv::Rect(assets.layout.canvas_pixels - assets.layout.marker_margin - assets.layout.marker_size,
                                                        assets.layout.marker_margin,
                                                        assets.layout.marker_size,
                                                        assets.layout.marker_size)));
        assets.marker.copyTo(assets.base_frame(cv::Rect(assets.layout.marker_margin,
                                                        assets.layout.canvas_pixels - assets.layout.marker_margin - assets.layout.marker_size,
                                                        assets.layout.marker_size,
                                                        assets.layout.marker_size)));
        assets.marker_br.copyTo(assets.base_frame(cv::Rect(assets.layout.canvas_pixels - assets.layout.marker_margin - assets.layout.marker_size,
                                                           assets.layout.canvas_pixels - assets.layout.marker_margin - assets.layout.marker_size,
                                                           assets.layout.marker_size,
                                                           assets.layout.marker_size)));
    }

    return assets;
}

const CarrierAssets& GetOrCreateCarrierAssets(const protocol_iso::EncoderOptions& options) {
    static std::map<CarrierCacheKey, CarrierAssets> assets_by_key;

    const CarrierCacheKey key = MakeCarrierCacheKey(options);
    const auto it = assets_by_key.find(key);
    if (it != assets_by_key.end()) {
        return it->second;
    }

    auto inserted = assets_by_key.emplace(key, BuildCarrierAssets(options));
    return inserted.first->second;
}

std::string BytesToString(const std::vector<uint8_t>& bytes) {
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

std::vector<uint8_t> StringToBytes(const std::string& value) {
    return std::vector<uint8_t>(value.begin(), value.end());
}

cv::Mat RenderMarker(int size_pixels, bool invert_center) {
    cv::Mat marker(7, 7, CV_8UC1, cv::Scalar(255));
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 7; ++x) {
            const int distance = std::max(std::abs(x - 3), std::abs(y - 3));
            bool black = (distance == 3) || (distance <= 1);
            if (invert_center && x >= 2 && x <= 4 && y >= 2 && y <= 4) {
                black = !black;
            }
            marker.at<unsigned char>(y, x) = black ? 0U : 255U;
        }
    }

    cv::Mat scaled;
    cv::resize(marker, scaled, cv::Size(size_pixels, size_pixels), 0.0, 0.0, cv::INTER_NEAREST);
    cv::cvtColor(scaled, scaled, cv::COLOR_GRAY2BGR);
    return scaled;
}

bool TryEncodeQrBytes(const std::vector<uint8_t>& frame_bytes,
                      const protocol_iso::EncoderOptions& options,
                      cv::Mat* qr_frame,
                      std::string* error_message) {
    try {
        const cv::Ptr<cv::QRCodeEncoder> encoder = GetOrCreateEncoder(options);
        cv::Mat qr_image;
        encoder->encode(BytesToString(frame_bytes), qr_image);
        if (qr_image.empty()) {
            if (error_message != nullptr) {
                *error_message = "OpenCV QRCodeEncoder returned an empty ISO QR image.";
            }
            return false;
        }
        if (qr_image.type() != CV_8UC1) {
            cv::cvtColor(qr_image, qr_image, cv::COLOR_BGR2GRAY);
        }
        if (qr_frame != nullptr) {
            *qr_frame = qr_image;
        }
        return true;
    } catch (const cv::Exception& error) {
        if (error_message != nullptr) {
            *error_message = error.what();
        }
        return false;
    }
}

bool ValidateRenderedScale(const cv::Mat& qr_frame,
                           const protocol_iso::EncoderOptions& options,
                           std::string* error_message) {
    if (qr_frame.empty()) {
        if (error_message != nullptr) {
            *error_message = "QR frame is empty; cannot validate rendered module scale.";
        }
        return false;
    }

    const CarrierLayout layout = BuildCarrierLayout(options);
    const double module_pixels = static_cast<double>(layout.qr_size) / static_cast<double>(qr_frame.cols);
    if (module_pixels < 4.0) {
        if (error_message != nullptr) {
            std::ostringstream stream;
            stream << "Rendered module scale is too small for reliable screen capture: "
                   << std::fixed << std::setprecision(2) << module_pixels
                   << " px/module. Increase --canvas or use a smaller ISO profile.";
            *error_message = stream.str();
        }
        return false;
    }
    return true;
}

int DetermineMaxFrameBytes(const protocol_iso::EncoderOptions& options, std::string* error_message) {
    static std::map<EncodeProfileKey, int> cached_max_frame_bytes;

    const EncodeProfileKey key = MakeEncodeProfileKey(options);
    const auto cached = cached_max_frame_bytes.find(key);
    if (cached != cached_max_frame_bytes.end()) {
        return cached->second;
    }

    int low = protocol_iso::kFrameOverheadBytes;
    int high = 4096;
    int best = 0;

    while (low <= high) {
        const int candidate = low + (high - low) / 2;
        std::vector<uint8_t> test_bytes(static_cast<std::size_t>(candidate), 0x41U);
        cv::Mat qr_image;
        std::string encode_error;
        if (TryEncodeQrBytes(test_bytes, options, &qr_image, &encode_error)) {
            best = candidate;
            low = candidate + 1;
        } else {
            high = candidate - 1;
        }
    }

    if (best < protocol_iso::kFrameOverheadBytes) {
        if (error_message != nullptr) {
            *error_message = "Unable to encode even the ISO application header using the selected profile/ecc.";
        }
        return -1;
    }

    cached_max_frame_bytes.emplace(key, best);
    return best;
}

cv::Mat RenderCarrierFrame(const cv::Mat& qr_frame, const protocol_iso::EncoderOptions& options) {
    const CarrierAssets& assets = GetOrCreateCarrierAssets(options);
    const CarrierLayout& layout = assets.layout;

    cv::Mat carrier = assets.base_frame.clone();
    cv::Mat qr_scaled;
    cv::resize(qr_frame, qr_scaled, cv::Size(layout.qr_size, layout.qr_size), 0.0, 0.0, cv::INTER_NEAREST);
    cv::cvtColor(qr_scaled, qr_scaled, cv::COLOR_GRAY2BGR);
    qr_scaled.copyTo(carrier(cv::Rect(layout.qr_x, layout.qr_y, layout.qr_size, layout.qr_size)));
    return carrier;
}

cv::Mat RenderSampleLayout(const protocol_iso::EncoderOptions& options, const cv::Mat& qr_preview) {
    const CarrierLayout layout = BuildCarrierLayout(options);
    cv::Mat layout_image = RenderCarrierFrame(qr_preview, options);

    cv::rectangle(layout_image, cv::Rect(layout.qr_x, layout.qr_y, layout.qr_size, layout.qr_size),
                  cv::Scalar(40, 120, 220), 3);
    cv::putText(layout_image, "ISO QR payload region", cv::Point(layout.qr_x + 18, layout.qr_y - 16),
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(40, 120, 220), 2);
    if (options.enable_carrier_markers) {
        cv::putText(layout_image, "carrier marker", cv::Point(layout.marker_margin, layout.marker_margin - 12),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(30, 30, 30), 2);
        cv::putText(layout_image, "carrier marker", cv::Point(layout.canvas_pixels - layout.marker_margin - 180,
                                                              layout.marker_margin - 12),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(30, 30, 30), 2);
    }
    cv::putText(layout_image,
                "Outer carrier is custom; inner QR remains ISO standard with intact quiet zone.",
                cv::Point(30, layout.canvas_pixels - 30),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(20, 20, 20), 2);
    return layout_image;
}

std::vector<EncodedFrame> EncodeBytesToFrames(const std::vector<uint8_t>& bytes,
                                              const protocol_iso::EncoderOptions& options) {
    std::string error_message;
    const int max_frame_bytes = DetermineMaxFrameBytes(options, &error_message);
    if (max_frame_bytes < 0) {
        throw std::runtime_error(error_message);
    }

    const int max_payload_bytes = max_frame_bytes - protocol_iso::kFrameOverheadBytes;
    const std::size_t chunk_size = static_cast<std::size_t>(std::max(1, max_payload_bytes));
    const std::size_t total_frame_count = std::max<std::size_t>(1U, (bytes.size() + chunk_size - 1U) / chunk_size);
    if (total_frame_count > 0xFFFFU) {
        throw std::runtime_error("Input is too large for the current ISO transport header: total_frames exceeds 65535.");
    }
    const uint16_t total_frames = static_cast<uint16_t>(total_frame_count);

    std::vector<EncodedFrame> frames;
    frames.reserve(total_frames);

    if (bytes.empty()) {
        EncodedFrame frame;
        frame.header.total_frames = 1;
        frame.header.payload_len = 0;
        frame.frame_bytes = protocol_iso::PackFrameBytes(frame.header, {});
        if (!TryEncodeQrBytes(frame.frame_bytes, options, &frame.qr_frame, &error_message)) {
            throw std::runtime_error(error_message);
        }
        if (!ValidateRenderedScale(frame.qr_frame, options, &error_message)) {
            throw std::runtime_error(error_message);
        }
        frame.carrier_frame = RenderCarrierFrame(frame.qr_frame, options);
        frames.push_back(std::move(frame));
        return frames;
    }

    for (std::size_t offset = 0; offset < bytes.size(); offset += chunk_size) {
        const std::size_t remaining = bytes.size() - offset;
        const std::size_t payload_size = std::min<std::size_t>(remaining, chunk_size);

        EncodedFrame frame;
        frame.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                             bytes.begin() + static_cast<std::ptrdiff_t>(offset + payload_size));
        frame.header.frame_seq = static_cast<uint16_t>(frames.size());
        frame.header.total_frames = total_frames;
        frame.header.payload_len = static_cast<uint16_t>(frame.payload.size());
        frame.frame_bytes = protocol_iso::PackFrameBytes(frame.header, frame.payload);

        if (!TryEncodeQrBytes(frame.frame_bytes, options, &frame.qr_frame, &error_message)) {
            throw std::runtime_error("Failed to encode ISO QR frame " + std::to_string(frames.size()) + ": " +
                                     error_message);
        }
        if (!ValidateRenderedScale(frame.qr_frame, options, &error_message)) {
            throw std::runtime_error(error_message);
        }
        frame.carrier_frame = RenderCarrierFrame(frame.qr_frame, options);
        frames.push_back(std::move(frame));
    }

    return frames;
}

bool WriteManifest(const std::filesystem::path& manifest_path,
                   const std::vector<EncodedFrame>& frames,
                   const protocol_iso::EncoderOptions& options,
                   std::string* error_message) {
    std::ofstream stream(manifest_path);
    if (!stream.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to write manifest: " + manifest_path.string();
        }
        return false;
    }

    stream << "file\tframe_seq\ttotal_frames\tpayload_len\tprofile\tecc\tcanvas_px\tframe_bytes\n";
    for (std::size_t index = 0; index < frames.size(); ++index) {
        stream << FrameFileName(index) << '\t'
               << frames[index].header.frame_seq << '\t'
               << frames[index].header.total_frames << '\t'
               << frames[index].header.payload_len << '\t'
               << protocol_iso::ProfileName(options.profile_id) << '\t'
               << protocol_iso::ErrorCorrectionName(options.error_correction) << '\t'
               << options.canvas_pixels << '\t'
               << frames[index].frame_bytes.size() << '\n';
    }
    return true;
}

bool TryWriteVideoWithOpenCv(const std::vector<EncodedFrame>& frames,
                             const std::filesystem::path& temp_path,
                             const protocol_iso::EncoderOptions& options,
                             std::string* error_message) {
    if (frames.empty()) {
        if (error_message != nullptr) {
            *error_message = "No frames were generated for video output.";
        }
        return false;
    }

    const cv::Size frame_size(frames.front().carrier_frame.cols, frames.front().carrier_frame.rows);
    cv::VideoWriter writer;
    writer.open(temp_path.string(), cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                static_cast<double>(options.fps), frame_size, true);
    if (!writer.isOpened()) {
        if (error_message != nullptr) {
            *error_message = "OpenCV VideoWriter failed to open output: " + temp_path.string();
        }
        return false;
    }

    for (const EncodedFrame& frame : frames) {
        for (int repeat_index = 0; repeat_index < options.repeat; ++repeat_index) {
            writer.write(frame.carrier_frame);
        }
    }
    writer.release();
    return true;
}

std::string QuotePath(const std::filesystem::path& path) {
    return "\"" + path.string() + "\"";
}

std::filesystem::path FindBundledFfmpeg() {
#ifdef _WIN32
    const std::filesystem::path candidate = std::filesystem::current_path() / "ffmpeg" / "bin" / "ffmpeg.exe";
#else
    const std::filesystem::path candidate = std::filesystem::current_path() / "ffmpeg" / "bin" / "ffmpeg";
#endif
    if (std::filesystem::exists(candidate)) {
        return candidate;
    }
    return {};
}

bool TryTranscodeToYuv420p(const std::filesystem::path& input_path,
                           const std::filesystem::path& output_path,
                           std::string* status_message) {
    std::filesystem::path ffmpeg_path = FindBundledFfmpeg();
    std::string executable = ffmpeg_path.empty() ? "ffmpeg" : QuotePath(ffmpeg_path);
#ifdef _WIN32
    const char* redirect = " > nul 2>&1";
#else
    const char* redirect = " > /dev/null 2>&1";
#endif
    const std::string command = executable + " -y -i " + QuotePath(input_path) +
                                " -pix_fmt yuv420p " + QuotePath(output_path) + redirect;
    const int exit_code = std::system(command.c_str());
    if (exit_code == 0) {
        if (status_message != nullptr) {
            *status_message = "Video transcoded with ffmpeg to yuv420p.";
        }
        return true;
    }
    return false;
}

bool WriteVideo(const std::vector<EncodedFrame>& frames,
                const std::filesystem::path& output_dir,
                const protocol_iso::EncoderOptions& options,
                std::string* error_message) {
    const std::filesystem::path temp_path = output_dir / "demo_temp.mp4";
    const std::filesystem::path output_path = output_dir / "demo.mp4";

    if (!TryWriteVideoWithOpenCv(frames, temp_path, options, error_message)) {
        return false;
    }

    std::string status_message;
    if (TryTranscodeToYuv420p(temp_path, output_path, &status_message)) {
        std::error_code error;
        std::filesystem::remove(temp_path, error);
        std::ofstream(output_dir / "video_status.txt") << status_message << '\n';
        return true;
    }

    std::error_code remove_error;
    std::filesystem::remove(output_path, remove_error);
    std::error_code rename_error;
    std::filesystem::rename(temp_path, output_path, rename_error);
    if (rename_error) {
        if (error_message != nullptr) {
            *error_message = "Failed to finalize demo video: " + rename_error.message();
        }
        return false;
    }

    std::ofstream(output_dir / "video_status.txt")
        << "ffmpeg was not available; kept the OpenCV-generated MP4. Pixel format is not guaranteed to be yuv420p.\n";
    return true;
}

std::vector<uint8_t> MakeSamplePayload(std::size_t payload_size, uint8_t seed) {
    std::vector<uint8_t> payload(payload_size);
    for (std::size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<uint8_t>((seed + index) & 0xFFU);
    }
    return payload;
}

bool WriteDebugImageIfEnabled(const std::filesystem::path& path,
                              const cv::Mat& image,
                              bool enabled) {
    if (!enabled || image.empty()) {
        return true;
    }
    return cv::imwrite(path.string(), image);
}

cv::Point2f AnchorPointForCorner(const cv::RotatedRect& rect, int corner_index) {
    std::array<cv::Point2f, 4> points;
    rect.points(points.data());

    auto score = [&](const cv::Point2f& point) {
        switch (corner_index) {
            case 0:
                return point.x + point.y;
            case 1:
                return -point.x + point.y;
            case 2:
                return point.x - point.y;
            case 3:
                return -(point.x + point.y);
            default:
                return point.x + point.y;
        }
    };

    return *std::min_element(points.begin(), points.end(),
                             [&](const cv::Point2f& left, const cv::Point2f& right) {
                                 return score(left) < score(right);
                             });
}

bool TryWarpCarrier(const cv::Mat& frame,
                    const protocol_iso::EncoderOptions& options,
                    cv::Mat* warped_frame,
                    cv::Mat* qr_crop,
                    std::string* error_message) {
    if (!options.enable_carrier_markers) {
        if (error_message != nullptr) {
            *error_message = "Carrier markers are disabled.";
        }
        return false;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    std::array<std::optional<cv::RotatedRect>, 4> best_rects;
    std::array<double, 4> best_areas = {0.0, 0.0, 0.0, 0.0};

    for (const std::vector<cv::Point>& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area < frame.cols * frame.rows * 0.0015) {
            continue;
        }

        const cv::RotatedRect rect = cv::minAreaRect(contour);
        const float width = rect.size.width;
        const float height = rect.size.height;
        if (width < 20.0F || height < 20.0F) {
            continue;
        }

        const float aspect = width > height ? width / height : height / width;
        if (aspect > 1.35F) {
            continue;
        }

        const cv::Point2f center = rect.center;
        int corner_index = -1;
        if (center.x < frame.cols * 0.35F && center.y < frame.rows * 0.35F) {
            corner_index = 0;
        } else if (center.x > frame.cols * 0.65F && center.y < frame.rows * 0.35F) {
            corner_index = 1;
        } else if (center.x < frame.cols * 0.35F && center.y > frame.rows * 0.65F) {
            corner_index = 2;
        } else if (center.x > frame.cols * 0.65F && center.y > frame.rows * 0.65F) {
            corner_index = 3;
        }
        if (corner_index < 0) {
            continue;
        }

        if (area > best_areas[corner_index]) {
            best_areas[corner_index] = area;
            best_rects[corner_index] = rect;
        }
    }

    for (const auto& rect : best_rects) {
        if (!rect.has_value()) {
            if (error_message != nullptr) {
                *error_message = "Failed to detect all four carrier corner markers.";
            }
            return false;
        }
    }

    const CarrierLayout layout = BuildCarrierLayout(options);
    std::array<cv::Point2f, 4> src = {
        AnchorPointForCorner(*best_rects[0], 0),
        AnchorPointForCorner(*best_rects[1], 1),
        AnchorPointForCorner(*best_rects[2], 2),
        AnchorPointForCorner(*best_rects[3], 3),
    };
    std::array<cv::Point2f, 4> dst = {
        cv::Point2f(0.0F, 0.0F),
        cv::Point2f(static_cast<float>(layout.canvas_pixels - 1), 0.0F),
        cv::Point2f(0.0F, static_cast<float>(layout.canvas_pixels - 1)),
        cv::Point2f(static_cast<float>(layout.canvas_pixels - 1), static_cast<float>(layout.canvas_pixels - 1)),
    };

    const cv::Mat transform = cv::getPerspectiveTransform(src.data(), dst.data());
    cv::Mat warped;
    cv::warpPerspective(frame, warped, transform, cv::Size(layout.canvas_pixels, layout.canvas_pixels));

    if (warped_frame != nullptr) {
        *warped_frame = warped;
    }
    if (qr_crop != nullptr) {
        *qr_crop = warped(cv::Rect(layout.qr_x, layout.qr_y, layout.qr_size, layout.qr_size)).clone();
    }
    return true;
}

std::vector<uint8_t> TryDecodeWithDetector(DecodeContext* context,
                                           const cv::Mat& image,
                                           std::string* method,
                                           std::string* message) {
    cv::Mat straight_qr;
    const std::string decoded = context->detector.detectAndDecode(image, cv::noArray(), straight_qr);
    if (!decoded.empty()) {
        if (method != nullptr) {
            *method = "direct";
        }
        if (message != nullptr) {
            *message = "Decoded with QRCodeDetector.";
        }
        return StringToBytes(decoded);
    }

    const std::string fallback = context->aruco_detector.detectAndDecode(image, cv::noArray(), straight_qr);
    if (!fallback.empty()) {
        if (method != nullptr) {
            *method = "aruco";
        }
        if (message != nullptr) {
            *message = "Decoded with QRCodeDetectorAruco.";
        }
        return StringToBytes(fallback);
    }

    return {};
}

DecodeAttemptResult TryDecodeFrame(DecodeContext* context,
                                   const cv::Mat& frame,
                                   const protocol_iso::EncoderOptions& options,
                                   bool capture_debug_images) {
    DecodeAttemptResult result;

    std::string method;
    std::string message;
    std::vector<uint8_t> decoded = TryDecodeWithDetector(context, frame, &method, &message);
    if (!decoded.empty()) {
        result.success = true;
        result.method = method;
        result.message = message;
        result.frame_bytes = std::move(decoded);
        return result;
    }

    if (!options.enable_carrier_markers) {
        result.message = "Carrier markers are disabled.";
        return result;
    }

    cv::Mat warped;
    cv::Mat qr_crop;
    std::string warp_error;
    if (!TryWarpCarrier(frame, options, &warped, &qr_crop, &warp_error)) {
        result.message = warp_error;
        return result;
    }

    decoded = TryDecodeWithDetector(context, qr_crop, &method, &message);
    if (!decoded.empty()) {
        result.success = true;
        result.method = "warped-" + method;
        result.message = message;
        result.frame_bytes = std::move(decoded);
        if (capture_debug_images) {
            result.warped_frame = warped;
            result.qr_crop = qr_crop;
        }
        return result;
    }

    result.message = "Carrier warp succeeded, but QR decode still failed.";
    if (capture_debug_images) {
        result.warped_frame = warped;
        result.qr_crop = qr_crop;
    }
    return result;
}

std::vector<std::filesystem::path> CollectImageFrames(const std::filesystem::path& input_path) {
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(input_path)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string extension = entry.path().extension().string();
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

bool WriteDecodeReport(const std::filesystem::path& report_path,
                       const std::vector<DecodedFrameReport>& reports,
                       std::string* error_message) {
    std::ofstream stream(report_path);
    if (!stream.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to write decode report: " + report_path.string();
        }
        return false;
    }

    stream << "source_index\tsuccess\tprofile\tecc\tmethod\tframe_seq\ttotal_frames\tpayload_len\tmessage\n";
    for (const DecodedFrameReport& report : reports) {
        stream << report.source_index << '\t'
               << (report.success ? "true" : "false") << '\t'
               << report.profile << '\t'
               << report.ecc << '\t'
               << report.method << '\t'
               << report.frame_seq << '\t'
               << report.total_frames << '\t'
               << report.payload_len << '\t'
               << report.message << '\n';
    }
    return true;
}

bool WriteMissingFrames(const std::filesystem::path& missing_path,
                       const std::vector<uint16_t>& missing_frames,
                       std::string* error_message) {
    std::ofstream stream(missing_path);
    if (!stream.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to write missing frame list: " + missing_path.string();
        }
        return false;
    }

    stream << "missing_count=" << missing_frames.size() << '\n';
    for (uint16_t frame_seq : missing_frames) {
        stream << frame_seq << '\n';
    }
    return true;
}

bool WriteDecodeSummary(const std::filesystem::path& summary_path,
                        const std::string& status,
                        std::size_t decoded_frames,
                        uint16_t total_frames,
                        std::size_t output_bytes,
                        std::size_t missing_frames,
                        std::string* error_message) {
    std::ofstream stream(summary_path);
    if (!stream.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to write decode summary: " + summary_path.string();
        }
        return false;
    }

    stream << "status=" << status << '\n'
           << "decoded_frames=" << decoded_frames << '\n'
           << "total_frames=" << total_frames << '\n'
           << "output_bytes=" << output_bytes << '\n'
           << "missing_frames=" << missing_frames << '\n';
    return true;
}

}  // namespace

std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& input_path) {
    std::ifstream stream(input_path, std::ios::binary);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open input file: " + input_path.string());
    }

    stream.seekg(0, std::ios::end);
    const std::streamoff size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> bytes;
    if (size > 0) {
        bytes.resize(static_cast<std::size_t>(size));
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    }
    return bytes;
}

bool WriteIsoSamples(const std::filesystem::path& output_dir,
                     const protocol_iso::EncoderOptions& options,
                     std::string* error_message) {
    if (!EnsureDirectory(output_dir, error_message)) {
        return false;
    }

    std::ofstream manifest(output_dir / "sample_manifest.tsv");
    if (!manifest.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to write ISO sample manifest.";
        }
        return false;
    }
    manifest << "file\tprofile\tversion\tlogical_grid\tecc\tcanvas_px\tmax_frame_bytes\tmax_payload_bytes\n";

    std::ofstream capacity_manifest(output_dir / "sample_capacity.tsv");
    if (!capacity_manifest.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to write ISO sample capacity matrix.";
        }
        return false;
    }
    capacity_manifest << "profile\tversion\tlogical_grid\tecc\tmax_frame_bytes\tmax_payload_bytes\trecommended\n";

    for (const protocol_iso::Profile& profile : protocol_iso::SupportedProfiles()) {
        protocol_iso::EncoderOptions profile_options = options;
        profile_options.profile_id = profile.id;

        std::string capacity_error;
        const int max_frame_bytes = DetermineMaxFrameBytes(profile_options, &capacity_error);
        if (max_frame_bytes < 0) {
            if (error_message != nullptr) {
                *error_message = capacity_error;
            }
            return false;
        }

        const std::size_t sample_payload_len = static_cast<std::size_t>(std::min(32, max_frame_bytes - protocol_iso::kFrameOverheadBytes));
        EncodedFrame frame;
        frame.payload = MakeSamplePayload(sample_payload_len, static_cast<uint8_t>(profile.logical_size & 0xFFU));
        frame.header.total_frames = 1;
        frame.header.payload_len = static_cast<uint16_t>(frame.payload.size());
        frame.frame_bytes = protocol_iso::PackFrameBytes(frame.header, frame.payload);

        std::string encode_error;
        if (!TryEncodeQrBytes(frame.frame_bytes, profile_options, &frame.qr_frame, &encode_error)) {
            if (error_message != nullptr) {
                *error_message = encode_error;
            }
            return false;
        }
        if (!ValidateRenderedScale(frame.qr_frame, profile_options, &encode_error)) {
            if (error_message != nullptr) {
                *error_message = encode_error;
            }
            return false;
        }
        frame.carrier_frame = RenderCarrierFrame(frame.qr_frame, profile_options);

        const std::string qr_name = "sample_" + protocol_iso::ProfileName(profile.id) + "_symbol.png";
        const std::string carrier_name = "sample_" + protocol_iso::ProfileName(profile.id) + "_carrier.png";
        const std::string layout_name = "sample_" + protocol_iso::ProfileName(profile.id) + "_layout.png";
        cv::imwrite((output_dir / qr_name).string(), frame.qr_frame);
        cv::imwrite((output_dir / carrier_name).string(), frame.carrier_frame);
        cv::imwrite((output_dir / layout_name).string(), RenderSampleLayout(profile_options, frame.qr_frame));

        manifest << qr_name << '\t'
                 << profile.name << '\t'
                 << profile.version << '\t'
                 << profile.logical_size << '\t'
                 << protocol_iso::ErrorCorrectionName(profile_options.error_correction) << '\t'
                 << profile_options.canvas_pixels << '\t'
                 << max_frame_bytes << '\t'
                 << (max_frame_bytes - protocol_iso::kFrameOverheadBytes) << '\n';

        for (const protocol_iso::ErrorCorrection error_correction : protocol_iso::SupportedErrorCorrections()) {
            protocol_iso::EncoderOptions capacity_options = profile_options;
            capacity_options.error_correction = error_correction;
            std::string matrix_error;
            const int matrix_frame_bytes = DetermineMaxFrameBytes(capacity_options, &matrix_error);
            if (matrix_frame_bytes < 0) {
                if (error_message != nullptr) {
                    *error_message = matrix_error;
                }
                return false;
            }

            capacity_manifest << profile.name << '\t'
                              << profile.version << '\t'
                              << profile.logical_size << '\t'
                              << protocol_iso::ErrorCorrectionName(error_correction) << '\t'
                              << matrix_frame_bytes << '\t'
                              << (matrix_frame_bytes - protocol_iso::kFrameOverheadBytes) << '\t'
                              << ((profile.id == protocol_iso::ProfileId::kIso133 &&
                                   error_correction == protocol_iso::ErrorCorrection::kQ)
                                      ? "true"
                                      : "false")
                              << '\n';
        }
    }

    return true;
}

bool WriteIsoPackage(const std::filesystem::path& input_path,
                     const std::filesystem::path& output_dir,
                     const protocol_iso::EncoderOptions& options,
                     std::string* error_message) {
    if (!EnsureDirectory(output_dir, error_message)) {
        return false;
    }

    const std::filesystem::path qr_dir = output_dir / "frames" / "qr";
    const std::filesystem::path carrier_dir = output_dir / "frames" / "carrier";
    const std::filesystem::path sample_dir = output_dir / "protocol_samples";
    if (!EnsureDirectory(qr_dir, error_message) ||
        !EnsureDirectory(carrier_dir, error_message) ||
        (options.write_protocol_samples && !EnsureDirectory(sample_dir, error_message))) {
        return false;
    }

    const std::vector<uint8_t> input_bytes = ReadFileBytes(input_path);
    std::vector<EncodedFrame> frames;
    try {
        frames = EncodeBytesToFrames(input_bytes, options);
    } catch (const std::exception& error) {
        if (error_message != nullptr) {
            *error_message = error.what();
        }
        return false;
    }

    if (options.write_protocol_samples &&
        !WriteIsoSamples(sample_dir, options, error_message)) {
        return false;
    }

    for (std::size_t index = 0; index < frames.size(); ++index) {
        cv::imwrite((qr_dir / FrameFileName(index)).string(), frames[index].qr_frame);
        cv::imwrite((carrier_dir / FrameFileName(index)).string(), frames[index].carrier_frame);
    }

    if (!WriteManifest(output_dir / "frame_manifest.tsv", frames, options, error_message)) {
        return false;
    }

    std::string capacity_error;
    const int max_frame_bytes = DetermineMaxFrameBytes(options, &capacity_error);
    if (max_frame_bytes < 0) {
        if (error_message != nullptr) {
            *error_message = capacity_error;
        }
        return false;
    }

    const protocol_iso::Profile profile = protocol_iso::ProfileFromId(options.profile_id);
    std::ofstream(output_dir / "input_info.txt")
        << "protocol=ISO/IEC 18004 QR Code Model 2\n"
        << "profile=" << profile.name << '\n'
        << "version=" << profile.version << '\n'
        << "logical_grid=" << profile.logical_size << '\n'
        << "ecc=" << protocol_iso::ErrorCorrectionName(options.error_correction) << '\n'
        << "canvas_px=" << options.canvas_pixels << '\n'
        << "input_path=" << input_path.string() << '\n'
        << "input_bytes=" << input_bytes.size() << '\n'
        << "frame_count=" << frames.size() << '\n'
        << "max_frame_bytes=" << max_frame_bytes << '\n'
        << "max_payload_bytes=" << (max_frame_bytes - protocol_iso::kFrameOverheadBytes) << '\n'
        << "fps=" << options.fps << '\n'
        << "repeat=" << options.repeat << '\n'
        << "carrier_markers=" << (options.enable_carrier_markers ? "true" : "false") << '\n'
        << "protocol_samples=" << (options.write_protocol_samples ? "true" : "false") << '\n';

    return WriteVideo(frames, output_dir, options, error_message);
}

bool DecodeIsoPackage(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_dir,
                      const protocol_iso::EncoderOptions& options,
                      std::string* error_message) {
    if (!EnsureDirectory(output_dir, error_message)) {
        return false;
    }

    const std::filesystem::path source_dir = output_dir / "decode_debug" / "source";
    const std::filesystem::path warped_dir = output_dir / "decode_debug" / "warped";
    const std::filesystem::path crop_dir = output_dir / "decode_debug" / "qr_crop";
    if (options.write_decode_debug &&
        (!EnsureDirectory(source_dir, error_message) ||
         !EnsureDirectory(warped_dir, error_message) ||
         !EnsureDirectory(crop_dir, error_message))) {
        return false;
    }

    std::vector<DecodedFrameReport> reports;
    std::vector<uint16_t> missing_frames;

    auto finalizeDecode = [&](const std::string& status,
                              std::size_t decoded_frames,
                              uint16_t total_frames,
                              std::size_t output_bytes,
                              const std::string& final_error) {
        if (!WriteDecodeReport(output_dir / "decode_report.tsv", reports, error_message)) {
            return false;
        }
        if (!WriteDecodeSummary(output_dir / "decode_summary.txt",
                                status,
                                decoded_frames,
                                total_frames,
                                output_bytes,
                                missing_frames.size(),
                                error_message)) {
            return false;
        }
        if (!missing_frames.empty() &&
            !WriteMissingFrames(output_dir / "missing_frames.txt", missing_frames, error_message)) {
            return false;
        }
        if (!final_error.empty() && error_message != nullptr) {
            *error_message = final_error;
        }
        return final_error.empty();
    };

    DecodeContext decode_context;
    std::optional<uint16_t> expected_total_frames;
    std::vector<std::vector<uint8_t>> payloads_by_seq;
    std::vector<bool> payload_present;
    std::size_t decoded_frame_count = 0U;
    std::size_t source_frame_count = 0U;

    auto processFrame = [&](const cv::Mat& frame) {
        const std::size_t index = source_frame_count++;
        WriteDebugImageIfEnabled(source_dir / FrameFileName(index), frame, options.write_decode_debug);

        DecodeAttemptResult attempt = TryDecodeFrame(&decode_context, frame, options, options.write_decode_debug);
        DecodedFrameReport report;
        report.source_index = static_cast<int>(index);
        report.profile = protocol_iso::ProfileName(options.profile_id);
        report.ecc = protocol_iso::ErrorCorrectionName(options.error_correction);
        report.method = attempt.method.empty() ? "none" : attempt.method;
        report.message = attempt.message;

        WriteDebugImageIfEnabled(warped_dir / FrameFileName(index), attempt.warped_frame, options.write_decode_debug);
        WriteDebugImageIfEnabled(crop_dir / FrameFileName(index), attempt.qr_crop, options.write_decode_debug);

        if (!attempt.success) {
            report.message = report.message.empty() ? "decode_failed" : "decode_failed: " + report.message;
            reports.push_back(std::move(report));
            return;
        }

        protocol_iso::FrameHeader header;
        std::vector<uint8_t> payload;
        std::string parse_error;
        if (!protocol_iso::ParseFrameBytes(attempt.frame_bytes, &header, &payload, &parse_error)) {
            report.message = parse_error;
            reports.push_back(std::move(report));
            return;
        }

        report.success = true;
        report.frame_seq = header.frame_seq;
        report.total_frames = header.total_frames;
        report.payload_len = header.payload_len;
        report.message = protocol_iso::HeaderToString(header);

        if (!expected_total_frames.has_value()) {
            expected_total_frames = header.total_frames;
            payloads_by_seq.resize(header.total_frames);
            payload_present.assign(header.total_frames, false);
        } else if (expected_total_frames.value() != header.total_frames) {
            report.success = false;
            report.message = "total_frames_conflict";
            reports.push_back(std::move(report));
            return;
        }

        if (header.frame_seq >= expected_total_frames.value()) {
            report.success = false;
            report.message = "frame_seq_out_of_range";
            reports.push_back(std::move(report));
            return;
        }

        if (!payload_present[header.frame_seq]) {
            payloads_by_seq[header.frame_seq] = std::move(payload);
            payload_present[header.frame_seq] = true;
            ++decoded_frame_count;
        } else {
            report.success = false;
            report.message = "duplicate_frame";
        }

        reports.push_back(std::move(report));
    };

    if (std::filesystem::is_directory(input_path)) {
        for (const std::filesystem::path& path : CollectImageFrames(input_path)) {
            cv::Mat frame = cv::imread(path.string(), cv::IMREAD_COLOR);
            if (!frame.empty()) {
                processFrame(frame);
            }
        }
    } else {
        cv::VideoCapture capture(input_path.string());
        if (!capture.isOpened()) {
            return finalizeDecode("read_error", 0U, 0U, 0U, "Failed to open input video: " + input_path.string());
        }

        cv::Mat frame;
        while (capture.read(frame)) {
            if (!frame.empty()) {
                processFrame(frame);
            }
        }
    }

    if (source_frame_count == 0U) {
        return finalizeDecode("read_error", 0U, 0U, 0U, "No frames could be read from the decode input.");
    }

    if (!expected_total_frames.has_value()) {
        return finalizeDecode("no_valid_frames", 0U, 0U, 0U, "No valid ISO frames were decoded from the input.");
    }

    for (uint16_t frame_seq = 0; frame_seq < expected_total_frames.value(); ++frame_seq) {
        if (!payload_present[frame_seq]) {
            missing_frames.push_back(frame_seq);
        }
    }

    if (!missing_frames.empty()) {
        return finalizeDecode("missing_frames",
                              decoded_frame_count,
                              expected_total_frames.value(),
                              0U,
                              "Decoded ISO frames are incomplete; see missing_frames.txt.");
    }

    std::vector<uint8_t> output_bytes;
    std::size_t output_bytes_size = 0U;
    for (const std::vector<uint8_t>& payload : payloads_by_seq) {
        output_bytes_size += payload.size();
    }
    output_bytes.reserve(output_bytes_size);
    for (uint16_t frame_seq = 0; frame_seq < expected_total_frames.value(); ++frame_seq) {
        const std::vector<uint8_t>& payload = payloads_by_seq[frame_seq];
        output_bytes.insert(output_bytes.end(), payload.begin(), payload.end());
    }

    const std::filesystem::path output_file = output_dir / "output.bin";
    std::ofstream stream(output_file, std::ios::binary);
    if (!stream.is_open()) {
        return finalizeDecode("write_error",
                              decoded_frame_count,
                              expected_total_frames.value(),
                              0U,
                              "Failed to write decoded output file: " + output_file.string());
    }
    stream.write(reinterpret_cast<const char*>(output_bytes.data()),
                 static_cast<std::streamsize>(output_bytes.size()));
    stream.close();
    return finalizeDecode("success",
                          decoded_frame_count,
                          expected_total_frames.value(),
                          output_bytes.size(),
                          "");
}

}  // namespace demo_encoder
