// fork: open replacement for the unpublished QIDI device websocket.
// Plain Moonraker JSON-RPC client: connects to ws://<ip>:7125/websocket, subscribes to every Klipper object and
// forwards all messages to QDSDeviceManager, which already understands Moonraker's notify_status_update etc.
#include "QIDIDeviceWebSocket.hpp"

#include <atomic>
#include <thread>

#include <boost/log/trivial.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#include "slic3r/GUI/QDSDeviceManager.hpp"

namespace Slic3r {

using nlohmann::json;
using ws_client = websocketpp::client<websocketpp::config::asio_client>;

struct QIDIDeviceWebSocket::Connection
{
    std::string                 device_id;
    std::string                 url;
    ws_client                   client;
    websocketpp::connection_hdl hdl;
    std::thread                 thread;
    std::atomic<bool>           open{false};
    std::atomic<bool>           closing{false};
    std::atomic<int>            next_id{1};
    std::atomic<int>            list_request_id{-1};
    std::mutex                  send_mutex;

    bool send(const json &msg)
    {
        if (!open.load())
            return false;
        websocketpp::lib::error_code ec;
        {
            std::lock_guard<std::mutex> lk(send_mutex);
            client.send(hdl, msg.dump(), websocketpp::frame::opcode::text, ec);
        }
        if (ec) {
            BOOST_LOG_TRIVIAL(warning) << "[WS] send to " << device_id << " failed: " << ec.message();
            return false;
        }
        return true;
    }

    bool call(const std::string &method, const json &params, int *out_id = nullptr)
    {
        const int id = next_id++;
        if (out_id)
            *out_id = id;
        json msg = {{"jsonrpc", "2.0"}, {"method", method}, {"id", id}};
        if (!params.is_null())
            msg["params"] = params;
        return send(msg);
    }

    void request_object_list()
    {
        int id = -1;
        if (call("printer.objects.list", json(), &id))
            list_request_id = id;
    }
};

QIDIDeviceWebSocket::QIDIDeviceWebSocket() = default;

QIDIDeviceWebSocket::~QIDIDeviceWebSocket() { stopAll(); }

std::shared_ptr<QIDIDeviceWebSocket::Connection> QIDIDeviceWebSocket::find(const std::string &device_id) const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_connections.find(device_id);
    return it == m_connections.end() ? nullptr : it->second;
}

bool QIDIDeviceWebSocket::connect(const std::string &device_id, const std::shared_ptr<GUI::QDSDevice> &device)
{
    if (!device || device->m_url.empty())
        return false;

    stopConnection(device_id);

    auto conn       = std::make_shared<Connection>();
    conn->device_id = device_id;
    conn->url       = device->m_url;

    std::weak_ptr<Connection> weak = conn;
    try {
        conn->client.clear_access_channels(websocketpp::log::alevel::all);
        conn->client.clear_error_channels(websocketpp::log::elevel::all);
        conn->client.init_asio();

        conn->client.set_open_handler([this, weak](websocketpp::connection_hdl) {
            auto c = weak.lock();
            if (!c)
                return;
            c->open = true;
            BOOST_LOG_TRIVIAL(info) << "[WS] connected " << c->device_id << " " << c->url;
            c->request_object_list();
        });
        conn->client.set_fail_handler([this, weak](websocketpp::connection_hdl hdl) {
            auto c = weak.lock();
            if (!c)
                return;
            c->open = false;
            std::string reason;
            try {
                reason = c->client.get_con_from_hdl(hdl)->get_ec().message();
            } catch (...) {}
            BOOST_LOG_TRIVIAL(warning) << "[WS] connection failed " << c->device_id << " " << c->url << ": " << reason;
            if (!c->closing && on_status_changed)
                on_status_changed(c->device_id, "offline");
        });
        conn->client.set_close_handler([this, weak](websocketpp::connection_hdl) {
            auto c = weak.lock();
            if (!c)
                return;
            c->open = false;
            BOOST_LOG_TRIVIAL(info) << "[WS] closed " << c->device_id;
            if (!c->closing && on_status_changed)
                on_status_changed(c->device_id, "offline");
        });
        conn->client.set_message_handler([this, weak](websocketpp::connection_hdl, ws_client::message_ptr msg) {
            auto c = weak.lock();
            if (!c || c->closing)
                return;
            json j;
            try {
                j = json::parse(msg->get_payload());
            } catch (const std::exception &e) {
                BOOST_LOG_TRIVIAL(warning) << "[WS] bad json from " << c->device_id << ": " << e.what();
                return;
            }
            // answer to printer.objects.list -> subscribe to every object
            if (j.contains("id") && j["id"].is_number_integer() && j["id"].get<int>() == c->list_request_id.load() &&
                j.contains("result") && j["result"].contains("objects") && j["result"]["objects"].is_array()) {
                json objects = json::object();
                for (const auto &o : j["result"]["objects"])
                    if (o.is_string())
                        objects[o.get<std::string>()] = nullptr;
                c->call("printer.objects.subscribe", json{{"objects", objects}});
                return;
            }
            if (on_message_received) {
                try {
                    on_message_received(c->device_id, j);
                } catch (const std::exception &e) {
                    BOOST_LOG_TRIVIAL(error) << "[WS] message handler failed for " << c->device_id << ": " << e.what();
                }
            }
        });

        websocketpp::lib::error_code ec;
        ws_client::connection_ptr    con = conn->client.get_connection(conn->url, ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "[WS] bad url " << conn->url << ": " << ec.message();
            return false;
        }
        con->set_open_handshake_timeout(5000);
        conn->hdl = con->get_handle();
        conn->client.connect(con);
    } catch (const std::exception &e) {
        BOOST_LOG_TRIVIAL(error) << "[WS] connect " << conn->url << " failed: " << e.what();
        return false;
    }

    conn->thread = std::thread([conn]() {
        try {
            conn->client.run();
        } catch (const std::exception &e) {
            BOOST_LOG_TRIVIAL(error) << "[WS] io loop of " << conn->device_id << " stopped: " << e.what();
        }
    });

    std::lock_guard<std::mutex> lk(m_mutex);
    m_connections[device_id] = conn;
    return true;
}

void QIDIDeviceWebSocket::stopConnection(const std::string &device_id)
{
    std::shared_ptr<Connection> conn;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_connections.find(device_id);
        if (it == m_connections.end())
            return;
        conn = it->second;
        m_connections.erase(it);
    }
    conn->closing = true;
    if (conn->open.exchange(false)) {
        websocketpp::lib::error_code ec;
        conn->client.close(conn->hdl, websocketpp::close::status::going_away, "", ec);
    }
    conn->client.stop();
    if (conn->thread.joinable()) {
        if (conn->thread.get_id() == std::this_thread::get_id())
            conn->thread.detach(); // called from our own io thread; the shared_ptr keeps the client alive
        else
            conn->thread.join();
    }
}

void QIDIDeviceWebSocket::stopAll()
{
    for (const std::string &id : getConnectionIds())
        stopConnection(id);
}

void QIDIDeviceWebSocket::sendSubscribeMessage(const std::string &device_id)
{
    if (auto c = find(device_id))
        c->request_object_list();
}

void QIDIDeviceWebSocket::getAllErrorList(const std::string &) {}

void QIDIDeviceWebSocket::sendCommand(const std::string &device_id, const std::string &script)
{
    // PrinterTaskDispatcher sends the bare words pause/resume/cancel as a script *and* as an action command;
    // only the action command maps to a valid Moonraker call.
    if (script == "pause" || script == "resume" || script == "cancel")
        return;
    if (auto c = find(device_id))
        c->call("printer.gcode.script", json{{"script", script}});
}

bool QIDIDeviceWebSocket::sendCommand(const std::string &device_id, const std::string &script_name, const std::string &script, const std::string &method)
{
    auto c = find(device_id);
    return c && c->call(method, json{{script_name, script}});
}

bool QIDIDeviceWebSocket::sendCommand(const std::string &device_id, const nlohmann::json &params, const std::string &method)
{
    auto c = find(device_id);
    return c && c->call(method, params);
}

void QIDIDeviceWebSocket::sendActionCommand(const std::string &device_id, const std::string &action_type)
{
    std::string method;
    if (action_type == "pause")
        method = "printer.print.pause";
    else if (action_type == "resume")
        method = "printer.print.resume";
    else if (action_type == "cancel")
        method = "printer.print.cancel";
    else {
        BOOST_LOG_TRIVIAL(warning) << "[WS] unknown action " << action_type;
        return;
    }
    if (auto c = find(device_id))
        c->call(method, json());
}

std::vector<std::string> QIDIDeviceWebSocket::getConnectionIds() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<std::string> ids;
    ids.reserve(m_connections.size());
    for (const auto &kv : m_connections)
        ids.push_back(kv.first);
    return ids;
}

} // namespace Slic3r
