// Even though this is called RE8VR, it also affects RE7.
#pragma once

#if defined(RE7) || defined(RE8)
#include <chrono>
#include <sdk/REMath.hpp>
#include <reframework/API.h>

#include "HookManager.hpp"
#include "../../../Mod.hpp"

class RE8VR : public Mod {
public:
    static std::shared_ptr<RE8VR>& get();

    std::string_view get_name() const override {
        return "RE8VR";
    }

    std::optional<std::string> on_initialize() override;
    void on_config_load(const utility::Config& cfg) override;
    void on_config_save(utility::Config& cfg) override;

    void on_lua_state_created(sol::state& lua) override;
    void on_lua_state_destroyed(sol::state& lua) override;

    void on_draw_ui() override;

    void on_pre_application_entry(void* entry, const char* name, size_t hash) override;
    void on_application_entry(void* entry, const char* name, size_t hash) override;

    // non-virtual callbacks
    void on_pre_lock_scene(void* entry);

    void reset_data();

    void set_hand_joints_to_tpose(::REManagedObject* hand_ik);
    void update_hand_ik();
    void update_body_ik(glm::quat* camera_rotation, Vector4f* camera_pos);
    void update_player_gestures();
    void fix_player_camera(::REManagedObject* player_camera);
    void fix_player_shadow();
    void slerp_gui(const glm::quat& new_gui_quat);

    void apply_recoil_kickback();
    void update_recoil(float dt);
    Vector3f get_recoil_position_offset_world(const glm::quat& camera_rotation) const;
    glm::quat get_recoil_rotation_offset_world(const glm::quat& camera_rotation) const;

    float get_weapon_recoil_multiplier() const;

    ::REManagedObject* get_localplayer() const;
    ::REManagedObject* get_weapon_object(::REGameObject* player) const;

    bool update_pointers();
    bool update_ik_pointers();

private:
    void update_block_gesture();
    void update_heal_gesture();

    static HookManager::PreHookResult pre_weapon_shoot(std::vector<uintptr_t>& args, std::vector<sdk::RETypeDefinition*>& arg_tys, uintptr_t ret_addr);
    static void post_weapon_shoot(uintptr_t& ret_val, sdk::RETypeDefinition* ret_ty, uintptr_t ret_addr);

    static HookManager::PreHookResult pre_shadow_late_update(std::vector<uintptr_t>& args, std::vector<sdk::RETypeDefinition*>& arg_tys, uintptr_t ret_addr);
    static void post_shadow_late_update(uintptr_t& ret_val, sdk::RETypeDefinition* ret_ty, uintptr_t ret_addr);

private:
    const ModToggle::Ptr m_hide_upper_body{ ModToggle::create(generate_name("HideUpperBody"), false) };
    const ModToggle::Ptr m_hide_lower_body{ ModToggle::create(generate_name("HideLowerBody"), false) };
    const ModToggle::Ptr m_hide_arms{ ModToggle::create(generate_name("HideArms"), false) };
    const ModToggle::Ptr m_hide_upper_body_cutscenes{ ModToggle::create(generate_name("AutoHideUpperBodyCutscenes"), true) };
    const ModToggle::Ptr m_hide_lower_body_cutscenes{ ModToggle::create(generate_name("AutoHideLowerBodyCutscenes"), true) };
    const ModToggle::Ptr m_recoil_enabled{ ModToggle::create(generate_name("RecoilEnabled"), true) };
    const ModSlider::Ptr m_recoil_intensity{ ModSlider::create(generate_name("RecoilIntensity"), 0.0f, 2.0f, 1.0f) };
    const ModSlider::Ptr m_recoil_attack_duration{ ModSlider::create(generate_name("RecoilAttackDuration"), 0.005f, 0.06f, 0.02f) };
    const ModSlider::Ptr m_recoil_spring_stiffness{ ModSlider::create(generate_name("RecoilSpringStiffness"), 80.0f, 250.0f, 160.0f) };
    const ModSlider::Ptr m_recoil_spring_damping{ ModSlider::create(generate_name("RecoilSpringDamping"), 12.0f, 35.0f, 22.0f) };
    const ModSlider::Ptr m_recoil_sustained_damping{ ModSlider::create(generate_name("RecoilSustainedDamping"), 24.0f, 45.0f, 32.0f) };
    const ModSlider::Ptr m_recoil_sustained_window{ ModSlider::create(generate_name("RecoilSustainedWindow"), 0.06f, 0.25f, 0.12f) };

    ValueList m_options {
        *m_hide_upper_body,
        *m_hide_lower_body,
        *m_hide_arms,
        *m_hide_upper_body_cutscenes,
        *m_hide_lower_body_cutscenes,
        *m_recoil_enabled,
        *m_recoil_intensity,
        *m_recoil_attack_duration,
        *m_recoil_spring_stiffness,
        *m_recoil_spring_damping,
        *m_recoil_sustained_damping,
        *m_recoil_sustained_window
    };

    enum PlayerType {
        ETHAN = 0,
        CHRIS_MERC = 1,
    };

    PlayerType m_player_type{PlayerType::ETHAN};

    union {
        ::REGameObject* m_player{nullptr};
        ::REManagedObject* m_player_downcast;
    };

    ::RETransform* m_transform{nullptr};

    ::REManagedObject* m_inventory{nullptr};
    ::REManagedObject* m_updater{nullptr};
    ::REManagedObject* m_weapon{nullptr};
    ::REManagedObject* m_hand_touch{nullptr};
    ::REManagedObject* m_order{nullptr};
    ::REManagedObject* m_status{nullptr};
    ::REManagedObject* m_event_action_controller{nullptr};
    ::REManagedObject* m_game_event_action_controller{nullptr};
    ::REManagedObject* m_hit_controller{nullptr};

    ::REManagedObject* m_left_hand_ik{nullptr};
    ::REManagedObject* m_right_hand_ik{nullptr};
    ::RETransform* m_left_hand_ik_transform{nullptr};
    ::RETransform* m_right_hand_ik_transform{nullptr};
    ::REManagedObject* m_left_hand_ik_object{nullptr};
    ::REManagedObject* m_right_hand_ik_object{nullptr};

    Vector4f m_left_hand_position_offset{};
    Vector4f m_right_hand_position_offset{};
    glm::quat m_left_hand_rotation_offset{};
    glm::quat m_right_hand_rotation_offset{};

    Vector4f m_last_right_hand_position{};
    glm::quat m_last_right_hand_rotation{};
    Vector4f m_last_left_hand_position{};
    glm::quat m_last_left_hand_rotation{};

    bool m_was_gripping_weapon{false};
    bool m_is_holding_left_grip{false};
    bool m_is_in_cutscene{false};
    bool m_is_grapple_aim{false};
    bool m_is_reloading{false};
    bool m_can_use_hands{true};
    bool m_is_arm_jacked{false};
    bool m_is_motion_play{false};
    bool m_has_vehicle{false};
    bool m_in_re8_end_game_event{false};

    bool m_wants_block{false};
    bool m_wants_heal{false};

    Vector3f m_hmd_delta_to_left{};
    Vector3f m_hmd_delta_to_right{};

    Vector3f m_hmd_dir_to_left{};
    Vector3f m_hmd_dir_to_right{};

    struct RecoilState {
        float spring_pos_y{0.0f};
        float spring_pos_z{0.0f};
        float spring_vel_y{0.0f};
        float spring_vel_z{0.0f};
        float spring_pitch{0.0f};
        float spring_vel_pitch{0.0f};
        float spring_yaw{0.0f};
        float spring_vel_yaw{0.0f};
    };
    RecoilState m_recoil{};

    bool m_recoil_attack_active{false};
    float m_recoil_attack_t{0.0f};
    float m_recoil_attack_pos_y{0.0f};
    float m_recoil_attack_pos_z{0.0f};
    float m_recoil_attack_pitch{0.0f};
    float m_recoil_attack_yaw{0.0f};
    float m_recoil_last_shot_t{0.0f};
    float m_recoil_last_t{0.0f};
    bool m_recoil_active{false};

    static constexpr float RECOIL_ATTACK_DURATION = 0.008f;
    static constexpr float RECOIL_SPRING_STIFFNESS = 160.0f;
    static constexpr float RECOIL_SPRING_DAMPING = 22.0f;
    static constexpr float RECOIL_SUSTAINED_DAMPING = 32.0f;
    static constexpr float RECOIL_SUSTAINED_WINDOW = 0.12f;
    static constexpr float RECOIL_SUBSTEP_DT = 0.008f;
    static constexpr float RECOIL_POSITION_INTENSITY = 0.012f;
    static constexpr float RECOIL_ROTATION_INTENSITY = 0.085f;
    static constexpr float RECOIL_HORIZONTAL_SPREAD = 0.015f;
    static constexpr float RECOIL_VERTICAL_SPREAD = 0.010f;
    static constexpr float RECOIL_RANDOMNESS = 0.35f;
    static constexpr float RECOIL_MULT_EXPONENT = 0.35f;
    static constexpr float RECOIL_STACK_CAP = 2.0f;

    Vector3f m_last_shoot_pos{};
    Vector3f m_last_shoot_dir{};
    Vector3f m_last_muzzle_pos{};
    Vector3f m_last_muzzle_forward{};

    float m_delta_time{1.0f/60.0f};
    float m_movement_speed_rate{0.0f};

    struct HealGesture {
        bool was_grip_down{false};
        bool was_trigger_down{false};
        ::REManagedObject* last_grip_weapon{nullptr};
        std::chrono::steady_clock::time_point last_grab_time{};

        glm::quat re8_medicine_rotation{0.728f, 0.409f, 0.222f, 0.504f};
        glm::quat re8_syringe_rotation{-0.375f, -0.322f, 0.515f, -0.701f};
    };

    HealGesture m_heal_gesture;

    struct CameraData {
        bool last_hmd_active_state{false};
        bool was_vert_limited{false};
        bool last_cutscene_state{false};
        float last_gui_dot{0.0f};
        glm::quat last_gui_quat{glm::identity<glm::quat>()};
        std::chrono::steady_clock::time_point last_time_not_maximum_controllable{};
        std::chrono::steady_clock::time_point last_gui_forced_slerp{};
    };

    CameraData m_camera_data;

    static inline std::unordered_set<std::string> s_re8_end_game_events {
        "c32e390_01",
        "c32e390_02",
        "c32e390_03",
        "c32e390_04",
        "c32e390_05",
        "c32e390_06",
        "c32e390_07",
        "c32e390_08",
        "c32e390_09",
    };

    static constexpr inline auto GUI_MAX_SLERP_TIME = 1.5f;
};
#endif