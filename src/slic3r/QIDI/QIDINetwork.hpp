// QIDI does not publish src/slic3r/QIDI; this stub stands in for headless builds.
#ifndef slic3r_QIDI_QIDINetwork_stub_hpp_
#define slic3r_QIDI_QIDINetwork_stub_hpp_

#include <functional>
#include <string>

namespace Slic3r {

class MakerHttpHandle
{
public:
    static MakerHttpHandle &getInstance()
    {
        static MakerHttpHandle handle;
        return handle;
    }

    void get_qidi_model_list(std::function<void(std::string, unsigned)>,
                             std::function<void(std::string, std::string, unsigned)>)
    {}
};

} // namespace Slic3r

#endif
