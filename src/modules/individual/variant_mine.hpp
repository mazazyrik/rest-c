#pragma once
// ============================================================================
// ВАРИАНТ 33: пропускная способность и стоимость CDN.
// ============================================================================

#include <string>
#include <sstream>
#include <iomanip>
#include <locale>
#include <limits>
#include <cmath>
#include "../../json.hpp"

namespace TaskVariant {

    // ------------------------------------------------------------------------
    // Структура входных данных. Соответствует полям JSON-запроса.
    // ------------------------------------------------------------------------
    struct Input {
        int    monthly_active_viewers;          // зрители за месяц
        double avg_streaming_hours_per_viewer;  // часы просмотра на зрителя
        double video_bitrate_kbps;              // битрейт, кбит/с
        double cdn_price_per_terabyte;          // цена CDN за ТБ
        bool   viewers_ok;                      // зрители заданы целым токеном
    };

    // ------------------------------------------------------------------------
    // Структура выходных данных. Соответствует полям JSON-ответа.
    // ------------------------------------------------------------------------
    struct Output {
        double total_monthly_traffic_tb;       // трафик за месяц, ТБ
        double total_cdn_invoice;              // счёт CDN
        double infrastructure_cost_per_user;   // стоимость на одного зрителя
    };

    // ------------------------------------------------------------------------
    // Проверка входных данных.
    // Возвращает "" если всё ок, иначе — текст ошибки.
    // ------------------------------------------------------------------------
    inline std::string validate(const Input& in) {
        if (!in.viewers_ok || in.monthly_active_viewers < 1)
            return "число зрителей должно быть целым и не меньше 1";
        if (!(in.avg_streaming_hours_per_viewer >= 0.01
              && in.avg_streaming_hours_per_viewer <= 744.0))
            return "часы просмотра на зрителя должны быть в диапазоне от 0.01 до 744.0";
        if (!(in.video_bitrate_kbps >= 300.0 && in.video_bitrate_kbps <= 50000.0))
            return "битрейт должен быть в диапазоне от 300.0 до 50000.0 кбит/с";
        if (!(in.cdn_price_per_terabyte > 0.0) || !std::isfinite(in.cdn_price_per_terabyte))
            return "цена за терабайт должна быть больше 0";
        return "";
    }

    // ------------------------------------------------------------------------
    // Основные расчёты.
    // Делитель 1000000000 берётся из формулы задания.
    // На примере 10000, 10, 4000, 5 это 180 ТБ, а не 18 из текста примера.
    // ------------------------------------------------------------------------
    inline Output calculate(const Input& in) {
        Output out{};

        out.total_monthly_traffic_tb =
            (double(in.monthly_active_viewers) * in.avg_streaming_hours_per_viewer * 3600.0
             * (in.video_bitrate_kbps / 8.0))
            / 1000000000.0;

        out.total_cdn_invoice = out.total_monthly_traffic_tb * in.cdn_price_per_terabyte;

        out.infrastructure_cost_per_user =
            out.total_cdn_invoice / double(in.monthly_active_viewers);

        return out;
    }

    // ------------------------------------------------------------------------
    // Разбор JSON-тела запроса в Input.
    // ------------------------------------------------------------------------
    inline Input parse(const std::string& body) {
        Input in{};

        auto get_d = [&](const std::string& key) -> double {
            auto p = body.find("\"" + key + "\"");
            if (p == std::string::npos) return 0.0;
            p = body.find(':', p);
            if (p == std::string::npos) return 0.0;
            try { return std::stod(body.substr(p + 1)); }
            catch (...) { return 0.0; }
        };

        // целый токен: без точки и без экспоненты
        auto p = body.find("\"monthly_active_viewers\"");
        if (p != std::string::npos)
            p = body.find(':', p);
        if (p != std::string::npos) {
            ++p;
            while (p < body.size()
                   && (body[p] == ' ' || body[p] == '\t' || body[p] == '\n' || body[p] == '\r'))
                ++p;

            auto start = p;
            if (p < body.size() && (body[p] == '+' || body[p] == '-'))
                ++p;
            auto digits = p;
            while (p < body.size() && body[p] >= '0' && body[p] <= '9')
                ++p;

            bool integer_token = digits != p
                && (p >= body.size() || (body[p] != '.' && body[p] != 'e' && body[p] != 'E'));
            if (integer_token) {
                try {
                    std::size_t idx = 0;
                    auto token = body.substr(start, p - start);
                    long long v = std::stoll(token, &idx, 10);
                    if (idx == token.size()
                        && v >= std::numeric_limits<int>::min()
                        && v <= std::numeric_limits<int>::max()) {
                        in.monthly_active_viewers = static_cast<int>(v);
                        in.viewers_ok = true;
                    }
                } catch (...) {}
            }
        }

        in.avg_streaming_hours_per_viewer = get_d("avg_streaming_hours_per_viewer");
        in.video_bitrate_kbps = get_d("video_bitrate_kbps");
        in.cdn_price_per_terabyte = get_d("cdn_price_per_terabyte");
        return in;
    }

    // ------------------------------------------------------------------------
    // Формирование JSON-ответа из Output.
    // Два знака после запятой — только здесь, само значение не округляется.
    // ------------------------------------------------------------------------
    inline std::string to_json(const Output& out) {
        auto fmt2 = [](double v) {
            std::ostringstream ss;
            ss.imbue(std::locale::classic());
            ss << std::fixed << std::setprecision(2) << v;
            return ss.str();
        };

        JObj o;
        o.raw("total_monthly_traffic_tb", fmt2(out.total_monthly_traffic_tb));
        o.raw("total_cdn_invoice", fmt2(out.total_cdn_invoice));
        o.raw("infrastructure_cost_per_user", fmt2(out.infrastructure_cost_per_user));
        return o.str();
    }

} // namespace TaskVariant
