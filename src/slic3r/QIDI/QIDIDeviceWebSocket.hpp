// fork: QIDI does not publish src/slic3r/QIDI. Open replacement of the device websocket:
// a plain Moonraker JSON-RPC client (ws://<ip>:7125/websocket), LAN only.
#ifndef slic3r_QIDI_QIDIDeviceWebSocket_hpp_
#define slic3r_QIDI_QIDIDeviceWebSocket_hpp_

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace Slic3r {
namespace GUI { class QDSDevice; }

class QIDIDeviceWebSocket
{
public:
    QIDIDeviceWebSocket();
    ~QIDIDeviceWebSocket();

    // (device_id, "offline" | "error")
    std::function<void(const std::string &, const std::string &)>    on_status_changed;
    // raw Moonraker JSON-RPC messages (notifications and responses)
    std::function<void(const std::string &, const nlohmann::json &)> on_message_received;

    // Starts an asynchronous connection; subscribes to all printer objects once it is open.
    bool connect(const std::string &device_id, const std::shared_ptr<GUI::QDSDevice> &device);
    void stopConnection(const std::string &device_id);
    void stopAll();
    // Re-subscribe (e.g. after notify_klippy_ready).
    void sendSubscribeMessage(const std::string &device_id);
    // QIDI agent specific, not available on stock firmware: no-op.
    void getAllErrorList(const std::string &device_id);
    // printer.gcode.script
    void sendCommand(const std::string &device_id, const std::string &script);
    // generic JSON-RPC call with a single named parameter: {script_name: script}
    bool sendCommand(const std::string &device_id, const std::string &script_name, const std::string &script, const std::string &method);
    // generic JSON-RPC call
    bool sendCommand(const std::string &device_id, const nlohmann::json &params, const std::string &method);
    // "pause" | "resume" | "cancel" -> printer.print.*
    void sendActionCommand(const std::string &device_id, const std::string &action_type);
    std::vector<std::string> getConnectionIds() const;

private:
    struct Connection;
    std::shared_ptr<Connection> find(const std::string &device_id) const;

    mutable std::mutex                                 m_mutex;
    std::map<std::string, std::shared_ptr<Connection>> m_connections;
};

} // namespace Slic3r

#endif
