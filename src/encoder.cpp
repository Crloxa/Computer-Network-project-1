#include "encoder.h"

#include "simple_image.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace demo_encoder {
namespace {

using protocol_v1::FrameHeader;
using protocol_v1::FrameType;

struct EncodedFrame {
    FrameHeader header;
    std::vector<uint8_t> payload;
    image_io::BitMatrix logical_modules;
    image_io::RgbImage logical_frame;
    image_io::RgbImage physical_frame;
};

struct ImageFile {
    std::filesystem::path source_path;
    std::filesystem::path bmp_path;
};

struct DecodedFrameData {
    FrameHeader header;
    std::vector<uint8_t> payload;
};

std::string FrameFileName(std::size_t index, const std::string& extension = ".bmp") {
    std::ostringstream stream;
    stream << "frame_" << std::setw(5) << std::setfill('0') << index << extension;
    return stream.str();
}

std::string QuotePath(const std::filesystem::path& path) {
    return "\"" + path.string() + "\"";
}

void PrintRuntimeWarning(const std::string& message) {
    std::cerr << "[warning] " << message << std::endl;
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

std::filesystem::path NormalizePath(const std::filesystem::path& path) {
    std::error_code error;
    const std::filesystem::path weak = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return weak;
    }
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (!error) {
        return absolute;
    }
    return path;
}

std::filesystem::path FindRepoRoot(const std::filesystem::path& start_path) {
    std::error_code error;
    std::filesystem::path cursor = NormalizePath(start_path);
    if (std::filesystem::is_regular_file(cursor, error)) {
        cursor = cursor.parent_path();
    }
    while (!cursor.empty()) {
        if (std::filesystem::exists(cursor / "Project1.sln", error)) {
            return cursor;
        }
        const std::filesystem::path parent = cursor.parent_path();
        if (parent == cursor) {
            break;
        }
        cursor = parent;
    }
    return {};
}

std::filesystem::path GetExecutableDirectory() {
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = 0;
    while (true) {
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0U) {
            return {};
        }
        if (length < buffer.size() - 1U) {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2U);
    }
#else
    std::error_code error;
    return std::filesystem::current_path(error);
#endif
}

std::vector<std::filesystem::path> BuildFfmpegSearchRoots() {
    std::vector<std::filesystem::path> roots;
    auto append_unique = [&roots](const std::filesystem::path& candidate) {
        if (candidate.empty()) {
            return;
        }
        const std::filesystem::path normalized = NormalizePath(candidate);
        const auto duplicate = std::find(roots.begin(), roots.end(), normalized);
        if (duplicate == roots.end()) {
            roots.push_back(normalized);
        }
    };

    const std::filesystem::path executable_dir = GetExecutableDirectory();
    append_unique(executable_dir);
    append_unique(FindRepoRoot(executable_dir));
    std::error_code error;
    append_unique(std::filesystem::current_path(error));
    append_unique(FindRepoRoot(std::filesystem::current_path(error)));
    return roots;
}

std::filesystem::path FindBundledFfmpeg() {
#ifdef _WIN32
    constexpr const char* kFfmpegName = "ffmpeg.exe";
#else
    constexpr const char* kFfmpegName = "ffmpeg";
#endif
    for (const std::filesystem::path& root : BuildFfmpegSearchRoots()) {
        const std::filesystem::path candidate = root / "ffmpeg" / "bin" / kFfmpegName;
        std::error_code error;
        if (std::filesystem::exists(candidate, error)) {
            return candidate;
        }
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
    return FindFfmpegCommand();
}

bool ConvertBmpToPng(const std::string& command,
                    const std::filesystem::path& bmp_path,
                     const std::filesystem::path& png_path,
                     std::string* error_message) {
    std::string invocation;
    if (command == "sips") {
#ifdef _WIN32
        invocation.clear();
#else
        invocation = "sips -s format png " + QuotePath(bmp_path) + " --out " + QuotePath(png_path) + RedirectOutput();
#endif
    } else {
        invocation = command + " -y -i " + QuotePath(bmp_path) + " " + QuotePath(png_path) + RedirectOutput();
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
            *error_message = "No image converter was found. Non-BMP inputs require ffmpeg"
#ifndef _WIN32
                             " or sips"
#endif
                             ".";
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
                         const std::optional<std::string>& image_converter,
                         std::string* error_message) {
    if (!EnsureDirectory(image_dir, error_message)) {
        return false;
    }
    const std::filesystem::path bmp_path = image_dir / file_name;
    if (!image_io::WriteBmp(bmp_path, image, error_message)) {
        return false;
    }
    if (!image_converter.has_value()) {
        return true;
    }
    if (!EnsureDirectory(png_dir, error_message)) {
        return false;
    }
    const std::filesystem::path png_path = png_dir / (std::filesystem::path(file_name).stem().string() + ".png");
    std::string png_error;
    if (!ConvertBmpToPng(image_converter.value(), bmp_path, png_path, &png_error)) {
        PrintRuntimeWarning("PNG 镜像生成失败，已保留 BMP：" + file_name + "；原因：" + png_error);
        return true;
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

bool ValidateOptions(const protocol_v1::EncoderOptions& options, std::string* error_message) {
    if (options.fps <= 0 || options.repeat <= 0) {
        if (error_message != nullptr) {
            *error_message = "--fps and --repeat must be positive.";
        }
        return false;
    }
    if (options.max_payload_bytes <= 0 || options.max_payload_bytes > protocol_v1::kMaxPayloadBytes) {
        if (error_message != nullptr) {
            *error_message = "max_payload_bytes is out of the supported V1.6 range.";
        }
        return false;
    }
    return true;
}

std::vector<uint8_t> MakeSamplePayload(std::size_t payload_size, uint8_t seed) {
    std::vector<uint8_t> payload(payload_size);
    for (std::size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<uint8_t>((seed + index) & 0xFFU);
    }
    return payload;
}

void SetModule(image_io::BitMatrix* modules, int x, int y, bool black) {
    if (protocol_v1::IsInBounds(x, y)) {
        modules->set(x, y, black);
    }
}

void DrawFinder(image_io::BitMatrix* modules, int origin_x, int origin_y, bool invert_center) {
    for (int y = 0; y < protocol_v1::kFinderSizeModules; ++y) {
        for (int x = 0; x < protocol_v1::kFinderSizeModules; ++x) {
            const int distance = std::max(std::abs(x - 3), std::abs(y - 3));
            bool black = (distance == 3) || (distance <= 1);
            if (invert_center && x >= 2 && x <= 4 && y >= 2 && y <= 4) {
                black = !black;
            }
            SetModule(modules, origin_x + x, origin_y + y, black);
        }
    }
}

void DrawTiming(image_io::BitMatrix* modules) {
    for (int x = protocol_v1::kTimingStart; x <= protocol_v1::kTimingEnd; ++x) {
        SetModule(modules, x, protocol_v1::kTimingHorizontalY,
                  ((x - protocol_v1::kTimingStart) % 2) == 0);
    }
    for (int y = protocol_v1::kTimingStart; y <= protocol_v1::kTimingEnd; ++y) {
        SetModule(modules, protocol_v1::kTimingVerticalX, y,
                  ((y - protocol_v1::kTimingStart) % 2) == 0);
    }
}

void DrawAlignment(image_io::BitMatrix* modules) {
    for (int y = 0; y < protocol_v1::kAlignmentSizeModules; ++y) {
        for (int x = 0; x < protocol_v1::kAlignmentSizeModules; ++x) {
            const int distance = std::max(std::abs(x - 2), std::abs(y - 2));
            const bool black = (distance == 2) || (distance == 0);
            SetModule(modules,
                      protocol_v1::kAlignmentOriginX + x,
                      protocol_v1::kAlignmentOriginY + y,
                      black);
        }
    }
}

void DrawHeader(const FrameHeader& header, image_io::BitMatrix* modules) {
    const std::vector<bool> header_bits = protocol_v1::PackHeaderBits(header);
    const std::vector<protocol_v1::GridPoint> header_cells = protocol_v1::HeaderCells();
    for (std::size_t index = 0; index < header_bits.size() && index < header_cells.size(); ++index) {
        SetModule(modules, header_cells[index].x, header_cells[index].y, header_bits[index]);
    }
}

void DrawPayload(const std::vector<uint8_t>& payload, image_io::BitMatrix* modules) {
    const std::vector<bool> payload_bits = protocol_v1::BytesToBitsMsbFirst(payload, payload.size());
    const std::vector<protocol_v1::GridPoint> payload_cells = protocol_v1::PayloadCells();
    for (std::size_t index = 0; index < payload_bits.size() && index < payload_cells.size(); ++index) {
        SetModule(modules, payload_cells[index].x, payload_cells[index].y, payload_bits[index]);
    }
}

image_io::BitMatrix BuildFrameModules(const FrameHeader& header, const std::vector<uint8_t>& payload) {
    image_io::BitMatrix modules(protocol_v1::kLogicalGridSize, protocol_v1::kLogicalGridSize, false);
    DrawFinder(&modules, 0, 0, false);
    DrawFinder(&modules, protocol_v1::kFinderTopRightMin + 1, 0, false);
    DrawFinder(&modules, 0, protocol_v1::kFinderBottomLeftMin + 1, false);
    DrawFinder(&modules, protocol_v1::kFinderTopRightMin + 1, protocol_v1::kFinderBottomLeftMin + 1, true);
    DrawTiming(&modules);
    DrawAlignment(&modules);
    DrawHeader(header, &modules);
    DrawPayload(payload, &modules);
    return modules;
}

image_io::RgbImage RenderLogicalFrame(const image_io::BitMatrix& modules) {
    image_io::RgbImage image(protocol_v1::kLogicalRenderPixels,
                             protocol_v1::kLogicalRenderPixels,
                             image_io::kWhite);
    for (int y = 0; y < protocol_v1::kLogicalGridSize; ++y) {
        for (int x = 0; x < protocol_v1::kLogicalGridSize; ++x) {
            const image_io::Rgb color = modules.get(x, y) ? image_io::kBlack : image_io::kWhite;
            image_io::FillRect(&image,
                               x * protocol_v1::kModulePixels,
                               y * protocol_v1::kModulePixels,
                               protocol_v1::kModulePixels,
                               protocol_v1::kModulePixels,
                               color);
        }
    }
    return image;
}

image_io::RgbImage RenderPhysicalFrame(const image_io::RgbImage& logical_frame) {
    image_io::RgbImage frame(protocol_v1::kPhysicalOutputPixels,
                             protocol_v1::kPhysicalOutputPixels,
                             image_io::kWhite);
    image_io::Blit(logical_frame, &frame, protocol_v1::kQuietZonePixels, protocol_v1::kQuietZonePixels);
    return frame;
}

void DrawModuleOutline(image_io::RgbImage* image,
                       int module_x,
                       int module_y,
                       int module_width,
                       int module_height,
                       image_io::Rgb color) {
    const int x = protocol_v1::kQuietZonePixels + module_x * protocol_v1::kModulePixels;
    const int y = protocol_v1::kQuietZonePixels + module_y * protocol_v1::kModulePixels;
    const int width = module_width * protocol_v1::kModulePixels;
    const int height = module_height * protocol_v1::kModulePixels;
    const int stroke = 3;
    image_io::FillRect(image, x, y, width, stroke, color);
    image_io::FillRect(image, x, y + height - stroke, width, stroke, color);
    image_io::FillRect(image, x, y, stroke, height, color);
    image_io::FillRect(image, x + width - stroke, y, stroke, height, color);
}

image_io::RgbImage RenderLayoutGuide() {
    FrameHeader header;
    header.frame_type = FrameType::kSingle;
    header.tail_len_bytes = 0;
    header.checkcode16 = 0;
    header.frame_seq = 0;
    image_io::BitMatrix modules = BuildFrameModules(header, {});
    image_io::RgbImage physical = RenderPhysicalFrame(RenderLogicalFrame(modules));
    DrawModuleOutline(&physical,
                      protocol_v1::kHeaderOriginX,
                      protocol_v1::kHeaderOriginY,
                      protocol_v1::kHeaderWidthModules,
                      protocol_v1::kHeaderHeightModules,
                      image_io::kBlue);
    DrawModuleOutline(&physical,
                      protocol_v1::kAlignmentOriginX,
                      protocol_v1::kAlignmentOriginY,
                      protocol_v1::kAlignmentSizeModules,
                      protocol_v1::kAlignmentSizeModules,
                      image_io::kBlue);
    return physical;
}

FrameType SelectFrameType(std::size_t frame_index, std::size_t total_frames) {
    if (total_frames == 1U) {
        return FrameType::kSingle;
    }
    if (frame_index == 0U) {
        return FrameType::kStart;
    }
    if (frame_index + 1U == total_frames) {
        return FrameType::kEnd;
    }
    return FrameType::kNormal;
}

FrameHeader BuildFrameHeader(FrameType frame_type,
                             uint16_t frame_seq,
                             const std::vector<uint8_t>& payload,
                             const protocol_v1::EncoderOptions& options) {
    FrameHeader header;
    header.frame_type = frame_type;
    header.frame_seq = frame_seq;
    header.tail_len_bytes = protocol_v1::IsEndFrame(frame_type)
        ? static_cast<uint16_t>(payload.size())
        : static_cast<uint16_t>(options.max_payload_bytes);
    header.checkcode16 = protocol_v1::ComputeCheckcode16(payload,
                                                         header.tail_len_bytes,
                                                         header.frame_seq,
                                                         header.frame_type);
    return header;
}

std::vector<EncodedFrame> EncodeBytesToFrames(const std::vector<uint8_t>& bytes,
                                              const protocol_v1::EncoderOptions& options) {
    const std::size_t chunk_size = static_cast<std::size_t>(options.max_payload_bytes);
    const std::size_t total_frame_count = std::max<std::size_t>(1U, (bytes.size() + chunk_size - 1U) / chunk_size);
    if (total_frame_count > 0xFFFFU) {
        throw std::runtime_error("Input is too large for the current V1.6 frame_seq range.");
    }

    std::vector<EncodedFrame> frames;
    frames.reserve(total_frame_count);
    if (bytes.empty()) {
        EncodedFrame frame;
        frame.payload = {};
        frame.header = BuildFrameHeader(FrameType::kSingle, 0U, frame.payload, options);
        frame.logical_modules = BuildFrameModules(frame.header, frame.payload);
        frame.logical_frame = RenderLogicalFrame(frame.logical_modules);
        frame.physical_frame = RenderPhysicalFrame(frame.logical_frame);
        frames.push_back(std::move(frame));
        return frames;
    }

    for (std::size_t offset = 0; offset < bytes.size(); offset += chunk_size) {
        const std::size_t payload_size = std::min<std::size_t>(bytes.size() - offset, chunk_size);
        EncodedFrame frame;
        frame.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                             bytes.begin() + static_cast<std::ptrdiff_t>(offset + payload_size));
        frame.header = BuildFrameHeader(SelectFrameType(frames.size(), total_frame_count),
                                        static_cast<uint16_t>(frames.size()),
                                        frame.payload,
                                        options);
        frame.logical_modules = BuildFrameModules(frame.header, frame.payload);
        frame.logical_frame = RenderLogicalFrame(frame.logical_modules);
        frame.physical_frame = RenderPhysicalFrame(frame.logical_frame);
        frames.push_back(std::move(frame));
    }
    return frames;
}

bool WriteSampleManifest(const std::filesystem::path& manifest_path, std::string* error_message) {
    std::ofstream stream(manifest_path);
    if (!stream.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to write sample manifest: " + manifest_path.string();
        }
        return false;
    }

    stream << "file\tkind\tprotocol\tlogical_grid\tmodule_px\tphysical_px\tmax_payload_bytes\n";
    stream << "layout_guide.bmp\tlayout\t" << protocol_v1::kProtocolId << '\t'
           << protocol_v1::kLogicalGridSize << '\t'
           << protocol_v1::kModulePixels << '\t'
           << protocol_v1::kPhysicalOutputPixels << '\t'
           << protocol_v1::kMaxPayloadBytes << '\n';
    stream << "sample_full_frame.bmp\tfull\t" << protocol_v1::kProtocolId << '\t'
           << protocol_v1::kLogicalGridSize << '\t'
           << protocol_v1::kModulePixels << '\t'
           << protocol_v1::kPhysicalOutputPixels << '\t'
           << protocol_v1::kMaxPayloadBytes << '\n';
    stream << "sample_short_frame.bmp\tshort\t" << protocol_v1::kProtocolId << '\t'
           << protocol_v1::kLogicalGridSize << '\t'
           << protocol_v1::kModulePixels << '\t'
           << protocol_v1::kPhysicalOutputPixels << '\t'
           << protocol_v1::kMaxPayloadBytes << '\n';
    return true;
}

bool WriteManifest(const std::filesystem::path& manifest_path,
                   const std::vector<EncodedFrame>& frames,
                   std::string* error_message) {
    std::ofstream stream(manifest_path);
    if (!stream.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to write manifest: " + manifest_path.string();
        }
        return false;
    }

    stream << "file\tframe_seq\tframe_type\tpayload_len_bytes\ttail_len_bytes\tcheckcode16\tlogical_grid\tmodule_px\tphysical_px\n";
    for (std::size_t index = 0; index < frames.size(); ++index) {
        stream << FrameFileName(index) << '\t'
               << frames[index].header.frame_seq << '\t'
               << protocol_v1::FrameTypeName(frames[index].header.frame_type) << '\t'
               << frames[index].payload.size() << '\t'
               << frames[index].header.tail_len_bytes << '\t'
               << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
               << frames[index].header.checkcode16 << std::dec << '\t'
               << protocol_v1::kLogicalGridSize << '\t'
               << protocol_v1::kModulePixels << '\t'
               << protocol_v1::kPhysicalOutputPixels << '\n';
    }
    return true;
}

bool WriteRepeatedVideo(const std::vector<EncodedFrame>& frames,
                        const std::filesystem::path& output_dir,
                        const protocol_v1::EncoderOptions& options,
                        std::string* error_message) {
    const std::optional<std::string> ffmpeg = FindFfmpegCommand();
    if (!ffmpeg.has_value()) {
        const std::string status = "ffmpeg executable was not found; demo.mp4 was not generated.\n";
        std::ofstream(output_dir / "video_status.txt") << status;
        PrintRuntimeWarning("未找到 ffmpeg，可继续使用 BMP 帧目录；demo.mp4 未生成。");
        if (error_message != nullptr) {
            error_message->clear();
        }
        return true;
    }

    const std::filesystem::path temp_dir = output_dir / "_video_bmp";
    if (!EnsureDirectory(temp_dir, error_message)) {
        return false;
    }

    std::size_t output_index = 0U;
    for (const EncodedFrame& frame : frames) {
        for (int repeat_index = 0; repeat_index < options.repeat; ++repeat_index) {
            if (!image_io::WriteBmp(temp_dir / FrameFileName(output_index++), frame.physical_frame, error_message)) {
                return false;
            }
        }
    }

    const std::filesystem::path output_path = output_dir / "demo.mp4";
    const std::string command = ffmpeg.value() + " -y -framerate " + std::to_string(options.fps) +
                                " -i " + QuotePath(temp_dir / "frame_%05d.bmp") +
                                " -pix_fmt yuv420p " + QuotePath(output_path) + RedirectOutput();
    if (!RunCommand(command)) {
        const std::string status = "ffmpeg failed while writing demo.mp4.\n";
        std::ofstream(output_dir / "video_status.txt") << status;
        PrintRuntimeWarning("ffmpeg 生成 demo.mp4 失败，已保留 BMP 帧目录。");
        if (error_message != nullptr) {
            error_message->clear();
        }
        return true;
    }
    std::filesystem::remove_all(temp_dir);
    std::ofstream(output_dir / "video_status.txt") << "Video encoded with ffmpeg from V1.6 internal BMP frames.\n";
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
        if (error_message != nullptr && !error_message->empty() &&
            error_message->find("No image converter was found") != std::string::npos) {
            *error_message = "Decoding PNG/JPG inputs requires ffmpeg (or sips on macOS): " + path.string();
        }
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
            *error_message = "Video decode requires an ffmpeg executable; BMP frame directories work without it.";
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

bool ValidateFinder(const image_io::BitMatrix& modules, int origin_x, int origin_y, bool invert_center) {
    for (int y = 0; y < protocol_v1::kFinderSizeModules; ++y) {
        for (int x = 0; x < protocol_v1::kFinderSizeModules; ++x) {
            const int distance = std::max(std::abs(x - 3), std::abs(y - 3));
            bool expected = (distance == 3) || (distance <= 1);
            if (invert_center && x >= 2 && x <= 4 && y >= 2 && y <= 4) {
                expected = !expected;
            }
            if (modules.get(origin_x + x, origin_y + y) != expected) {
                return false;
            }
        }
    }
    return true;
}

bool ValidateTiming(const image_io::BitMatrix& modules) {
    for (int x = protocol_v1::kTimingStart; x <= protocol_v1::kTimingEnd; ++x) {
        const bool expected = ((x - protocol_v1::kTimingStart) % 2) == 0;
        if (modules.get(x, protocol_v1::kTimingHorizontalY) != expected) {
            return false;
        }
    }
    for (int y = protocol_v1::kTimingStart; y <= protocol_v1::kTimingEnd; ++y) {
        const bool expected = ((y - protocol_v1::kTimingStart) % 2) == 0;
        if (modules.get(protocol_v1::kTimingVerticalX, y) != expected) {
            return false;
        }
    }
    return true;
}

bool ValidateAlignment(const image_io::BitMatrix& modules) {
    for (int y = 0; y < protocol_v1::kAlignmentSizeModules; ++y) {
        for (int x = 0; x < protocol_v1::kAlignmentSizeModules; ++x) {
            const int distance = std::max(std::abs(x - 2), std::abs(y - 2));
            const bool expected = (distance == 2) || (distance == 0);
            if (modules.get(protocol_v1::kAlignmentOriginX + x,
                            protocol_v1::kAlignmentOriginY + y) != expected) {
                return false;
            }
        }
    }
    return true;
}

bool ValidateStaticPatterns(const image_io::BitMatrix& modules, std::string* error_message) {
    if (!ValidateFinder(modules, 0, 0, false) ||
        !ValidateFinder(modules, protocol_v1::kFinderTopRightMin + 1, 0, false) ||
        !ValidateFinder(modules, 0, protocol_v1::kFinderBottomLeftMin + 1, false) ||
        !ValidateFinder(modules,
                        protocol_v1::kFinderTopRightMin + 1,
                        protocol_v1::kFinderBottomLeftMin + 1,
                        true)) {
        if (error_message != nullptr) {
            *error_message = "finder_pattern_mismatch";
        }
        return false;
    }
    if (!ValidateTiming(modules)) {
        if (error_message != nullptr) {
            *error_message = "timing_pattern_mismatch";
        }
        return false;
    }
    if (!ValidateAlignment(modules)) {
        if (error_message != nullptr) {
            *error_message = "alignment_pattern_mismatch";
        }
        return false;
    }
    return true;
}

image_io::BitMatrix SamplePhysicalFrame(const image_io::RgbImage& image) {
    const image_io::RgbImage normalized =
        (image.width() == protocol_v1::kPhysicalOutputPixels && image.height() == protocol_v1::kPhysicalOutputPixels)
            ? image
            : image_io::ResizeNearest(image,
                                      protocol_v1::kPhysicalOutputPixels,
                                      protocol_v1::kPhysicalOutputPixels);
    const image_io::RgbImage logical_crop = image_io::Crop(normalized,
                                                           protocol_v1::kQuietZonePixels,
                                                           protocol_v1::kQuietZonePixels,
                                                           protocol_v1::kLogicalRenderPixels,
                                                           protocol_v1::kLogicalRenderPixels);
    const image_io::GrayImage gray = image_io::ToGray(logical_crop);
    const uint8_t threshold = image_io::ComputeOtsuThreshold(gray);

    image_io::BitMatrix modules(protocol_v1::kLogicalGridSize, protocol_v1::kLogicalGridSize, false);
    for (int module_y = 0; module_y < protocol_v1::kLogicalGridSize; ++module_y) {
        const int y_begin = module_y * protocol_v1::kModulePixels;
        const int y_end = y_begin + protocol_v1::kModulePixels - 1;
        const int sample_y = (y_begin + y_end) / 2;
        for (int module_x = 0; module_x < protocol_v1::kLogicalGridSize; ++module_x) {
            const int x_begin = module_x * protocol_v1::kModulePixels;
            const int x_end = x_begin + protocol_v1::kModulePixels - 1;
            const int sample_x = (x_begin + x_end) / 2;
            modules.set(module_x, module_y, gray.get(sample_x, sample_y) <= threshold);
        }
    }
    return modules;
}

std::vector<bool> ExtractHeaderBits(const image_io::BitMatrix& modules) {
    const std::vector<protocol_v1::GridPoint> header_cells = protocol_v1::HeaderCells();
    std::vector<bool> bits;
    bits.reserve(header_cells.size());
    for (const protocol_v1::GridPoint& cell : header_cells) {
        bits.push_back(modules.get(cell.x, cell.y));
    }
    return bits;
}

bool DecodeFrameModules(const image_io::BitMatrix& modules,
                        const protocol_v1::EncoderOptions& options,
                        DecodedFrameData* decoded_frame,
                        std::string* error_message) {
    if (!ValidateStaticPatterns(modules, error_message)) {
        return false;
    }

    FrameHeader header;
    const std::vector<bool> header_bits = ExtractHeaderBits(modules);
    if (!protocol_v1::ParseHeaderBits(header_bits, &header, error_message)) {
        return false;
    }

    const std::size_t payload_len = protocol_v1::IsEndFrame(header.frame_type)
        ? static_cast<std::size_t>(header.tail_len_bytes)
        : static_cast<std::size_t>(options.max_payload_bytes);
    const std::vector<protocol_v1::GridPoint> payload_cells = protocol_v1::PayloadCells();
    const std::size_t required_bits = payload_len * 8U;
    if (required_bits > payload_cells.size()) {
        if (error_message != nullptr) {
            *error_message = "payload_len_out_of_capacity";
        }
        return false;
    }

    std::vector<bool> payload_bits;
    payload_bits.reserve(required_bits);
    for (std::size_t index = 0; index < required_bits; ++index) {
        payload_bits.push_back(modules.get(payload_cells[index].x, payload_cells[index].y));
    }
    std::vector<uint8_t> payload = protocol_v1::BitsToBytesMsbFirst(payload_bits);
    payload.resize(payload_len);

    const uint16_t expected = protocol_v1::ComputeCheckcode16(payload,
                                                              header.tail_len_bytes,
                                                              header.frame_seq,
                                                              header.frame_type);
    if (expected != header.checkcode16) {
        if (error_message != nullptr) {
            std::ostringstream stream;
            stream << "checkcode16_mismatch expected=0x" << std::uppercase << std::hex
                   << std::setw(4) << std::setfill('0') << header.checkcode16
                   << " actual=0x" << std::setw(4) << expected;
            *error_message = stream.str();
        }
        return false;
    }

    if (decoded_frame != nullptr) {
        decoded_frame->header = header;
        decoded_frame->payload = std::move(payload);
    }
    return true;
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
    stream << "source_index\tsuccess\tframe_seq\tframe_type\tpayload_len_bytes\ttail_len_bytes\tmessage\n";
    for (const DecodedFrameReport& report : reports) {
        stream << report.source_index << '\t'
               << (report.success ? "true" : "false") << '\t'
               << report.frame_seq << '\t'
               << report.frame_type << '\t'
               << report.payload_len_bytes << '\t'
               << report.tail_len_bytes << '\t'
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
           << "output_bytes=" << output_bytes << '\n'
           << "missing_frames=" << missing_frames << '\n';
    return true;
}

}  // namespace

std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& input_path) {
    return ReadFileBytesInternal(input_path);
}

bool WriteV1Samples(const std::filesystem::path& output_dir,
                    const protocol_v1::EncoderOptions& options,
                    std::string* error_message) {
    if (!ValidateOptions(options, error_message) || !EnsureDirectory(output_dir, error_message)) {
        return false;
    }

    const std::optional<std::string> image_converter = FindImageConverterCommand();
    if (!image_converter.has_value()) {
        PrintRuntimeWarning("未找到 PNG 转换器，samples 只会生成 BMP 文件。");
    }

    const EncodedFrame full_frame = EncodeBytesToFrames(
        MakeSamplePayload(static_cast<std::size_t>(options.max_payload_bytes), 0x29U), options).front();
    const EncodedFrame short_frame = EncodeBytesToFrames(MakeSamplePayload(32U, 0x6AU), options).front();
    const image_io::RgbImage layout_guide = RenderLayoutGuide();

    if (!WriteImageArtifacts(output_dir, output_dir / "png_mirror", "layout_guide.bmp", layout_guide, image_converter, error_message) ||
        !WriteImageArtifacts(output_dir, output_dir / "png_mirror", "sample_full_frame.bmp", full_frame.physical_frame, image_converter, error_message) ||
        !WriteImageArtifacts(output_dir, output_dir / "png_mirror", "sample_short_frame.bmp", short_frame.physical_frame, image_converter, error_message) ||
        !WriteSampleManifest(output_dir / "sample_manifest.tsv", error_message)) {
        return false;
    }
    return true;
}

bool WriteV1Package(const std::filesystem::path& input_path,
                    const std::filesystem::path& output_dir,
                    const protocol_v1::EncoderOptions& options,
                    std::string* error_message) {
    if (!ValidateOptions(options, error_message) || !EnsureDirectory(output_dir, error_message)) {
        return false;
    }

    const std::filesystem::path logical_dir = output_dir / "frames" / "logical";
    const std::filesystem::path physical_dir = output_dir / "frames" / "physical";
    const std::filesystem::path sample_dir = output_dir / "protocol_samples";
    if (!EnsureDirectory(logical_dir, error_message) ||
        !EnsureDirectory(physical_dir, error_message) ||
        !EnsureDirectory(sample_dir, error_message)) {
        return false;
    }

    const std::optional<std::string> image_converter = FindImageConverterCommand();
    if (!image_converter.has_value()) {
        PrintRuntimeWarning("未找到 PNG 转换器，encode 只会生成 BMP 帧与 manifest。");
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

    if (!WriteV1Samples(sample_dir, options, error_message)) {
        return false;
    }

    for (std::size_t index = 0; index < frames.size(); ++index) {
        const std::string file_name = FrameFileName(index);
        if (!WriteImageArtifacts(logical_dir, logical_dir / "png_mirror", file_name, frames[index].logical_frame, image_converter, error_message) ||
            !WriteImageArtifacts(physical_dir, physical_dir / "png_mirror", file_name, frames[index].physical_frame, image_converter, error_message)) {
            return false;
        }
    }

    if (!WriteManifest(output_dir / "frame_manifest.tsv", frames, error_message)) {
        return false;
    }

    std::ofstream(output_dir / "input_info.txt")
        << "protocol=" << protocol_v1::kProtocolId << '\n'
        << "logical_grid=" << protocol_v1::kLogicalGridSize << '\n'
        << "module_px=" << protocol_v1::kModulePixels << '\n'
        << "physical_px=" << protocol_v1::kPhysicalOutputPixels << '\n'
        << "quiet_zone_px=" << protocol_v1::kQuietZonePixels << '\n'
        << "input_path=" << input_path.string() << '\n'
        << "input_bytes=" << input_bytes.size() << '\n'
        << "frame_count=" << frames.size() << '\n'
        << "max_payload_bytes=" << options.max_payload_bytes << '\n'
        << "fps=" << options.fps << '\n'
        << "repeat=" << options.repeat << '\n';

    if (!WriteRepeatedVideo(frames, output_dir, options, error_message)) {
        return false;
    }
    return true;
}

bool DecodeV1Package(const std::filesystem::path& input_path,
                     const std::filesystem::path& output_dir,
                     const protocol_v1::EncoderOptions& options,
                     std::string* error_message) {
    if (!ValidateOptions(options, error_message) || !EnsureDirectory(output_dir, error_message)) {
        return false;
    }

    const std::filesystem::path temp_root = output_dir / "_decode_temp";
    if (!EnsureDirectory(temp_root, error_message)) {
        return false;
    }

    const std::vector<ImageFile> decode_images = CollectDecodeImages(input_path, temp_root, error_message);
    if (decode_images.empty()) {
        return false;
    }

    std::vector<DecodedFrameReport> reports;
    std::map<uint16_t, DecodedFrameData> valid_frames;

    auto finalizeDecode = [&](const std::string& status,
                              std::size_t output_bytes,
                              const std::vector<uint16_t>& missing_frames,
                              const std::string& final_error) {
        if (!WriteDecodeReport(output_dir / "decode_report.tsv", reports, error_message)) {
            return false;
        }
        if (!WriteDecodeSummary(output_dir / "decode_summary.txt",
                                status,
                                valid_frames.size(),
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
            return finalizeDecode("read_error", 0U, {}, *error_message);
        }

        DecodedFrameReport report;
        report.source_index = static_cast<int>(index);

        DecodedFrameData decoded_frame;
        std::string decode_error;
        if (!DecodeFrameModules(SamplePhysicalFrame(frame), options, &decoded_frame, &decode_error)) {
            report.message = decode_error;
            reports.push_back(std::move(report));
            continue;
        }

        report.success = true;
        report.frame_seq = decoded_frame.header.frame_seq;
        report.frame_type = protocol_v1::FrameTypeName(decoded_frame.header.frame_type);
        report.payload_len_bytes = static_cast<uint16_t>(decoded_frame.payload.size());
        report.tail_len_bytes = decoded_frame.header.tail_len_bytes;
        report.message = protocol_v1::HeaderToString(decoded_frame.header);

        const auto [iterator, inserted] = valid_frames.emplace(decoded_frame.header.frame_seq, decoded_frame);
        if (!inserted) {
            report.success = false;
            report.message = "duplicate_frame";
        }
        reports.push_back(std::move(report));
    }

    if (valid_frames.empty()) {
        return finalizeDecode("no_valid_frames", 0U, {}, "No valid V1.6 frames were decoded from the input.");
    }

    std::vector<uint8_t> output_bytes;
    std::vector<uint16_t> missing_frames;
    std::string final_error;

    const auto single_iterator = std::find_if(valid_frames.begin(), valid_frames.end(), [](const auto& item) {
        return item.second.header.frame_type == FrameType::kSingle;
    });
    if (single_iterator != valid_frames.end()) {
        if (valid_frames.size() != 1U || single_iterator->first != 0U) {
            final_error = "Single frame stream conflicts with additional frames.";
            return finalizeDecode("frame_type_conflict", 0U, {}, final_error);
        }
        output_bytes = single_iterator->second.payload;
    } else {
        auto start_iterator = valid_frames.find(0U);
        if (start_iterator == valid_frames.end() || start_iterator->second.header.frame_type != FrameType::kStart) {
            final_error = "Missing frame_seq=0 start frame.";
            return finalizeDecode("missing_start_frame", 0U, {0U}, final_error);
        }

        bool seen_end = false;
        uint16_t expected_seq = 0;
        while (true) {
            const auto iterator = valid_frames.find(expected_seq);
            if (iterator == valid_frames.end()) {
                missing_frames.push_back(expected_seq);
                break;
            }

            const FrameType frame_type = iterator->second.header.frame_type;
            if (expected_seq == 0U && frame_type != FrameType::kStart) {
                final_error = "frame_seq=0 is not a Start frame.";
                return finalizeDecode("frame_type_conflict", 0U, {}, final_error);
            }
            if (expected_seq > 0U && !seen_end && frame_type == FrameType::kStart) {
                final_error = "Unexpected Start frame after frame_seq=0.";
                return finalizeDecode("frame_type_conflict", 0U, {}, final_error);
            }
            if (frame_type == FrameType::kSingle) {
                final_error = "Single frame appeared inside a multi-frame stream.";
                return finalizeDecode("frame_type_conflict", 0U, {}, final_error);
            }
            if (frame_type == FrameType::kEnd) {
                output_bytes.insert(output_bytes.end(),
                                    iterator->second.payload.begin(),
                                    iterator->second.payload.end());
                seen_end = true;
                ++expected_seq;
                break;
            }
            if (frame_type != FrameType::kStart && frame_type != FrameType::kNormal) {
                final_error = "Unexpected frame type while rebuilding stream.";
                return finalizeDecode("frame_type_conflict", 0U, {}, final_error);
            }

            output_bytes.insert(output_bytes.end(),
                                iterator->second.payload.begin(),
                                iterator->second.payload.end());
            ++expected_seq;
        }

        if (!missing_frames.empty()) {
            final_error = "Decoded V1.6 frames are incomplete; see missing_frames.txt.";
            return finalizeDecode("missing_frames", 0U, missing_frames, final_error);
        }
        if (!seen_end) {
            final_error = "Missing End frame.";
            return finalizeDecode("missing_end_frame", 0U, {}, final_error);
        }
        if (valid_frames.size() != static_cast<std::size_t>(expected_seq)) {
            final_error = "Unexpected frames were found after End.";
            return finalizeDecode("frame_type_conflict", 0U, {}, final_error);
        }
    }

    std::ofstream output_stream(output_dir / "output.bin", std::ios::binary);
    if (!output_stream.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to write decode output file.";
        }
        return finalizeDecode("write_error", 0U, {}, *error_message);
    }
    if (!output_bytes.empty()) {
        output_stream.write(reinterpret_cast<const char*>(output_bytes.data()),
                            static_cast<std::streamsize>(output_bytes.size()));
    }
    output_stream.close();

    return finalizeDecode("success", output_bytes.size(), {}, "");
}

}  // namespace demo_encoder
