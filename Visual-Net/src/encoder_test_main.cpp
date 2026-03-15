#include "code.h"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
constexpr std::size_t kBytesPerFrame = 1878;
constexpr int kExpectedWidth = 1330;
constexpr int kExpectedHeight = 1330;
constexpr int kExpectedChannels = 3;
const char* kManifestFileName = "encoder_test_manifest.tsv";

struct FrameRecord
{
    int frameIndex = -1;
    std::string fileName;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::uintmax_t fileSize = 0;
    bool passed = true;
    std::string detail;
};

[[noreturn]] void Fail(const std::string& message)
{
    throw std::runtime_error(message);
}

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string NormalizeFormat(std::string value)
{
    if (!value.empty() && value.front() == '.') {
        value.erase(value.begin());
    }
    value = ToLower(value);
    if (value.empty()) {
        Fail("output_format must not be empty");
    }
    return value;
}

int ParsePositiveInt(const char* text, const char* label)
{
    if (text == nullptr || text[0] == '\0') {
        std::ostringstream oss;
        oss << label << " must not be empty";
        Fail(oss.str());
    }
    std::size_t parsed = 0;
    int value = 0;
    try
    {
        value = std::stoi(text, &parsed);
    }
    catch (const std::exception&)
    {
        std::ostringstream oss;
        oss << "invalid " << label << ": " << text;
        Fail(oss.str());
    }
    if (parsed != std::strlen(text) || value <= 0) {
        std::ostringstream oss;
        oss << label << " must be a positive integer: " << text;
        Fail(oss.str());
    }
    return value;
}

std::vector<char> ReadFileBytes(const fs::path& inputPath)
{
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        std::ostringstream oss;
        oss << "failed to open input file: " << inputPath.string();
        Fail(oss.str());
    }

    in.seekg(0, std::ios::end);
    const std::streamoff end = in.tellg();
    if (end <= 0) {
        std::ostringstream oss;
        oss << "input file is empty or unreadable: " << inputPath.string();
        Fail(oss.str());
    }
    in.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<std::size_t>(end));
    in.read(buffer.data(), end);
    if (!in) {
        std::ostringstream oss;
        oss << "failed to read input file: " << inputPath.string();
        Fail(oss.str());
    }
    return buffer;
}

void PrepareOutputDirectory(const fs::path& outputDir, const std::string& format)
{
    std::error_code ec;
    if (fs::exists(outputDir, ec) && !fs::is_directory(outputDir, ec)) {
        std::ostringstream oss;
        oss << "output path exists but is not a directory: " << outputDir.string();
        Fail(oss.str());
    }
    fs::create_directories(outputDir, ec);
    if (ec) {
        std::ostringstream oss;
        oss << "failed to create output directory: " << outputDir.string();
        Fail(oss.str());
    }

    const std::regex framePattern("^\\d{5}\\." + format + "$", std::regex_constants::icase);
    const std::regex layoutPattern("^\\d{5}_layout\\." + format + "$", std::regex_constants::icase);
    for (const auto& entry : fs::directory_iterator(outputDir, ec))
    {
        if (ec) {
            std::ostringstream oss;
            oss << "failed to scan output directory: " << outputDir.string();
            Fail(oss.str());
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (std::regex_match(name, framePattern) ||
            std::regex_match(name, layoutPattern) ||
            name == kManifestFileName) {
            fs::remove(entry.path(), ec);
            if (ec) {
                std::ostringstream oss;
                oss << "failed to clean stale output file: " << entry.path().string();
                Fail(oss.str());
            }
        }
    }
}

std::vector<FrameRecord> CollectFrameRecords(const fs::path& outputDir, const std::string& format, std::size_t& layoutCount)
{
    std::vector<FrameRecord> records;
    layoutCount = 0;
    const std::regex framePattern("^(\\d{5})\\." + format + "$", std::regex_constants::icase);
    const std::regex layoutPattern("^\\d{5}_layout\\." + format + "$", std::regex_constants::icase);
    std::smatch match;
    std::error_code ec;

    for (const auto& entry : fs::directory_iterator(outputDir, ec))
    {
        if (ec) {
            std::ostringstream oss;
            oss << "failed to scan generated output files in: " << outputDir.string();
            Fail(oss.str());
        }
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::string name = entry.path().filename().string();
        if (std::regex_match(name, layoutPattern)) {
            ++layoutCount;
            continue;
        }

        if (!std::regex_match(name, match, framePattern)) {
            continue;
        }

        FrameRecord record;
        record.frameIndex = std::stoi(match[1].str());
        record.fileName = name;
        record.fileSize = fs::file_size(entry.path(), ec);
        if (ec) {
            record.passed = false;
            record.detail = "failed_to_get_file_size";
            ec.clear();
        }

        const cv::Mat image = cv::imread(entry.path().string(), cv::IMREAD_UNCHANGED);
        if (image.empty()) {
            record.passed = false;
            record.detail = "opencv_imread_failed";
        }
        else {
            record.width = image.cols;
            record.height = image.rows;
            record.channels = image.channels();
            if (record.width != kExpectedWidth) {
                record.passed = false;
                record.detail += record.detail.empty() ? "" : ";";
                record.detail += "unexpected_width";
            }
            if (record.height != kExpectedHeight) {
                record.passed = false;
                record.detail += record.detail.empty() ? "" : ";";
                record.detail += "unexpected_height";
            }
            if (record.channels != kExpectedChannels) {
                record.passed = false;
                record.detail += record.detail.empty() ? "" : ";";
                record.detail += "unexpected_channels";
            }
        }

        if (record.detail.empty()) {
            record.detail = "ok";
        }
        records.push_back(record);
    }

    std::sort(records.begin(), records.end(), [](const FrameRecord& lhs, const FrameRecord& rhs) {
        return lhs.frameIndex < rhs.frameIndex;
    });
    return records;
}

void MarkFrameFailure(FrameRecord& record, const std::string& reason)
{
    record.passed = false;
    if (record.detail == "ok") {
        record.detail.clear();
    }
    if (!record.detail.empty()) {
        record.detail += ";";
    }
    record.detail += reason;
}

std::size_t ComputeExpectedFrameCount(std::size_t inputBytes, const std::optional<int>& frameLimit)
{
    const std::size_t totalFrames = (inputBytes + kBytesPerFrame - 1) / kBytesPerFrame;
    if (!frameLimit.has_value()) {
        return totalFrames;
    }
    return std::min<std::size_t>(totalFrames, static_cast<std::size_t>(*frameLimit));
}

void WriteManifest(const fs::path& manifestPath, const std::vector<FrameRecord>& records)
{
    std::ofstream out(manifestPath, std::ios::binary);
    if (!out) {
        std::ostringstream oss;
        oss << "failed to create manifest file: " << manifestPath.string();
        Fail(oss.str());
    }

    out << "frame_index\tfile_name\twidth\theight\tchannels\tfile_size\tstatus\tdetail\n";
    for (const auto& record : records)
    {
        out << record.frameIndex << '\t'
            << record.fileName << '\t'
            << record.width << '\t'
            << record.height << '\t'
            << record.channels << '\t'
            << record.fileSize << '\t'
            << (record.passed ? "PASS" : "FAIL") << '\t'
            << record.detail << '\n';
    }
}

void PrintUsage()
{
    std::cout << "Usage: encoder_test <input_file> <output_dir> [output_format] [frame_limit]\n"
              << "Example: encoder_test input.bin output_frames png 10\n";
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 3 || argc > 5) {
        PrintUsage();
        return 1;
    }

    try
    {
        const fs::path inputPath = fs::absolute(argv[1]);
        const fs::path outputDir = fs::absolute(argv[2]);
        const std::string outputFormat = NormalizeFormat(argc >= 4 ? argv[3] : "png");
        const std::optional<int> frameLimit = argc >= 5 ? std::optional<int>(ParsePositiveInt(argv[4], "frame_limit")) : std::nullopt;

        if (!fs::exists(inputPath) || !fs::is_regular_file(inputPath)) {
            std::ostringstream oss;
            oss << "input file does not exist: " << inputPath.string();
            Fail(oss.str());
        }

        std::vector<char> inputBytes = ReadFileBytes(inputPath);
        PrepareOutputDirectory(outputDir, outputFormat);

        Code::Main(inputBytes.data(),
                   static_cast<int>(inputBytes.size()),
                   outputDir.string().c_str(),
                   outputFormat.c_str(),
                   frameLimit.value_or(std::numeric_limits<int>::max()));

        std::size_t layoutCount = 0;
        std::vector<FrameRecord> records = CollectFrameRecords(outputDir, outputFormat, layoutCount);
        std::vector<std::string> failures;

        if (records.empty()) {
            failures.push_back("no frame image was generated");
        }

        const std::size_t expectedFrames = ComputeExpectedFrameCount(inputBytes.size(), frameLimit);
        if (records.size() != expectedFrames) {
            std::ostringstream oss;
            oss << "generated frame count mismatch: expected " << expectedFrames
                << ", actual " << records.size();
            failures.push_back(oss.str());
        }

        if (inputBytes.size() <= kBytesPerFrame && records.size() != 1) {
            failures.push_back("single-frame input should generate exactly one frame");
        }

        if (frameLimit.has_value() && records.size() > static_cast<std::size_t>(*frameLimit)) {
            failures.push_back("generated frame count exceeds frame_limit");
        }

        for (std::size_t i = 0; i < records.size(); ++i)
        {
            if (records[i].frameIndex != static_cast<int>(i)) {
                std::ostringstream oss;
                oss << "frame naming is not continuous from 00000 at position " << i
                    << ", actual index " << records[i].frameIndex;
                failures.push_back(oss.str());
                MarkFrameFailure(records[i], "non_continuous_name");
            }
        }

        if (layoutCount > 0 && layoutCount != records.size()) {
            std::ostringstream oss;
            oss << "layout preview count mismatch: found " << layoutCount
                << ", frame count " << records.size();
            failures.push_back(oss.str());
        }

        for (const auto& record : records)
        {
            if (!record.passed) {
                std::ostringstream oss;
                oss << "frame " << record.fileName << " failed checks: " << record.detail;
                failures.push_back(oss.str());
            }
        }

        WriteManifest(outputDir / kManifestFileName, records);

        std::cout << "Encoder Test Summary\n"
                  << "input_file: " << inputPath.string() << "\n"
                  << "input_size: " << inputBytes.size() << " bytes\n"
                  << "output_dir: " << outputDir.string() << "\n"
                  << "output_format: " << outputFormat << "\n"
                  << "frame_limit: " << (frameLimit.has_value() ? std::to_string(*frameLimit) : std::string("none")) << "\n"
                  << "generated_frames: " << records.size() << "\n"
                  << "expected_frames: " << expectedFrames << "\n"
                  << "layout_previews: " << layoutCount << "\n"
                  << "manifest: " << (outputDir / kManifestFileName).string() << "\n"
                  << "result: " << (failures.empty() ? "PASS" : "FAIL") << "\n";

        if (!failures.empty()) {
            std::cout << "failure_reasons:\n";
            const std::size_t limit = std::min<std::size_t>(failures.size(), 10);
            for (std::size_t i = 0; i < limit; ++i)
            {
                std::cout << "  - " << failures[i] << "\n";
            }
            if (failures.size() > limit) {
                std::cout << "  - ... " << (failures.size() - limit) << " more\n";
            }
            return 1;
        }

        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "encoder_test error: " << ex.what() << "\n";
        PrintUsage();
        return 1;
    }
}
