#include <platform/engine.hpp>

#include <print>
#include <memory>

int main()
{
    auto engine_info = cannele::platform::EngineCreateInfo{.initial_window_size = {900, 600}};
    auto engine = std::make_unique<cannele::platform::Engine>(&engine_info);

    engine->run();

    return 0;
}
