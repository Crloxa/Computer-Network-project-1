#include "encoder.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace demo_encoder {
namespace {

using protocol_v1::FrameHeader;
using protocol_v1::GridPoint;

constexpr int kFinderTopLeft = protocol_v1::kQuietZoneModules;
constexpr int kFinderTopRight = protocol_v1::kLogicalGridSize - protocol_v1::kQuietZoneModules - protocol_v1::kFinderSizeModules;
constexpr int kFinderBottomLeft = protocol_v1::kLogicalGridSize - protocol_v1::kQuietZoneModules - protocol_v1::kFinderSizeModules;

void SetCell(cv::Mat& logical_frame, int x, int y, bool black) {
    logical_frame.at<unsigned char>(y, x) = black ? 0U : 255U;
}

void DrawFinder(cv::Mat& logical_frame, int origin_x, int origin_y) {
    for (int dy = 0; dy < protocol_v1::kFinderSizeModules; ++dy) {
        for (int dx = 0; dx < protocol_v1::kFinderSizeModules; ++dx) {
            const int max_distance = std::max(std::abs(dx - 3), std::abs(dy - 3));
            const bool black = (max_distance == 3) || (max_distance <= 1);
            SetCell(logical_frame, origin_x + dx, origin_y + dy, black);
        }
    }
}

void DrawTimingPatterns(cv::Mat& logical_frame) {
    const int timing_x = protocol_v1::kInnerStart - 1;
    const int timing_y = protocol_v1::kInnerStart - 1;

    for (int x = protocol_v1::kInnerStart; x <= protocol_v1::kInnerEnd; ++x) {
        SetCell(logical_frame, x, timing_y, ((x - protocol_v1::kInnerStart) % 2) == 0);
    }
    for (int y = protocol_v1::kInnerStart; y <= protocol_v1::kInnerEnd; ++y) {
        SetCell(logical_frame, timing_x, y, ((y - protocol_v1::kInnerStart) % 2) == 0);
    }
}

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

    stream << "file\tframe_seq\tpayload_len\tcrc32\n";
    for (std::size_t index = 0; index < frames.size(); ++index) {
        stream << FrameFileName(index) << '\t'
               << frames[index].header.frame_seq << '\t'
               << frames[index].header.payload_len_bytes << '\t'
               << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
               << frames[index].header.crc32_payload << std::dec << '\n';
    }
    return true;
}

bool TryWriteVideoWithOpenCv(const std::vector<EncodedFrame>& frames,
                             const std::filesystem::path& temp_path,
                             const protocol_v1::EncoderOptions& options,
                             std::string* error_message) {
    const int frame_size = options.logical_grid_size * options.module_pixels;
    cv::VideoWriter writer;
    writer.open(temp_path.string(), cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                static_cast<double>(options.fps), cv::Size(frame_size, frame_size), true);
    if (!writer.isOpened()) {
        if (error_message != nullptr) {
            *error_message = "OpenCV VideoWriter failed to open output: " + temp_path.string();
        }
        return false;
    }

    for (const EncodedFrame& frame : frames) {
        for (int repeat_index = 0; repeat_index < options.repeat; ++repeat_index) {
            writer.write(frame.physical_frame);
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
                const protocol_v1::EncoderOptions& options,
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

void WriteSampleFrame(const std::filesystem::path& path,
                      uint16_t frame_seq,
                      const std::vector<uint8_t>& payload,
                      int module_pixels) {
    const FrameHeader header{frame_seq,
                             static_cast<uint16_t>(payload.size()),
                             protocol_v1::ComputeCrc32(payload)};
    const cv::Mat logical = RenderLogicalFrame(header, payload);
    const cv::Mat physical = RenderPhysicalFrame(logical, module_pixels);
    cv::imwrite(path.string(), physical);
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

std::vector<EncodedFrame> EncodeBytesToFrames(const std::vector<uint8_t>& bytes,
                                              const protocol_v1::EncoderOptions& options) {
    const int chunk_size = std::min(options.max_payload_bytes, protocol_v1::kMaxPayloadBytes);
    std::vector<EncodedFrame> frames;

    if (bytes.empty()) {
        const FrameHeader header{0, 0, 0};
        EncodedFrame frame;
        frame.header = header;
        frame.logical_frame = RenderLogicalFrame(header, {});
        frame.physical_frame = RenderPhysicalFrame(frame.logical_frame, options.module_pixels);
        frames.push_back(frame);
        return frames;
    }

    for (std::size_t offset = 0; offset < bytes.size(); offset += static_cast<std::size_t>(chunk_size)) {
        const std::size_t remaining = bytes.size() - offset;
        const std::size_t payload_size = std::min<std::size_t>(remaining, static_cast<std::size_t>(chunk_size));
        std::vector<uint8_t> payload(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                     bytes.begin() + static_cast<std::ptrdiff_t>(offset + payload_size));
        const FrameHeader header{
            static_cast<uint16_t>(frames.size() & 0xFFFFU),
            static_cast<uint16_t>(payload.size()),
            protocol_v1::ComputeCrc32(payload),
        };

        EncodedFrame frame;
        frame.header = header;
        frame.payload = std::move(payload);
        frame.logical_frame = RenderLogicalFrame(frame.header, frame.payload);
        frame.physical_frame = RenderPhysicalFrame(frame.logical_frame, options.module_pixels);
        frames.push_back(std::move(frame));
    }

    return frames;
}

cv::Mat RenderLogicalFrame(const protocol_v1::FrameHeader& header, const std::vector<uint8_t>& payload) {
    if (payload.size() > static_cast<std::size_t>(protocol_v1::kMaxPayloadBytes)) {
        throw std::runtime_error("Payload exceeds v1 limit of 1024 bytes.");
    }

    cv::Mat logical_frame(protocol_v1::kLogicalGridSize, protocol_v1::kLogicalGridSize, CV_8UC1,
                          cv::Scalar(255));

    DrawFinder(logical_frame, kFinderTopLeft, kFinderTopLeft);
    DrawFinder(logical_frame, kFinderTopRight, kFinderTopLeft);
    DrawFinder(logical_frame, kFinderTopLeft, kFinderBottomLeft);
    DrawTimingPatterns(logical_frame);

    const std::vector<bool> header_bits = protocol_v1::PackHeaderBits(header);
    std::size_t bit_index = 0;
    for (int row = 0; row < protocol_v1::kHeaderSizeModules; ++row) {
        for (int col = 0; col < protocol_v1::kHeaderSizeModules; ++col) {
            const int x = protocol_v1::kHeaderOriginX + col;
            const int y = protocol_v1::kHeaderOriginY + row;
            SetCell(logical_frame, x, y, header_bits[bit_index++]);
        }
    }

    const std::vector<bool> payload_bits = protocol_v1::BytesToBitsMsbFirst(payload, payload.size());
    const std::vector<GridPoint> payload_cells = protocol_v1::PayloadCells();
    if (payload_bits.size() > payload_cells.size()) {
        throw std::runtime_error("Payload bits exceed drawable payload cells.");
    }

    for (std::size_t index = 0; index < payload_bits.size(); ++index) {
        SetCell(logical_frame, payload_cells[index].x, payload_cells[index].y, payload_bits[index]);
    }

    return logical_frame;
}

cv::Mat RenderPhysicalFrame(const cv::Mat& logical_frame, int module_pixels) {
    cv::Mat physical_frame;
    cv::resize(logical_frame, physical_frame,
               cv::Size(logical_frame.cols * module_pixels, logical_frame.rows * module_pixels),
               0.0, 0.0, cv::INTER_NEAREST);
    cv::cvtColor(physical_frame, physical_frame, cv::COLOR_GRAY2BGR);
    return physical_frame;
}

cv::Mat RenderLayoutGuide(int module_pixels) {
    const FrameHeader empty_header{};
    const cv::Mat guide_base = RenderPhysicalFrame(RenderLogicalFrame(empty_header, {}), module_pixels);
    cv::Mat guide;
    guide_base.copyTo(guide);

    const int inner_px = protocol_v1::kInnerStart * module_pixels;
    const int inner_size_px = protocol_v1::kInnerSize * module_pixels;
    const int header_px = protocol_v1::kHeaderSizeModules * module_pixels;

    cv::rectangle(guide,
                  cv::Rect(inner_px, inner_px, inner_size_px, inner_size_px),
                  cv::Scalar(0, 180, 0), 3);
    cv::rectangle(guide,
                  cv::Rect(protocol_v1::kHeaderOriginX * module_pixels,
                           protocol_v1::kHeaderOriginY * module_pixels,
                           header_px, header_px),
                  cv::Scalar(255, 120, 0), 3);

    cv::putText(guide, "108x108 logical grid", cv::Point(20, 40), cv::FONT_HERSHEY_SIMPLEX,
                0.9, cv::Scalar(40, 40, 200), 2);
    cv::putText(guide, "Inner area: 92x92 cells", cv::Point(20, 80), cv::FONT_HERSHEY_SIMPLEX,
                0.8, cv::Scalar(0, 150, 0), 2);
    cv::putText(guide, "Header: 8x8 cells @ (12,12)", cv::Point(20, 120), cv::FONT_HERSHEY_SIMPLEX,
                0.8, cv::Scalar(255, 120, 0), 2);
    cv::putText(guide, "Payload scan: row-major, skip header", cv::Point(20, 160), cv::FONT_HERSHEY_SIMPLEX,
                0.8, cv::Scalar(80, 80, 80), 2);

    return guide;
}

bool WriteProtocolSamples(const std::filesystem::path& output_dir,
                          const protocol_v1::EncoderOptions& options,
                          std::string* error_message) {
    if (!EnsureDirectory(output_dir, error_message)) {
        return false;
    }

    const std::filesystem::path layout_path = output_dir / "layout_guide.png";
    const std::filesystem::path full_path = output_dir / "sample_full_frame.png";
    const std::filesystem::path short_path = output_dir / "sample_short_frame.png";
    const std::filesystem::path manifest_path = output_dir / "sample_manifest.tsv";

    cv::imwrite(layout_path.string(), RenderLayoutGuide(options.module_pixels));
    WriteSampleFrame(full_path, 0, MakeSamplePayload(protocol_v1::kMaxPayloadBytes, 0x11U), options.module_pixels);
    WriteSampleFrame(short_path, 1, MakeSamplePayload(173, 0x7AU), options.module_pixels);

    std::ofstream manifest(manifest_path);
    if (!manifest.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Failed to write sample manifest.";
        }
        return false;
    }

    const auto full_payload = MakeSamplePayload(protocol_v1::kMaxPayloadBytes, 0x11U);
    const auto short_payload = MakeSamplePayload(173, 0x7AU);
    manifest << "file\tframe_seq\tpayload_len\tcrc32\n";
    manifest << "sample_full_frame.png\t0\t" << full_payload.size() << '\t'
             << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
             << protocol_v1::ComputeCrc32(full_payload) << std::dec << '\n';
    manifest << "sample_short_frame.png\t1\t" << short_payload.size() << '\t'
             << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
             << protocol_v1::ComputeCrc32(short_payload) << std::dec << '\n';
    return true;
}

bool WriteDemoPackage(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_dir,
                      const protocol_v1::EncoderOptions& options,
                      std::string* error_message) {
    if (!EnsureDirectory(output_dir, error_message)) {
        return false;
    }

    const std::filesystem::path logical_dir = output_dir / "frames" / "logical";
    const std::filesystem::path physical_dir = output_dir / "frames" / "physical";
    const std::filesystem::path protocol_dir = output_dir / "protocol_samples";

    if (!EnsureDirectory(logical_dir, error_message) ||
        !EnsureDirectory(physical_dir, error_message) ||
        !EnsureDirectory(protocol_dir, error_message)) {
        return false;
    }

    const std::vector<uint8_t> input_bytes = ReadFileBytes(input_path);
    const std::vector<EncodedFrame> frames = EncodeBytesToFrames(input_bytes, options);

    if (!WriteProtocolSamples(protocol_dir, options, error_message)) {
        return false;
    }

    for (std::size_t index = 0; index < frames.size(); ++index) {
        const std::filesystem::path logical_path = logical_dir / FrameFileName(index);
        const std::filesystem::path physical_path = physical_dir / FrameFileName(index);
        cv::imwrite(logical_path.string(), frames[index].logical_frame);
        cv::imwrite(physical_path.string(), frames[index].physical_frame);
    }

    if (!WriteManifest(output_dir / "frame_manifest.tsv", frames, error_message)) {
        return false;
    }

    std::ofstream(output_dir / "input_info.txt")
        << "input_path=" << input_path.string() << '\n'
        << "input_bytes=" << input_bytes.size() << '\n'
        << "logical_frames=" << frames.size() << '\n'
        << "module_px=" << options.module_pixels << '\n'
        << "fps=" << options.fps << '\n'
        << "repeat=" << options.repeat << '\n';

    return WriteVideo(frames, output_dir, options, error_message);
}

}  // namespace demo_encoder
