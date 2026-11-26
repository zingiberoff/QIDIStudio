// QIDI does not publish src/slic3r/QIDI; this stub stands in for headless builds.
#ifndef slic3r_QIDI_P2PManager_stub_hpp_
#define slic3r_QIDI_P2PManager_stub_hpp_

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace Slic3r {

class P2PManager
{
public:
    struct FileTransferOptions
    {
        std::atomic<bool>                      *cancel{nullptr};
        std::function<void(int64_t, int64_t)> progress;
    };

    struct FileTransferResult
    {
        bool ok{false};
        bool cancelled{false};
    };

    static P2PManager &instance()
    {
        static P2PManager manager;
        return manager;
    }

    FileTransferResult uploadFile(const std::string &, const std::string &, const FileTransferOptions &) { return {}; }
};

} // namespace Slic3r

#endif
