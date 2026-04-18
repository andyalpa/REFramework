// Even though this is called RE8VR, it also affects RE7.
#pragma once

#if defined(RE7) || defined(RE8)
#include <array>
#include <chrono>
#include <string>
#include <tuple>
#include <unordered_map>
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
    /// Use when called from shoot hook so the multiplier is looked up from the actual shooting weapon.
    void apply_recoil_kickback(::REManagedObject* weapon_for_id);
    /// Cancel all recoil state (e.g. when shoot() did not actually fire because of no ammo).
    void cancel_recoil_state();
    void update_recoil(float dt);
    Vector3f get_recoil_position_offset_world(const glm::quat& camera_rotation) const;
    glm::quat get_recoil_rotation_offset_world(const glm::quat& camera_rotation) const;

    // Lua UI helpers (scripts/recoil_config.lua expects these).
    bool get_recoil_enabled() const;
    void set_recoil_enabled(bool enabled);
    float get_recoil_intensity() const;
    void set_recoil_intensity(float intensity);
    float get_recoil_attack_duration() const;
    void set_recoil_attack_duration(float v);
    float get_recoil_spring_stiffness() const;
    void set_recoil_spring_stiffness(float v);
    float get_recoil_spring_damping() const;
    void set_recoil_spring_damping(float v);
    float get_recoil_sustained_damping() const;
    void set_recoil_sustained_damping(float v);
    float get_recoil_sustained_window() const;
    void set_recoil_sustained_window(float v);

    /// Global intensity is multiplied by per-weapon intensity. Returns effective multiplier for current weapon (uses m_weapon).
    float get_weapon_recoil_multiplier() const;
    /// Look up per-weapon intensity for a specific weapon (e.g. the one firing).
    float get_weapon_recoil_multiplier(::REManagedObject* weapon) const;
    float get_weapon_recoil_multiplier(::REManagedObject* weapon, bool is_two_hands) const;

    /// Weapon ID mapping: stable string from weapon object's type (e.g. "app.WeaponGunLemi"). Used for per-weapon recoil config.
    std::string get_weapon_recoil_id(::REManagedObject* weapon) const;
    /// Current equipped weapon's recoil ID; empty if none.
    std::string get_current_weapon_recoil_id() const;
    // Per-weapon recoil profiles (persisted to recoil_settings.json as { weapons: { id: { one_hand, two_hands } } } )
    void set_per_weapon_recoil_one_hand(const std::string& weapon_id, float intensity);
    float get_per_weapon_recoil_one_hand(const std::string& weapon_id) const;
    void set_per_weapon_recoil_two_hands(const std::string& weapon_id, float intensity);
    float get_per_weapon_recoil_two_hands(const std::string& weapon_id) const;

    // Back-compat: treats "intensity" as two-hands value.
    void set_per_weapon_recoil_intensity(const std::string& weapon_id, float intensity);
    float get_per_weapon_recoil_intensity(const std::string& weapon_id) const;
    void load_recoil_settings();
    void save_recoil_settings();

    ::REManagedObject* get_localplayer() const;
    ::REManagedObject* get_weapon_object(::REGameObject* player) const;

    bool update_pointers();
    bool update_ik_pointers();

    // Holster persistence + HMD-space tuning (Lua / JSON). Slot indices 0–6 match HolsterSlot.
    void set_holster_assignment(int slot, const std::string& weapon_id);
    void set_holster_slot_persist(int slot, const std::string& weapon_id, const std::string& type_id);
    void clear_holster_assignment(int slot);
    std::string get_holster_assignment(int slot) const;
    std::string get_holster_type_assignment(int slot) const;
    void set_holster_slot_offset(int slot, float right_m, float up_m, float forward_m);
    void set_holster_tune_radii(float weapon_hover_m, float heal_inner_m, float heal_blocks_weapon_m);
    std::tuple<float, float, float> get_holster_slot_hmd_offset_m(int slot) const;

private:
    /// Returns current ammo count, or -1 if unknown (no recoil cancel). Used to skip recoil when weapon has no ammo.
    int32_t get_weapon_ammo_count(::REManagedObject* weapon) const;

    void update_block_gesture();
    void update_heal_gesture();
    void update_weapon_holster_gesture();
    void holster_clear_weapon_from_other_slots(const std::string& instance_key, const std::string& type_id, int except_slot);

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
    /// Global recoil intensity. Range 1–4; per-weapon intensity multiplies this (see per-weapon recoil section).
    const ModSlider::Ptr m_recoil_intensity{ ModSlider::create(generate_name("RecoilIntensity"), 1.0f, 4.0f, 1.0f) };
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

    enum class HolsterSlot : int {
        LeftShoulder = 0,
        LeftChest = 1,
        RightChest = 2, // Not used as a weapon holster (overlaps medicine volume); native index kept for array layout.
        LeftWaist = 3,
        RightWaist = 4,
        RightShoulder = 5,
        RightShoulderHeal = 6,
    };

    struct WeaponHolsterState {
        std::array<std::string, 7> slot_weapon_id{};
        std::array<std::string, 7> slot_weapon_json_label{};
        std::array<std::string, 7> slot_weapon_type_id{};
        HolsterSlot hovered_slot{HolsterSlot::LeftShoulder};
        bool has_hovered_slot{false};
        bool was_grip_down{false};
        std::chrono::steady_clock::time_point last_haptic_time{};
        bool holster_grip_ever_outside_slots{false};
        /// Previous frame left grip (for release edge); live holster offset drag uses left grip while right hand hovers slot.
        bool was_left_grip_for_holster_tune{false};
        int holster_tune_drag_slot{-1};
        std::chrono::steady_clock::time_point last_holster_tune_mode_haptic_time{};
    };

    Vector3f compute_holster_slot_world_position(HolsterSlot slot, const Vector3f& head_pos, const Vector3f& head_right,
        const Vector3f& head_up, const Vector3f& head_forward) const;
    Vector3f compute_holster_slot_base_world_position(HolsterSlot slot, const Vector3f& head_pos, const Vector3f& head_right,
        const Vector3f& head_up, const Vector3f& head_forward) const;

    WeaponHolsterState m_weapon_holster{};
    std::array<Vector3f, 7> m_holster_hmd_offset_m{};
    float m_holster_weapon_hover_m{0.24f};
    float m_holster_heal_inner_m{0.16f};
    float m_holster_heal_blocks_weapon_m{0.17f};
    int m_holster_assignment_nonce{0};
    int m_holster_last_assigned_slot{-1};
    std::string m_holster_last_assigned_weapon_id{};
    int m_holster_tune_nonce{0};
    int m_holster_tune_last_slot{-1};

#ifdef RE7
    // RE7: short lockout to prevent native quickslot/dpad from immediately re-equipping
    // a weapon right after we stow it into a holster slot.
    std::chrono::steady_clock::time_point m_re7_stow_lockout_until{};
#endif

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
        bool raw_was_grip_down{false};
        bool heal_grip_began_inside_slot{false};
        ::REManagedObject* cached_medicine_item{nullptr};
        ::REGameObject* cached_medicine_owner{nullptr};
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

    struct PerWeaponRecoilProfile {
        float one_hand{1.0f};
        float two_hands{1.0f};
    };

    /// Per-weapon recoil intensity multipliers (key = weapon recoil id). Default 1.0 if not present. Persisted via recoil_settings.json.
    std::unordered_map<std::string, PerWeaponRecoilProfile> m_per_weapon_recoil{};
    static constexpr const char* RECOIL_SETTINGS_FILENAME = "recoil_settings.json";
};
#endif
