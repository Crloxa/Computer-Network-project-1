#include "encoder.h"

#include "simple_image.h"
#include "qr_iso.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace demo_encoder {
namespace {

constexpr int kMaxFrameBytes = qr_iso::kMaxFrameBytes;
constexpr int kMaxPayloadBytes = kMaxFrameBytes - protocol_iso::kFrameOverheadBytes;

struct CarrierLayout {
    int canvas_pixels = 1440;
    int marker_margin = 0;
    int marker_size = 0;
    int qr_size = 0;
    int qr_x = 0;
    int qr_y = 0;
};

struct EncodedFrame {
    protocol_iso::FrameHeader header;
    std::vector<uint8_t> payload;
    std::vector<uint8_t> frame_bytes;
    image_io::RgbImage qr_frame;
    image_io::RgbImage carrier_frame;
};

struct MarkerBox {
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
    int pixels = 0;

    int width() const { return max_x - min_x + 1; }
    int height() const { return max_y - min_y + 1; }
};

struct DecodeAttemptResult {
    bool success = false;
    std::string method;
    std::string message;
    std::vector<uint8_t> frame_bytes;
    image_io::RgbImage normalized_frame;
    image_io::RgbImage qr_crop;
};

struct ImageFile {
    std::filesystem::path source_path;
    std::filesystem::path bmp_path;
};

std::string FrameFileName(std::size_t index, const std::string& extension = ".bmp") {
    std::ostringstream stream;
    stream << "frame_" << std::setw(5) << std::setfill('0') << index << extension;
    return stream.str();
}

std::string QuotePath(const std::filesystem::path& path) {
    return "\"" + path.string() + "\"";
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

bool CommandExists(const std::string& command) {
#ifdef _WIN32
    const std::string probe = "where " + command + " > nul 2>&1";
#else
    const std::string probe = "command -v " + command + " >/dev/null 2>&1";
#endif
    return std::system(probe.c_str()) == 0;
}

std::string RedirectOutput() {
#ifdef _WIN32
    return " > nul 2>&1";
#else
    return " > /dev/null 2>&1";
#endif
}

bool RunCommand(const std::string& command) {
    return std::system(command.c_str()) == 0;
}

std::optional<std::string> FindFfmpegCommand() {
    const std::filesystem::path bundled = FindBundledFfmpeg();
    if (!bundled.empty()) {
        return QuotePath(bundled);
    }
    if (CommandExists("ffmpeg")) {
        return std::string("ffmpeg");
    }
    return std::nullopt;
}

std::optional<std::string> FindImageConverterCommand() {
#ifndef _WIN32
    if (CommandExists("sips")) {
        return std::string("sips");
    }
#endif
    const std::optional<std::string> ffmpeg = FindFfmpegCommand();
    if (ffmpeg.has_value()) {
        return ffmpeg;
    }
    return std::nullopt;
}

bool ValidateOptions(const protocol_iso::EncoderOptions& options, std::string* error_message) {
    if (options.profile_id != protocol_iso::ProfileId::kIso133) {
        if (error_message != nullptr) {
            *error_message = "当前自研 isoqrv2 首版仅支持 --profile iso133。";
        }
        return false;
    }
    if (options.error_correction != protocol_iso::ErrorCorrection::kQ) {
        if (error_message != nullptr) {
            *error_message = "当前自研 isoqrv2 首版仅支持 --ecc Q。";
        }
        return false;
    }
    if (!options.enable_carrier_markers) {
        if (error_message != nullptr) {
            *error_message = "当前自研解码链路依赖 carrier marker，--markers 只能为 on。";
        }
        return false;
    }
    if (options.canvas_pixels < 720) {
        if (error_message != nullptr) {
            *error_message = "--canvas must be at least 720.";
        }
        return false;
    }
    if (options.fps <= 0 || options.repeat <= 0) {
        if (error_message != nullptr) {
            *error_message = "--fps and --repeat must be positive.";
        }
        return false;
    }
    return true;
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

image_io::RgbImage RenderMarker(int size_pixels, bool invert_center) {
    image_io::RgbImage marker(7, 7, image_io::kWhite);
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 7; ++x) {
            const int distance = std::max(std::abs(x - 3), std::abs(y - 3));
            bool black = (distance == 3) || (distance <= 1);
            if (invert_center && x >= 2 && x <= 4 && y >= 2 && y <= 4) {
                black = !black;
            }
            marker.set(x, y, black ? image_io::kBlack : image_io::kWhite);
        }
    }
    return image_io::ResizeNearest(marker, size_pixels, size_pixels);
}

image_io::RgbImage RenderQrModules(const image_io::BitMatrix& modules) {
    image_io::RgbImage image(qr_iso::kRenderSize, qr_iso::kRenderSize, image_io::kWhite);
    for (int y = 0; y < qr_iso::kSymbolSize; ++y) {
        for (int x = 0; x < qr_iso::kSymbolSize; ++x) {
            if (modules.get(x, y)) {
                image.set(x + qr_iso::kQuietZoneModules, y + qr_iso::kQuietZoneModules, image_io::kBlack);
            }
        }
    }
    return image;
}

bool ValidateRenderedScale(const protocol_iso::EncoderOptions& options, std::string* error_message) {
    const CarrierLayout layout = BuildCarrierLayout(options);
    const double module_pixels = static_cast<double>(layout.qr_size) / static_cast<double>(qr_iso::kRenderSize);
    if (module_pixels < 4.0) {
        if (error_message != nullptr) {
            std::ostringstream stream;
            stream << "Rendered module scale is too small: "
                   << std::fixed << std::setprecision(2) << module_pixels
                   << " px/module. Increase --canvas.";
            *error_message = stream.str();
        }
        return false;
    }
    return true;
}

image_io::RgbImage RenderCarrierFrame(const image_io::RgbImage& qr_frame,
                                      const protocol_iso::EncoderOptions& options) {
    const CarrierLayout layout = BuildCarrierLayout(options);
    image_io::RgbImage carrier(layout.canvas_pixels, layout.canvas_pixels, image_io::kWhite);
    const image_io::RgbImage marker = RenderMarker(layout.marker_size, false);
    const image_io::RgbImage marker_br = RenderMarker(layout.marker_size, true);

    const image_io::RgbImage qr_scaled = image_io::ResizeNearest(qr_frame, layout.qr_size, layout.qr_size);
    image_io::Blit(qr_scaled, &carrier, layout.qr_x, layout.qr_y);
    image_io::Blit(marker, &carrier, layout.marker_margin, layout.marker_margin);
    image_io::Blit(marker, &carrier,
                   layout.canvas_pixels - layout.marker_margin - layout.marker_size,
                   layout.marker_margin);
    image_io::Blit(marker, &carrier,
                   layout.marker_margin,
                   layout.canvas_pixels - layout.marker_margin - layout.marker_size);
    image_io::Blit(marker_br, &carrier,
                   layout.canvas_pixels - layout.marker_margin - layout.marker_size,
                   layout.canvas_pixels - layout.marker_margin - layout.marker_size);
    return carrier;
}

image_io::RgbImage RenderLayoutPreview(const protocol_iso::EncoderOptions& options,
                                       const image_io::RgbImage& qr_frame) {
    const CarrierLayout layout = BuildCarrierLayout(options);
    image_io::RgbImage preview = RenderCarrierFrame(qr_frame, options);
    image_io::FillRect(&preview, layout.qr_x, layout.qr_y, layout.qr_size, 3, image_io::kBlue);
    image_io::FillRect(&preview, layout.qr_x, layout.qr_y + layout.qr_size - 3, layout.qr_size, 3, image_io::kBlue);
    image_io::FillRect(&preview, layout.qr_x, layout.qr_y, 3, layout.qr_size, image_io::kBlue);
    image_io::FillRect(&preview, layout.qr_x + layout.qr_size - 3, layout.qr_y, 3, layout.qr_size, image_io::kBlue);
    return preview;
}

bool ConvertBmpToPng(const std::filesystem::path& bmp_path,
                     const std::filesystem::path& png_path,
                     std::string* error_message) {
    const std::optional<std::string> command = FindImageConverterCommand();
    if (!command.has_value()) {
        if (error_message != nullptr) {
            *error_message = "No image converter was found (ffmpeg or sips).";
        }
        return false;
    }

    std::string invocation;
    if (command.value() == "sips") {
#ifdef _WIN32
        invocation.clear();
#else
        invocation = "sips -s format png " + QuotePath(bmp_path) + " --out " + QuotePath(png_path) + RedirectOutput();
#endif
    } else {
        invocation = command.value() + " -y -i " + QuotePath(bmp_path) + " " + QuotePath(png_path) + RedirectOutput();
    }
    if (invocation.empty() || !RunCommand(invocation)) {
        if (error_message != nullptr) {
            *error_message = "Failed to convert BMP to PNG: " + png_path.string();
        }
        return false;
    }
    return true;
}

bool ConvertImageToBmp(const std::filesystem::path& input_path,
                       const std::filesystem::path& output_path,
                       std::string* error_message) {
    const std::optional<std::string> command = FindImageConverterCommand();
    if (!command.has_value()) {
        if (error_message != nullptr) {
            *error_message = "No image converter was found (ffmpeg or sips).";
        }
        return false;
    }

    std::string invocation;
    if (command.value() == "sips") {
#ifdef _WIN32
        invocation.clear();
#else
        invocation = "sips -s format bmp " + QuotePath(input_path) + " --out " + QuotePath(output_path) + RedirectOutput();
#endif
    } else {
        invocation = command.value() + " -y -i " + QuotePath(input_path) + " " + QuotePath(output_path) + RedirectOutput();
    }
    if (invocation.empty() || !RunCommand(invocation)) {
        if (error_message != nullptr) {
            *error_message = "Failed to convert image to BMP: " + input_path.string();
        }
        return false;
    }
    return true;
}

bool WriteImageArtifacts(const std::filesystem::path& image_dir,
                         const std::filesystem::path& png_dir,
                         const std::string& file_name,
                         const image_io::RgbImage& image,
                         std::string* error_message) {
    if (!EnsureDirectory(image_dir, error_message)) {
        return false;
    }
    const std::filesystem::path bmp_path = image_dir / file_name;
    if (!image_io::WriteBmp(bmp_path, image, error_message)) {
        return false;
    }

    if (!png_dir.empty()) {
        if (!EnsureDirectory(png_dir, error_message)) {
            return false;
        }
        const std::filesystem::path png_path = png_dir / (std::filesystem::path(file_name).stem().string() + ".png");
        std::string ignored_error;
        ConvertBmpToPng(bmp_path, png_path, &ignored_error);
    }
    return true;
}

std::vector<uint8_t> ReadFileBytesInternal(const std::filesystem::path& input_path) {
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

std::vector<uint8_t> MakeSamplePayload(std::size_t payload_size, uint8_t seed) {
    std::vector<uint8_t> payload(payload_size);
    for (std::size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<uint8_t>((seed + index) & 0xFFU);
    }
    return payload;
}

std::vector<EncodedFrame> EncodeBytesToFrames(const std::vector<uint8_t>& bytes,
                                              const protocol_iso::EncoderOptions& options) {
    std::string error_message;
    if (!ValidateRenderedScale(options, &error_message)) {
        throw std::runtime_error(error_message);
    }

    const std::size_t chunk_size = static_cast<std::size_t>(kMaxPayloadBytes);
    const std::size_t total_frame_count = std::max<std::size_t>(1U, (bytes.size() + chunk_size - 1U) / chunk_size);
    if (total_frame_count > 0xFFFFU) {
        throw std::runtime_error("Input is too large for the current transport header: total_frames exceeds 65535.");
    }

    std::vector<EncodedFrame> frames;
    frames.reserve(total_frame_count);

    auto encodeOne = [&](const std::vector<uint8_t>& payload, uint16_t frame_seq, uint16_t total_frames) {
        EncodedFrame frame;
        frame.payload = payload;
        frame.header.frame_seq = frame_seq;
        frame.header.total_frames = total_frames;
        frame.header.payload_len = static_cast<uint16_t>(payload.size());
        frame.frame_bytes = protocol_iso::PackFrameBytes(frame.header, frame.payload);

        image_io::BitMatrix modules;
        if (!qr_iso::EncodeBytes(frame.frame_bytes, &modules, nullptr, &error_message)) {
            throw std::runtime_error(error_message);
        }
        frame.qr_frame = RenderQrModules(modules);
        frame.carrier_frame = RenderCarrierFrame(frame.qr_frame, options);
        frames.push_back(std::move(frame));
    };

    if (bytes.empty()) {
        encodeOne({}, 0U, 1U);
        return frames;
    }

    for (std::size_t offset = 0; offset < bytes.size(); offset += chunk_size) {
        const std::size_t payload_size = std::min<std::size_t>(bytes.size() - offset, chunk_size);
        std::vector<uint8_t> payload(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                     bytes.begin() + static_cast<std::ptrdiff_t>(offset + payload_size));
        encodeOne(payload, static_cast<uint16_t>(frames.size()), static_cast<uint16_t>(total_frame_count));
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

bool WriteRepeatedVideo(const std::vector<EncodedFrame>& frames,
                        const std::filesystem::path& output_dir,
                        const protocol_iso::EncoderOptions& options,
                        std::string* error_message) {
    const std::optional<std::string> ffmpeg = FindFfmpegCommand();
    if (!ffmpeg.has_value()) {
        if (error_message != nullptr) {
            *error_message = "ffmpeg executable was not found; demo.mp4 was not generated.";
        }
        return false;
    }

    const std::filesystem::path temp_dir = output_dir / "_video_bmp";
    if (!EnsureDirectory(temp_dir, error_message)) {
        return false;
    }

    std::size_t output_index = 0U;
    for (const EncodedFrame& frame : frames) {
        for (int repeat_index = 0; repeat_index < options.repeat; ++repeat_index) {
            if (!image_io::WriteBmp(temp_dir / FrameFileName(output_index++), frame.carrier_frame, error_message)) {
                return false;
            }
        }
    }

    const std::filesystem::path output_path = output_dir / "demo.mp4";
    const std::string command = ffmpeg.value() + " -y -framerate " + std::to_string(options.fps) +
                                " -i " + QuotePath(temp_dir / "frame_%05d.bmp") +
                                " -pix_fmt yuv420p " + QuotePath(output_path) + RedirectOutput();
    if (!RunCommand(command)) {
        if (error_message != nullptr) {
            *error_message = "ffmpeg failed while writing demo.mp4.";
        }
        return false;
    }
    std::filesystem::remove_all(temp_dir);
    std::ofstream(output_dir / "video_status.txt") << "Video encoded with ffmpeg from internal BMP frames.\n";
    return true;
}

bool LoadImageAnyFormat(const std::filesystem::path& path,
                        const std::filesystem::path& temp_dir,
                        image_io::RgbImage* image,
                        std::string* error_message) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (extension == ".bmp") {
        return image_io::ReadBmp(path, image, error_message);
    }

    if (!EnsureDirectory(temp_dir, error_message)) {
        return false;
    }
    const std::filesystem::path bmp_path = temp_dir / (path.stem().string() + ".bmp");
    if (!ConvertImageToBmp(path, bmp_path, error_message)) {
        return false;
    }
    return image_io::ReadBmp(bmp_path, image, error_message);
}

std::vector<ImageFile> CollectDecodeImages(const std::filesystem::path& input_path,
                                           const std::filesystem::path& temp_root,
                                           std::string* error_message) {
    std::vector<ImageFile> files;
    if (std::filesystem::is_directory(input_path)) {
        std::vector<std::filesystem::path> paths;
        for (const auto& entry : std::filesystem::directory_iterator(input_path)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (extension == ".bmp" || extension == ".png" || extension == ".jpg" || extension == ".jpeg") {
                paths.push_back(entry.path());
            }
        }
        std::sort(paths.begin(), paths.end());
        for (const auto& path : paths) {
            files.push_back({path, path});
        }
        return files;
    }

    const std::optional<std::string> ffmpeg = FindFfmpegCommand();
    if (!ffmpeg.has_value()) {
        if (error_message != nullptr) {
            *error_message = "Video decode requires an ffmpeg executable.";
        }
        return {};
    }
    const std::filesystem::path temp_dir = temp_root / "video_frames";
    if (!EnsureDirectory(temp_dir, error_message)) {
        return {};
    }
    const std::string command = ffmpeg.value() + " -y -i " + QuotePath(input_path) + " " +
                                QuotePath(temp_dir / "frame_%05d.bmp") + RedirectOutput();
    if (!RunCommand(command)) {
        if (error_message != nullptr) {
            *error_message = "ffmpeg failed while extracting video frames: " + input_path.string();
        }
        return {};
    }
    for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".bmp") {
            files.push_back({entry.path(), entry.path()});
        }
    }
    std::sort(files.begin(), files.end(), [](const ImageFile& left, const ImageFile& right) {
        return left.bmp_path < right.bmp_path;
    });
    return files;
}

std::optional<MarkerBox> FindCornerMarker(const image_io::GrayImage& image, int quadrant_x, int quadrant_y) {
    const int width = image.width();
    const int height = image.height();
    const int x_start = quadrant_x == 0 ? 0 : width / 2;
    const int x_end = quadrant_x == 0 ? width / 2 : width;
    const int y_start = quadrant_y == 0 ? 0 : height / 2;
    const int y_end = quadrant_y == 0 ? height / 2 : height;

    std::vector<uint8_t> visited(static_cast<std::size_t>(width * height), 0U);
    std::vector<std::pair<int, int>> stack;
    stack.reserve(2048U);
    std::optional<MarkerBox> best;

    for (int y = y_start; y < y_end; ++y) {
        for (int x = x_start; x < x_end; ++x) {
            const std::size_t index = static_cast<std::size_t>(y * width + x);
            if (visited[index] != 0U || image.get(x, y) >= 128U) {
                continue;
            }

            visited[index] = 1U;
            stack.clear();
            stack.push_back({x, y});
            MarkerBox box{x, y, x, y, 0};

            while (!stack.empty()) {
                const auto [cx, cy] = stack.back();
                stack.pop_back();
                ++box.pixels;
                box.min_x = std::min(box.min_x, cx);
                box.max_x = std::max(box.max_x, cx);
                box.min_y = std::min(box.min_y, cy);
                box.max_y = std::max(box.max_y, cy);

                static constexpr std::array<std::pair<int, int>, 4> kDirs = {
                    std::pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                };
                for (const auto& [dx, dy] : kDirs) {
                    const int nx = cx + dx;
                    const int ny = cy + dy;
                    if (nx < x_start || nx >= x_end || ny < y_start || ny >= y_end) {
                        continue;
                    }
                    const std::size_t nindex = static_cast<std::size_t>(ny * width + nx);
                    if (visited[nindex] != 0U || image.get(nx, ny) >= 128U) {
                        continue;
                    }
                    visited[nindex] = 1U;
                    stack.push_back({nx, ny});
                }
            }

            const int component_area = box.width() * box.height();
            if (box.pixels < (width * height) / 3000 || component_area < (width * height) / 4000) {
                continue;
            }
            if (!best.has_value() || box.pixels > best->pixels) {
                best = box;
            }
        }
    }
    return best;
}

image_io::RgbImage NormalizeCarrier(const image_io::RgbImage& source,
                                    const protocol_iso::EncoderOptions& options,
                                    std::string* error_message) {
    const CarrierLayout layout = BuildCarrierLayout(options);
    const image_io::GrayImage gray = image_io::ToGray(source);
    const std::optional<MarkerBox> tl = FindCornerMarker(gray, 0, 0);
    const std::optional<MarkerBox> tr = FindCornerMarker(gray, 1, 0);
    const std::optional<MarkerBox> bl = FindCornerMarker(gray, 0, 1);
    const std::optional<MarkerBox> br = FindCornerMarker(gray, 1, 1);
    if (!tl.has_value() || !tr.has_value() || !bl.has_value() || !br.has_value()) {
        if (error_message != nullptr) {
            *error_message = "Failed to detect all four carrier markers.";
        }
        return {};
    }

    const int min_x = std::min(tl->min_x, bl->min_x);
    const int min_y = std::min(tl->min_y, tr->min_y);
    const int max_x = std::max(tr->max_x, br->max_x);
    const int max_y = std::max(bl->max_y, br->max_y);
    const int crop_width = std::max(1, max_x - min_x + 1);
    const int crop_height = std::max(1, max_y - min_y + 1);
    const image_io::RgbImage cropped = image_io::Crop(source, min_x, min_y, crop_width, crop_height);

    const int inner_size = layout.canvas_pixels - 2 * layout.marker_margin;
    const image_io::RgbImage resized = image_io::ResizeNearest(cropped, inner_size, inner_size);
    image_io::RgbImage normalized(layout.canvas_pixels, layout.canvas_pixels, image_io::kWhite);
    image_io::Blit(resized, &normalized, layout.marker_margin, layout.marker_margin);
    return normalized;
}

image_io::BitMatrix SampleQrModules(const image_io::GrayImage& qr_crop) {
    image_io::BitMatrix sampled(qr_iso::kRenderSize, qr_iso::kRenderSize, false);
    const uint8_t threshold = image_io::ComputeOtsuThreshold(qr_crop);
    for (int module_y = 0; module_y < qr_iso::kRenderSize; ++module_y) {
        // 编码端的最近邻放大使用 floor(dst * src / dst_size) 选源像素。
        // 这里反推每个模块在目标图上的像素区间，再取区间中点采样，保证与编码映射一致。
        const int y_begin = (module_y * qr_crop.height() + qr_iso::kRenderSize - 1) / qr_iso::kRenderSize;
        const int y_end = (((module_y + 1) * qr_crop.height() + qr_iso::kRenderSize - 1) / qr_iso::kRenderSize) - 1;
        const int sample_y = std::clamp((y_begin + std::max(y_begin, y_end)) / 2, 0, qr_crop.height() - 1);
        for (int module_x = 0; module_x < qr_iso::kRenderSize; ++module_x) {
            const int x_begin = (module_x * qr_crop.width() + qr_iso::kRenderSize - 1) / qr_iso::kRenderSize;
            const int x_end = (((module_x + 1) * qr_crop.width() + qr_iso::kRenderSize - 1) / qr_iso::kRenderSize) - 1;
            const int sample_x = std::clamp((x_begin + std::max(x_begin, x_end)) / 2, 0, qr_crop.width() - 1);
            sampled.set(module_x, module_y, qr_crop.get(sample_x, sample_y) <= threshold);
        }
    }
    return sampled;
}

image_io::BitMatrix StripQuietZone(const image_io::BitMatrix& full_modules) {
    image_io::BitMatrix symbol(qr_iso::kSymbolSize, qr_iso::kSymbolSize, false);
    for (int y = 0; y < qr_iso::kSymbolSize; ++y) {
        for (int x = 0; x < qr_iso::kSymbolSize; ++x) {
            symbol.set(x, y, full_modules.get(x + qr_iso::kQuietZoneModules, y + qr_iso::kQuietZoneModules));
        }
    }
    return symbol;
}

image_io::BitMatrix DownsampleQrModules(const image_io::RgbImage& qr_crop) {
    const image_io::RgbImage resized = image_io::ResizeNearest(qr_crop, qr_iso::kRenderSize, qr_iso::kRenderSize);
    const image_io::GrayImage gray = image_io::ToGray(resized);
    const uint8_t threshold = image_io::ComputeOtsuThreshold(gray);

    image_io::BitMatrix full_modules(qr_iso::kRenderSize, qr_iso::kRenderSize, false);
    for (int y = 0; y < qr_iso::kRenderSize; ++y) {
        for (int x = 0; x < qr_iso::kRenderSize; ++x) {
            full_modules.set(x, y, gray.get(x, y) <= threshold);
        }
    }
    return full_modules;
}

image_io::BitMatrix ShiftModules(const image_io::BitMatrix& modules, int offset_x, int offset_y) {
    image_io::BitMatrix shifted(modules.width(), modules.height(), false);
    for (int y = 0; y < modules.height(); ++y) {
        for (int x = 0; x < modules.width(); ++x) {
            const int source_x = x - offset_x;
            const int source_y = y - offset_y;
            if (source_x < 0 || source_x >= modules.width() || source_y < 0 || source_y >= modules.height()) {
                continue;
            }
            shifted.set(x, y, modules.get(source_x, source_y));
        }
    }
    return shifted;
}

DecodeAttemptResult TryDecodeFrame(const image_io::RgbImage& source,
                                   const protocol_iso::EncoderOptions& options) {
    DecodeAttemptResult result;
    if (source.width() == options.canvas_pixels && source.height() == options.canvas_pixels) {
        result.normalized_frame = source;
    } else {
        std::string normalize_error;
        result.normalized_frame = NormalizeCarrier(source, options, &normalize_error);
        if (result.normalized_frame.width() == 0) {
            result.message = normalize_error;
            return result;
        }
    }

    const CarrierLayout layout = BuildCarrierLayout(options);
    std::string last_qr_error = "Failed to decode QR format information.";
    for (int offset_y = -2; offset_y <= 2; ++offset_y) {
        for (int offset_x = -2; offset_x <= 2; ++offset_x) {
            image_io::RgbImage candidate_crop =
                image_io::Crop(result.normalized_frame,
                               layout.qr_x + offset_x,
                               layout.qr_y + offset_y,
                               layout.qr_size,
                               layout.qr_size);
            const image_io::BitMatrix base_modules = DownsampleQrModules(candidate_crop);
            for (int module_offset_y = -1; module_offset_y <= 1; ++module_offset_y) {
                for (int module_offset_x = -1; module_offset_x <= 1; ++module_offset_x) {
                    const image_io::BitMatrix adjusted_modules =
                        ShiftModules(base_modules, module_offset_x, module_offset_y);
                    const image_io::BitMatrix symbol = StripQuietZone(adjusted_modules);
                    const qr_iso::DecodeResult qr = qr_iso::DecodeModules(symbol);
                    if (!qr.success) {
                        last_qr_error = qr.message;
                        continue;
                    }

                    result.success = true;
                    result.method = "carrier-bbox";
                    result.message = qr.message;
                    result.frame_bytes = qr.bytes;
                    result.qr_crop = std::move(candidate_crop);
                    return result;
                }
            }
        }
    }

    result.method = "carrier-bbox";
    result.message = last_qr_error;
    result.qr_crop = image_io::Crop(result.normalized_frame, layout.qr_x, layout.qr_y, layout.qr_size, layout.qr_size);
    return result;
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

bool WriteDebugImageIfEnabled(const std::filesystem::path& path,
                              const image_io::RgbImage& image,
                              bool enabled,
                              std::string* error_message) {
    if (!enabled || image.width() == 0 || image.height() == 0) {
        return true;
    }
    return image_io::WriteBmp(path, image, error_message);
}

}  // namespace

std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& input_path) {
    return ReadFileBytesInternal(input_path);
}

bool WriteIsoSamples(const std::filesystem::path& output_dir,
                     const protocol_iso::EncoderOptions& options,
                     std::string* error_message) {
    if (!ValidateOptions(options, error_message) || !EnsureDirectory(output_dir, error_message)) {
        return false;
    }

    const std::vector<uint8_t> payload =
        MakeSamplePayload(std::min<std::size_t>(32U, static_cast<std::size_t>(kMaxPayloadBytes)), 0x29U);
    EncodedFrame frame;
    frame.payload = payload;
    frame.header.total_frames = 1;
    frame.header.payload_len = static_cast<uint16_t>(payload.size());
    frame.frame_bytes = protocol_iso::PackFrameBytes(frame.header, frame.payload);

    image_io::BitMatrix modules;
    if (!qr_iso::EncodeBytes(frame.frame_bytes, &modules, nullptr, error_message)) {
        return false;
    }
    frame.qr_frame = RenderQrModules(modules);
    frame.carrier_frame = RenderCarrierFrame(frame.qr_frame, options);

    if (!WriteImageArtifacts(output_dir, output_dir / "png_mirror", "sample_iso_qr_v2_symbol.bmp", frame.qr_frame, error_message) ||
        !WriteImageArtifacts(output_dir, output_dir / "png_mirror", "sample_iso_qr_v2_carrier.bmp", frame.carrier_frame, error_message) ||
        !WriteImageArtifacts(output_dir, output_dir / "png_mirror", "sample_iso_qr_v2_layout.bmp",
                             RenderLayoutPreview(options, frame.qr_frame), error_message)) {
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
    manifest << "sample_iso_qr_v2_symbol.bmp\tiso133\t29\t133\tQ\t"
             << options.canvas_pixels << '\t' << kMaxFrameBytes << '\t' << kMaxPayloadBytes << '\n';

    std::ofstream capacity_manifest(output_dir / "sample_capacity.tsv");
    if (!capacity_manifest.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to write ISO sample capacity matrix.";
        }
        return false;
    }
    capacity_manifest << "profile\tversion\tlogical_grid\tecc\tmax_frame_bytes\tmax_payload_bytes\trecommended\tsupported\n";
    for (const protocol_iso::Profile& profile : protocol_iso::SupportedProfiles()) {
        for (const protocol_iso::ErrorCorrection error_correction : protocol_iso::SupportedErrorCorrections()) {
            const bool supported = profile.id == protocol_iso::ProfileId::kIso133 &&
                                   error_correction == protocol_iso::ErrorCorrection::kQ;
            capacity_manifest << profile.name << '\t'
                              << profile.version << '\t'
                              << profile.logical_size << '\t'
                              << protocol_iso::ErrorCorrectionName(error_correction) << '\t'
                              << (supported ? std::to_string(kMaxFrameBytes) : "0") << '\t'
                              << (supported ? std::to_string(kMaxPayloadBytes) : "0") << '\t'
                              << (supported ? "true" : "false") << '\t'
                              << (supported ? "true" : "false") << '\n';
        }
    }
    return true;
}

bool WriteIsoPackage(const std::filesystem::path& input_path,
                     const std::filesystem::path& output_dir,
                     const protocol_iso::EncoderOptions& options,
                     std::string* error_message) {
    if (!ValidateOptions(options, error_message) || !EnsureDirectory(output_dir, error_message)) {
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

    const std::vector<uint8_t> input_bytes = ReadFileBytesInternal(input_path);
    std::vector<EncodedFrame> frames;
    try {
        frames = EncodeBytesToFrames(input_bytes, options);
    } catch (const std::exception& error) {
        if (error_message != nullptr) {
            *error_message = error.what();
        }
        return false;
    }

    if (options.write_protocol_samples && !WriteIsoSamples(sample_dir, options, error_message)) {
        return false;
    }

    for (std::size_t index = 0; index < frames.size(); ++index) {
        const std::string file_name = FrameFileName(index);
        if (!WriteImageArtifacts(qr_dir, qr_dir / "png_mirror", file_name, frames[index].qr_frame, error_message) ||
            !WriteImageArtifacts(carrier_dir, carrier_dir / "png_mirror", file_name, frames[index].carrier_frame, error_message)) {
            return false;
        }
    }

    if (!WriteManifest(output_dir / "frame_manifest.tsv", frames, options, error_message)) {
        return false;
    }

    const protocol_iso::Profile profile = protocol_iso::ProfileFromId(options.profile_id);
    std::ofstream(output_dir / "input_info.txt")
        << "protocol=ISO/IEC 18004 QR Code Model 2 (self-built encoder)\n"
        << "profile=" << profile.name << '\n'
        << "version=" << profile.version << '\n'
        << "logical_grid=" << profile.logical_size << '\n'
        << "ecc=" << protocol_iso::ErrorCorrectionName(options.error_correction) << '\n'
        << "canvas_px=" << options.canvas_pixels << '\n'
        << "input_path=" << input_path.string() << '\n'
        << "input_bytes=" << input_bytes.size() << '\n'
        << "frame_count=" << frames.size() << '\n'
        << "max_frame_bytes=" << kMaxFrameBytes << '\n'
        << "max_payload_bytes=" << kMaxPayloadBytes << '\n'
        << "fps=" << options.fps << '\n'
        << "repeat=" << options.repeat << '\n'
        << "carrier_markers=true\n"
        << "protocol_samples=" << (options.write_protocol_samples ? "true" : "false") << '\n';

    if (!WriteRepeatedVideo(frames, output_dir, options, error_message)) {
        std::ofstream(output_dir / "video_status.txt")
            << (error_message != nullptr ? *error_message : "demo.mp4 was not generated.") << '\n';
        return false;
    }
    return true;
}

bool DecodeIsoPackage(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_dir,
                      const protocol_iso::EncoderOptions& options,
                      std::string* error_message) {
    if (!ValidateOptions(options, error_message) || !EnsureDirectory(output_dir, error_message)) {
        return false;
    }

    const std::filesystem::path debug_dir = output_dir / "decode_debug";
    const std::filesystem::path source_dir = debug_dir / "source";
    const std::filesystem::path normalized_dir = debug_dir / "warped";
    const std::filesystem::path crop_dir = debug_dir / "qr_crop";
    const std::filesystem::path temp_root = output_dir / "_decode_temp";
    if (options.write_decode_debug &&
        (!EnsureDirectory(source_dir, error_message) ||
         !EnsureDirectory(normalized_dir, error_message) ||
         !EnsureDirectory(crop_dir, error_message))) {
        return false;
    }
    if (!EnsureDirectory(temp_root, error_message)) {
        return false;
    }

    const std::vector<ImageFile> decode_images = CollectDecodeImages(input_path, temp_root, error_message);
    if (decode_images.empty()) {
        return false;
    }

    std::vector<DecodedFrameReport> reports;
    std::vector<uint16_t> missing_frames;
    std::optional<uint16_t> expected_total_frames;
    std::vector<std::vector<uint8_t>> payloads_by_seq;
    std::vector<bool> payload_present;
    std::size_t decoded_frame_count = 0U;

    auto finalizeDecode = [&](const std::string& status,
                              std::size_t output_bytes,
                              const std::string& final_error) {
        if (!WriteDecodeReport(output_dir / "decode_report.tsv", reports, error_message)) {
            return false;
        }
        if (!WriteDecodeSummary(output_dir / "decode_summary.txt",
                                status,
                                decoded_frame_count,
                                expected_total_frames.value_or(0U),
                                output_bytes,
                                missing_frames.size(),
                                error_message)) {
            return false;
        }
        if (!missing_frames.empty() &&
            !WriteMissingFrames(output_dir / "missing_frames.txt", missing_frames, error_message)) {
            return false;
        }
        std::filesystem::remove_all(temp_root);
        if (!final_error.empty() && error_message != nullptr) {
            *error_message = final_error;
        }
        return final_error.empty();
    };

    for (std::size_t index = 0; index < decode_images.size(); ++index) {
        image_io::RgbImage frame;
        if (!LoadImageAnyFormat(decode_images[index].source_path, temp_root / "images", &frame, error_message)) {
            return finalizeDecode("read_error", 0U, *error_message);
        }

        WriteDebugImageIfEnabled(source_dir / FrameFileName(index), frame, options.write_decode_debug, error_message);

        DecodeAttemptResult attempt = TryDecodeFrame(frame, options);
        DecodedFrameReport report;
        report.source_index = static_cast<int>(index);
        report.profile = protocol_iso::ProfileName(options.profile_id);
        report.ecc = protocol_iso::ErrorCorrectionName(options.error_correction);
        report.method = attempt.method.empty() ? "none" : attempt.method;
        report.message = attempt.message;

        WriteDebugImageIfEnabled(normalized_dir / FrameFileName(index),
                                 attempt.normalized_frame,
                                 options.write_decode_debug,
                                 error_message);
        WriteDebugImageIfEnabled(crop_dir / FrameFileName(index),
                                 attempt.qr_crop,
                                 options.write_decode_debug,
                                 error_message);

        if (!attempt.success) {
            report.message = report.message.empty() ? "decode_failed" : "decode_failed: " + report.message;
            reports.push_back(std::move(report));
            continue;
        }

        protocol_iso::FrameHeader header;
        std::vector<uint8_t> payload;
        std::string parse_error;
        if (!protocol_iso::ParseFrameBytes(attempt.frame_bytes, &header, &payload, &parse_error)) {
            report.message = parse_error;
            reports.push_back(std::move(report));
            continue;
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
            continue;
        }

        if (header.frame_seq >= expected_total_frames.value()) {
            report.success = false;
            report.message = "frame_seq_out_of_range";
            reports.push_back(std::move(report));
            continue;
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
    }

    if (!expected_total_frames.has_value()) {
        return finalizeDecode("no_valid_frames", 0U, "No valid ISO frames were decoded from the input.");
    }

    for (uint16_t frame_seq = 0; frame_seq < expected_total_frames.value(); ++frame_seq) {
        if (!payload_present[frame_seq]) {
            missing_frames.push_back(frame_seq);
        }
    }
    if (!missing_frames.empty()) {
        return finalizeDecode("missing_frames", 0U, "Decoded ISO frames are incomplete; see missing_frames.txt.");
    }

    std::vector<uint8_t> output_bytes;
    for (const auto& payload : payloads_by_seq) {
        output_bytes.insert(output_bytes.end(), payload.begin(), payload.end());
    }

    std::ofstream stream(output_dir / "output.bin", std::ios::binary);
    if (!stream.is_open()) {
        return finalizeDecode("write_error", 0U, "Failed to write decoded output file.");
    }
    stream.write(reinterpret_cast<const char*>(output_bytes.data()), static_cast<std::streamsize>(output_bytes.size()));
    return finalizeDecode("success", output_bytes.size(), "");
}

}  // namespace demo_encoder
