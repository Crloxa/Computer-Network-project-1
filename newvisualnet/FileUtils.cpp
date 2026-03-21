#include "FileUtils.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define PATH_SEP "\\"
const std::string RM_FILE_CMD = "del /q ";
const std::string RM_DIR_CMD = "rmdir /s /q ";
#else
#include <sys/stat.h>
#include <dirent.h>
#define PATH_SEP "/"
const std::string RM_FILE_CMD = "rm -f ";
const std::string RM_DIR_CMD = "rm -rf ";
#endif

namespace FileUtils {
    bool readFileToBytes(const std::string& filePath, std::vector<uint8_t>& outData) {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Read file failed: " << filePath << std::endl;
            return false;
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        outData.resize(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(outData.data()), size);
        file.close();
        return true;
    }

    bool writeBytesToFile(const std::string& filePath, const std::vector<uint8_t>& data) {
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Write file failed: " << filePath << std::endl;
            return false;
        }
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        return true;
    }

    bool fileExists(const std::string& filePath) {
#ifdef _WIN32
        DWORD attr = GetFileAttributesA(filePath.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat st;
        return (stat(filePath.c_str(), &st) == 0) && S_ISREG(st.st_mode);
#endif
    }

    bool dirExists(const std::string& dirPath) {
#ifdef _WIN32
        DWORD attr = GetFileAttributesA(dirPath.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat st;
        return (stat(dirPath.c_str(), &st) == 0) && S_ISDIR(st.st_mode);
#endif
    }

    bool createDirectory(const std::string& dirPath) {
#ifdef _WIN32
        std::string temp = dirPath;
        std::replace(temp.begin(), temp.end(), '/', '\\');
        return CreateDirectoryA(temp.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
        std::string cmd = "mkdir -p \"" + dirPath + "\"";
        return system(cmd.c_str()) == 0;
#endif
    }

    bool clearDirectory(const std::string& dirPath) {
        if (!dirExists(dirPath)) return createDirectory(dirPath);
#ifdef _WIN32
        std::string searchPath = joinPath(dirPath, "*");
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
        // 修复HANDLE类型转换错误：使用INVALID_HANDLE_VALUE直接比较
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;
                std::string fullPath = joinPath(dirPath, findData.cFileName);
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    std::string cmd = RM_DIR_CMD + "\"" + fullPath + "\"";
                    system(cmd.c_str());
                }
                else {
                    std::string cmd = RM_FILE_CMD + "\"" + fullPath + "\"";
                    system(cmd.c_str());
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
#else
        DIR* dir = opendir(dirPath.c_str());
        if (dir != nullptr) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
                std::string fullPath = joinPath(dirPath, entry->d_name);
                if (entry->d_type == DT_DIR) {
                    std::string cmd = RM_DIR_CMD + "\"" + fullPath + "\"";
                    system(cmd.c_str());
                }
                else {
                    std::string cmd = RM_FILE_CMD + "\"" + fullPath + "\"";
                    system(cmd.c_str());
                }
            }
            closedir(dir);
        }
#endif
        return true;
    }

    std::string joinPath(const std::string& parent, const std::string& child) {
        if (parent.empty()) return child;
        if (child.empty()) return parent;
        char lastChar = parent.back();
        if (lastChar == '/' || lastChar == '\\') return parent + child;
#ifdef _WIN32
        return parent + "\\" + child;
#else
        return parent + "/" + child;
#endif
    }
}