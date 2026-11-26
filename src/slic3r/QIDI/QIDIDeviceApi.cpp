// fork: open replacement for the unpublished QIDI device HTTP API (Moonraker only).
#include "QIDIDeviceApi.hpp"

#include <boost/log/trivial.hpp>

#include "slic3r/Utils/Http.hpp"

namespace Slic3r {

std::string QIDIDeviceApi::moonraker_url(const std::string &base_url)
{
    std::string url = base_url;
    while (!url.empty() && url.back() == '/')
        url.pop_back();
    if (url.find("://") == std::string::npos)
        url = "http://" + url;
    // add the default Moonraker port when the authority part has none
    const size_t host_begin = url.find("://") + 3;
    const size_t path_begin = url.find('/', host_begin);
    const std::string authority = url.substr(host_begin, path_begin == std::string::npos ? std::string::npos : path_begin - host_begin);
    if (authority.find(':') == std::string::npos)
        url.insert(path_begin == std::string::npos ? url.size() : path_begin, ":7125");
    return url;
}

static bool http_get(const std::string &url, std::string &out)
{
    bool ok = false;
    Http::get(url)
        .timeout_connect(3)
        .timeout_max(10)
        .on_complete([&](std::string body, unsigned status) {
            ok  = status == 200;
            out = std::move(body);
        })
        .on_error([&](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(warning) << "QIDIDeviceApi: GET " << url << " failed: " << status << " " << error;
            ok = false;
        })
        .perform_sync();
    return ok;
}

bool QIDIDeviceApi::get_file_list(const std::string &base_url, std::string &body)
{
    return http_get(moonraker_url(base_url) + "/server/files/list?root=gcodes", body);
}

bool QIDIDeviceApi::get_timelapse_directory(const std::string &base_url, std::string &body)
{
    return http_get(moonraker_url(base_url) + "/server/files/directory?path=timelapse&extended=false", body);
}

bool QIDIDeviceApi::get_system_info(const std::string &base_url, std::string &body)
{
    return http_get(moonraker_url(base_url) + "/machine/system_info", body);
}

bool QIDIDeviceApi::get_client_info(const std::string &, std::string &) { return false; }

} // namespace Slic3r
