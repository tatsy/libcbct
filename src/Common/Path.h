#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_PATH_H
#define LIBCBCT_PATH_H

#include <string>

#if defined(_WIN32)
#    define PATH_WIN32_IMPL  // Win32
#    include <Windows.h>
#else
#    define PATH_UNIX_IMPL  // UNIX
#    include <sys/types.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

#if defined(PATH_WIN32_IMPL)
#    define PATH_DELIM "\\"
#elif defined(PATH_UNIX_IMPL)
#    define PATH_DELIM "/"
#endif

class filepath {
public:
    filepath(const std::string &path = "")
        : path_{path} {
        cleanup();
    }

    filepath(const char *path = "")
        : path_{path} {
        cleanup();
    }

    filepath(const filepath &path) = default;

    filepath &operator=(const filepath &path) = default;

    operator std::string() const { return path_; }

    filepath operator+(const filepath &other) const { return this->string() + other.string(); }
    filepath operator/(const filepath &other) const { return this->string() + "/" + other.string(); }
    bool operator==(const filepath &other) const { return path_ == other.path_; }
    bool operator!=(const filepath &other) const { return path_ != other.path_; }

    //! Basename
    filepath filename() const {
        const int pos = path_.find_last_of("/");
        if (pos == std::string::npos) {
            return path_;
        }
        return path_.substr(pos + 1);
    }

    //! Directory name
    filepath dirname() const {
        const int pos = path_.find_last_of("/");
        if (pos == std::string::npos) {
            return "";
        }
        return path_.substr(0, pos);
    }

    //! Basename without extension
    filepath stem() const {
        const std::string base = filename().string();
        const int pos = base.find_last_of(".");
        if (pos == std::string::npos) {
            return base;
        }
        return base.substr(0, pos);
    }

    //! Extension of file path
    filepath extension() const {
        const std::string base = filename().string();
        const int pos = base.find_last_of(".");
        if (pos == std::string::npos) {
            return "";
        }
        return base.substr(pos);
    }

    //! file existence
    bool exists() const {
#if defined(PATH_WIN32_IMPL)
        DWORD dwAttrib = GetFileAttributes(path_.c_str());
        return (dwAttrib != INVALID_FILE_ATTRIBUTES);
#else
        return (access(path_.c_str(), F_OK) == 0);
#endif
    }

    bool isfile() const {
#if defined(PATH_WIN32_IMPL)
        DWORD dwAttrib = GetFileAttributes(path_.c_str());
        return (dwAttrib != INVALID_FILE_ATTRIBUTES) && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat buf;
        if (stat(path_.c_str(), &buf) != 0) {
            return false;
        }
        return S_ISREG(buf.st_mode);
#endif
    }

    bool isdir() const {
#if defined(PATH_WIN32_IMPL)
        DWORD dwAttrib = GetFileAttributes(path_.c_str());
        return (dwAttrib != INVALID_FILE_ATTRIBUTES) && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat buf;
        if (stat(path_.c_str(), &buf) != 0) {
            return false;
        }
        return S_ISDIR(buf.st_mode);
#endif
    }

    //! Internal path string
    std::string string() const { return path_; }

    //! Internal path const char*
    const char *const c_str() const { return path_.c_str(); }

private:
    void cleanup() {
        size_t pos = -1;
        while ((pos = path_.find_first_of("\\")) != std::string::npos) {
            path_.replace(pos, 1, "/");
        }
    }

    std::string path_;
};  // class path

#endif  // LIBCBCT_PATH_H
