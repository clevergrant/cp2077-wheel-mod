#include "config.h"
#include "logging.h"
#include "button_map.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

namespace gwheel::config
{
    namespace
    {
        struct Store
        {
            std::mutex            writerMutex;
            std::atomic<int>      publishedIdx{0};
            Config                slots[2]{};
            std::filesystem::path path; // resolved lazily
        };

        Store& S()
        {
            static Store s;
            return s;
        }

        std::filesystem::path ResolvePath()
        {
            // gwheel.dll lives at <CP2077>/red4ext/plugins/gwheel/gwheel.dll
            // — config.json sits next to the DLL.
            HMODULE mod = nullptr;
            GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                    | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&ResolvePath),
                &mod);
            wchar_t buf[MAX_PATH] = {};
            GetModuleFileNameW(mod, buf, MAX_PATH);
            std::filesystem::path p(buf);
            p.replace_filename(L"config.json");
            return p;
        }

        // --- very small JSON helpers, sufficient for our flat schema ------------

        void EscapeJsonTo(std::string& dst, std::string_view s)
        {
            dst.push_back('"');
            for (char c : s)
            {
                switch (c)
                {
                case '"':  dst += "\\\""; break;
                case '\\': dst += "\\\\"; break;
                case '\b': dst += "\\b";  break;
                case '\f': dst += "\\f";  break;
                case '\n': dst += "\\n";  break;
                case '\r': dst += "\\r";  break;
                case '\t': dst += "\\t";  break;
                default:   dst.push_back(c); break;
                }
            }
            dst.push_back('"');
        }

        std::string Emit(const Config& c)
        {
            std::ostringstream out;
            out << "{\n";
            out << "  \"version\": " << c.version << ",\n";

            out << "  \"input\": {\n";
            out << "    \"enabled\": " << (c.input.enabled ? "true" : "false") << ",\n";
            out << "    \"steerDeadzonePct\": "    << c.input.steerDeadzonePct    << ",\n";
            out << "    \"throttleDeadzonePct\": " << c.input.throttleDeadzonePct << ",\n";
            out << "    \"brakeDeadzonePct\": "    << c.input.brakeDeadzonePct    << ",\n";
            {
                std::string esc;
                EscapeJsonTo(esc, c.input.responseCurve);
                out << "    \"responseCurve\": " << esc << "\n";
            }
            out << "  },\n";

            out << "  \"ffb\": {\n";
            out << "    \"enabled\": "      << (c.ffb.enabled ? "true" : "false")      << ",\n";
            out << "    \"strengthPct\": "  << c.ffb.strengthPct                       << ",\n";
            out << "    \"debugLogging\": " << (c.ffb.debugLogging ? "true" : "false") << "\n";
            out << "  },\n";

            out << "  \"override\": {\n";
            out << "    \"enabled\": "            << (c.override_.enabled ? "true" : "false") << ",\n";
            out << "    \"sensitivity\": "        << c.override_.sensitivity                  << ",\n";
            out << "    \"rangeDeg\": "           << c.override_.rangeDeg                     << ",\n";
            out << "    \"centeringSpringPct\": " << c.override_.centeringSpringPct           << "\n";
            out << "  },\n";

            auto emitVeh = [&](const char* name, const PerVehicle& pv, bool last) {
                out << "    \"" << name << "\": { "
                    << "\"steeringMultiplier\": " << pv.steeringMultiplier << ", "
                    << "\"responseDelayMs\": " << pv.responseDelayMs << " }"
                    << (last ? "\n" : ",\n");
            };
            out << "  \"perVehicle\": {\n";
            emitVeh("car", c.car, false);
            emitVeh("motorcycle", c.motorcycle, false);
            emitVeh("truck", c.truck, false);
            emitVeh("van", c.van, true);
            out << "  },\n";

            out << "  \"buttons\": [";
            for (size_t i = 0; i < c.buttons.size(); ++i)
            {
                const auto& b = c.buttons[i];
                if (i) out << ", ";
                out << "{\"button\": " << b.button << ", \"action\": ";
                std::string esc;
                EscapeJsonTo(esc, b.action);
                out << esc << "}";
            }
            out << "]\n";

            out << "}\n";
            return out.str();
        }

        // Parse the "buttons" array. Tolerant and schema-light - we accept any
        // sequence of "{...}" records and pick out the button+action fields by
        // substring, same approach as the rest of this mini-parser.
        void ParseButtons(const std::string& text, std::vector<ButtonBinding>& out)
        {
            out.clear();
            size_t arr = text.find("\"buttons\"");
            if (arr == std::string::npos) return;
            size_t lbrack = text.find('[', arr);
            if (lbrack == std::string::npos) return;
            size_t rbrack = text.find(']', lbrack);
            if (rbrack == std::string::npos) return;

            size_t i = lbrack + 1;
            while (i < rbrack)
            {
                size_t lbrace = text.find('{', i);
                if (lbrace == std::string::npos || lbrace >= rbrack) break;
                size_t rbrace = text.find('}', lbrace);
                if (rbrace == std::string::npos || rbrace > rbrack) break;

                std::string_view record(text.data() + lbrace, rbrace - lbrace + 1);

                ButtonBinding b;
                auto keyPos = record.find("\"button\"");
                if (keyPos != std::string_view::npos)
                {
                    auto colon = record.find(':', keyPos);
                    if (colon != std::string_view::npos)
                    {
                        char* endp = nullptr;
                        b.button = static_cast<int32_t>(std::strtol(record.data() + colon + 1, &endp, 10));
                    }
                }
                auto actPos = record.find("\"action\"");
                if (actPos != std::string_view::npos)
                {
                    auto colon = record.find(':', actPos);
                    if (colon != std::string_view::npos)
                    {
                        auto q1 = record.find('"', colon);
                        if (q1 != std::string_view::npos)
                        {
                            auto q2 = record.find('"', q1 + 1);
                            if (q2 != std::string_view::npos)
                                b.action.assign(record.data() + q1 + 1, q2 - q1 - 1);
                        }
                    }
                }
                if (b.button >= 0 && !b.action.empty())
                    out.push_back(std::move(b));

                i = rbrace + 1;
            }
        }

        // Case-sensitive "find `"key"` after the last occurrence of `section`".
        // Returns the offset of the value's first char, or std::string::npos.
        size_t FindValueOffset(const std::string& text, std::string_view section, std::string_view key)
        {
            size_t sec = section.empty() ? 0 : text.find(std::string("\"") + std::string(section) + "\"");
            if (sec == std::string::npos) return std::string::npos;
            size_t k = text.find(std::string("\"") + std::string(key) + "\"", sec);
            if (k == std::string::npos) return std::string::npos;
            size_t colon = text.find(':', k);
            if (colon == std::string::npos) return std::string::npos;
            size_t v = colon + 1;
            while (v < text.size() && (text[v] == ' ' || text[v] == '\t')) ++v;
            return v;
        }

        bool ExtractBool(const std::string& text, std::string_view section, std::string_view key, bool& out)
        {
            size_t v = FindValueOffset(text, section, key);
            if (v == std::string::npos) return false;
            if (text.compare(v, 4, "true") == 0)  { out = true; return true; }
            if (text.compare(v, 5, "false") == 0) { out = false; return true; }
            return false;
        }

        bool ExtractInt(const std::string& text, std::string_view section, std::string_view key, int32_t& out)
        {
            size_t v = FindValueOffset(text, section, key);
            if (v == std::string::npos) return false;
            char* endp = nullptr;
            long val = std::strtol(text.c_str() + v, &endp, 10);
            if (endp == text.c_str() + v) return false;
            out = static_cast<int32_t>(val);
            return true;
        }

        bool ExtractFloat(const std::string& text, std::string_view section, std::string_view key, float& out)
        {
            size_t v = FindValueOffset(text, section, key);
            if (v == std::string::npos) return false;
            char* endp = nullptr;
            double val = std::strtod(text.c_str() + v, &endp);
            if (endp == text.c_str() + v) return false;
            out = static_cast<float>(val);
            return true;
        }

        bool ExtractString(const std::string& text, std::string_view section, std::string_view key, std::string& out)
        {
            size_t v = FindValueOffset(text, section, key);
            if (v == std::string::npos || v >= text.size() || text[v] != '"') return false;
            size_t end = text.find('"', v + 1);
            if (end == std::string::npos) return false;
            out.assign(text, v + 1, end - v - 1);
            return true;
        }

        void Parse(const std::string& text, Config& c)
        {
            ExtractInt   (text, {},         "version",                c.version);

            ExtractBool  (text, "input",    "enabled",                c.input.enabled);
            ExtractInt   (text, "input",    "steerDeadzonePct",       c.input.steerDeadzonePct);
            ExtractInt   (text, "input",    "throttleDeadzonePct",    c.input.throttleDeadzonePct);
            ExtractInt   (text, "input",    "brakeDeadzonePct",       c.input.brakeDeadzonePct);
            ExtractString(text, "input",    "responseCurve",          c.input.responseCurve);

            ExtractBool  (text, "ffb",      "enabled",                c.ffb.enabled);
            ExtractInt   (text, "ffb",      "strengthPct",            c.ffb.strengthPct);
            ExtractBool  (text, "ffb",      "debugLogging",           c.ffb.debugLogging);

            ExtractBool  (text, "override", "enabled",                c.override_.enabled);
            ExtractFloat (text, "override", "sensitivity",            c.override_.sensitivity);
            ExtractInt   (text, "override", "rangeDeg",               c.override_.rangeDeg);
            ExtractInt   (text, "override", "centeringSpringPct",     c.override_.centeringSpringPct);

            auto vehExtract = [&](const char* section, PerVehicle& pv) {
                ExtractFloat(text, section, "steeringMultiplier", pv.steeringMultiplier);
                ExtractInt  (text, section, "responseDelayMs",    pv.responseDelayMs);
            };
            vehExtract("car", c.car);
            vehExtract("motorcycle", c.motorcycle);
            vehExtract("truck", c.truck);
            vehExtract("van", c.van);

            ParseButtons(text, c.buttons);
        }

        void SaveLocked(const Config& c)
        {
            auto& st = S();
            std::error_code ec;
            std::filesystem::create_directories(st.path.parent_path(), ec);
            if (ec)
            {
                log::WarnF("[gwheel] could not create config directory: %s", ec.message().c_str());
            }
            std::ofstream out(st.path, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                char utf8Path[MAX_PATH * 4];
                WideCharToMultiByte(CP_UTF8, 0, st.path.c_str(), -1, utf8Path, sizeof(utf8Path), nullptr, nullptr);
                log::WarnF("[gwheel] failed to open %s for write — settings will not persist", utf8Path);
                return;
            }
            out << Emit(c);
            log::Debug("[gwheel] config saved");
        }

        void ApplyDerived(const Config& c)
        {
            // Anything that must take effect immediately when config changes.
            log::SetDebugEnabled(c.ffb.debugLogging);

            std::vector<button_map::Binding> bm;
            bm.reserve(c.buttons.size());
            for (const auto& b : c.buttons)
                bm.push_back({ b.button, b.action });
            button_map::ReplaceAll(bm);
        }

        void Publish(const Config& next)
        {
            auto& st = S();
            const int writeIdx = 1 - st.publishedIdx.load(std::memory_order_relaxed);
            st.slots[writeIdx] = next;
            st.publishedIdx.store(writeIdx, std::memory_order_release);
            ApplyDerived(next);
            SaveLocked(next);
        }

        template <typename F>
        void Mutate(F&& f)
        {
            auto& st = S();
            std::lock_guard lock(st.writerMutex);
            Config next = Current();
            f(next);
            Publish(next);
        }
    }

    Config Current()
    {
        auto& st = S();
        const int idx = st.publishedIdx.load(std::memory_order_acquire);
        return st.slots[idx];
    }

    void Load()
    {
        auto& st = S();
        std::lock_guard lock(st.writerMutex);
        st.path = ResolvePath();

        char utf8Path[MAX_PATH * 4];
        WideCharToMultiByte(CP_UTF8, 0, st.path.c_str(), -1, utf8Path, sizeof(utf8Path), nullptr, nullptr);
        log::InfoF("[gwheel] config path: %s", utf8Path);

        Config c;
        if (std::filesystem::exists(st.path))
        {
            std::ifstream in(st.path, std::ios::binary);
            std::stringstream buf;
            buf << in.rdbuf();
            try
            {
                Parse(buf.str(), c);
                log::InfoF("[gwheel] config loaded (version=%d, input.enabled=%s, ffb.enabled=%s, override.enabled=%s)",
                           c.version,
                           c.input.enabled ? "true" : "false",
                           c.ffb.enabled ? "true" : "false",
                           c.override_.enabled ? "true" : "false");
            }
            catch (...)
            {
                log::Warn("[gwheel] config parse failed — using defaults. "
                          "If config.json has been hand-edited, check for invalid JSON or out-of-range values.");
                c = {};
            }
        }
        else
        {
            log::Info("[gwheel] config.json missing — using defaults (file will be written on first change)");
        }

        Publish(c);
    }

    std::string ReadAsJson()
    {
        return Emit(Current());
    }

    void SetInputEnabled(bool v)            { Mutate([&](Config& c){ c.input.enabled = v; }); }
    void SetSteerDeadzonePct(int32_t v)     { Mutate([&](Config& c){ c.input.steerDeadzonePct    = std::clamp(v, 0, 20); }); }
    void SetThrottleDeadzonePct(int32_t v)  { Mutate([&](Config& c){ c.input.throttleDeadzonePct = std::clamp(v, 0, 20); }); }
    void SetBrakeDeadzonePct(int32_t v)     { Mutate([&](Config& c){ c.input.brakeDeadzonePct    = std::clamp(v, 0, 20); }); }

    void SetResponseCurve(std::string_view v)
    {
        std::string s(v);
        if (s != "default" && s != "subdued" && s != "sharp") return;
        Mutate([&](Config& c){ c.input.responseCurve = s; });
    }

    void SetFfbEnabled(bool v)              { Mutate([&](Config& c){ c.ffb.enabled = v; }); }
    void SetFfbStrengthPct(int32_t v)       { Mutate([&](Config& c){ c.ffb.strengthPct = std::clamp(v, 0, 100); }); }
    void SetFfbDebugLogging(bool v)         { Mutate([&](Config& c){ c.ffb.debugLogging = v; }); }

    void SetOverrideEnabled(bool v)         { Mutate([&](Config& c){ c.override_.enabled = v; }); }
    void SetOverrideSensitivity(float v)    { Mutate([&](Config& c){ c.override_.sensitivity = std::clamp(v, 0.25f, 2.0f); }); }
    void SetOverrideRangeDeg(int32_t v)     { Mutate([&](Config& c){ c.override_.rangeDeg = std::clamp(v, 200, 900); }); }
    void SetOverrideCenteringSpringPct(int32_t v) { Mutate([&](Config& c){ c.override_.centeringSpringPct = std::clamp(v, 0, 100); }); }

    void SetButtonBinding(int32_t button, std::string_view action)
    {
        if (button < 0 || button >= 32)
        {
            log::WarnF("[gwheel] SetButtonBinding: button %d out of range [0..32)", button);
            return;
        }
        std::string act(action);
        Mutate([&](Config& c) {
            for (auto& b : c.buttons)
            {
                if (b.button == button)
                {
                    if (act.empty()) { b.button = -1; } // marks for filtering below
                    else             { b.action = act; }
                    goto compact;
                }
            }
            if (!act.empty()) c.buttons.push_back({ button, act });
        compact:
            c.buttons.erase(std::remove_if(c.buttons.begin(), c.buttons.end(),
                [](const ButtonBinding& b){ return b.button < 0 || b.action.empty(); }),
                c.buttons.end());
        });
    }

    void ClearButtonBinding(int32_t button) { SetButtonBinding(button, {}); }
}
