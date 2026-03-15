#include "instance.hpp"

#include "support/logging.hpp"

#include <new>
#include <string_view>

namespace goggles::filter_chain::runtime {

auto Instance::allocate() -> Instance* {
    return new (std::nothrow) Instance();
}

Instance::~Instance() {
    // If this instance is the active router, unregister so stale pointers
    // are never used by macro-based logging.
    if (detail::log_route_get_active() == &m_log_router) {
        detail::log_route_set_active(nullptr);
    }
    m_magic = 0;
}

auto Instance::set_log_callback(goggles_fc_log_callback_t callback, void* user_data)
    -> goggles_fc_status_t {
    m_log_router.callback = callback;
    m_log_router.user_data = user_data;

    // NOTE: We deliberately do NOT auto-register as the global macro router here.
    // Multi-instance hosts would silently overwrite each other's routing. Instead,
    // callers who want macro-based log output to flow through this instance must
    // call activate_as_global_router() explicitly. The c_api layer does this
    // automatically for single-instance use cases (at instance_create time when
    // a callback is provided, and at set_log_callback time).

    return GOGGLES_FC_STATUS_OK;
}

void Instance::activate_as_global_router() {
    detail::log_route_set_active(&m_log_router);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Instance::log(goggles_fc_log_level_t level, const char* domain, const char* message) const {
    std::string_view domain_view =
        domain != nullptr ? std::string_view{domain} : std::string_view{};
    std::string_view message_view =
        message != nullptr ? std::string_view{message} : std::string_view{};
    detail::log_route(&m_log_router, level, domain_view, message_view);
}

auto Instance::as_handle() -> goggles_fc_instance_t* {
    // The opaque handle is just a reinterpret_cast of the Instance pointer.
    return reinterpret_cast<goggles_fc_instance_t*>(this);
}

auto Instance::from_handle(goggles_fc_instance_t* handle) -> Instance* {
    return reinterpret_cast<Instance*>(handle);
}

auto Instance::from_handle(const goggles_fc_instance_t* handle) -> const Instance* {
    return reinterpret_cast<const Instance*>(handle);
}

} // namespace goggles::filter_chain::runtime
