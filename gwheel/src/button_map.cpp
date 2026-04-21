#include "button_map.h"
#include "logging.h"

#include <array>
#include <atomic>
#include <mutex>
#include <sstream>

namespace gwheel::button_map
{
    namespace
    {
        struct Store
        {
            std::mutex                          mtx;
            std::array<std::string, kMaxButtons> bindings{};
            std::atomic<uint32_t>               pressedBits{0};
            std::atomic<int32_t>                lastPressed{-1};
            uint32_t                             prevBits = 0;
        };

        Store& S() { static Store s; return s; }

        bool InRange(int32_t b)
        {
            return b >= 0 && static_cast<size_t>(b) < kMaxButtons;
        }
    }

    void Set(int32_t button, std::string_view action)
    {
        if (!InRange(button))
        {
            log::WarnF("[gwheel:btn] Set: button %d out of range [0..%zu)",
                       button, kMaxButtons);
            return;
        }
        std::string copy(action);
        {
            std::lock_guard lk(S().mtx);
            S().bindings[button] = copy;
        }
        log::InfoF("[gwheel:btn] bind button %d -> \"%s\"",
                   button, copy.empty() ? "(cleared)" : copy.c_str());
    }

    void Clear(int32_t button)
    {
        if (!InRange(button)) return;
        {
            std::lock_guard lk(S().mtx);
            S().bindings[button].clear();
        }
        log::InfoF("[gwheel:btn] cleared binding for button %d", button);
    }

    std::string Get(int32_t button)
    {
        if (!InRange(button)) return {};
        std::lock_guard lk(S().mtx);
        return S().bindings[button];
    }

    std::vector<Binding> Snapshot()
    {
        std::vector<Binding> out;
        std::lock_guard lk(S().mtx);
        for (int32_t i = 0; i < static_cast<int32_t>(kMaxButtons); ++i)
        {
            if (!S().bindings[i].empty())
                out.push_back({ i, S().bindings[i] });
        }
        return out;
    }

    bool IsPressed(int32_t button)
    {
        if (!InRange(button)) return false;
        return (S().pressedBits.load(std::memory_order_acquire) & (1u << button)) != 0;
    }

    int32_t LastPressed()
    {
        return S().lastPressed.load(std::memory_order_acquire);
    }

    void OnWheelTick(uint32_t currentBits)
    {
        auto& st = S();
        st.pressedBits.store(currentBits, std::memory_order_release);

        const uint32_t prev      = st.prevBits;
        const uint32_t rising    = currentBits & ~prev;
        const uint32_t falling   = prev & ~currentBits;
        st.prevBits = currentBits;

        if (!rising && !falling) return;

        // Update lastPressed to the lowest-indexed button that rose this tick.
        if (rising)
        {
            for (int32_t i = 0; i < static_cast<int32_t>(kMaxButtons); ++i)
            {
                if (rising & (1u << i))
                {
                    st.lastPressed.store(i, std::memory_order_release);
                    break;
                }
            }
        }

        for (int32_t i = 0; i < static_cast<int32_t>(kMaxButtons); ++i)
        {
            const uint32_t mask = (1u << i);
            const bool roseNow  = (rising  & mask) != 0;
            const bool fellNow  = (falling & mask) != 0;
            if (!roseNow && !fellNow) continue;

            std::string action;
            {
                std::lock_guard lk(st.mtx);
                action = st.bindings[i];
            }
            if (action.empty())
            {
                if (roseNow) log::DebugF("[gwheel:btn] button %d pressed (unbound)", i);
                continue;
            }
            if (roseNow)
                log::InfoF("[gwheel:btn] button %d pressed -> action \"%s\"",
                           i, action.c_str());
            else
                log::DebugF("[gwheel:btn] button %d released -> action \"%s\"",
                            i, action.c_str());

            // Once sigs.h is populated, the action-dispatch detour consults
            // button_map::IsPressed() on the bound action name to override
            // the in-game button state. For now this is just a log trail so
            // users can confirm their wheel -> action mapping without a
            // working hook.
        }
    }

    void ReplaceAll(const std::vector<Binding>& bindings)
    {
        std::lock_guard lk(S().mtx);
        for (auto& slot : S().bindings) slot.clear();
        for (const auto& b : bindings)
        {
            if (InRange(b.button)) S().bindings[b.button] = b.action;
        }
    }

    std::string SerializeJson()
    {
        std::ostringstream out;
        out << "[";
        bool first = true;
        {
            std::lock_guard lk(S().mtx);
            for (int32_t i = 0; i < static_cast<int32_t>(kMaxButtons); ++i)
            {
                if (S().bindings[i].empty()) continue;
                if (!first) out << ", ";
                first = false;
                out << "{\"button\": " << i << ", \"action\": \"";
                for (char c : S().bindings[i])
                {
                    switch (c)
                    {
                    case '"':  out << "\\\""; break;
                    case '\\': out << "\\\\"; break;
                    default:   out << c; break;
                    }
                }
                out << "\"}";
            }
        }
        out << "]";
        return out.str();
    }
}
