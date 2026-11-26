// fork: QIDI does not publish src/slic3r/QIDI. This is an open replacement that talks to the
// printer's Moonraker HTTP API directly (LAN only, no QIDI cloud).
#ifndef slic3r_QIDI_QIDIDeviceApi_hpp_
#define slic3r_QIDI_QIDIDeviceApi_hpp_

#include <string>
#include <wx/string.h>

#include "QIDINetworkTypes.hpp"

namespace Slic3r {

class QIDIDeviceApi
{
public:
    // base_url: "http://<ip>" or "http://<ip>:<port>"; Moonraker's port 7125 is added when missing.
    static bool get_file_list(const std::string &base_url, std::string &body);
    static bool get_timelapse_directory(const std::string &base_url, std::string &body);
    static bool get_system_info(const std::string &base_url, std::string &body);
    // QIDI's own "agent" client is not present on stock firmware 01.01.02.x; always false.
    static bool get_client_info(const std::string &base_url, std::string &body);

    // Used only by the cloud build (QDT_RELEASE_TO_PUBLIC=1).
    bool          get_box_state(wxString &, const std::string &, const std::string &) { return false; }
    GUI::Box_info get_box_info(wxString &, const std::string &, const std::string &) { return {}; }
    void          get_color_filament_str(wxString &, GUI::Box_info &, const std::string &, const std::string &) {}

    static std::string moonraker_url(const std::string &base_url);
};

} // namespace Slic3r

#endif
