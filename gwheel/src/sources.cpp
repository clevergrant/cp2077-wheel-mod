#include "sources.h"

#include <atomic>
#include <mutex>

namespace gwheel::sources
{
    namespace
    {
        struct State
        {
            std::mutex         mtx;
            Frame              frame{};
            std::atomic<bool>  inVehicle{false};
        };

        State& S() { static State s; return s; }
    }

    void Publish(const Frame& f)
    {
        auto& st = S();
        std::lock_guard lk(st.mtx);
        st.frame = f;
    }

    Frame Current()
    {
        auto& st = S();
        std::lock_guard lk(st.mtx);
        return st.frame;
    }

    void SetInVehicle(bool v) { S().inVehicle.store(v, std::memory_order_release); }
    bool InVehicle()          { return S().inVehicle.load(std::memory_order_acquire); }
}
