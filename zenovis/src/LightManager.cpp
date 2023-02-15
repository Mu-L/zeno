//
// Created by zh on 2023/2/15.
//

#include "zenovis/LightManager.h"
#include "zeno/types/UserData.h"
#include "zeno/funcs/PrimitiveTools.h"
#include "glm/gtx/transform.hpp"
#include "glm/gtx/quaternion.hpp"
#include "zeno/logger.h"

void zenovis::LightManager::addLightArea() {
    auto nid = next_id("LightArea");
    auto item = std::make_shared<LightAreaObject>();
    items[nid] = item;
    proxy_prims[nid] = std::make_shared<zeno::PrimitiveObject>();
    item->proxy_prim(proxy_prims[nid].get());
}

std::string zenovis::LightManager::next_id(const std::string& prefix) {
    std::string nid {};
    int i = 0;
    while (true) {
        nid = zeno::format("{}-{}", prefix, i);
        if (!items.count(nid)) {
            break;
        }
        i++;
    }
    return nid;
}

void zenovis::LightManager::update_proxy_prims() {
    for (const auto&[name, item]: this->items) {
        item->proxy_prim(proxy_prims[name].get());
    }
}

bool zenovis::LightManager::has(std::string nid) const {
    return proxy_prims.count(nid);
}


void zenovis::LightAreaObject::proxy_prim(zeno::PrimitiveObject* prim) {
    auto& user_data = prim->userData();
    user_data.set2("_translate", this->data.translation);
    user_data.set2("_rotate", this->data.quatRotation);
    user_data.set2("_scale", this->data.scaling);

    auto translation = glm::translate(zeno::vec_to_other<glm::vec3>(data.translation));
    auto scaling = glm::scale(zeno::vec_to_other<glm::vec3>(data.scaling));
    auto transform_matrix = translation * scaling;
    user_data.set2<int>("interactive", 1);
    prim->verts.resize(4);
    prim->verts[0] = zeno::vec3f(0.5, 0, 0.5);
    prim->verts[1] = zeno::vec3f(0.5, 0, -0.5);
    prim->verts[2] = zeno::vec3f(-0.5, 0, 0.5);
    prim->verts[3] = zeno::vec3f(-0.5, 0, -0.5);
    for (auto i = 0; i < 4; i++) {
        auto p = zeno::vec_to_other<glm::vec3>(prim->verts[i]);
        auto t = transform_matrix * glm::vec4(p, 1.0f);
        prim->verts[i] = zeno::other_to_vec<3>(t);
    }
    prim->tris.resize(2);
    prim->tris[0] = zeno::vec3i(0, 3, 1);
    prim->tris[1] = zeno::vec3i(2, 3, 0);
}