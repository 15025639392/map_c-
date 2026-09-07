#include "earth_engine/scene/LayerStack.h"

#include <algorithm>
#include <utility>

namespace earth_engine {
namespace scene {

Layer::Layer(std::string name, std::string kind)
    : name_(std::move(name)), kind_(std::move(kind)) {}

Layer* LayerStack::addLayer(std::unique_ptr<Layer> layer) {
    if (!layer) {
        return nullptr;
    }
    layers_.push_back(std::move(layer));
    return layers_.back().get();
}

Layer* LayerStack::insertLayer(std::unique_ptr<Layer> layer, size_t index) {
    if (!layer) {
        return nullptr;
    }
    if (index > layers_.size()) {
        index = layers_.size();
    }
    layers_.insert(layers_.begin() + static_cast<std::ptrdiff_t>(index), std::move(layer));
    return layers_[index].get();
}

std::unique_ptr<Layer> LayerStack::removeLayer(const std::string& name) {
    for (auto it = layers_.begin(); it != layers_.end(); ++it) {
        if ((*it)->name() == name) {
            std::unique_ptr<Layer> out = std::move(*it);
            layers_.erase(it);
            return out;
        }
    }
    return nullptr;
}

Layer* LayerStack::find(const std::string& name) {
    for (auto& l : layers_) {
        if (l->name() == name) {
            return l.get();
        }
    }
    return nullptr;
}

const Layer* LayerStack::find(const std::string& name) const {
    for (const auto& l : layers_) {
        if (l->name() == name) {
            return l.get();
        }
    }
    return nullptr;
}

void LayerStack::clear() { layers_.clear(); }

std::vector<Layer*> LayerStack::activeOrder() {
    std::vector<Layer*> out;
    out.reserve(layers_.size());
    for (auto& l : layers_) {
        if (l->active()) {
            out.push_back(l.get());
        }
    }
    return out;
}

std::vector<LayerStack::SnapshotEntry> LayerStack::revisions() const {
    std::vector<SnapshotEntry> out;
    out.reserve(layers_.size());
    for (const auto& l : layers_) {
        SnapshotEntry e;
        e.name = &l->name();
        e.kind = &l->kind();
        e.revision = l->revision();
        out.push_back(e);
    }
    return out;
}

} // namespace scene
} // namespace earth_engine
