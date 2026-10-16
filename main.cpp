// ============================================================================
// REST-сервер: точка входа.
// ============================================================================

#include "src/lib/httplib.h"
#include "src/json.hpp"
#include "src/crypto.hpp"
#include "src/database.hpp"
#include "src/routes/auth.hpp"
#include "src/routes/public.hpp"
#include "src/modules/example/stats.hpp"
#include "src/modules/individual/task.hpp"


#include <iostream>
#include <exception>
#include <fstream>
#include <iterator>
#include <string>

int main() {
    // ---- 1. Инициализация ----
    if (!load_sqlite_dll()) { std::cerr << "[FATAL] SQLite loader failed\n"; return 1; }
    if (!init_database())   { std::cerr << "[FATAL] Database init failed\n"; return 1; }

    sqlite3_exec(db, "DELETE FROM sessions;", nullptr, nullptr, nullptr);

    // ---- 2. Создаём сервер ----
    httplib::Server svr;

    // ---- 3. Ограничение размера тела запроса ----
    svr.set_payload_max_length(1024 * 1024);

    // ---- 4. Логирование запросов ----
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << "[" << req.method << "] " << req.path
                  << " -> " << res.status << std::endl;
    });

    // ---- 5. Единый формат ошибок ----
    svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        if (!res.body.empty()) return;
        JObj o;
        o.int_("status", res.status);
        o.str("error", "Request failed");
        o.str("path", req.path);
        res.set_content(o.str(), "application/json");
    });

    // ---- 6. Перехват исключений ----
    svr.set_exception_handler(
        [](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
            std::string msg = "unknown";
            try { std::rethrow_exception(ep); }
            catch (const std::exception& e) { msg = e.what(); }
            catch (...) {}
            JObj o;
            o.str("error", "Internal error");
            o.str("detail", msg);
            res.status = 500;
            res.set_content(o.str(), "application/json");
        });

    // ---- 7. CORS ----
    svr.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Authorization,Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });

   svr.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });    

    // ---- 8. Страница запросов и healthcheck ----
    auto send_frontend = [](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("frontend/index.html");
        if (!file) {
            res.status = 404;
            return;
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        res.set_header("Cache-Control", "no-store");
        res.set_content(content, "text/html; charset=utf-8");
    };
    svr.Get("/", send_frontend);
    svr.Get("/frontend/index.html", send_frontend);

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        JObj o;
        o.str("status", "ok");
        res.set_content(o.str(), "application/json");
    });

    // ---- 9. Роуты ----
    Auth::init_routes(svr);
    Public::init_routes(svr);
    Example::init_routes(svr);      
    Individual::init_routes(svr);    

    // ---- 10. Старт ----
    std::cout << "=============================================" << std::endl;
    std::cout << "  Server running on http://localhost:8080"    << std::endl;
    std::cout << "  Healthcheck: http://localhost:8080/health"  << std::endl;
    std::cout << "=============================================" << std::endl;

    svr.listen("0.0.0.0", 8080);

    sqlite3_close(db);
    return 0;
}