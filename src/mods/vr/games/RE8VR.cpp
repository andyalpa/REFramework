#if defined(RE7) || defined(RE8)
#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sdk/SceneManager.hpp>
#include <sdk/MurmurHash.hpp>
#include <sdk/Application.hpp>
#include <sdk/REGameObject.hpp>
#include <json.hpp>

#include "HookManager.hpp"
#include "../../../REFramework.hpp"

// Reminder: THIS MUST BE INCLUDED OR THE LOG FILE WILL BALLOON TO GIGANTIC SIZE
// AND THE GAME MAY CRASH. THIS IS REQUIRED FOR THE sol_lua_push DECLARATION.
#include "../../../mods/ScriptRunner.hpp"
#include "../../../mods/VR.hpp"

#include "RE8VR.hpp"

using json = nlohmann::json;

std::shared_ptr<RE8VR>& RE8VR::get() {
    static auto inst = std::make_shared<RE8VR>();
    return inst;
}

std::optional<std::string> RE8VR::on_initialize() {
    auto app_player_shadow_late_update = sdk::find_method_definition("app.PlayerShadow", "lateUpdate");

    if (app_player_shadow_late_update == nullptr) {
        spdlog::info("[RE8VR] Could not find app.PlayerShadow.lateUpdate");
        spdlog::info("[RE8VR] Shadows may look abnormal");
    } else {
        spdlog::info("[RE8VR] Found app.PlayerShadow.lateUpdate");

        g_hookman.add(app_player_shadow_late_update, &RE8VR::pre_shadow_late_update, &RE8VR::post_shadow_late_update);
    }

#ifdef RE7
    auto weapon_shoot_method = sdk::find_method_definition("app.WeaponGun", "shoot");
#else
    auto weapon_shoot_method = sdk::find_method_definition("app.WeaponGunCore", "shoot");
#endif
    if (weapon_shoot_method != nullptr) {
        g_hookman.add(weapon_shoot_method, &RE8VR::pre_weapon_shoot, &RE8VR::post_weapon_shoot);
        spdlog::info("[RE8VR] Hooked weapon shoot for VR recoil");
    } else {
        spdlog::info("[RE8VR] Could not find weapon shoot method");
    }

    return std::nullopt;
}

void RE8VR::on_config_load(const utility::Config& cfg) {
    for (IModValue& option : m_options) {
        option.config_load(cfg);
    }
    load_recoil_settings();
}

void RE8VR::on_config_save(utility::Config& cfg) {
    for (IModValue& option : m_options) {
        option.config_save(cfg);
    }
}

void RE8VR::on_lua_state_created(sol::state& lua) {
    lua.new_usertype<RE8VR>("RE8VR",
        "player", &RE8VR::m_player_downcast,
        "transform", &RE8VR::m_transform,
        "inventory", &RE8VR::m_inventory,
        "updater", &RE8VR::m_updater,
        "weapon", &RE8VR::m_weapon,
        "hand_touch", &RE8VR::m_hand_touch,
        "order", &RE8VR::m_order,
        "status", &RE8VR::m_status,
        "event_action_controller", &RE8VR::m_event_action_controller,
        "game_event_action_controller", &RE8VR::m_game_event_action_controller,
        "hit_controller", &RE8VR::m_hit_controller,
        "left_hand_ik", &RE8VR::m_left_hand_ik,
        "right_hand_ik", &RE8VR::m_right_hand_ik,
        "left_hand_ik_transform", &RE8VR::m_left_hand_ik_transform,
        "right_hand_ik_transform", &RE8VR::m_right_hand_ik_transform,
        "left_hand_ik_object", &RE8VR::m_left_hand_ik_object,
        "right_hand_ik_object", &RE8VR::m_right_hand_ik_object,
        "left_hand_position_offset", &RE8VR::m_left_hand_position_offset,
        "right_hand_position_offset", &RE8VR::m_right_hand_position_offset,
        "left_hand_rotation_offset", &RE8VR::m_left_hand_rotation_offset,
        "right_hand_rotation_offset", &RE8VR::m_right_hand_rotation_offset,
        "last_right_hand_position", &RE8VR::m_last_right_hand_position,
        "last_right_hand_rotation", &RE8VR::m_last_right_hand_rotation,
        "last_left_hand_position", &RE8VR::m_last_left_hand_position,
        "last_left_hand_rotation", &RE8VR::m_last_left_hand_rotation,
        "last_shoot_pos", &RE8VR::m_last_shoot_pos,
        "last_shoot_dir", &RE8VR::m_last_shoot_dir,
        "last_muzzle_pos", &RE8VR::m_last_muzzle_pos,
        "last_muzzle_forward", &RE8VR::m_last_muzzle_forward,
        "was_gripping_weapon", &RE8VR::m_was_gripping_weapon,
        "is_holding_left_grip", &RE8VR::m_is_holding_left_grip,
        "is_in_cutscene", &RE8VR::m_is_in_cutscene,
        "is_grapple_aim", &RE8VR::m_is_grapple_aim,
        "is_reloading", &RE8VR::m_is_reloading,
        "is_motion_play", &RE8VR::m_is_motion_play,
        "in_re8_end_game_event", &RE8VR::m_in_re8_end_game_event,
        "has_vehicle", &RE8VR::m_has_vehicle,
        "can_use_hands", &RE8VR::m_can_use_hands,
        "is_arm_jacked", &RE8VR::m_is_arm_jacked,
        "wants_block", &RE8VR::m_wants_block,
        "wants_heal", &RE8VR::m_wants_heal,
        "delta_time", &RE8VR::m_delta_time,
        "movement_speed_rate", &RE8VR::m_movement_speed_rate,
        "holster_assignment_nonce", &RE8VR::m_holster_assignment_nonce,
        "holster_tune_nonce", &RE8VR::m_holster_tune_nonce,
        "holster_tune_last_slot", &RE8VR::m_holster_tune_last_slot,
        "holster_last_assigned_slot", &RE8VR::m_holster_last_assigned_slot,
        "holster_last_assigned_weapon_id", &RE8VR::m_holster_last_assigned_weapon_id,
        "get_recoil_enabled", &RE8VR::get_recoil_enabled,
        "set_recoil_enabled", &RE8VR::set_recoil_enabled,
        "get_recoil_intensity", &RE8VR::get_recoil_intensity,
        "set_recoil_intensity", &RE8VR::set_recoil_intensity,
        "get_recoil_attack_duration", &RE8VR::get_recoil_attack_duration,
        "set_recoil_attack_duration", &RE8VR::set_recoil_attack_duration,
        "get_recoil_spring_stiffness", &RE8VR::get_recoil_spring_stiffness,
        "set_recoil_spring_stiffness", &RE8VR::set_recoil_spring_stiffness,
        "get_recoil_spring_damping", &RE8VR::get_recoil_spring_damping,
        "set_recoil_spring_damping", &RE8VR::set_recoil_spring_damping,
        "get_recoil_sustained_damping", &RE8VR::get_recoil_sustained_damping,
        "set_recoil_sustained_damping", &RE8VR::set_recoil_sustained_damping,
        "get_recoil_sustained_window", &RE8VR::get_recoil_sustained_window,
        "set_recoil_sustained_window", &RE8VR::set_recoil_sustained_window,
        "set_hand_joints_to_tpose", &RE8VR::set_hand_joints_to_tpose,
        "update_hand_ik", &RE8VR::update_hand_ik,
        "update_body_ik", &RE8VR::update_body_ik,
        "update_player_gestures", &RE8VR::update_player_gestures,
        "update_pointers", &RE8VR::update_pointers,
        "update_ik_pointers", &RE8VR::update_ik_pointers,
        "fix_player_camera", &RE8VR::fix_player_camera,
        "fix_player_shadow", &RE8VR::fix_player_shadow,
        "get_localplayer", &RE8VR::get_localplayer,
        "get_weapon_object", &RE8VR::get_weapon_object,
        "set_holster_assignment", &RE8VR::set_holster_assignment,
        "set_holster_slot_persist", &RE8VR::set_holster_slot_persist,
        "clear_holster_assignment", &RE8VR::clear_holster_assignment,
        "get_holster_assignment", &RE8VR::get_holster_assignment,
        "get_holster_type_assignment", &RE8VR::get_holster_type_assignment,
        "set_holster_slot_offset", &RE8VR::set_holster_slot_offset,
        "set_holster_tune_radii", &RE8VR::set_holster_tune_radii,
        "get_holster_slot_hmd_offset_m", &RE8VR::get_holster_slot_hmd_offset_m,
        "get_weapon_recoil_id", &RE8VR::get_weapon_recoil_id,
        "get_current_weapon_recoil_id", &RE8VR::get_current_weapon_recoil_id,
        "set_per_weapon_recoil_one_hand", &RE8VR::set_per_weapon_recoil_one_hand,
        "get_per_weapon_recoil_one_hand", &RE8VR::get_per_weapon_recoil_one_hand,
        "set_per_weapon_recoil_two_hands", &RE8VR::set_per_weapon_recoil_two_hands,
        "get_per_weapon_recoil_two_hands", &RE8VR::get_per_weapon_recoil_two_hands,
        "set_per_weapon_recoil_intensity", &RE8VR::set_per_weapon_recoil_intensity,
        "get_per_weapon_recoil_intensity", &RE8VR::get_per_weapon_recoil_intensity,
        "load_recoil_settings", &RE8VR::load_recoil_settings,
        "save_recoil_settings", &RE8VR::save_recoil_settings);

    lua["re8vr"] = this;
}

void RE8VR::on_lua_state_destroyed(sol::state& lua) {
    
}

namespace {

bool holster_saved_id_is_inventory_bucket(const std::string& id) {
    return !id.empty() && id.find("Inventory") != std::string::npos;
}

std::string holster_try_item_numeric_key(::REManagedObject* obj) {
    if (obj == nullptr) {
        return {};
    }

    static const char* const k_method_names[] = {
        "get_ItemId",
        "get_ItemID",
        "get_itemId",
        "get_ContextId",
    };

    for (const auto* mn : k_method_names) {
        try {
            const auto id = sdk::call_object_func_easy<int32_t>(obj, mn);
            if (id != 0) {
                return std::string("item:") + std::to_string(id);
            }
        } catch (...) {
        }
    }

    static const char* const k_field_names[] = {
        "<ItemId>k__BackingField",
        "<itemId>k__BackingField",
        "_ItemId",
        "_itemId",
        "ItemId",
    };

    for (const auto* fn : k_field_names) {
        try {
            const auto* p = sdk::get_object_field<int32_t>(obj, fn);
            if (p != nullptr && *p != 0) {
                return std::string("item:") + std::to_string(*p);
            }
        } catch (...) {
        }
    }

    return {};
}

std::string holster_weapon_instance_key(::REManagedObject* item) {
    if (item == nullptr) {
        return {};
    }

    try {
        if (auto* go = sdk::call_object_func_easy<::REGameObject*>(item, "get_GameObject"); go != nullptr) {
            auto n = utility::re_game_object::get_name(go);
            if (n.empty()) {
                n = utility::re_string::get_string(go->name);
            }
            if (!n.empty()) {
                return n;
            }
        }
    } catch (...) {
    }

    if (auto nested = holster_try_item_numeric_key(item); !nested.empty()) {
        return nested;
    }

    try {
        if (auto* inner = sdk::get_object_field<::REManagedObject*>(item, "Item"); inner != nullptr && *inner != nullptr) {
            if (auto nested = holster_try_item_numeric_key(*inner); !nested.empty()) {
                return nested;
            }
        }
    } catch (...) {
    }

    auto owner = *sdk::get_object_field<::REGameObject*>(item, "<owner>k__BackingField");
    if (owner != nullptr) {
        const auto on = utility::re_string::get_string(owner->name);
        if (!holster_saved_id_is_inventory_bucket(on)) {
            return on;
        }
    }

    char buf[48]{};
    (void)std::snprintf(buf, sizeof(buf), "reobj:%" PRIxPTR, reinterpret_cast<uintptr_t>(item));
    return buf;
}

std::string holster_weapon_go_name_only(::REManagedObject* item) {
    if (item == nullptr) {
        return {};
    }

    try {
        if (auto* go = sdk::call_object_func_easy<::REGameObject*>(item, "get_GameObject"); go != nullptr) {
            auto n = utility::re_game_object::get_name(go);
            if (n.empty()) {
                n = utility::re_string::get_string(go->name);
            }
            return n;
        }
    } catch (...) {
    }

    return {};
}

std::string holster_weapon_json_label(::REManagedObject* item, const std::string& match_key) {
    if (item == nullptr) {
        return match_key;
    }

    const auto go_name = holster_weapon_go_name_only(item);
    if (!go_name.empty()) {
        return go_name;
    }

    if (match_key.size() >= 5 && match_key.compare(0, 5, "item:") == 0) {
        return match_key;
    }

    auto owner = *sdk::get_object_field<::REGameObject*>(item, "<owner>k__BackingField");
    if (owner != nullptr) {
        const auto on = utility::re_string::get_string(owner->name);
        if (!on.empty()) {
            return on;
        }
    }

    return match_key;
}

std::string holster_match_key_from_saved_weapon_id(const std::string& saved_id) {
    if (saved_id.size() >= 6 && saved_id.compare(0, 6, "reobj:") == 0) {
        return saved_id;
    }

    if (saved_id.size() >= 5 && saved_id.compare(0, 5, "item:") == 0) {
        return saved_id;
    }

    if (holster_saved_id_is_inventory_bucket(saved_id)) {
        return {};
    }

    return saved_id;
}

bool holster_item_matches_slot_assignment(const std::string& item_key, const std::string& owner_name,
    const std::string& item_type_name, const std::string& assigned_id, const std::string& assigned_type_id) {
    if (assigned_id.empty() && assigned_type_id.empty()) {
        return false;
    }

    if (!assigned_type_id.empty() && item_type_name != assigned_type_id) {
        return false;
    }

    if (assigned_id.empty()) {
        return true;
    }

    if (holster_saved_id_is_inventory_bucket(assigned_id)) {
        return true;
    }

    if (!item_key.empty() && item_key == assigned_id) {
        return true;
    }

    return owner_name == assigned_id;
}

} // namespace

void RE8VR::on_draw_ui() {
    ImGui::SetNextItemOpen(false, ImGuiCond_::ImGuiCond_FirstUseEver);

    if (!ImGui::CollapsingHeader(get_name().data())) {
        return;
    }

    m_hide_upper_body->draw("Hide Upper Body");
    m_hide_lower_body->draw("Hide Lower Body");
    m_hide_arms->draw("Hide Arms");
    m_hide_upper_body_cutscenes->draw("Auto Hide Upper Body in Cutscenes");
    m_hide_lower_body_cutscenes->draw("Auto Hide Lower Body in Cutscenes");
    m_recoil_enabled->draw("Enable VR Recoil");
    m_recoil_intensity->draw("Recoil Intensity (1–4)");

    // Per-weapon recoil: menu integration with Lua config. Current weapon + slider; save writes recoil_settings.json.
    if (ImGui::CollapsingHeader("Per-weapon recoil", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        const std::string current_id = get_current_weapon_recoil_id();
        if (current_id.empty()) {
            ImGui::TextUnformatted("Current weapon: (none equipped)");
        } else {
            ImGui::Text("Current weapon: %s", current_id.c_str());
            float per_weapon = get_per_weapon_recoil_intensity(current_id);
            if (ImGui::SliderFloat("Weapon recoil intensity (1–4)", &per_weapon, 1.0f, 4.0f, "%.2f")) {
                set_per_weapon_recoil_intensity(current_id, per_weapon);
            }
            if (ImGui::Button("Save recoil settings")) {
                save_recoil_settings();
            }
        }
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Recoil advanced", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        m_recoil_attack_duration->draw("Attack duration (s)");
        m_recoil_spring_stiffness->draw("Spring stiffness");
        m_recoil_spring_damping->draw("Spring damping");
        m_recoil_sustained_damping->draw("Sustained fire damping");
        m_recoil_sustained_window->draw("Sustained fire window (s)");
        ImGui::Unindent();
    }
}

void RE8VR::on_pre_application_entry(void* entry, const char* name, size_t hash) {
    switch (hash) {
    case "LockScene"_fnv:
        on_pre_lock_scene(entry);
        break;
    default:
        break;
    }
}

void RE8VR::on_application_entry(void* entry, const char* name, size_t hash) {
    
}

void RE8VR::on_pre_lock_scene(void* entry) {
    auto& vr = VR::get();

    if (!vr->is_hmd_active()) {
        return;
    }

    fix_player_shadow();
}

void RE8VR::reset_data() {
    m_player = nullptr;
    m_transform = nullptr;
    m_inventory = nullptr;
    m_updater = nullptr;
    m_weapon = nullptr;
    m_hand_touch = nullptr;
    m_order = nullptr;
    m_status = nullptr;
    m_event_action_controller = nullptr;
    m_game_event_action_controller = nullptr;
    m_hit_controller = nullptr;
    m_left_hand_ik = nullptr;
    m_right_hand_ik = nullptr;
    m_left_hand_ik_transform = nullptr;
    m_right_hand_ik_transform = nullptr;
    m_left_hand_ik_object = nullptr;
    m_right_hand_ik_object = nullptr;
    m_recoil = {};
    m_recoil_attack_active = false;
    m_recoil_attack_t = 0.0f;
    m_recoil_attack_pos_y = 0.0f;
    m_recoil_attack_pos_z = 0.0f;
    m_recoil_attack_pitch = 0.0f;
    m_recoil_attack_yaw = 0.0f;
    m_recoil_last_shot_t = 0.0f;
    m_recoil_last_t = 0.0f;
    m_recoil_active = false;
}

void RE8VR::set_hand_joints_to_tpose(::REManagedObject* hand_ik) {
    //fixes an instance in!a hero where
    // chris is supposed to jump down something without tripping a laser
    if (m_player == nullptr || m_is_in_cutscene) {
        return;
    }

#ifdef RE7
        std::vector<uint32_t> hashes = {
            *sdk::get_object_field<uint32_t>(hand_ik, "HashJoint0"),
            *sdk::get_object_field<uint32_t>(hand_ik, "HashJoint1"),
            *sdk::get_object_field<uint32_t>(hand_ik, "HashJoint2"),
            *sdk::get_object_field<uint32_t>(hand_ik, "HashJoint3")
        };
#else
        std::vector<uint32_t> hashes = {
            *sdk::get_object_field<uint32_t>(hand_ik, "<HashJoint0>k__BackingField"),
            *sdk::get_object_field<uint32_t>(hand_ik, "<HashJoint1>k__BackingField"),
            *sdk::get_object_field<uint32_t>(hand_ik, "<HashJoint2>k__BackingField")
        };
#endif

    std::vector<::REJoint*> joints{};
    auto player_transform = m_player->transform;

    for (auto hash : hashes) {
        if (hash == 0) {
            continue;
        }

        auto joint = sdk::get_transform_joint_by_hash(player_transform, hash);

        if (joint != nullptr) {
            joints.push_back(sdk::get_joint_parent(joint));
        }
    }

    uint32_t additional_parents = 0;

    if (!m_is_grapple_aim) {
        additional_parents = 2;
    }

    utility::re_transform::apply_joints_tpose(*player_transform, joints, additional_parents);
}

void RE8VR::update_hand_ik() {
    if (m_in_re8_end_game_event) {
        return;
    }

    static auto motion_get_joint_index_by_name_hash = sdk::find_type_definition("via.motion.Motion")->get_method("getJointIndexByNameHash");
    static auto motion_get_world_position = sdk::find_type_definition("via.motion.Motion")->get_method("getWorldPosition");
    static auto motion_get_world_rotation = sdk::find_type_definition("via.motion.Motion")->get_method("getWorldRotation");
    static auto motion_typedef = sdk::find_type_definition("via.motion.Motion");
    static auto motion_type = motion_typedef->get_type();
    static uint32_t head_hash = 0;

    auto& vr = VR::get();
    
    if (m_player == nullptr || m_left_hand_ik == nullptr || m_right_hand_ik == nullptr) {
        m_was_gripping_weapon = false;
        m_is_holding_left_grip = false;
        return;
    }

    if (!vr->is_hmd_active() || !vr->is_using_controllers()) {
        m_was_gripping_weapon = false;
        m_is_holding_left_grip = false;
        return;
    }

    if (!m_can_use_hands || m_has_vehicle) {
        m_was_gripping_weapon = false;
        return;
    }

    const auto controllers = vr->get_controllers();

    ::REJoint* head_joint = nullptr;
    auto motion = utility::re_component::find<::REManagedObject>(m_player->transform, motion_type);

    std::optional<glm::quat> original_head_rotation{};
    auto original_right_rot = glm::identity<glm::quat>();
    auto original_left_rot_relative = glm::identity<glm::quat>();
    auto original_left_pos_relative = Vector4f{0, 0, 0, 1};
    auto original_right_rot_relative = glm::identity<glm::quat>();
    auto original_right_pos_relative = Vector4f{0, 0, 0, 1};

    if (motion != nullptr) {
        auto transform = m_player->transform;

        if (head_hash == 0) {
            auto head_joint = sdk::get_transform_joint_by_name(transform, L"Head");

            if (head_joint != nullptr) {
                head_hash = sdk::get_joint_hash(head_joint);
            }
        }

        if (head_hash != 0) {
            head_joint = sdk::get_transform_joint_by_hash(transform, head_hash);

            if (head_joint != nullptr) {
                original_head_rotation = sdk::get_joint_rotation(head_joint);
            }
        }

        uint32_t left_hash = 0;
        uint32_t right_hash = 0;

#ifdef RE7
        left_hash = *sdk::get_object_field<uint32_t>(m_left_hand_ik, "HashJoint2");
        right_hash = *sdk::get_object_field<uint32_t>(m_right_hand_ik, "HashJoint2");
#else
        left_hash = *sdk::get_object_field<uint32_t>(m_left_hand_ik, "<HashJoint2>k__BackingField");
        right_hash = *sdk::get_object_field<uint32_t>(m_right_hand_ik, "<HashJoint2>k__BackingField");
#endif

        const auto left_index = motion_get_joint_index_by_name_hash->call<uint32_t>(sdk::get_thread_context(), motion, left_hash);
        const auto right_index = motion_get_joint_index_by_name_hash->call<uint32_t>(sdk::get_thread_context(), motion, right_hash);

        Vector4f original_left_pos{};
        Vector4f original_right_pos{};
        glm::quat original_left_rot{};
        glm::quat original_right_rot{};

        motion_get_world_position->call(&original_left_pos, sdk::get_thread_context(), motion, left_index);
        motion_get_world_position->call(&original_right_pos, sdk::get_thread_context(), motion, right_index);
        motion_get_world_rotation->call(&original_left_rot, sdk::get_thread_context(), motion, left_index);
        motion_get_world_rotation->call(&original_right_rot, sdk::get_thread_context(), motion, right_index);

        const auto right_rot_inverse = glm::inverse(original_right_rot);
        original_left_pos_relative = right_rot_inverse * (original_left_pos - original_right_pos);
        original_left_rot_relative = right_rot_inverse * original_left_rot;

        const auto left_rot_inverse = glm::inverse(original_left_rot);
        original_right_pos_relative = left_rot_inverse * (original_right_pos - original_left_pos);
        original_right_rot_relative = left_rot_inverse * original_right_rot;
    }

    const auto left_controller_transform = vr->get_transform(controllers[0]);
    const auto right_controller_transform = vr->get_transform(controllers[1]);
    const auto left_controller_rotation = glm::quat{left_controller_transform};
    const auto right_controller_rotation = glm::quat{right_controller_transform};

    const auto hmd_transform = vr->get_transform(0);

    const auto left_controller_offset = left_controller_transform[3] - hmd_transform[3];
    const auto right_controller_offset = right_controller_transform[3] - hmd_transform[3];

    auto camera = sdk::get_primary_camera();
    const auto camera_rotation = glm::quat{vr->get_last_render_matrix()};

    auto original_camera_matrix = sdk::call_object_func_easy<Matrix4x4f>(camera, "get_WorldMatrix");
    auto original_camera_rotation = glm::quat{original_camera_matrix};
    auto updated_camera_pos = original_camera_matrix[3];

    vr->apply_hmd_transform(original_camera_rotation, updated_camera_pos);

    original_camera_rotation = glm::normalize(original_camera_rotation * glm::inverse(glm::quat{hmd_transform}));

    auto rh_rotation = original_camera_rotation * right_controller_rotation * m_right_hand_rotation_offset;
    auto rh_pos = updated_camera_pos
                + ((original_camera_rotation * right_controller_offset) 
                + (glm::normalize(original_camera_rotation * right_controller_rotation) * m_right_hand_position_offset));

    rh_pos.w = 1.0f;

    auto lh_grip_position = rh_pos + (glm::normalize(rh_rotation) * original_left_pos_relative);
    lh_grip_position.w = 1.0f;

    auto lh_rotation = original_camera_rotation * left_controller_rotation * m_left_hand_rotation_offset;
    auto lh_pos = updated_camera_pos
                + ((original_camera_rotation * left_controller_offset)
                + (glm::normalize(original_camera_rotation * left_controller_rotation) * m_left_hand_position_offset));

    lh_pos.w = 1.0f;

    const auto lh_delta_to_rh = (lh_pos - rh_pos);
    const auto lh_grip_delta_to_rh = (lh_grip_position - rh_pos);
    const auto lh_grip_delta = (lh_grip_position - lh_pos);
    const auto lh_grip_distance = glm::length(lh_grip_delta);

    m_was_gripping_weapon = lh_grip_distance <= 0.1f || (m_was_gripping_weapon && m_is_holding_left_grip);

    // Lets the player hold their left hand near the original (grip) position of the weapon
    if (m_was_gripping_weapon && !m_is_reloading) {
        if (glm::length(original_left_pos_relative) >= 0.1f) {
            auto original_grip_rot = utility::math::to_quat(glm::normalize(lh_grip_delta_to_rh));
            auto current_grip_rot = utility::math::to_quat(glm::normalize(lh_delta_to_rh));

            auto grip_rot_delta = glm::normalize(current_grip_rot * glm::inverse(original_grip_rot));

            // Adjust the right hand rotation;
            rh_rotation = glm::normalize(grip_rot_delta * rh_rotation);

            // Adjust the grip position
            lh_grip_position = rh_pos + (rh_rotation * original_left_pos_relative);
            lh_grip_position.w = 1.0f;
        }

        // Set the left hand position and rotation to the grip position
        lh_pos = lh_grip_position;
        lh_rotation = rh_rotation * original_left_rot_relative;
    } else {
        if (m_is_reloading) {
            lh_pos = lh_grip_position;
            lh_rotation = rh_rotation * original_left_rot_relative;
        } else {
            lh_pos = lh_pos;
            lh_rotation = lh_rotation;
        }
    }

    // Recoil: apply to weapon (right hand) first; left hand follows in weapon space for two-handed grip.
    if (*m_recoil_enabled) {
        const auto recoil_pos = get_recoil_position_offset_world(original_camera_rotation);
        const auto recoil_rot = get_recoil_rotation_offset_world(original_camera_rotation);
        rh_pos += Vector4f(recoil_pos, 0.0f);
        rh_rotation = glm::normalize(recoil_rot * rh_rotation);
    }

    if (m_was_gripping_weapon || m_is_reloading) {
        // Parent left hand to recoiled right hand: same spring-damper, 1:1 sync, no detach/jitter.
        lh_pos = rh_pos + (glm::normalize(rh_rotation) * original_left_pos_relative);
        lh_pos.w = 1.0f;
        lh_rotation = glm::normalize(rh_rotation * original_left_rot_relative);
    } else {
        // One-handed aiming: only the weapon hand recoils.
    }

    m_last_left_hand_position = lh_pos;
    m_last_left_hand_rotation = glm::normalize(lh_rotation);

    set_hand_joints_to_tpose(m_left_hand_ik);

    sdk::set_transform_position(m_left_hand_ik_transform, lh_pos);
    sdk::set_transform_rotation(m_left_hand_ik_transform, lh_rotation);
    *sdk::get_object_field<float>(m_left_hand_ik, "Transition") = 1.0f;
    sdk::call_object_func_easy<void*>(m_left_hand_ik, "calc");

    set_hand_joints_to_tpose(m_right_hand_ik);

    sdk::set_transform_position(m_right_hand_ik_transform, rh_pos);
    sdk::set_transform_rotation(m_right_hand_ik_transform, rh_rotation);
    *sdk::get_object_field<float>(m_right_hand_ik, "Transition") = 1.0f;
    sdk::call_object_func_easy<void*>(m_right_hand_ik, "calc");

    m_last_right_hand_position = rh_pos;
    m_last_right_hand_rotation = glm::normalize(rh_rotation);

    if (head_joint != nullptr && original_head_rotation) {
        sdk::set_joint_rotation(head_joint, *original_head_rotation);
    }
}

void RE8VR::update_body_ik(glm::quat* camera_rotation, Vector4f* camera_pos) {
    if (m_player == nullptr) {
        return;
    }

    if (m_in_re8_end_game_event) {
        return;
    }

    static auto via_motion_ik_leg = sdk::find_type_definition("via.motion.IkLeg");
    static auto via_motion_ik_leg_type = via_motion_ik_leg->get_type();
    static auto via_motion_motion = sdk::find_type_definition("via.motion.Motion");
    static auto via_motion_motion_type = via_motion_motion->get_type();
    static auto ik_leg_set_center_offset = via_motion_ik_leg->get_method("set_CenterOffset");
    static auto ik_leg_set_center_adjust = via_motion_ik_leg->get_method("setCenterAdjust");
    static auto ik_leg_set_center_position_ctrl = via_motion_ik_leg->get_method("set_CenterPositionCtrl");
    static auto ik_leg_set_ground_contact_up_distance = via_motion_ik_leg->get_method("set_GroundContactUpDistance");
    static auto ik_leg_set_enabled = via_motion_ik_leg->get_method("set_Enabled");

    auto& vr = VR::get();
    auto ik_leg = utility::re_component::find<::REManagedObject>(m_player->transform, via_motion_ik_leg_type);

    if (ik_leg == nullptr) {
        if (!vr->is_using_controllers() || m_is_in_cutscene || camera_rotation == nullptr || camera_pos == nullptr) {
            return;
        }

        ik_leg = sdk::call_object_func_easy<::REManagedObject*>(m_player, "createComponent(System.Type)", via_motion_ik_leg->get_runtime_type());

        if (ik_leg == nullptr) {
            spdlog::error("[RE8VR] Failed to create IK leg component");
            return;
        }
    }

    if (m_is_in_cutscene || m_has_vehicle || !vr->is_hmd_active() || !vr->is_using_controllers()) {
        //ik_leg:call("set_Enabled", false)
        const auto zero_vec = Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
        ik_leg_set_center_offset->call(sdk::get_thread_context(), ik_leg, &zero_vec);
        ik_leg_set_center_adjust->call(sdk::get_thread_context(), ik_leg, 0);
        ik_leg_set_center_position_ctrl->call(sdk::get_thread_context(), ik_leg, 2); // world offset
        ik_leg_set_ground_contact_up_distance->call(sdk::get_thread_context(), ik_leg, 0.0f); // Fixes the whole player being jarringly moved upwards.

        if (!vr->is_using_controllers()) {
            sdk::call_object_func<void*>(ik_leg, "destroy", sdk::get_thread_context(), ik_leg);
        }
        
        return;
    } else {
        ik_leg_set_enabled->call(sdk::get_thread_context(), ik_leg, true);
    }

    if (camera_rotation == nullptr || camera_pos == nullptr) {
        return;
    }

    auto motion = utility::re_component::find<::REManagedObject>(m_player->transform, via_motion_motion_type);

    if (motion == nullptr) {
        spdlog::error("[RE8VR] Failed to get motion component");
        return;
    }

    auto transform = m_player->transform;

    static uint32_t head_hash = sdk::murmur_hash::calc32("Head");
    
    const auto transform_rot = sdk::get_transform_rotation(transform);
    const auto transform_pos = sdk::get_transform_position(transform);

    auto head_joint = sdk::get_transform_joint_by_hash(transform, head_hash);

    const auto normal_dir = *camera_rotation * Vector3f{0, 0, 1};
    auto flattened_dir = *camera_rotation * Vector3f{0, 0, 1};
    flattened_dir.y = 0.0f;
    flattened_dir = glm::normalize(flattened_dir);

    const auto original_head_pos = Vector3f{utility::re_transform::calculate_tpose_pos_world(*transform, head_joint, 4)} + (flattened_dir * (glm::abs(normal_dir.y) * -0.1f)) + (flattened_dir * 0.025f);
    const auto diff_to_camera = Vector4f{(Vector3f{*camera_pos} - original_head_pos), 1.0f};

    //ik_leg:call("set_CenterJointName", "Hip")
    ik_leg_set_center_offset->call(sdk::get_thread_context(), ik_leg, &diff_to_camera);
    ik_leg_set_center_adjust->call(sdk::get_thread_context(), ik_leg, 0);
    ik_leg_set_center_position_ctrl->call(sdk::get_thread_context(), ik_leg, 2); // world offset
    ik_leg_set_ground_contact_up_distance->call(sdk::get_thread_context(), ik_leg, 0.0f); // Fixes the whole player being jarringly moved upwards.
    //ik_leg:call("set_UpdateTiming", 2) -- ConstraintsBegin
}

void RE8VR::update_player_gestures() {
    auto& vr = VR::get();

    if (m_player == nullptr || !vr->is_using_controllers()) {
        m_wants_block = false;
        m_wants_heal = false;
        m_heal_gesture.was_grip_down = false;
        m_heal_gesture.was_trigger_down = false;
        m_heal_gesture.raw_was_grip_down = false;
        m_heal_gesture.heal_grip_began_inside_slot = false;
        m_weapon_holster.was_grip_down = false;
        m_weapon_holster.holster_grip_ever_outside_slots = false;

        return;
    }

    const auto& controllers = vr->get_controllers();

    const auto hmd = vr->get_transform(0);
    const auto left_hand = vr->get_transform(controllers[0]);
    const auto right_hand = vr->get_transform(controllers[1]);

    m_hmd_delta_to_left = left_hand[3] - hmd[3];
    m_hmd_delta_to_right = right_hand[3] - hmd[3];

    m_hmd_dir_to_left = glm::normalize(m_hmd_delta_to_left);
    m_hmd_dir_to_right = glm::normalize(m_hmd_delta_to_right);

    update_block_gesture();
    update_weapon_holster_gesture();
    update_heal_gesture();
}

void RE8VR::set_holster_assignment(int slot, const std::string& weapon_id) {
    if (slot < 0 || slot >= static_cast<int>(m_weapon_holster.slot_weapon_id.size())) {
        return;
    }

    if (slot == static_cast<int>(HolsterSlot::RightChest)) {
        clear_holster_assignment(slot);
        return;
    }

    if (slot == static_cast<int>(HolsterSlot::RightShoulderHeal)) {
        return;
    }

    m_weapon_holster.slot_weapon_json_label[slot] = weapon_id;
    m_weapon_holster.slot_weapon_id[slot] = holster_match_key_from_saved_weapon_id(weapon_id);
    m_weapon_holster.slot_weapon_type_id[slot].clear();
}

void RE8VR::set_holster_slot_persist(int slot, const std::string& weapon_id, const std::string& type_id) {
    if (slot < 0 || slot >= static_cast<int>(m_weapon_holster.slot_weapon_id.size())) {
        return;
    }

    if (slot == static_cast<int>(HolsterSlot::RightChest)) {
        clear_holster_assignment(slot);
        return;
    }

    if (slot == static_cast<int>(HolsterSlot::RightShoulderHeal)) {
        return;
    }

    m_weapon_holster.slot_weapon_json_label[slot] = weapon_id;
    m_weapon_holster.slot_weapon_id[slot] = holster_match_key_from_saved_weapon_id(weapon_id);
    m_weapon_holster.slot_weapon_type_id[slot] = type_id;
}

void RE8VR::clear_holster_assignment(int slot) {
    if (slot < 0 || slot >= static_cast<int>(m_weapon_holster.slot_weapon_id.size())) {
        return;
    }

    if (slot == static_cast<int>(HolsterSlot::RightShoulderHeal)) {
        return;
    }

    m_weapon_holster.slot_weapon_id[slot].clear();
    m_weapon_holster.slot_weapon_json_label[slot].clear();
    m_weapon_holster.slot_weapon_type_id[slot].clear();
}

std::string RE8VR::get_holster_assignment(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(m_weapon_holster.slot_weapon_json_label.size())) {
        return {};
    }

    return m_weapon_holster.slot_weapon_json_label[slot];
}

std::string RE8VR::get_holster_type_assignment(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(m_weapon_holster.slot_weapon_type_id.size())) {
        return {};
    }

    return m_weapon_holster.slot_weapon_type_id[slot];
}

void RE8VR::set_holster_slot_offset(int slot, float right_m, float up_m, float forward_m) {
    if (slot < 0 || slot >= static_cast<int>(m_holster_hmd_offset_m.size())) {
        return;
    }

    m_holster_hmd_offset_m[static_cast<size_t>(slot)] = Vector3f{right_m, up_m, forward_m};
}

void RE8VR::set_holster_tune_radii(float weapon_hover_m, float heal_inner_m, float heal_blocks_weapon_m) {
    m_holster_weapon_hover_m = std::clamp(weapon_hover_m, 0.05f, 0.80f);
    m_holster_heal_inner_m = std::clamp(heal_inner_m, 0.05f, 0.50f);
    m_holster_heal_blocks_weapon_m = std::clamp(heal_blocks_weapon_m, 0.05f, 0.50f);
}

std::tuple<float, float, float> RE8VR::get_holster_slot_hmd_offset_m(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(m_holster_hmd_offset_m.size())) {
        return {0.f, 0.f, 0.f};
    }

    const auto& v = m_holster_hmd_offset_m[static_cast<size_t>(slot)];
    return {v.x, v.y, v.z};
}

Vector3f RE8VR::compute_holster_slot_base_world_position(const RE8VR::HolsterSlot holster_slot, const Vector3f& head_pos,
    const Vector3f& head_right, const Vector3f& head_up, const Vector3f& head_forward) const {
    switch (holster_slot) {
    case HolsterSlot::LeftShoulder:
        return head_pos + (head_right * -0.34f) + (head_up * -0.04f) + (head_forward * 0.10f);
    case HolsterSlot::LeftChest:
        return head_pos + (head_right * -0.18f) + (head_up * -0.20f) + (head_forward * -0.18f);
    case HolsterSlot::RightChest:
        return head_pos + (head_right * 0.22f) + (head_up * -0.36f) + (head_forward * -0.08f);
    case HolsterSlot::LeftWaist:
        return head_pos + (head_right * -0.28f) + (head_up * -0.48f) + (head_forward * -0.06f);
    case HolsterSlot::RightWaist:
        return head_pos + (head_right * 0.28f) + (head_up * -0.48f) + (head_forward * -0.06f);
    case HolsterSlot::RightShoulder:
        return head_pos + (head_right * 0.34f) + (head_up * -0.04f) + (head_forward * 0.10f);
    case HolsterSlot::RightShoulderHeal:
        return head_pos + (head_right * 0.18f) + (head_up * -0.20f) + (head_forward * -0.18f);
    default:
        return head_pos;
    }
}

Vector3f RE8VR::compute_holster_slot_world_position(const RE8VR::HolsterSlot holster_slot, const Vector3f& head_pos,
    const Vector3f& head_right, const Vector3f& head_up, const Vector3f& head_forward) const {
    const Vector3f base = compute_holster_slot_base_world_position(holster_slot, head_pos, head_right, head_up, head_forward);
    const auto& u = m_holster_hmd_offset_m[static_cast<size_t>(holster_slot)];
    return base + head_right * u.x + head_up * u.y + head_forward * u.z;
}

void RE8VR::holster_clear_weapon_from_other_slots(const std::string& instance_key, const std::string& type_id, int except_slot) {
    static constexpr int k_holster_weapon_slot_count = 6;

    for (int i = 0; i < k_holster_weapon_slot_count; ++i) {
        if (i == except_slot) {
            continue;
        }

        const auto ui = static_cast<size_t>(i);
        const auto& sid = m_weapon_holster.slot_weapon_id[ui];
        const auto& stype = m_weapon_holster.slot_weapon_type_id[ui];

        bool clear_slot = false;
        if (!instance_key.empty() && sid == instance_key) {
            clear_slot = true;
        }

        if (!clear_slot && !type_id.empty() && stype == type_id && holster_saved_id_is_inventory_bucket(sid)
            && holster_saved_id_is_inventory_bucket(instance_key)) {
            clear_slot = true;
        }

        if (clear_slot) {
            clear_holster_assignment(i);
        }
    }
}

void RE8VR::fix_player_camera(::REManagedObject* player_camera) {
    auto& vr = VR::get();

    m_in_re8_end_game_event = false;

    if (!vr->is_hmd_active()) {
        // so the camera doesn't go wacky
        if (m_camera_data.last_hmd_active_state) {
            // disables the body IK component
            update_body_ik(nullptr, nullptr);

            vr->set_gui_rotation_offset(glm::identity<glm::quat>());
            vr->recenter_view();

            m_camera_data.last_hmd_active_state = false;
        }

        // Restore the vertical camera movement after taking headset off/not using controllers
        if (m_camera_data.was_vert_limited) {
           auto base_transform_solver = sdk::get_object_field<::REManagedObject*>(player_camera, "BaseTransSolver");

            if (base_transform_solver != nullptr && *base_transform_solver != nullptr) {
                auto camera_controller = sdk::get_object_field<::REManagedObject*>(*base_transform_solver, "CurrentController");
    
                if (camera_controller == nullptr) {
                    camera_controller = sdk::get_object_field<::REManagedObject*>(*base_transform_solver, "<CurrentController>k__BackingField");
                }
    
                if (camera_controller != nullptr && *camera_controller != nullptr) {
                    *sdk::get_object_field<bool>(*camera_controller, "IsVerticalRotateLimited") = false;
                }
            }

           m_camera_data.was_vert_limited = false;
        }

        return;
    }

#ifdef RE8
    /*
    Check whether we're in the event at the end of RE8
    and return early if we are.
    */
    if (m_is_in_cutscene && m_game_event_action_controller != nullptr) {
        auto event_action = sdk::get_object_field<::REManagedObject*>(m_game_event_action_controller, "_GameEventAction");

        if (event_action != nullptr && *event_action != nullptr) {
            auto event_name = sdk::get_object_field<::SystemString*>(*event_action, "_EventName");

            if (event_name != nullptr && *event_name != nullptr) {
                if (s_re8_end_game_events.contains(utility::re_string::get_string(*event_name))) {
                    m_in_re8_end_game_event = true;
                    return;
                }
            }
        }
    }
#endif

    m_camera_data.last_hmd_active_state = true;

    auto base_transform_solver = sdk::get_object_field<::REManagedObject*>(player_camera, "BaseTransSolver");
    auto is_maximum_controllable = true;

    if (base_transform_solver != nullptr && *base_transform_solver != nullptr) {
#ifdef RE8
        auto current_type_obj = *sdk::get_object_field<::REManagedObject*>(*base_transform_solver, "<currentType>k__BackingField");
        auto current_type = *sdk::get_object_field<int>(current_type_obj, "Value");

        auto vehicle = sdk::get_object_field<::REGameObject*>(player_camera, "RideVehicleObject");
        m_has_vehicle = vehicle != nullptr && *vehicle != nullptr;

        // Fixes special cutscene near the end of the game.
        if (m_has_vehicle && m_is_arm_jacked) {
            return;
        }
#else
        auto current_type = *sdk::get_object_field<int>(*base_transform_solver, "<currentType>k__BackingField");
#endif

        if (current_type != 0 && !m_has_vehicle) { // MaximumOperatable
            m_is_in_cutscene = true;
            is_maximum_controllable = false;
            m_camera_data.last_time_not_maximum_controllable = std::chrono::steady_clock::now();
        } else {
            if (std::chrono::steady_clock::now() - m_camera_data.last_time_not_maximum_controllable <= std::chrono::seconds(1)) {
                m_is_in_cutscene = true;
            }

            if (m_has_vehicle) {
                m_is_in_cutscene = false;
            }
        }
    }

    auto wants_recenter = false;

    if (m_is_in_cutscene && !m_camera_data.last_cutscene_state) {
        // force the gui to be recentered when we exit the cutscene
        m_camera_data.last_gui_forced_slerp = std::chrono::steady_clock::now();
        m_camera_data.last_gui_quat = glm::identity<glm::quat>();
        wants_recenter = true;

        vr->recenter_gui(m_camera_data.last_gui_quat);
    } else if (!m_is_in_cutscene && m_camera_data.last_cutscene_state) {
        m_camera_data.last_gui_forced_slerp = std::chrono::steady_clock::now();
        m_camera_data.last_gui_quat = glm::inverse(glm::quat{vr->get_rotation(0)});
        wants_recenter = true;

        vr->recenter_gui(glm::quat{vr->get_rotation(0)});
    }

    auto camera = sdk::get_primary_camera();

    if (camera == nullptr) {
        return;
    }

    auto camera_gameobject = sdk::call_object_func_easy<::REGameObject*>(camera, "get_GameObject");
    if (camera_gameobject == nullptr) {
        return;
    }

    auto camera_transform = camera_gameobject->transform;
    if (camera_transform == nullptr) {
        return;
    }

    auto camera_rot = sdk::get_transform_rotation(camera_transform);
    auto camera_pos = sdk::get_transform_position(camera_transform);

    // fix camera position.
    if (is_maximum_controllable && vr->is_using_controllers()) {
        auto param_container = sdk::get_object_field<::REManagedObject*>(player_camera, "_CurrentParamContainer");

        if (param_container == nullptr) {
            param_container = sdk::get_object_field<::REManagedObject*>(player_camera, "CurrentParamContainer");
        }

        if (param_container != nullptr && *param_container != nullptr) {
            auto posture_param = sdk::get_object_field<::REManagedObject*>(*param_container, "PostureParam");

            if (posture_param != nullptr && *posture_param != nullptr) {
                auto current_camera_offset_ptr = sdk::get_object_field<glm::vec4>(*posture_param, "CameraOffset");

                if (current_camera_offset_ptr != nullptr) {
                    auto current_camera_offset = *current_camera_offset_ptr;
                    current_camera_offset.y = 0.0f;
                    camera_pos += camera_rot * current_camera_offset;
                    camera_pos.w = 1.0f;
                }
            }
        }
    }

    // Advance recoil state only; do NOT apply to camera (reticle stays 1:1 with HMD).
    update_recoil(m_delta_time);

    auto camera_rot_pre_hmd = camera_rot;
    auto camera_pos_pre_hmd = camera_pos;

    auto camera_rot_no_shake_field = sdk::get_object_field<glm::quat>(player_camera, "<CameraRotation>k__BackingField");

    if (camera_rot_no_shake_field == nullptr) {
        camera_rot_no_shake_field = sdk::get_object_field<glm::quat>(player_camera, "<cameraRotation>k__BackingField");
    }

    auto camera_rot_no_shake = *camera_rot_no_shake_field;

    Vector4f zero_v4{0.0f, 0.0f, 0.0f, 0.0f};
    vr->apply_hmd_transform(camera_rot_no_shake, zero_v4);
    vr->apply_hmd_transform(camera_rot, camera_pos);
    camera_pos.w = 1.0f;

    auto camera_joint = utility::re_transform::get_joint(*camera_transform, 0);

    if (camera_joint == nullptr) {
        return;
    }

    // Transform is used for things like Ethan's light
    // and determining where the player is looking
    sdk::set_transform_position(camera_transform, camera_pos);
    sdk::set_transform_rotation(camera_transform, camera_rot);
    
    // Joint is used for the actual final rendering of the game world
    if (m_is_in_cutscene) {
        sdk::set_joint_position(camera_joint, camera_pos_pre_hmd);
        sdk::set_joint_rotation(camera_joint, camera_rot_pre_hmd);
    } else {
        const auto rot_delta = glm::inverse(camera_rot_pre_hmd) * camera_rot;

        auto forward = rot_delta * Vector3f{0.0f, 0.0f, 1.0f};
        forward = glm::normalize(Vector3f{forward.x, 0.0, forward.z});

        sdk::set_joint_position(camera_joint, camera_pos_pre_hmd);
        sdk::set_joint_rotation(camera_joint, camera_rot_pre_hmd * utility::math::to_quat(forward));
    }

    update_body_ik(&camera_rot, &camera_pos);

    glm::quat slerp_quat{};

    if (m_is_in_cutscene) {
        slerp_quat = camera_rot_pre_hmd * glm::inverse(camera_rot);
    } else {
        slerp_quat = glm::inverse(glm::quat{vr->get_rotation(0)});
    }

    slerp_gui(slerp_quat);

    static auto neg_forward_identity = glm::quat{Matrix4x4f{-1, 0, 0, 0,
                                                            0, 1, 0, 0,
                                                            0, 0, -1, 0,
                                                            0, 0, 0, 1}};

    const auto fixed_dir = glm::normalize((neg_forward_identity * camera_rot_no_shake) * Vector3f{0.0f, 0.0f, -1.0f});
    const auto fixed_rot = utility::math::to_quat(fixed_dir);

     // RE8 pre oct14 update
    auto camera_rotation_field = sdk::get_object_field<glm::quat>(player_camera, "<CameraRotation>k__BackingField");

    if (camera_rotation_field == nullptr) {
        camera_rotation_field = sdk::get_object_field<glm::quat>(player_camera, "<cameraRotation>k__BackingField");
    }

    *camera_rotation_field = fixed_rot;

     // RE8 pre oct14 update
    auto camera_position_field = sdk::get_object_field<glm::vec4>(player_camera, "<CameraPosition>k__BackingField");

    if (camera_position_field == nullptr) {
        camera_position_field = sdk::get_object_field<glm::vec4>(player_camera, "<cameraPosition>k__BackingField");
    }

    *camera_position_field = camera_pos;

#ifdef RE8
    // RE8 pre oct14 update
    auto fixed_aim_rotation_field = sdk::get_object_field<glm::quat>(player_camera, "FixedAimRotation");

    if (fixed_aim_rotation_field == nullptr) {
        fixed_aim_rotation_field = sdk::get_object_field<glm::quat>(player_camera, "<fixedAimRotation>k__BackingField");
    }

    *fixed_aim_rotation_field = fixed_rot;
#endif

    auto camera_rotation_with_movement_shake_field = sdk::get_object_field<glm::quat>(player_camera, "CameraRotationWithMovementShake");

    if (camera_rotation_with_movement_shake_field == nullptr) {
        camera_rotation_with_movement_shake_field = sdk::get_object_field<glm::quat>(player_camera, "cameraRotationWithMovementShake");
    }

    auto camera_rotation_with_camera_shake_field = sdk::get_object_field<glm::quat>(player_camera, "CameraRotationWithCameraShake");

    if (camera_rotation_with_camera_shake_field == nullptr) {
        camera_rotation_with_camera_shake_field = sdk::get_object_field<glm::quat>(player_camera, "cameraRotationWithCameraShake");
    }

    auto prev_camera_rotation_field = sdk::get_object_field<glm::quat>(player_camera, "PrevCameraRotation");

    if (prev_camera_rotation_field == nullptr) {
        prev_camera_rotation_field = sdk::get_object_field<glm::quat>(player_camera, "PrevcameraRotation"); // ???? why
    }

    *camera_rotation_with_movement_shake_field = fixed_rot;
    *camera_rotation_with_camera_shake_field = fixed_rot;
    *prev_camera_rotation_field = fixed_rot;

    auto camera_controller_param = sdk::get_object_field<::REManagedObject*>(player_camera, "CameraCtrlParam");

    if (camera_controller_param != nullptr) {
        *sdk::get_object_field<glm::quat>(*camera_controller_param, "CameraRotation") = fixed_rot;
    }

    if (base_transform_solver != nullptr && *base_transform_solver != nullptr) {
        auto camera_controller = sdk::get_object_field<::REManagedObject*>(*base_transform_solver, "CurrentController");

        if (camera_controller == nullptr) {
            camera_controller = sdk::get_object_field<::REManagedObject*>(*base_transform_solver, "<CurrentController>k__BackingField");
        }

        if (camera_controller != nullptr && *camera_controller != nullptr) {
            auto camera_controller_rot = glm::identity<glm::quat>();

            if (m_is_in_cutscene) {
#ifdef RE7
                camera_controller_rot = *sdk::get_object_field<glm::quat>(*camera_controller, "<rotation>k__BackingField");
#else
                camera_controller_rot = *sdk::get_object_field<glm::quat>(*camera_controller, "<Rotation>k__BackingField");
#endif
            } else {
                camera_controller_rot = fixed_rot;
            }

            camera_controller_rot = utility::math::flatten(camera_controller_rot);
            
            if (!m_is_in_cutscene || is_maximum_controllable) {
                if (!m_is_in_cutscene) {
                    vr->recenter_view();
                }
#ifdef RE7
                *sdk::get_object_field<glm::quat>(*camera_controller, "<rotation>k__BackingField") = camera_controller_rot;
#else
                *sdk::get_object_field<glm::quat>(*camera_controller, "<Rotation>k__BackingField") = camera_controller_rot;
#endif
            }

            *sdk::get_object_field<glm::quat>(*base_transform_solver, "<rotation>k__BackingField") = camera_controller_rot;
            *sdk::get_object_field<bool>(*camera_controller, "IsVerticalRotateLimited") = is_maximum_controllable;

            m_camera_data.was_vert_limited = true;
        }
    }

    struct Ray {
        glm::vec4 from;
        glm::vec4 dir;
    };

    auto look_ray = sdk::get_object_field<Ray>(player_camera, "LookRay");
    auto shoot_ray = sdk::get_object_field<Ray>(player_camera, "ShootRay");

    if (look_ray != nullptr) {
        look_ray->from = glm::vec4{camera_pos.x, camera_pos.y, camera_pos.z, 1.0f};
        look_ray->dir = glm::vec4{fixed_dir.x, fixed_dir.y, fixed_dir.z, 1.0f};
    }

    if (shoot_ray != nullptr) {
        if (!m_has_vehicle && vr->is_using_controllers() && m_weapon != nullptr) {
            const auto pos = m_last_muzzle_pos + (m_last_muzzle_forward * 0.02f);

            shoot_ray->from = glm::vec4{pos.x, pos.y, pos.z, 1.0f};
            shoot_ray->dir = glm::vec4{m_last_muzzle_forward.x, m_last_muzzle_forward.y, m_last_muzzle_forward.z, 1.0f};
        } else {
            m_last_shoot_pos = camera_pos;
            m_last_shoot_dir = fixed_dir;

            shoot_ray->from = glm::vec4{camera_pos.x, camera_pos.y, camera_pos.z, 1.0f};
            shoot_ray->dir = glm::vec4{fixed_dir.x, fixed_dir.y, fixed_dir.z, 1.0f};
        }
    }

    m_camera_data.last_cutscene_state = m_is_in_cutscene;
}

void RE8VR::slerp_gui(const glm::quat& new_gui_quat) {
    if (m_movement_speed_rate > 0.0f) {
        m_camera_data.last_gui_forced_slerp = std::chrono::steady_clock::now() - std::chrono::duration_cast<std::chrono::seconds>(std::chrono::duration<float>(1.0f - m_movement_speed_rate));
    }
    

    m_camera_data.last_gui_dot = glm::dot(m_camera_data.last_gui_quat, new_gui_quat);
    const auto dot_dist = 1.0f - std::abs(m_camera_data.last_gui_dot);
    const auto dot_ang = std::acos(std::abs(m_camera_data.last_gui_dot)) * (180.0f / glm::pi<float>());
    m_camera_data.last_gui_dot = dot_ang;

    auto now = std::chrono::steady_clock::now();

    // trigger gui slerp
    if (dot_ang >= 20.0f || m_is_in_cutscene) {
        m_camera_data.last_gui_forced_slerp = now;
    }

    const auto slerp_time_diff = std::chrono::duration<float>(now - m_camera_data.last_gui_forced_slerp).count();

    if (slerp_time_diff <= GUI_MAX_SLERP_TIME) {
        if (dot_ang >= 10.0f) {
            m_camera_data.last_gui_forced_slerp = now;
        }

        m_camera_data.last_gui_quat = glm::slerp(m_camera_data.last_gui_quat, new_gui_quat, dot_dist * std::max((GUI_MAX_SLERP_TIME - slerp_time_diff) * m_delta_time, 0.0f));
    }

    if (m_is_in_cutscene) {
        VR::get()->recenter_gui(m_camera_data.last_gui_quat);
    } else {
        VR::get()->recenter_gui(m_camera_data.last_gui_quat * glm::inverse(new_gui_quat));
    }
}

void RE8VR::fix_player_shadow() {
    if (m_player == nullptr || m_player->transform == nullptr) {
        return;
    }

    auto& vr = VR::get();

    if (!vr->is_hmd_active()) {
        return;
    }

    static auto app_player_mesh_controller = sdk::find_type_definition("app.PlayerMeshController");

#ifdef RE8
    if (m_updater == nullptr) {
        return;
    }

    auto mesh_controller = sdk::call_object_func_easy<::REManagedObject*>(m_updater, "get_playerMeshController");

    if (mesh_controller == nullptr && m_order != nullptr) {
        mesh_controller = sdk::call_object_func_easy<::REManagedObject*>(m_order, "get_playerMeshController");
    }
#else
    static auto app_player_mesh_controller_type = app_player_mesh_controller->get_type();
    auto mesh_controller = utility::re_component::find<::REManagedObject>(m_player->transform, app_player_mesh_controller_type);
#endif

    if (mesh_controller == nullptr) {
        return;
    }

    static auto upper_body_mesh_field = app_player_mesh_controller->get_field("UpperBodyMesh");
    static auto lower_body_mesh_field = app_player_mesh_controller->get_field("LowerBodyMesh");
    static auto l_arm_mesh_field = app_player_mesh_controller->get_field("LArmMesh");
    static auto r_arm_mesh_field = app_player_mesh_controller->get_field("RArmMesh");

    static auto upper_body_shadow_mesh_field = app_player_mesh_controller->get_field("UpperBodyShadowMesh");
    static auto lower_body_shadow_mesh_field = app_player_mesh_controller->get_field("LowerBodyShadowMesh");
    static auto l_arm_shadow_mesh_field = app_player_mesh_controller->get_field("LArmShadowMesh");
    static auto r_arm_shadow_mesh_field = app_player_mesh_controller->get_field("RArmShadowMesh");
    static auto head_shadow_mesh_field = app_player_mesh_controller->get_field("HeadShadowMesh");

    static auto via_render_mesh = sdk::find_type_definition("via.render.Mesh");
    static auto set_draw_shadow_cast_method = via_render_mesh->get_method("set_DrawShadowCast");
    static auto set_enabled_method = via_render_mesh->get_method("set_Enabled");
    static auto set_draw_default_method = via_render_mesh->get_method("set_DrawDefault");

    auto toggle_shadow = [&](sdk::REField* field, bool state) {
        if (field == nullptr) {
            return;
        }

        auto data = (::REManagedObject**)field->get_data_raw(mesh_controller);

        if (data == nullptr) {
            return;
        }

        auto mesh = *data;

        if (mesh == nullptr) {
            return;
        }

        set_draw_shadow_cast_method->call(sdk::get_thread_context(), mesh, state);
    };

    auto toggle_enabled = [&](sdk::REField* field, bool state) {
        if (field == nullptr) {
            return;
        }

        auto data = (::REManagedObject**)field->get_data_raw(mesh_controller);

        if (data == nullptr) {
            return;
        }

        auto mesh = *data;

        if (mesh == nullptr) {
            return;
        }

        set_draw_default_method->call(sdk::get_thread_context(), mesh, state);
    };

    auto copy_joint = [&](uint32_t hash, ::RETransform* src, ::RETransform* dest) {
        if (src == nullptr || dest == nullptr) {
            return;
        }

        auto src_joint = sdk::get_transform_joint_by_hash(src, hash);
        auto dst_joint = sdk::get_transform_joint_by_hash(dest, hash);

        if (src_joint == nullptr || dst_joint == nullptr) {
            return;
        }

        sdk::set_joint_position(dst_joint, sdk::get_joint_position(src_joint));
        sdk::set_joint_rotation(dst_joint, sdk::get_joint_rotation(src_joint));
    };

    auto upper_mesh_ptr = (::REComponent**)upper_body_shadow_mesh_field->get_data_raw(mesh_controller);
    auto head_mesh_ptr = (::REComponent**)head_shadow_mesh_field->get_data_raw(mesh_controller);

    auto copy_mesh = [&](::REComponent** meshcomp) {
        if (meshcomp != nullptr) {
            auto mesh = *meshcomp;

            if (mesh != nullptr) {
                auto mesh_gameobject = mesh->ownerGameObject;

                if (mesh_gameobject != nullptr && mesh_gameobject->transform != nullptr) {
                    static auto head_hash = sdk::murmur_hash::calc32("Head");
                    static auto neck_hash = sdk::murmur_hash::calc32("Neck");
                    static auto neck_1_hash = sdk::murmur_hash::calc32("Neck_1");
                    static auto neck_0_hash = sdk::murmur_hash::calc32("Neck_0");
                    static auto chest_hash = sdk::murmur_hash::calc32("Chest");
                    
                    // Must be done in reverse order to preserve the head position.
                    copy_joint(chest_hash, m_player->transform, mesh_gameobject->transform);
                    copy_joint(neck_hash, m_player->transform, mesh_gameobject->transform);
                    copy_joint(neck_0_hash, m_player->transform, mesh_gameobject->transform);
                    copy_joint(neck_1_hash, m_player->transform, mesh_gameobject->transform);
                    copy_joint(head_hash, m_player->transform, mesh_gameobject->transform);
                }
            }
        }
    };

    copy_mesh(upper_mesh_ptr);
    copy_mesh(head_mesh_ptr);

    // Fix the head joint of the shadow mesh.
    if (upper_mesh_ptr != nullptr) {
        auto upper_mesh = *upper_mesh_ptr;

        if (upper_mesh != nullptr) {
            auto mesh_gameobject = upper_mesh->ownerGameObject;

            if (mesh_gameobject != nullptr && mesh_gameobject->transform != nullptr) {
                static auto head_hash = sdk::murmur_hash::calc32("Head");
                static auto neck_hash = sdk::murmur_hash::calc32("Neck");
                static auto neck_1_hash = sdk::murmur_hash::calc32("Neck_1");
                static auto neck_0_hash = sdk::murmur_hash::calc32("Neck_0");
                static auto chest_hash = sdk::murmur_hash::calc32("Chest");
                
                // Must be done in reverse order to preserve the head position.
                copy_joint(chest_hash, m_player->transform, mesh_gameobject->transform);
                copy_joint(neck_hash, m_player->transform, mesh_gameobject->transform);
                copy_joint(neck_0_hash, m_player->transform, mesh_gameobject->transform);
                copy_joint(neck_1_hash, m_player->transform, mesh_gameobject->transform);
                copy_joint(head_hash, m_player->transform, mesh_gameobject->transform);
            }
        }
    }

    const auto in_cutscene = m_is_in_cutscene || !m_can_use_hands || m_is_grapple_aim || m_has_vehicle;
    const auto using_controllers = vr->is_using_controllers();

    const auto wants_hide_upper_body = !using_controllers || m_hide_upper_body->value() || (in_cutscene && m_hide_upper_body_cutscenes->value());
    const auto wants_hide_lower_body = !using_controllers || m_hide_lower_body->value() || (in_cutscene && m_hide_lower_body_cutscenes->value());
    const auto wants_hide_arms = m_hide_arms->value();

    // These are the meshes for the real player body.
    toggle_shadow(upper_body_mesh_field, true);
    toggle_shadow(lower_body_mesh_field, true);
    toggle_shadow(l_arm_mesh_field, true);
    toggle_shadow(r_arm_mesh_field, true);

    // This is the fake player shadow meshes.
    toggle_shadow(upper_body_shadow_mesh_field, false);
    toggle_shadow(lower_body_shadow_mesh_field, false);
    toggle_shadow(l_arm_shadow_mesh_field, false);
    toggle_shadow(r_arm_shadow_mesh_field, false);

    // Disable drawing of the player body if the user wants it.
    toggle_enabled(upper_body_mesh_field, !wants_hide_upper_body);
    toggle_enabled(lower_body_mesh_field, !wants_hide_lower_body);
    toggle_enabled(l_arm_mesh_field, !wants_hide_arms);
    toggle_enabled(r_arm_mesh_field, !wants_hide_arms);
}

::REManagedObject* RE8VR::get_localplayer() const {
#ifdef RE7
    auto object_man = sdk::get_managed_singleton<::REManagedObject>("app.ObjectManager");

    if (object_man == nullptr) {
        return nullptr;
    }

    static auto field = sdk::find_type_definition("app.ObjectManager")->get_field("PlayerObj");

    return field->get_data<::REManagedObject*>(object_man);
#else
    auto propsman = sdk::get_managed_singleton<::REManagedObject>("app.PropsManager");

    if (propsman == nullptr) {
        return nullptr;
    }

    static auto field = sdk::find_type_definition("app.PropsManager")->get_field("<Player>k__BackingField");

    return field->get_data<::REManagedObject*>(propsman);
#endif
}

::REManagedObject* RE8VR::get_weapon_object(::REGameObject* player) const {
#ifdef RE7
    static auto equip_manager_type = sdk::find_type_definition("app.EquipManager")->get_type();
    auto equip_manager = utility::re_component::find<::REManagedObject>(player->transform, equip_manager_type);

    if (equip_manager == nullptr) {
        return nullptr;
    }

    static auto get_equip_weapon_right_method = sdk::find_method_definition("app.EquipManager", "get_equipWeaponRight");

    return get_equip_weapon_right_method->call<::REManagedObject*>(sdk::get_thread_context(), equip_manager);
#else
    if (m_updater == nullptr) {
        return nullptr;
    }

    auto find_fps_method = [](std::string_view tname, std::string_view method_name) -> sdk::REMethodDefinition* {
        auto t = sdk::find_type_definition(tname);

        if (t == nullptr) {
            t = sdk::find_type_definition(std::string{ tname } + "FPS");
        }

        if (t == nullptr) {
            return nullptr;
        }

        return t->get_method(method_name);
    };

    if (m_player_type == PlayerType::ETHAN) {
        static auto get_player_gun_method = find_fps_method("app.PlayerUpdater", "get_playerGun");

        auto player_gun = get_player_gun_method->call<::REGameObject*>(sdk::get_thread_context(), m_updater);

        if (player_gun == nullptr) {
            return nullptr;
        }

        static auto get_equip_weapon_object_method = sdk::find_method_definition("app.PlayerGun", "get_equipWeaponObject");

        return get_equip_weapon_object_method->call<::REManagedObject*>(sdk::get_thread_context(), player_gun);
    } else if (m_player_type == PlayerType::CHRIS_MERC) {
        static auto get_player_gun_method = find_fps_method("app.PlayerUpdaterPl2001", "get_playerGun");

        auto player_gun = get_player_gun_method->call<::REGameObject*>(sdk::get_thread_context(), m_updater);

        if (player_gun == nullptr) {
            return nullptr;
        }

        static auto get_equip_weapon_object_method = sdk::find_method_definition("app.PlayerGunPl2001", "get_equipWeaponObject");

        return get_equip_weapon_object_method->call<::REManagedObject*>(sdk::get_thread_context(), player_gun);
    }

    return nullptr;
#endif
}

bool RE8VR::update_pointers() {
    m_player_downcast = get_localplayer();

    if (m_player == nullptr) {
        reset_data();
        return false;
    }
    
    m_transform = m_player->transform;

    if (m_transform == nullptr) {
        reset_data();
        return false;
    }

    m_delta_time = sdk::Application::get()->get_delta_time();

    auto get_ambiguous_re_type = [](std::string_view name) -> ::REType* {
        auto tdef = sdk::find_type_definition(name);

        if (tdef == nullptr) {
            tdef = sdk::find_type_definition(std::string{ name } + "FPS");
        }

        if (tdef == nullptr) {
            return nullptr;
        }

        return tdef->get_type();
    };

    auto assign_component = [this](::REManagedObject*& a, ::REType* t) {
        if (t == nullptr) {
            a = nullptr;
            return;
        }

        a = utility::re_component::find<::REManagedObject>(m_player->transform, t);
    };

    static auto hand_touch_type = get_ambiguous_re_type("app.PlayerHandTouch");
    assign_component(m_hand_touch, hand_touch_type);

    static auto updater_type = get_ambiguous_re_type("app.PlayerUpdater");
    assign_component(m_updater, updater_type);

#ifdef RE8
    if (m_updater == nullptr) {
        static auto updater_type_chris = get_ambiguous_re_type("app.PlayerUpdaterPl2001");
        assign_component(m_updater, updater_type_chris);

        if (m_updater != nullptr) {
            m_player_type = PlayerType::CHRIS_MERC;
        }
    } else {
        m_player_type = PlayerType::ETHAN;
    }
#else
    m_player_type = PlayerType::ETHAN;
#endif

    static auto order_type = get_ambiguous_re_type("app.PlayerOrder");
    assign_component(m_order, order_type);

#ifdef RE8
    if (m_order == nullptr) {
        static auto order_type_chris = get_ambiguous_re_type("app.PlayerOrderPl2001");
        assign_component(m_order, order_type_chris);
    }
#endif

    static auto event_action_controller_type = get_ambiguous_re_type("app.EventActionController");
    assign_component(m_event_action_controller, event_action_controller_type);

    static auto game_event_action_controller_type = get_ambiguous_re_type("app.GameEventActionController");
    assign_component(m_game_event_action_controller, game_event_action_controller_type);

#ifdef RE7
    static auto hit_controller_type = get_ambiguous_re_type("app.Collision.HitController");
    assign_component(m_hit_controller, hit_controller_type);
#else
    // TODO
#endif

#ifdef RE8
    if (m_game_event_action_controller != nullptr) {
        m_is_motion_play = *sdk::get_object_field<bool>(m_game_event_action_controller, "_isMotionPlay");
    }
#else 
    m_is_motion_play = false;
#endif

    if (m_order != nullptr) {
        m_is_grapple_aim = *sdk::get_object_field<bool>(m_order, "IsGrappleAimEnable");
    }

    m_weapon = get_weapon_object(m_player);

    static auto inventory_type = get_ambiguous_re_type("app.Inventory");

#ifdef RE7
    assign_component(m_inventory, inventory_type);

    static auto status_type = get_ambiguous_re_type("app.PlayerStatus");
    assign_component(m_status, status_type);
#else
    if (m_updater != nullptr) {
        m_status = sdk::call_object_func_easy<::REManagedObject*>(m_updater, "get_playerstatus");

        auto container = sdk::get_object_field<::REManagedObject*>(m_updater, "playerContainer");

        if (container == nullptr ) {
            container = sdk::get_object_field<::REManagedObject*>(m_updater, "<playerContainer>k__BackingField");
        }
        
        if (container != nullptr && *container != nullptr) {
            m_inventory = sdk::call_object_func_easy<::REManagedObject*>(*container, "get_inventory");
        } else {
            m_inventory = nullptr;
        }
    } else {
        m_status = nullptr;
        m_inventory = nullptr;
    }
#endif

    if (m_status != nullptr) {
        m_is_reloading = sdk::call_object_func_easy<bool>(m_status, "get_isReload");
    }

    update_ik_pointers();
    return true;
}

bool RE8VR::update_ik_pointers() {
    auto reset_hand_ik = [&]() {
        m_right_hand_ik = nullptr;
        m_left_hand_ik = nullptr;
        m_right_hand_ik_object = nullptr;
        m_left_hand_ik_object = nullptr;
        m_right_hand_ik_transform = nullptr;
        m_left_hand_ik_transform = nullptr;
    };

    if (m_hand_touch == nullptr) {
        reset_hand_ik();
        return false;
    }

    auto hand_ik = *sdk::get_object_field<sdk::SystemArray*>(m_hand_touch, "HandIK");

    if (hand_ik == nullptr || hand_ik->size() < 2) {
        spdlog::info("[RE8VR] HandIK is null or empty");
        reset_hand_ik();
        return false;
    }

    m_right_hand_ik = hand_ik->get_element(0);
    m_left_hand_ik = hand_ik->get_element(1);

    if (m_right_hand_ik != nullptr && m_left_hand_ik != nullptr) {
        m_right_hand_ik_object = *sdk::get_object_field<::REGameObject*>(m_right_hand_ik, "TargetGameObject");
        m_left_hand_ik_object = *sdk::get_object_field<::REGameObject*>(m_left_hand_ik, "TargetGameObject");
        m_right_hand_ik_transform = *sdk::get_object_field<::RETransform*>(m_right_hand_ik, "Target");
        m_left_hand_ik_transform = *sdk::get_object_field<::RETransform*>(m_left_hand_ik, "Target");
    } else {
        return false;
    }

    return true;
}

void RE8VR::update_block_gesture() {
    auto& vr = VR::get();

    const auto& controllers = vr->get_controllers();
    const auto left_hand = vr->get_transform(controllers[0]);
    const auto right_hand = vr->get_transform(controllers[1]);
    const auto hmd_forward = vr->get_transform(0)[2];

    const auto left_hand_dot_raw = glm::dot(Vector3f{hmd_forward}, m_hmd_dir_to_left);
    const auto right_hand_dot_raw = glm::dot(Vector3f{hmd_forward}, m_hmd_dir_to_right);
    const auto left_hand_dot = glm::abs(left_hand_dot_raw);
    const auto right_hand_dot = glm::abs(right_hand_dot_raw);

    const auto left_hand_in_front = left_hand_dot >= 0.8f;
    const auto right_hand_in_front = right_hand_dot >= 0.8f;

    const auto first_test = left_hand_in_front && right_hand_in_front;

    if (!first_test) {
        m_wants_block = false;
        return;
    }

    // now we need to check if the hands are facing up
    const auto left_hand_up_dot = glm::abs(glm::dot(hmd_forward, left_hand[0]));
    const auto right_hand_up_dot = glm::abs(glm::dot(hmd_forward, right_hand[0]));

    const auto left_hand_up = left_hand_up_dot >= 0.5f;
    const auto right_hand_up = right_hand_up_dot >= 0.5f;

    m_wants_block = left_hand_up && right_hand_up;
}

void RE8VR::update_weapon_holster_gesture() {
 #ifdef RE8
    if (m_player == nullptr || m_inventory == nullptr || m_updater == nullptr) {
 #else
    // RE7: m_updater can be null; weapon_change/equip_manager are resolved via components below.
    if (m_player == nullptr || m_inventory == nullptr) {
 #endif
        m_weapon_holster.was_grip_down = false;
        m_weapon_holster.holster_grip_ever_outside_slots = false;
        m_weapon_holster.was_left_grip_for_holster_tune = false;
        m_weapon_holster.holster_tune_drag_slot = -1;
        return;
    }

    if (m_is_in_cutscene || !m_can_use_hands || m_is_grapple_aim || m_has_vehicle || m_is_reloading) {
        m_weapon_holster.was_grip_down = false;
        m_weapon_holster.holster_grip_ever_outside_slots = false;
        m_weapon_holster.was_left_grip_for_holster_tune = false;
        m_weapon_holster.holster_tune_drag_slot = -1;
        return;
    }

    auto& vr = VR::get();
    if (!vr->is_using_controllers()) {
        m_weapon_holster.was_grip_down = false;
        m_weapon_holster.holster_grip_ever_outside_slots = false;
        m_weapon_holster.was_left_grip_for_holster_tune = false;
        m_weapon_holster.holster_tune_drag_slot = -1;
        return;
    }

    const auto right_joystick = vr->get_right_joystick();
    const auto left_joystick = vr->get_left_joystick();
    const auto action_grip = vr->get_action_grip();
    const auto is_grip_down = vr->is_action_active(action_grip, right_joystick);
    const bool is_left_grip_down = vr->is_action_active(action_grip, left_joystick);

    const auto now = std::chrono::steady_clock::now();
    const auto hmd = vr->get_transform(0);
    const auto right_hand = vr->get_transform(vr->get_controllers()[1]);

    const Vector3f head_pos{hmd[3]};
    const Vector3f head_right{hmd[0]};
    const Vector3f head_up{hmd[1]};
    const Vector3f head_forward{hmd[2]};
    const Vector3f rh_pos{right_hand[3]};
    const Vector3f heal_slot_pos =
        compute_holster_slot_world_position(HolsterSlot::RightShoulderHeal, head_pos, head_right, head_up, head_forward);
    const float hover_r = m_holster_weapon_hover_m;
    const float hand_h = glm::dot(rh_pos - head_pos, head_up);

    static auto app_weapon_core = sdk::find_type_definition("app.WeaponCore");
#ifdef RE7
    static auto app_player_weapon_change_tdef = sdk::find_type_definition("app.PlayerWeaponChange");
    static auto app_player_weapon_change_type = app_player_weapon_change_tdef != nullptr ? app_player_weapon_change_tdef->get_type() : nullptr;
    static auto app_equip_manager_tdef = sdk::find_type_definition("app.EquipManager");
    static auto app_equip_manager_type = app_equip_manager_tdef != nullptr ? app_equip_manager_tdef->get_type() : nullptr;
    static auto app_weapon_tdef = sdk::find_type_definition("app.Weapon");
#endif

    auto slot_world_pos = [&](HolsterSlot holster_slot) -> Vector3f {
        return compute_holster_slot_world_position(holster_slot, head_pos, head_right, head_up, head_forward);
    };

    auto holster_effective_dist = [&](HolsterSlot s, float d) -> float {
        float adj = 0.f;
        switch (s) {
        case HolsterSlot::LeftShoulder:
        case HolsterSlot::RightShoulder:
            if (hand_h < -0.20f) {
                adj += 0.20f;
            }
            break;
        case HolsterSlot::LeftChest:
            if (hand_h > -0.10f) {
                adj += 0.16f;
            }
            if (hand_h < -0.45f) {
                adj += 0.14f;
            }
            break;
        case HolsterSlot::LeftWaist:
        case HolsterSlot::RightWaist:
            if (hand_h > -0.30f) {
                adj += 0.18f;
            }
            break;
        default:
            break;
        }
        return d + adj;
    };

    auto find_hovered_slot = [&]() -> std::optional<HolsterSlot> {
        // RightChest (index 2) is unused: same body region as the medicine/heal volume (RightShoulderHeal).
        static constexpr HolsterSlot weapon_slots[] = {
            HolsterSlot::LeftShoulder,
            HolsterSlot::LeftChest,
            HolsterSlot::RightShoulder,
            HolsterSlot::LeftWaist,
            HolsterSlot::RightWaist
        };

        float best_score = 999.0f;
        std::optional<HolsterSlot> best{};

        for (auto s : weapon_slots) {
            const auto pos = slot_world_pos(s);
            const auto d = glm::length(rh_pos - pos);
            if (d > hover_r) {
                continue;
            }
            const auto score = holster_effective_dist(s, d);
            if (score < best_score) {
                best_score = score;
                best = s;
            }
        }

        if (best) {
            return best;
        }

        return std::nullopt;
    };

    auto hovered = find_hovered_slot();

    if (glm::length(rh_pos - heal_slot_pos) <= m_holster_heal_blocks_weapon_m) {
        hovered = std::nullopt;
    }

    if (hovered) {
        const auto new_hover = !m_weapon_holster.has_hovered_slot || m_weapon_holster.hovered_slot != *hovered;
        const auto time_ok = (now - m_weapon_holster.last_haptic_time) > std::chrono::milliseconds(150);

        if (new_hover && time_ok) {
            vr->trigger_haptic_vibration(0.0f, 0.06f, 1.0f, 2.5f, right_joystick);
            m_weapon_holster.last_haptic_time = now;
        }

        m_weapon_holster.hovered_slot = *hovered;
        m_weapon_holster.has_hovered_slot = true;
    } else {
        m_weapon_holster.has_hovered_slot = false;
    }

    auto holster_weapon_slot_has_assignment = [&](HolsterSlot s) -> bool {
        const auto si = static_cast<size_t>(s);
        return !m_weapon_holster.slot_weapon_id[si].empty() || !m_weapon_holster.slot_weapon_type_id[si].empty();
    };

    const bool left_grip_released =
        m_weapon_holster.was_left_grip_for_holster_tune && !is_left_grip_down;

    if (is_left_grip_down) {
        if (m_weapon_holster.holster_tune_drag_slot < 0) {
            if (hovered && holster_weapon_slot_has_assignment(*hovered)) {
                m_weapon_holster.holster_tune_drag_slot = static_cast<int>(*hovered);
            }
        }
        if (m_weapon_holster.holster_tune_drag_slot >= 0) {
            const auto drag_slot = static_cast<HolsterSlot>(m_weapon_holster.holster_tune_drag_slot);
            const Vector3f base =
                compute_holster_slot_base_world_position(drag_slot, head_pos, head_right, head_up, head_forward);
            const Vector3f delta = rh_pos - base;
            m_holster_hmd_offset_m[static_cast<size_t>(m_weapon_holster.holster_tune_drag_slot)] = Vector3f{
                glm::dot(delta, head_right), glm::dot(delta, head_up), glm::dot(delta, head_forward)};

            const auto pulse_ok =
                (now - m_weapon_holster.last_holster_tune_mode_haptic_time) > std::chrono::milliseconds(300);
            if (pulse_ok) {
                vr->trigger_haptic_vibration(0.0f, 0.035f, 1.0f, 0.35f, right_joystick);
                m_weapon_holster.last_holster_tune_mode_haptic_time = now;
            }
        }
    } else {
        if (left_grip_released && m_weapon_holster.holster_tune_drag_slot >= 0) {
            m_holster_tune_last_slot = m_weapon_holster.holster_tune_drag_slot;
            m_holster_tune_nonce++;
            vr->trigger_haptic_vibration(0.0f, 0.14f, 1.0f, 5.0f, right_joystick);
        }
        m_weapon_holster.holster_tune_drag_slot = -1;
    }

    ::REManagedObject* weapon_change = nullptr;
    ::REManagedObject* equip_controller = nullptr;
#ifdef RE8
    weapon_change = sdk::call_object_func_easy<::REManagedObject*>(m_updater, "get_playerWeaponChange");
    equip_controller = sdk::call_object_func_easy<::REManagedObject*>(m_updater, "get_equipController");
    if (equip_controller == nullptr && m_order != nullptr) {
        equip_controller = sdk::call_object_func_easy<::REManagedObject*>(m_order, "get_equipController");
    }
#else
    if (app_player_weapon_change_type != nullptr) {
        weapon_change = utility::re_component::find<::REManagedObject>(m_player->transform, app_player_weapon_change_type);
    }
    if (app_equip_manager_type != nullptr) {
        equip_controller = utility::re_component::find<::REManagedObject>(m_player->transform, app_equip_manager_type);
    }
#endif

    const auto current_weapon = get_weapon_object(m_player);
    std::string current_weapon_id{};
    std::string current_weapon_type_id{};
    if (current_weapon != nullptr) {
        current_weapon_id = holster_weapon_instance_key(current_weapon);

        if (const auto w_tdef = utility::re_managed_object::get_type_definition(current_weapon); w_tdef != nullptr) {
            current_weapon_type_id = w_tdef->get_full_name();
        }
    }

    const bool has_weapon_in_hand = current_weapon != nullptr
        && (!current_weapon_id.empty() || !current_weapon_type_id.empty());

#ifdef RE7
    // RE7: during stow lockout, force de-equip if the game re-equips due to native quickslot/dpad.
    if (weapon_change != nullptr && now < m_re7_stow_lockout_until) {
        if (current_weapon != nullptr) {
            try {
                sdk::call_object_func_easy<void*>(weapon_change, "removeWeapon");
            } catch (...) {
            }

            try {
                static auto app_player_mesh_controller = sdk::find_type_definition("app.PlayerMeshController");
                static auto app_player_mesh_controller_type = app_player_mesh_controller != nullptr ? app_player_mesh_controller->get_type() : nullptr;
                if (app_player_mesh_controller_type != nullptr) {
                    if (auto mesh_controller = utility::re_component::find<::REManagedObject>(m_player->transform, app_player_mesh_controller_type);
                        mesh_controller != nullptr) {
                        try {
                            sdk::call_object_func_easy<void*>(mesh_controller, "onEquipWeaponChanged", nullptr, current_weapon);
                        } catch (...) {
                        }

                        try {
                            auto weapon_mesh_object = sdk::call_object_func_easy<::REGameObject*>(mesh_controller, "get_weaponMeshObject");
                            if (weapon_mesh_object != nullptr) {
                                sdk::call_object_func_easy<void*>(weapon_mesh_object, "setActive", false);
                            }
                        } catch (...) {
                        }
                    }
                }
            } catch (...) {
            }

            if (equip_controller != nullptr) {
                try {
                    sdk::call_object_func_easy<void*>(equip_controller, "set_equipWeaponRight", nullptr);
                } catch (...) {
                }
            }

            try {
                sdk::call_object_func_easy<void*>(weapon_change, "setWeaponDisp", false);
            } catch (...) {
            }

            try {
                sdk::call_object_func_easy<void*>(current_weapon, "disp", false);
            } catch (...) {
            }
        }
    }
#endif

    const bool grip_pressed = is_grip_down && !m_weapon_holster.was_grip_down;
    const bool grip_released = !is_grip_down && m_weapon_holster.was_grip_down;

    if (is_grip_down) {
        if (grip_pressed) {
            m_weapon_holster.holster_grip_ever_outside_slots = !hovered.has_value();
        } else {
            m_weapon_holster.holster_grip_ever_outside_slots =
                m_weapon_holster.holster_grip_ever_outside_slots || !hovered.has_value();
        }
    }

    if (grip_released && hovered && has_weapon_in_hand && m_weapon_holster.holster_grip_ever_outside_slots
        && weapon_change != nullptr) {
        const auto slot_index = static_cast<int>(*hovered);

        holster_clear_weapon_from_other_slots(current_weapon_id, current_weapon_type_id, slot_index);

        const auto json_label = holster_weapon_json_label(current_weapon, current_weapon_id);
        m_weapon_holster.slot_weapon_id[static_cast<size_t>(slot_index)] = current_weapon_id;
        m_weapon_holster.slot_weapon_json_label[static_cast<size_t>(slot_index)] = json_label;
        m_weapon_holster.slot_weapon_type_id[static_cast<size_t>(slot_index)] = current_weapon_type_id;
        m_holster_last_assigned_slot = slot_index;
        m_holster_last_assigned_weapon_id = json_label;
        m_holster_assignment_nonce++;

 #ifdef RE8
        sdk::call_object_func_easy<void*>(weapon_change, "removeWeaponWithNoAction");
 #else
        // RE7: removeWeapon() alone can leave EquipManager/MeshController state stale.
        // Mirror the de-equip logic used by RE7 heal gesture.
        sdk::call_object_func_easy<void*>(weapon_change, "removeWeapon");
        try {
            static auto app_player_mesh_controller = sdk::find_type_definition("app.PlayerMeshController");
            static auto app_player_mesh_controller_type = app_player_mesh_controller != nullptr ? app_player_mesh_controller->get_type() : nullptr;

            if (app_player_mesh_controller_type != nullptr) {
                if (auto mesh_controller = utility::re_component::find<::REManagedObject>(m_player->transform, app_player_mesh_controller_type);
                    mesh_controller != nullptr) {
                    sdk::call_object_func_easy<void*>(mesh_controller, "onEquipWeaponChanged", nullptr, current_weapon);

                    // Some RE7 states keep the mesh visible unless explicitly hidden.
                    try {
                        auto weapon_mesh_object = sdk::call_object_func_easy<::REGameObject*>(mesh_controller, "get_weaponMeshObject");
                        if (weapon_mesh_object != nullptr) {
                            sdk::call_object_func_easy<void*>(weapon_mesh_object, "setActive", false);
                        }
                    } catch (...) {
                    }
                }
            }

            if (equip_controller != nullptr) {
                auto current_equipped_right = *sdk::get_object_field<::REManagedObject*>(equip_controller, "<equipWeaponRight>k__BackingField");
                if (current_equipped_right == current_weapon) {
                    sdk::call_object_func_easy<void*>(equip_controller, "set_equipWeaponRight", nullptr);
                }

                // Optional: clear item id string if present (harmless if not used).
                try {
                    sdk::call_object_func_easy<void*>(equip_controller, "set_equipWeaponItemIDRight", sdk::VM::create_managed_string(L""));
                } catch (...) {
                }
            }

            // Try to hide weapon display state at the weapon-change layer (present on inherited API in RE7).
            try {
                sdk::call_object_func_easy<void*>(weapon_change, "setWeaponDisp", false);
            } catch (...) {
            }

            // Last resort: hide the weapon itself (should be safe after removeWeapon).
            try {
                if (current_weapon != nullptr) {
                    sdk::call_object_func_easy<void*>(current_weapon, "disp", false);
                }
            } catch (...) {
            }

            // Lockout: if native quickslot re-equips immediately, force it back off for a brief window.
            m_re7_stow_lockout_until = now + std::chrono::milliseconds(250);
        } catch (...) {
        }
 #endif
        vr->trigger_haptic_vibration(0.0f, 0.08f, 1.0f, 5.0f, right_joystick);
    }

    if (grip_pressed && hovered && !has_weapon_in_hand && weapon_change != nullptr && equip_controller != nullptr) {
        const auto slot_index = static_cast<int>(*hovered);
        const auto& assigned_id = m_weapon_holster.slot_weapon_id[static_cast<size_t>(slot_index)];
        const auto& assigned_type_id = m_weapon_holster.slot_weapon_type_id[static_cast<size_t>(slot_index)];

        if (!assigned_id.empty() || !assigned_type_id.empty()) {
#ifdef RE8
            auto items_list = *sdk::get_object_field<::REManagedObject*>(m_inventory, "<items>k__BackingField");
#else
            auto items_list = *sdk::get_object_field<::REManagedObject*>(m_inventory, "_ItemList");
#endif
            if (items_list != nullptr) {
                auto items = *sdk::get_object_field<sdk::SystemArray*>(items_list, "mItems");
                if (items != nullptr) {
                    ::REManagedObject* found_item = nullptr;
                    ::REGameObject* found_owner = nullptr;

                    for (auto i = 0; i < items->size(); i++) {
                        auto item = items->get_element(i);
                        if (item == nullptr) {
                            continue;
                        }

 #ifdef RE8
                        auto owner = *sdk::get_object_field<::REGameObject*>(item, "<owner>k__BackingField");
 #else
                        auto owner = *sdk::get_object_field<::REGameObject*>(item, "Owner");
 #endif
                        if (owner == nullptr) {
                            continue;
                        }

                        const auto owner_name = utility::re_string::get_string(owner->name);
                        std::string item_key{};
                        std::string item_type_name{};

#ifdef RE7
                        // RE7: match holstered weapons using the actual app.Weapon / app.Item, not the inventory wrapper entry.
                        // The slot stores the current equipped weapon's key/type (from get_weapon_object), so we must compare against
                        // the weapon resolved from each inventory entry.
                        ::REManagedObject* item_internal = nullptr;
                        ::REManagedObject* item_weapon = nullptr;
                        try {
                            item_internal = *sdk::get_object_field<::REManagedObject*>(item, "Item");
                        } catch (...) {
                        }
                        if (item_internal != nullptr) {
                            try {
                                item_weapon = sdk::call_object_func_easy<::REManagedObject*>(item_internal, "get_weapon");
                            } catch (...) {
                            }
                            if (item_weapon == nullptr) {
                                try {
                                    item_weapon = *sdk::get_object_field<::REManagedObject*>(item_internal, "<weapon>k__BackingField");
                                } catch (...) {
                                }
                            }
                        }

                        if (item_weapon != nullptr) {
                            item_key = holster_weapon_instance_key(item_weapon);
                            if (const auto wtd = utility::re_managed_object::get_type_definition(item_weapon); wtd != nullptr) {
                                item_type_name = wtd->get_full_name();
                            }
                        } else if (item_internal != nullptr) {
                            item_key = holster_weapon_instance_key(item_internal);
                            if (const auto itd = utility::re_managed_object::get_type_definition(item_internal); itd != nullptr) {
                                item_type_name = itd->get_full_name();
                            }
                        } else {
                            item_key = holster_weapon_instance_key(item);
                            if (const auto td = utility::re_managed_object::get_type_definition(item); td != nullptr) {
                                item_type_name = td->get_full_name();
                            }
                        }
#else
                        item_key = holster_weapon_instance_key(item);
                        if (const auto item_tdef = utility::re_managed_object::get_type_definition(item); item_tdef != nullptr) {
                            item_type_name = item_tdef->get_full_name();
                        }
#endif

                        if (holster_item_matches_slot_assignment(
                                item_key, owner_name, item_type_name, assigned_id, assigned_type_id)) {
                            found_item = item;
                            found_owner = owner;
                            break;
                        }
                    }

                    if (found_owner != nullptr) {
 #ifdef RE8
                        sdk::call_object_func_easy<void*>(weapon_change, "removeWeaponWithNoAction");
 #else
                        sdk::call_object_func_easy<void*>(weapon_change, "removeWeapon");
 #endif
                        bool equipped = false;

#ifdef RE7
                        // RE7: equip is driven by app.PlayerWeaponChange.equipWeapon(app.Item, app.Weapon).
                        // Inventory list entries vary; resolve an actual app.Item first, then ask it for its app.Weapon.
                        static auto app_item_tdef = sdk::find_type_definition("app.Item");
                        const auto is_item = [&](::REManagedObject* o) -> bool {
                            if (o == nullptr || app_item_tdef == nullptr) {
                                return false;
                            }
                            const auto td = utility::re_managed_object::get_type_definition(o);
                            return td != nullptr && td->is_a(app_item_tdef);
                        };

                        auto resolve_item_from_inventory_entry = [&](::REManagedObject* entry) -> ::REManagedObject* {
                            if (entry == nullptr) {
                                return nullptr;
                            }
                            if (is_item(entry)) {
                                return entry;
                            }

                            // Common wrapper patterns in RE7 inventory lists.
                            for (const char* field : { "Item", "<Item>k__BackingField", "<item>k__BackingField", "_Item", "_item" }) {
                                try {
                                    if (auto p = sdk::get_object_field<::REManagedObject*>(entry, field); p != nullptr && *p != nullptr && is_item(*p)) {
                                        return *p;
                                    }
                                } catch (...) {
                                }
                            }

                            return nullptr;
                        };

                        ::REManagedObject* found_item_internal = resolve_item_from_inventory_entry(found_item);
                        ::REManagedObject* found_weapon = nullptr;
                        if (found_item_internal != nullptr) {
                            try {
                                found_weapon = sdk::call_object_func_easy<::REManagedObject*>(found_item_internal, "get_weapon");
                            } catch (...) {
                            }

                            if (found_weapon == nullptr) {
                                try {
                                    found_weapon = *sdk::get_object_field<::REManagedObject*>(found_item_internal, "<weapon>k__BackingField");
                                } catch (...) {
                                }
                            }
                        }

                        const auto is_weapon = [&](::REManagedObject* o) -> bool {
                            if (o == nullptr || app_weapon_tdef == nullptr) {
                                return false;
                            }
                            const auto td = utility::re_managed_object::get_type_definition(o);
                            return td != nullptr && td->is_a(app_weapon_tdef);
                        };

                        if (!equipped && found_item_internal != nullptr && is_weapon(found_weapon)) {
                            try {
                                // Primary: PlayerWeaponChange equips from app.Item + app.Weapon.
                                sdk::call_object_func_easy<void*>(weapon_change, "equipWeapon(app.Item, app.Weapon)", found_item_internal, found_weapon);
                                equipped = true;
                            } catch (...) {
                            }
                        }

                        if (!equipped && equip_controller != nullptr && is_weapon(found_weapon)) {
                            try {
                                // Fallback: EquipManager equips a weapon into a hand (0 = right).
                                sdk::call_object_func_easy<void*>(equip_controller, "equipWeapon(app.Weapon, app.CharacterDefine.Hand)", found_weapon, 0);
                                equipped = true;
                            } catch (...) {
                            }
                        }

                        if (!equipped && equip_controller != nullptr && is_weapon(found_weapon)) {
                            try {
                                // Alternate RE7 path: EquipManager.applyEquipWeapon(app.Weapon, Hand).
                                sdk::call_object_func_easy<void*>(equip_controller, "applyEquipWeapon(app.Weapon, app.CharacterDefine.Hand)", found_weapon, 0);
                                equipped = true;
                            } catch (...) {
                            }
                        }

                        // RE7: ensure the visible weapon mesh state is restored after a stow path that explicitly hid it.
                        if (equipped) {
                            try {
                                // RE7: synchronize native "equipped weapon id" state used by dpad weapon switching.
                                // If we don't update this, the game can immediately snap back to whatever WeaponID the native dpad system thinks is selected.
                                if (equip_controller != nullptr && found_weapon != nullptr) {
                                    // Keep EquipManager's object and id fields consistent.
                                    try {
                                        sdk::call_object_func_easy<void*>(equip_controller, "set_equipWeaponRight", found_weapon);
                                    } catch (...) {
                                    }

                                    // EquipWeaponIdRight is an app.WeaponID (enum); store underlying int32.
                                    try {
                                        const auto weapon_id = sdk::call_object_func_easy<int32_t>(found_weapon, "get_weaponID");
                                        if (auto p = sdk::get_object_field<int32_t>(equip_controller, "EquipWeaponIdRight"); p != nullptr) {
                                            *p = weapon_id;
                                        }
                                    } catch (...) {
                                    }
                                }

                                // EquipWeaponItemIDRight also participates in native selection/state in RE7.
                                if (equip_controller != nullptr && found_item_internal != nullptr) {
                                    try {
                                        auto item_id = *sdk::get_object_field<::SystemString*>(found_item_internal, "ItemDataID");
                                        if (item_id != nullptr) {
                                            // Setter expects a managed System.String.
                                            sdk::call_object_func_easy<void*>(equip_controller, "set_equipWeaponItemIDRight", item_id);
                                        }
                                    } catch (...) {
                                    }
                                }

                                // CRITICAL (RE7): update the native inventory equip selection so the game's dpad system adopts our choice.
                                // Without this, the game can snap back to the previously-selected quickslot weapon after our holster equip.
                                if (m_inventory != nullptr && found_item_internal != nullptr) {
                                    try {
                                        auto item_id = *sdk::get_object_field<::SystemString*>(found_item_internal, "ItemDataID");
                                        if (item_id != nullptr) {
                                            // Knife/melee handling:
                                            // Quickslot/dpad is primarily for Gun category. For Melee, applying quickslot can immediately overwrite
                                            // the newly equipped melee weapon back to the current quickslot gun selection.
                                            bool is_melee = false;
                                            try {
                                                // app.WeaponCategory is an enum (Gun=0, Melee=1, Others=2).
                                                const auto cat = sdk::call_object_func_easy<int32_t>(found_weapon, "get_weaponCategory");
                                                is_melee = (cat == 1);
                                            } catch (...) {
                                            }

                                            // Reset native equip state, then equip by ItemDataID (the same path used by the game's own UI/quickslot).
                                            try {
                                                sdk::call_object_func_easy<void*>(m_inventory, "removeEquip");
                                            } catch (...) {
                                            }

                                            sdk::call_object_func_easy<void*>(m_inventory, "equipWeapon", item_id);

                                            // Apply quickslot mapping after changing equip state (prevents immediate overwrite by the quickslot system).
                                            // For melee (knife), DO NOT apply quickslot because it can force-switch back to the selected gun.
                                            if (!is_melee) {
                                                try {
                                                    sdk::call_object_func_easy<void*>(m_inventory, "applyQuickSlot");
                                                } catch (...) {
                                                }
                                            }
                                        }
                                    } catch (...) {
                                    }
                                }

                                // Re-enable weapon display at the weapon-change layer if supported.
                                try {
                                    sdk::call_object_func_easy<void*>(weapon_change, "setWeaponDisp", true);
                                } catch (...) {
                                }

                                static auto app_player_mesh_controller = sdk::find_type_definition("app.PlayerMeshController");
                                static auto app_player_mesh_controller_type = app_player_mesh_controller != nullptr ? app_player_mesh_controller->get_type() : nullptr;

                                if (app_player_mesh_controller_type != nullptr) {
                                    if (auto mesh_controller = utility::re_component::find<::REManagedObject>(m_player->transform, app_player_mesh_controller_type);
                                        mesh_controller != nullptr) {
                                        // Notify mesh controller of new weapon and refresh parts.
                                        sdk::call_object_func_easy<void*>(mesh_controller, "onEquipWeaponChanged", found_weapon, nullptr);

                                        try {
                                            auto weapon_mesh_object = sdk::call_object_func_easy<::REGameObject*>(mesh_controller, "get_weaponMeshObject");
                                            if (weapon_mesh_object != nullptr) {
                                                sdk::call_object_func_easy<void*>(weapon_mesh_object, "setActive", true);
                                            }
                                        } catch (...) {
                                        }

                                        try {
                                            sdk::call_object_func_easy<void*>(mesh_controller, "updateParts");
                                        } catch (...) {
                                        }
                                    }
                                }

                                // Ensure the weapon itself is visible.
                                try {
                                    if (found_weapon != nullptr) {
                                        sdk::call_object_func_easy<void*>(found_weapon, "disp", true);
                                    }
                                } catch (...) {
                                }
                            } catch (...) {
                            }
                        }
#endif

                        if (found_item != nullptr && app_weapon_core != nullptr
                            && utility::re_managed_object::get_type_definition(found_item)->is_a(app_weapon_core)) {
                            try {
                                sdk::call_object_func_easy<void*>(weapon_change, "equipWeapon(app.WeaponCore, System.Boolean)", found_item, false);
                                equipped = true;
                            } catch (...) {}

                            if (!equipped) {
                                try {
                                    sdk::call_object_func_easy<void*>(equip_controller, "equipWeapon(app.WeaponCore)", found_item);
                                    equipped = true;
                                } catch (...) {}
                            }

                            if (!equipped) {
                                try {
                                    sdk::call_object_func_easy<void*>(equip_controller, "equipWeapon(app.WeaponCore, app.EquipSlot)", found_item, 0);
                                    equipped = true;
                                } catch (...) {}
                            }
                        }

                        if (!equipped) {
#ifdef RE8
                            sdk::call_object_func_easy<void*>(equip_controller, "equipObject", found_owner);
#else
                            // RE7 last resort: try equipping by WeaponID into right hand if possible.
                            // (Leaving empty if we couldn't resolve an app.Weapon.)
#endif
                        }

                        vr->trigger_haptic_vibration(0.0f, 0.06f, 1.0f, 4.0f, right_joystick);
                    }
                }
            }
        }
    }

    if (!is_grip_down) {
        m_weapon_holster.holster_grip_ever_outside_slots = false;
    }

    m_weapon_holster.was_left_grip_for_holster_tune = is_left_grip_down;
    m_weapon_holster.was_grip_down = is_grip_down;
}

HookManager::PreHookResult RE8VR::pre_shadow_late_update(std::vector<uintptr_t>& args, std::vector<sdk::RETypeDefinition*>& arg_tys, uintptr_t ret_addr) {
    auto& vr = VR::get();
    auto& re8vr = RE8VR::get();

    if (re8vr->m_player == nullptr || re8vr->m_transform == nullptr) {
        return HookManager::PreHookResult::CALL_ORIGINAL;
    }

    if (!vr->is_using_controllers()) {
        return HookManager::PreHookResult::CALL_ORIGINAL;
    }

    if (!re8vr->m_is_in_cutscene && re8vr->m_can_use_hands && !re8vr->m_is_grapple_aim) {
        return HookManager::PreHookResult::SKIP_ORIGINAL;
    }

    return HookManager::PreHookResult::CALL_ORIGINAL;
}

void RE8VR::post_shadow_late_update(uintptr_t& ret_val, sdk::RETypeDefinition* ret_ty, uintptr_t ret_addr) {
}

void RE8VR::apply_recoil_kickback() {
    apply_recoil_kickback(m_weapon);
}

void RE8VR::apply_recoil_kickback(::REManagedObject* weapon_for_id) {
    auto& vr = VR::get();
    if (!vr->is_hmd_active() || !vr->is_using_controllers()) {
        return;
    }
    if (!*m_recoil_enabled) {
        return;
    }
    const float intensity = *m_recoil_intensity;
    if (intensity <= 0.0f) {
        return;
    }

    // Per-weapon intensity (1–4, default 1). Use the weapon that is actually firing for the lookup when called from the shoot hook.
    const bool is_two_hands = m_was_gripping_weapon || m_is_reloading;
    const float weapon_mult = get_weapon_recoil_multiplier(weapon_for_id, is_two_hands);
    if (weapon_mult <= 0.0f) {
        return;
    }

    const float random_factor = 1.0f + (std::rand() / (float)RAND_MAX - 0.5f) * RECOIL_RANDOMNESS;
    const float total_mult = weapon_mult * random_factor * intensity;

    // [0, 1) uniform per shot; separate rand() so pitch, yaw, and position vary independently
    auto rnd = [](float& u) { u = std::rand() / (float)(RAND_MAX + 1u); };
    float u1, u2, u3, u4, u5;
    rnd(u1); rnd(u2); rnd(u3); rnd(u4); rnd(u5);

    // Position kick: mostly back and up, with per-shot variation so direction isn't identical every time
    const float pos_peak = RECOIL_POSITION_INTENSITY * total_mult;
    const float pos_y_scale = 0.5f + 0.25f * u1;   // up component 0.5–0.75
    const float pos_z_scale = 0.9f + 0.2f * u2;    // back component 0.9–1.1
    const float new_pos_y = pos_peak * pos_y_scale;
    const float new_pos_z = -pos_peak * pos_z_scale;

    // Pitch (vertical): dominant upward, with magnitude variation per shot (0.8–1.2) and a small symmetric spread so it's not always the same amount
    const float pitch_base = RECOIL_ROTATION_INTENSITY * total_mult * (0.8f + 0.4f * u3);
    const float pitch_spread = (u4 - 0.5f) * 2.0f * RECOIL_VERTICAL_SPREAD * total_mult;  // [-spread, +spread]
    const float pitch_peak = pitch_base + pitch_spread;

    // Yaw (horizontal): symmetric left/right, uniformly distributed so recoil goes left and right equally often (avoid upward+right bias). Slightly scaled up so left/right are noticeable alongside dominant up.
    const float yaw_signed = (u5 - 0.5f) * 2.0f;  // [-1, 1) uniform
    const float yaw_peak = yaw_signed * RECOIL_HORIZONTAL_SPREAD * total_mult * 1.25f;

    m_recoil_attack_pos_y += new_pos_y;
    m_recoil_attack_pos_z += new_pos_z;
    m_recoil_attack_pitch += pitch_peak;
    m_recoil_attack_yaw += yaw_peak;

    // Scale the stack cap by total_mult so higher per-weapon/global intensity allows visibly more recoil (was fixed cap so all intensities looked the same).
    if (RECOIL_STACK_CAP > 0.0f) {
        const float cap_scale = RECOIL_STACK_CAP * total_mult;
        const float cap_pos = RECOIL_POSITION_INTENSITY * cap_scale;
        const float cap_rot = RECOIL_ROTATION_INTENSITY * cap_scale;
        const float cap_yaw = RECOIL_HORIZONTAL_SPREAD * cap_scale;
        m_recoil_attack_pos_y = std::min(m_recoil_attack_pos_y, cap_pos * 0.6f);
        m_recoil_attack_pos_z = std::max(m_recoil_attack_pos_z, -cap_pos);
        m_recoil_attack_pitch = std::min(m_recoil_attack_pitch, cap_rot);
        m_recoil_attack_yaw = std::max(-cap_yaw, std::min(cap_yaw, m_recoil_attack_yaw));
    }

    m_recoil_attack_t = 0.0f;
    m_recoil_attack_active = true;
    m_recoil_active = true;
    const float now = (float)std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (m_recoil_last_t == 0.0f) {
        m_recoil_last_t = now;
    }
    m_recoil_last_shot_t = now;
}

void RE8VR::cancel_recoil_state() {
    m_recoil = {};
    m_recoil_attack_active = false;
    m_recoil_attack_t = 0.0f;
    m_recoil_attack_pos_y = 0.0f;
    m_recoil_attack_pos_z = 0.0f;
    m_recoil_attack_pitch = 0.0f;
    m_recoil_attack_yaw = 0.0f;
    m_recoil_active = false;
    m_recoil_last_t = 0.0f;
}

void RE8VR::update_recoil(float dt) {
    constexpr float pi_half = 1.5707963267948966f;
    dt = std::min(dt, 0.05f);

    // Cancel recoil when weapon has no ammo so the view doesn't stay kicked.
    // NOTE: On RE7, the *last* bullet sets ammo to 0 immediately after a successful shot.
    // We must not cancel recoil in that case, otherwise the last shot appears to have "no recoil".
    if (m_weapon != nullptr && get_weapon_ammo_count(m_weapon) == 0) {
        const float now = (float)std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const float since_last_shot = (m_recoil_last_shot_t > 0.0f) ? (now - m_recoil_last_shot_t) : 999.0f;

        // Allow the recoil impulse to play out briefly even at 0 ammo.
        // This still prevents "stuck kicked view" on empty/dry-fire because last_shot_t won't be updated on failed shots.
        if (!m_recoil_attack_active && since_last_shot > 0.25f) {
            m_recoil = {};
            m_recoil_attack_active = false;
            m_recoil_attack_t = 0.0f;
            m_recoil_attack_pos_y = 0.0f;
            m_recoil_attack_pos_z = 0.0f;
            m_recoil_attack_pitch = 0.0f;
            m_recoil_attack_yaw = 0.0f;
            m_recoil_active = false;
            m_recoil_last_t = 0.0f;
            return;
        }
    }

    if (!m_recoil_active && !m_recoil_attack_active) {
        return;
    }

    const float now = (float)std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (m_recoil_last_t > 0.0f) {
        dt = std::min(now - m_recoil_last_t, 0.05f);
    }
    m_recoil_last_t = now;

    const float attack_dur_val = *m_recoil_attack_duration;
    const float attack_duration = (attack_dur_val >= 0.001f) ? attack_dur_val : 0.001f;
    const float k = *m_recoil_spring_stiffness;
    const float c_base = *m_recoil_spring_damping;
    const float c_sustained = *m_recoil_sustained_damping;
    const float sustained_win_val = *m_recoil_sustained_window;
    const float sustained_window = (sustained_win_val >= 0.01f) ? sustained_win_val : 0.01f;

    if (m_recoil_attack_active && dt > 0.0f) {
        m_recoil_attack_t += dt;

        if (m_recoil_attack_t >= attack_duration) {
            m_recoil.spring_pos_y = m_recoil_attack_pos_y;
            m_recoil.spring_pos_z = m_recoil_attack_pos_z;
            m_recoil.spring_pitch = m_recoil_attack_pitch;
            m_recoil.spring_yaw = m_recoil_attack_yaw;
            m_recoil.spring_vel_y = 0.0f;
            m_recoil.spring_vel_z = 0.0f;
            m_recoil.spring_vel_pitch = 0.0f;
            m_recoil.spring_vel_yaw = 0.0f;
            m_recoil_attack_pos_y = 0.0f;
            m_recoil_attack_pos_z = 0.0f;
            m_recoil_attack_pitch = 0.0f;
            m_recoil_attack_yaw = 0.0f;
            m_recoil_attack_t = 0.0f;
            m_recoil_attack_active = false;
        } else {
            const float s = std::sin((m_recoil_attack_t / attack_duration) * pi_half);
            m_recoil.spring_pos_y = m_recoil_attack_pos_y * s;
            m_recoil.spring_pos_z = m_recoil_attack_pos_z * s;
            m_recoil.spring_pitch = m_recoil_attack_pitch * s;
            m_recoil.spring_yaw = m_recoil_attack_yaw * s;
            m_recoil.spring_vel_y = 0.0f;
            m_recoil.spring_vel_z = 0.0f;
            m_recoil.spring_vel_pitch = 0.0f;
            m_recoil.spring_vel_yaw = 0.0f;
        }
    }

    if (!m_recoil_attack_active && dt > 0.0f) {
        float c = c_base;
        if (m_recoil_last_shot_t > 0.0f) {
            const float since_last = now - m_recoil_last_shot_t;
            if (since_last < sustained_window) {
                const float t_blend = 1.0f - (since_last / sustained_window);
                c = c + (c_sustained - c) * t_blend;
            }
        }

        const int steps = std::max(1, (int)(dt / RECOIL_SUBSTEP_DT));
        const float sub = dt / (float)steps;

        for (int i = 0; i < steps; ++i) {
            float ay = -k * m_recoil.spring_pos_y - c * m_recoil.spring_vel_y;
            m_recoil.spring_vel_y += ay * sub;
            m_recoil.spring_pos_y += m_recoil.spring_vel_y * sub;

            float az = -k * m_recoil.spring_pos_z - c * m_recoil.spring_vel_z;
            m_recoil.spring_vel_z += az * sub;
            m_recoil.spring_pos_z += m_recoil.spring_vel_z * sub;

            float ap = -k * m_recoil.spring_pitch - c * m_recoil.spring_vel_pitch;
            m_recoil.spring_vel_pitch += ap * sub;
            m_recoil.spring_pitch += m_recoil.spring_vel_pitch * sub;

            float aw = -k * m_recoil.spring_yaw - c * m_recoil.spring_vel_yaw;
            m_recoil.spring_vel_yaw += aw * sub;
            m_recoil.spring_yaw += m_recoil.spring_vel_yaw * sub;
        }

        const float pos_mag = std::abs(m_recoil.spring_pos_y) + std::abs(m_recoil.spring_pos_z);
        const float rot_mag = std::abs(m_recoil.spring_pitch) + std::abs(m_recoil.spring_yaw);
        const float vel_mag = std::abs(m_recoil.spring_vel_y) + std::abs(m_recoil.spring_vel_z)
            + std::abs(m_recoil.spring_vel_pitch) + std::abs(m_recoil.spring_vel_yaw);

        if (pos_mag < 0.00005f && rot_mag < 0.0002f && vel_mag < 0.001f) {
            m_recoil = {};
            m_recoil_active = false;
            m_recoil_last_t = 0.0f;
        }
    }
}

std::string RE8VR::get_weapon_recoil_id(::REManagedObject* weapon) const {
    if (weapon == nullptr) {
        return {};
    }
    // Weapon ID mapping: stable per-weapon identifier for recoil config. RE7 and RE8 use different structures.
#ifdef RE8
    // RE8: weapon is app.WeaponGunCore; type name is not unique. Use owner GameObject name (e.g. "ri3042_Inventory").
    auto owner_ptr = sdk::get_object_field<::REGameObject*>(weapon, "<owner>k__BackingField");
    if (owner_ptr != nullptr && *owner_ptr != nullptr) {
        std::string name = utility::re_game_object::get_name(*owner_ptr);
        if (!name.empty()) {
            return name;
        }
    }
#else
    // RE7: weapon is app.WeaponGun; use the weapon's own GameObject name (get_GameObject()) to identify the gun type.
    auto* go = sdk::call_object_func_easy<::REGameObject*>(weapon, "get_GameObject");
    if (go != nullptr) {
        std::string name = utility::re_game_object::get_name(go);
        if (!name.empty()) {
            return name;
        }
    }
#endif
    // Fallback to type name if owner/GameObject name unavailable
    auto* tdef = utility::re_managed_object::get_type_definition(weapon);
    if (tdef != nullptr) {
        return tdef->get_full_name();
    }
    return {};
}

std::string RE8VR::get_current_weapon_recoil_id() const {
    return get_weapon_recoil_id(m_weapon);
}

void RE8VR::set_per_weapon_recoil_one_hand(const std::string& weapon_id, float intensity) {
    if (weapon_id.empty()) {
        return;
    }
    intensity = std::max(1.0f, std::min(4.0f, intensity));
    auto& p = m_per_weapon_recoil[weapon_id];
    p.one_hand = intensity;
}

float RE8VR::get_per_weapon_recoil_one_hand(const std::string& weapon_id) const {
    if (weapon_id.empty()) {
        return 1.0f;
    }
    if (auto it = m_per_weapon_recoil.find(weapon_id); it != m_per_weapon_recoil.end()) {
        return it->second.one_hand;
    }
    return 1.0f;
}

void RE8VR::set_per_weapon_recoil_two_hands(const std::string& weapon_id, float intensity) {
    if (weapon_id.empty()) {
        return;
    }
    intensity = std::max(1.0f, std::min(4.0f, intensity));
    auto& p = m_per_weapon_recoil[weapon_id];
    p.two_hands = intensity;
}

float RE8VR::get_per_weapon_recoil_two_hands(const std::string& weapon_id) const {
    if (weapon_id.empty()) {
        return 1.0f;
    }
    if (auto it = m_per_weapon_recoil.find(weapon_id); it != m_per_weapon_recoil.end()) {
        return it->second.two_hands;
    }
    return 1.0f;
}

void RE8VR::set_per_weapon_recoil_intensity(const std::string& weapon_id, float intensity) {
    // Back-compat: treat as two-handed value.
    set_per_weapon_recoil_two_hands(weapon_id, intensity);
}

float RE8VR::get_per_weapon_recoil_intensity(const std::string& weapon_id) const {
    // Back-compat: treat as two-handed value.
    return get_per_weapon_recoil_two_hands(weapon_id);
}

void RE8VR::load_recoil_settings() {
    const auto path = REFramework::get_persistent_dir() / "reframework" / "data" / RECOIL_SETTINGS_FILENAME;
    std::ifstream f(path);
    if (!f) {
        return;
    }
    try {
        json j = json::parse(f);
        m_per_weapon_recoil.clear();
        if (j.contains("weapons") && j["weapons"].is_object()) {
            for (auto& [key, val] : j["weapons"].items()) {
                if (!val.is_object()) {
                    continue;
                }

                auto& p = m_per_weapon_recoil[key];

                // Preferred schema: { one_hand, two_hands }
                if (val.contains("one_hand") && val["one_hand"].is_number()) {
                    p.one_hand = std::max(1.0f, std::min(4.0f, val["one_hand"].get<float>()));
                }
                if (val.contains("two_hands") && val["two_hands"].is_number()) {
                    p.two_hands = std::max(1.0f, std::min(4.0f, val["two_hands"].get<float>()));
                }

                // Back-compat: { intensity } means "two_hands" (and if one_hand not provided, copy it)
                if (val.contains("intensity") && val["intensity"].is_number()) {
                    const float v = std::max(1.0f, std::min(4.0f, val["intensity"].get<float>()));
                    if (!(val.contains("two_hands") && val["two_hands"].is_number())) {
                        p.two_hands = v;
                    }
                    if (!(val.contains("one_hand") && val["one_hand"].is_number())) {
                        p.one_hand = v;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("[RE8VR] load_recoil_settings: {}", e.what());
    }
}

void RE8VR::save_recoil_settings() {
    const auto path = REFramework::get_persistent_dir() / "reframework" / "data" / RECOIL_SETTINGS_FILENAME;
    try {
        std::filesystem::create_directories(path.parent_path());
        json j = json::object();
        j["weapons"] = json::object();
        for (const auto& [weapon_id, p] : m_per_weapon_recoil) {
            j["weapons"][weapon_id] = json::object({ {"one_hand", p.one_hand}, {"two_hands", p.two_hands} });
        }
        std::ofstream f(path);
        if (f) {
            f << j.dump(2);
        }
    } catch (const std::exception& e) {
        spdlog::warn("[RE8VR] save_recoil_settings: {}", e.what());
    }
}

float RE8VR::get_weapon_recoil_multiplier() const {
    return get_weapon_recoil_multiplier(m_weapon);
}

float RE8VR::get_weapon_recoil_multiplier(::REManagedObject* weapon) const {
    return get_weapon_recoil_multiplier(weapon, true);
}

float RE8VR::get_weapon_recoil_multiplier(::REManagedObject* weapon, const bool is_two_hands) const {
    if (weapon == nullptr) {
        return 1.0f;
    }

    const auto id = get_weapon_recoil_id(weapon);
    if (id.empty()) {
        return 1.0f;
    }

    if (auto it = m_per_weapon_recoil.find(id); it != m_per_weapon_recoil.end()) {
        return is_two_hands ? it->second.two_hands : it->second.one_hand;
    }

    // Default when missing: no one-hand boost unless explicitly configured.
    return 1.0f;
}

Vector3f RE8VR::get_recoil_position_offset_world(const glm::quat& camera_rotation) const {
    // Invert Z so weapon kicks backward toward shoulder (fixes "flicking forward").
    const Vector3f local_offset(0.0f, m_recoil.spring_pos_y, -m_recoil.spring_pos_z);
    return camera_rotation * local_offset;
}

glm::quat RE8VR::get_recoil_rotation_offset_world(const glm::quat& camera_rotation) const {
    // Muzzle flip UP: positive pitch = rotate around local X so barrel rises (pivot at hand).
    const glm::quat pitch_rot = glm::angleAxis(m_recoil.spring_pitch, Vector3f{1.0f, 0.0f, 0.0f});
    const glm::quat yaw_rot = glm::angleAxis(m_recoil.spring_yaw, Vector3f{0.0f, 1.0f, 0.0f});
    return camera_rotation * pitch_rot * yaw_rot * glm::inverse(camera_rotation);
}

int32_t RE8VR::get_weapon_ammo_count(::REManagedObject* weapon) const {
    if (weapon == nullptr) {
        return -1;
    }

#ifndef RE8
    // RE7 fast-path: WeaponGun exposes get_loadNum(). Cache method lookup.
    static sdk::RETypeDefinition* s_weapon_gun_tdef = sdk::find_type_definition("app.WeaponGun");
    static sdk::REMethodDefinition* s_weapon_gun_get_load_num = s_weapon_gun_tdef != nullptr ? s_weapon_gun_tdef->get_method("get_loadNum") : nullptr;
    if (s_weapon_gun_tdef != nullptr && s_weapon_gun_get_load_num != nullptr) {
        if (auto* tdef = utility::re_managed_object::get_type_definition(weapon); tdef != nullptr && tdef->is_a(s_weapon_gun_tdef)) {
            try {
                return s_weapon_gun_get_load_num->call<int32_t>(sdk::get_thread_context(), weapon);
            } catch (...) {
            }
        }
    }
#endif

    auto try_obj = [](::REManagedObject* obj) -> int32_t {
        if (obj == nullptr) return -1;
        auto* tdef = utility::re_managed_object::get_type_definition(obj);
        if (tdef == nullptr) return -1;
        static const char* field_names[] = {
            "<RemainBullet>k__BackingField", "RemainBullet", "_RemainBullet",
            "<CurrentBullet>k__BackingField", "CurrentBullet", "Num", "<Num>k__BackingField",
            "<loadNum>k__BackingField", "loadNum", "_loadNum"
        };
        for (const char* name : field_names) {
            auto* pi32 = sdk::get_object_field<int32_t>(obj, name, false);
            if (pi32 != nullptr) return *pi32;
            auto* pu32 = sdk::get_object_field<uint32_t>(obj, name, false);
            if (pu32 != nullptr) return (int32_t)*pu32;
        }
        static const char* method_names[] = {
            "get_RemainBullet", "get_CurrentBullet", "get_remainBullet",
            "get_RemainNum", "get_CurrentNum", "get_LeftBullet", "get_NumRemain", "get_Num",
            "get_loadNum"
        };
        for (const char* method_name : method_names) {
            auto* method = tdef->get_method(method_name);
            if (method != nullptr) {
                try {
                    return method->call<int32_t>(sdk::get_thread_context(), obj);
                } catch (...) { break; }
            }
        }
        return -1;
    };
    int32_t v = try_obj(weapon);
    if (v >= 0) return v;
#ifdef RE8
    // RE8: ammo may be on the weapon's "work" object (e.g. InstanceWork). RE7 app.WeaponGun may not have get_work.
    auto* work = sdk::call_object_func_easy<::REManagedObject*>(weapon, "get_work");
    return try_obj(work);
#else
    return -1;
#endif
}

HookManager::PreHookResult RE8VR::pre_weapon_shoot(std::vector<uintptr_t>& args, std::vector<sdk::RETypeDefinition*>& arg_tys, uintptr_t ret_addr) {
    auto& re8vr = RE8VR::get();
    if (re8vr->m_player == nullptr || re8vr->m_weapon == nullptr) {
        return HookManager::PreHookResult::CALL_ORIGINAL;
    }
    if (args.size() < 2) {
        return HookManager::PreHookResult::CALL_ORIGINAL;
    }
    const auto weapon_this = (::REManagedObject*)args[1];
    const auto current_weapon = re8vr->get_weapon_object(re8vr->m_player);
    if (weapon_this != current_weapon) {
        return HookManager::PreHookResult::CALL_ORIGINAL;
    }
    // No recoil when weapon has no ammo (empty click or ran out mid-burst)
    if (re8vr->get_weapon_ammo_count(weapon_this) == 0) {
        return HookManager::PreHookResult::CALL_ORIGINAL;
    }
    re8vr->apply_recoil_kickback(weapon_this);
    return HookManager::PreHookResult::CALL_ORIGINAL;
}

void RE8VR::post_weapon_shoot(uintptr_t& ret_val, sdk::RETypeDefinition* ret_ty, uintptr_t ret_addr) {
    // When shoot() returns false (no ammo / didn't fire), cancel recoil so dry fire doesn't kick the view
    if (ret_ty != nullptr && ret_val == 0) {
        auto& re8vr = RE8VR::get();
        re8vr->cancel_recoil_state();
    }
}

bool RE8VR::get_recoil_enabled() const {
    return m_recoil_enabled != nullptr ? m_recoil_enabled->value() : false;
}

void RE8VR::set_recoil_enabled(const bool enabled) {
    if (m_recoil_enabled != nullptr) {
        m_recoil_enabled->value() = enabled;
    }
}

float RE8VR::get_recoil_intensity() const {
    return m_recoil_intensity != nullptr ? m_recoil_intensity->value() : 1.0f;
}

void RE8VR::set_recoil_intensity(const float intensity) {
    if (m_recoil_intensity != nullptr) {
        m_recoil_intensity->value() = std::clamp(intensity, 1.0f, 4.0f);
    }
}

float RE8VR::get_recoil_attack_duration() const {
    return m_recoil_attack_duration != nullptr ? m_recoil_attack_duration->value() : RECOIL_ATTACK_DURATION;
}

void RE8VR::set_recoil_attack_duration(const float v) {
    if (m_recoil_attack_duration != nullptr) {
        m_recoil_attack_duration->value() = std::clamp(v, 0.005f, 0.06f);
    }
}

float RE8VR::get_recoil_spring_stiffness() const {
    return m_recoil_spring_stiffness != nullptr ? m_recoil_spring_stiffness->value() : RECOIL_SPRING_STIFFNESS;
}

void RE8VR::set_recoil_spring_stiffness(const float v) {
    if (m_recoil_spring_stiffness != nullptr) {
        m_recoil_spring_stiffness->value() = std::clamp(v, 80.0f, 250.0f);
    }
}

float RE8VR::get_recoil_spring_damping() const {
    return m_recoil_spring_damping != nullptr ? m_recoil_spring_damping->value() : RECOIL_SPRING_DAMPING;
}

void RE8VR::set_recoil_spring_damping(const float v) {
    if (m_recoil_spring_damping != nullptr) {
        m_recoil_spring_damping->value() = std::clamp(v, 12.0f, 35.0f);
    }
}

float RE8VR::get_recoil_sustained_damping() const {
    return m_recoil_sustained_damping != nullptr ? m_recoil_sustained_damping->value() : RECOIL_SUSTAINED_DAMPING;
}

void RE8VR::set_recoil_sustained_damping(const float v) {
    if (m_recoil_sustained_damping != nullptr) {
        m_recoil_sustained_damping->value() = std::clamp(v, 24.0f, 45.0f);
    }
}

float RE8VR::get_recoil_sustained_window() const {
    return m_recoil_sustained_window != nullptr ? m_recoil_sustained_window->value() : RECOIL_SUSTAINED_WINDOW;
}

void RE8VR::set_recoil_sustained_window(const float v) {
    if (m_recoil_sustained_window != nullptr) {
        m_recoil_sustained_window->value() = std::clamp(v, 0.06f, 0.25f);
    }
}

void RE8VR::update_heal_gesture() {
#ifdef RE7
    if (m_inventory == nullptr) {
#else
    if (m_inventory == nullptr || m_updater == nullptr) {
#endif
        m_wants_heal = false;
        m_heal_gesture.was_grip_down = false;
        m_heal_gesture.was_trigger_down = false;
        m_heal_gesture.raw_was_grip_down = false;
        m_heal_gesture.heal_grip_began_inside_slot = false;
        m_heal_gesture.cached_medicine_item = nullptr;
        m_heal_gesture.cached_medicine_owner = nullptr;

        return;
    }

    auto& vr = VR::get();

    const auto hmd = vr->get_transform(0);
    const Vector3f head_pos{hmd[3]};
    const Vector3f head_right{hmd[0]};
    const Vector3f head_up{hmd[1]};
    const Vector3f head_forward{hmd[2]};

    const auto right_hand = vr->get_transform(vr->get_controllers()[1]);
    const Vector3f rh_pos{right_hand[3]};

    const Vector3f heal_slot_pos =
        compute_holster_slot_world_position(HolsterSlot::RightShoulderHeal, head_pos, head_right, head_up, head_forward);
    const auto in_heal_slot = glm::length(rh_pos - heal_slot_pos) <= m_holster_heal_inner_m;

    const auto right_joystick = vr->get_right_joystick();
    const auto action_trigger = vr->get_action_trigger();
    const auto action_grip = vr->get_action_grip();
    const auto is_trigger_down = vr->is_action_active(action_trigger, right_joystick);
    const auto is_grip_down = vr->is_action_active(action_grip, right_joystick);

    const bool heal_raw_grip_pressed = is_grip_down && !m_heal_gesture.raw_was_grip_down;
    if (!is_grip_down) {
        m_heal_gesture.heal_grip_began_inside_slot = false;
    } else if (heal_raw_grip_pressed) {
        m_heal_gesture.heal_grip_began_inside_slot = in_heal_slot;
    }
    m_heal_gesture.raw_was_grip_down = is_grip_down;

#ifdef RE8
    static auto app_medicine_core = sdk::find_type_definition("app.MedicineCore");

    auto items_list = *sdk::get_object_field<::REManagedObject*>(m_inventory, "<items>k__BackingField");
    auto weapon_change = sdk::call_object_func_easy<::REManagedObject*>(m_updater, "get_playerWeaponChange");
    auto mesh_controller = sdk::call_object_func_easy<::REManagedObject*>(m_updater, "get_playerMeshController");

    if (mesh_controller == nullptr && m_order != nullptr) {
        mesh_controller =  sdk::call_object_func_easy<::REManagedObject*>(m_order, "get_playerMeshController");
    }
#else
    static auto app_player_weapon_change = sdk::find_type_definition("app.PlayerWeaponChange");
    static auto app_player_mesh_controller = sdk::find_type_definition("app.PlayerMeshController");
    static auto app_player_weapon_change_type = app_player_weapon_change->get_type();
    static auto app_player_mesh_controller_type = app_player_mesh_controller->get_type();

    auto items_list = *sdk::get_object_field<::REManagedObject*>(m_inventory, "_ItemList");
    auto weapon_change = utility::re_component::find<::REManagedObject>(m_player->transform, app_player_weapon_change_type);
    auto mesh_controller = utility::re_component::find<::REManagedObject>(m_player->transform, app_player_mesh_controller_type);
#endif

    if (items_list == nullptr || weapon_change == nullptr || mesh_controller == nullptr) {
        spdlog::info("[RE8VR] Could not find inventory, weapon change, or mesh controller");
        m_heal_gesture.heal_grip_began_inside_slot = false;
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    auto items = *sdk::get_object_field<sdk::SystemArray*>(items_list, "mItems");

    if (items == nullptr) {
        spdlog::info("[RE8VR] mItems is null");
        return;
    }

    ::REManagedObject* medicine_item = nullptr;
    ::REGameObject* owner = nullptr;

    if (m_heal_gesture.cached_medicine_item != nullptr && m_heal_gesture.cached_medicine_owner != nullptr) {
        medicine_item = m_heal_gesture.cached_medicine_item;
        owner = m_heal_gesture.cached_medicine_owner;

        // Validate cache quickly (owner link can break when inventory refreshes).
#ifdef RE7
        auto cached_owner = *sdk::get_object_field<::REGameObject*>(medicine_item, "Owner");
#else
        auto cached_owner = *sdk::get_object_field<::REGameObject*>(medicine_item, "<owner>k__BackingField");
#endif
        if (cached_owner == nullptr || cached_owner != owner) {
            medicine_item = nullptr;
            owner = nullptr;
            m_heal_gesture.cached_medicine_item = nullptr;
            m_heal_gesture.cached_medicine_owner = nullptr;
        }
    }

    if (medicine_item == nullptr) {
        for (auto i = 0; i < items->size(); i++) {
            auto item = items->get_element(i);

            if (item == nullptr) {
                continue;
            }

#ifdef RE8
            const auto is_medicine = utility::re_managed_object::get_type_definition(item)->is_a(app_medicine_core);
#else
            bool is_medicine = false;
            auto item_internal = *sdk::get_object_field<::REManagedObject*>(item, "Item");

            if (item_internal != nullptr) {
                auto item_name = *sdk::get_object_field<::SystemString*>(item_internal, "ItemDataID");

                if (item_name != nullptr) {
                    is_medicine = utility::re_string::get_string(item_name).find("Remedy") != std::string::npos;
                }
            }
#endif

            if (is_medicine) {
                medicine_item = item;
                break;
            }
        }
    }

    if (medicine_item == nullptr) {
        m_wants_heal = false;
        m_heal_gesture.was_grip_down = false;
        m_heal_gesture.was_trigger_down = false;
        m_heal_gesture.heal_grip_began_inside_slot = false;

        return;
    }

#ifdef RE7
    auto item_internal = *sdk::get_object_field<::REManagedObject*>(medicine_item, "Item");
    if (owner == nullptr) {
        owner = *sdk::get_object_field<::REGameObject*>(medicine_item, "Owner");
    }
#else
    if (owner == nullptr) {
        owner = *sdk::get_object_field<::REGameObject*>(medicine_item, "<owner>k__BackingField");
    }
#endif

    if (owner == nullptr) {
        m_wants_heal = false;
        m_heal_gesture.was_grip_down = false;
        m_heal_gesture.was_trigger_down = false;
        m_heal_gesture.heal_grip_began_inside_slot = false;
        m_heal_gesture.cached_medicine_item = nullptr;
        m_heal_gesture.cached_medicine_owner = nullptr;

        spdlog::info("[RE8VR] Medicine has no owner");

        return;
    }

    if (m_heal_gesture.cached_medicine_item != medicine_item || m_heal_gesture.cached_medicine_owner != owner) {
        m_heal_gesture.cached_medicine_item = medicine_item;
        m_heal_gesture.cached_medicine_owner = owner;
    }

    static auto via_render_mesh = sdk::find_type_definition("via.render.Mesh");
    static auto via_render_mesh_type = via_render_mesh->get_type();

    auto current_mesh = *sdk::get_object_field<::REManagedObject*>(mesh_controller, "WeaponMesh");
    auto item_mesh = utility::re_component::find<::REManagedObject>(owner->transform, via_render_mesh_type);

    const auto is_same_mesh = current_mesh == item_mesh;

    if (current_mesh == nullptr) {
        m_heal_gesture.was_grip_down = false;
        m_heal_gesture.was_trigger_down = false;
    }

    auto dequip_item = [&]() {
#ifdef RE7
        auto equip_manager = utility::re_component::find<::REManagedObject>(m_player->transform, "app.EquipManager");
        if (equip_manager == nullptr) {
            return;
        }

        auto item_weapon = *sdk::get_object_field<::REManagedObject*>(item_internal, "<weapon>k__BackingField");

        sdk::call_object_func_easy<void*>(weapon_change, "removeWeapon");
        sdk::call_object_func_easy<void*>(mesh_controller, "onEquipWeaponChanged", nullptr, item_weapon);

        auto current_equipped_right = *sdk::get_object_field<::REManagedObject*>(equip_manager, "<equipWeaponRight>k__BackingField");

        if (current_equipped_right == item_weapon) {
           sdk::call_object_func_easy<void*>(equip_manager, "set_equipWeaponRight", nullptr);
        }
#endif
    };

    if (!is_same_mesh) {
        const bool heal_post_grab_grace = m_heal_gesture.heal_grip_began_inside_slot
            && (now - m_heal_gesture.last_grab_time) < std::chrono::milliseconds(500);
        if (!is_trigger_down && m_heal_gesture.heal_grip_began_inside_slot && (in_heal_slot || heal_post_grab_grace)) {
            vr->trigger_haptic_vibration(0.0f, 0.1f, 1.0f, 5.0f, right_joystick);

            if (is_grip_down) {
#ifdef RE8
                auto equip_manager = sdk::call_object_func_easy<::REManagedObject*>(m_updater, "get_equipController");

                if (equip_manager == nullptr && m_order != nullptr) {
                    equip_manager = sdk::call_object_func_easy<::REManagedObject*>(m_order, "get_equipController");
                }

                sdk::call_object_func_easy<void*>(medicine_item, "setActive", true);
                sdk::call_object_func_easy<void*>(weapon_change, "removeWeaponWithNoAction");

                if (equip_manager != nullptr) {
                    sdk::call_object_func_easy<void*>(equip_manager, "equipObject", owner);
                }

                m_heal_gesture.last_grab_time = now;
#else
                auto equip_manager = utility::re_component::find<::REManagedObject>(m_player->transform, "app.EquipManager");

                if (equip_manager == nullptr) {
                    spdlog::info("[RE8VR] No equip manager found");
                    return;
                }

                auto current_equipped_right = *sdk::get_object_field<::REManagedObject*>(equip_manager, "<equipWeaponRight>k__BackingField");
                auto item_weapon = *sdk::get_object_field<::REManagedObject*>(item_internal, "<weapon>k__BackingField");

                sdk::call_object_func_easy<void*>(weapon_change, "removeWeapon");
                
                try {
                    sdk::call_object_func_easy<void*>(equip_manager, "equipWeapon(app.Weapon, app.CharacterDefine.Hand)", item_weapon, 0);
                } catch(...) {}

                try {
                    sdk::call_object_func_easy<void*>(weapon_change, "equipWeapon", item_internal, item_weapon);
                } catch(...) {}

                if (m_weapon != nullptr) {
                    sdk::call_object_func_easy<void*>(mesh_controller, "onEquipWeaponChanged", item_weapon, m_weapon);
                }

                if (current_equipped_right != item_weapon) {
                    sdk::call_object_func_easy<void*>(equip_manager, "set_equipWeaponRight", item_weapon);
                }

                m_heal_gesture.last_grab_time = now;
#endif
            }
        }
    } else if (is_trigger_down) {
        if (!m_heal_gesture.was_trigger_down) {
#ifdef RE7
            auto item_weapon = *sdk::get_object_field<::REManagedObject*>(item_internal, "<weapon>k__BackingField");

            dequip_item();
            sdk::call_object_func_easy<void*>(weapon_change, "useItem", item_internal, item_weapon);
#else
            sdk::call_object_func_easy<void*>(weapon_change, "requestUseItem", medicine_item, false, false);
#endif
        }
    } else if (!is_grip_down) {
        if (is_same_mesh) {
#ifdef RE7
            auto item_weapon = *sdk::get_object_field<::REManagedObject*>(item_internal, "<weapon>k__BackingField");
            sdk::call_object_func_easy<void*>(weapon_change, "removeWeapon");
            sdk::call_object_func_easy<void*>(mesh_controller, "onEquipWeaponChanged", nullptr, item_weapon);
#else
            sdk::call_object_func_easy<void*>(weapon_change, "removeWeaponWithNoAction");
            sdk::call_object_func_easy<void*>(medicine_item, "setActive", false);
#endif
        }
    }

    m_heal_gesture.was_grip_down = is_grip_down && m_heal_gesture.last_grip_weapon == m_weapon; 
    m_heal_gesture.was_trigger_down = is_trigger_down && m_heal_gesture.last_grip_weapon == m_weapon;
    m_heal_gesture.last_grip_weapon = m_weapon;

#ifdef RE8
    static auto common_use_remedy_action = *sdk::get_static_field<::REManagedObject*>("app.PlayerDefineEnumLikeArray.UpperActionID", "CommonUseRemedy");

    const auto is_syringe = utility::re_string::get_string(owner->name) == "ri1022_Inventory";
    const auto upper_action_id = *sdk::get_object_field<::REManagedObject*>(m_status, "<upperActionID>k__BackingField");
    const auto using_effect = *sdk::get_object_field<::REManagedObject*>(medicine_item, "usingEffect") != nullptr
                                || upper_action_id == common_use_remedy_action;



    // In RE8 the medicine is rotated all weird.
    if (!is_trigger_down && !using_effect) {
        if (!is_syringe) {
            sdk::call_object_func_easy<void*>(owner->transform, "set_LocalRotation", &m_heal_gesture.re8_medicine_rotation);
        } else {
            sdk::call_object_func_easy<void*>(owner->transform, "set_LocalRotation", &m_heal_gesture.re8_syringe_rotation);
        }
    } else if (is_syringe && using_effect) {
        glm::quat zero_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        sdk::call_object_func_easy<void*>(owner->transform, "set_LocalRotation", &zero_rotation);
    }
#endif
}
#endif
