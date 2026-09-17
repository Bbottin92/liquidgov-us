#include "placed.hpp"

namespace commons {

std::string PlacedStore::get(const std::string& instance) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = placed_.find(instance);
    return it == placed_.end() ? std::string() : it->second;
}

void PlacedStore::set(const std::string& instance, const std::string& item) {
    std::lock_guard<std::mutex> lock(mu_);
    if (item.empty()) placed_.erase(instance);
    else placed_[instance] = item;
}

} // namespace commons
