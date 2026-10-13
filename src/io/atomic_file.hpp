#pragma once
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace dps::detail {
// Claim a unique directory beside the destination so concurrent writers cannot
// share a temporary file. Only a complete, closed file replaces the original.
class AtomicFile {
    std::filesystem::path destination_, directory_, temporary_;
    std::ofstream output_;
public:
    explicit AtomicFile(const std::filesystem::path& destination)
        : destination_(destination) {
        for(unsigned attempt=0;attempt<1000;++attempt) {
            directory_=destination_;
            directory_ += ".pending-" + std::to_string(attempt);
            std::error_code error;
            if(std::filesystem::create_directory(directory_,error)) break;
            if(error) throw std::filesystem::filesystem_error("Cannot stage file",directory_,error);
            if(attempt==999) throw std::runtime_error("Cannot reserve temporary file");
        }
        temporary_=directory_/"content";
        output_.open(temporary_,std::ios::binary);
        if(!output_) {
            std::error_code ignored;std::filesystem::remove(directory_,ignored);
            throw std::runtime_error("Cannot open temporary file");
        }
    }
    AtomicFile(const AtomicFile&)=delete;
    AtomicFile& operator=(const AtomicFile&)=delete;
    ~AtomicFile() {
        output_.close();
        std::error_code ignored;
        std::filesystem::remove(temporary_,ignored);
        std::filesystem::remove(directory_,ignored);
    }
    std::ostream& stream() noexcept {return output_;}
    void commit() {
        output_.flush();
        if(!output_) throw std::runtime_error("File write failed");
        output_.close();
        if(!output_) throw std::runtime_error("File close failed");
#ifdef _WIN32
        if(!MoveFileExW(temporary_.c_str(),destination_.c_str(),
                       MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Cannot replace file");
#else
        std::filesystem::rename(temporary_,destination_);
#endif
    }
};
}
