//
// Created by zh on 2023/2/15.
//

#ifndef ZENO_LIGHTMANAGER_H
#define ZENO_LIGHTMANAGER_H

#include <unordered_map>
#include <any>

#include "zeno/utils/disable_copy.h"
#include "zeno/utils/vec.h"
#include <memory>
#include "zeno/types/PrimitiveObject.h"

namespace zenovis {
struct LightAreaData {
    bool enable = true;
    bool display = true;
    zeno::vec3f color = {1};
    float intensity = 1;
    zeno::vec3f translation = {0};
    zeno::vec3f eulerXYZ = {0};
    zeno::vec4f quatRotation = {0, 0, 0, 1};
    zeno::vec3f scaling = {1};
};

struct LightCameraItem {
    LightAreaData data;
    std::unordered_map<std::string, std::any> channel_curves;
    float get_bool(std::string channel);
    float get_float(std::string channel);
    zeno::vec2f get_vec2f(std::string channel);
    zeno::vec3f get_vec3f(std::string channel);
    zeno::vec4f get_vec4f(std::string channel);
    zeno::vec4f get_quaternion(std::string channel);
    virtual void proxy_prim(zeno::PrimitiveObject* prim) = 0;
};

struct LightAreaObject : LightCameraItem {
    LightAreaData get_data();
    void proxy_prim(zeno::PrimitiveObject* prim) override;
};

struct LightManager  : zeno::disable_copy {
    std::unordered_map<std::string, std::shared_ptr<LightCameraItem>> items;
    std::unordered_map<std::string, std::shared_ptr<zeno::PrimitiveObject>> proxy_prims;
    void update_proxy_prims();
    void addLightArea();
    std::string next_id(const std::string& prefix);
    bool has(std::string nid) const;
};
}


#endif //ZENO_LIGHTMANAGER_H
