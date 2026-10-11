#include <cstdio>
#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

using namespace godot;

extern "C" {
#include "../src/gamestate.h"
#include "../src/quest.h"
#include "../src/notify.h"
#include "../src/stats.h"
#include "../src/skills.h"
#include "../src/items.h"
#include "../src/inventory.h"
#include "../src/equipment.h"
#include "../src/party.h"
#include "../src/characters.h"
#include "../src/enemy_registry.h"
#include "../src/bond.h"
#include "../src/combat.h"
#include "../src/time_system.h"
#include "../src/dialogue.h"
#include "../src/map.h"
#include "../src/entity.h"
#include "../src/dungeon.h"
#include "../src/dungeon_db.h"
#include "../src/social_stats.h"
#include "../src/activities.h"
#include "../src/world.h"
#include "../src/summon.h"
#include "../src/summon_menu.h"
#include "../src/events.h"
#include "../src/npc.h"
#include "../src/shop.h"
#include "../src/save.h"
// Global enemy registry used by dungeon builders — defined in enemy_registry.c
extern EnemyRegistry g_enemy_registry;
// Global summon system — defined in summon.c
extern SummonDB         g_summon_db;
extern SummonStock      g_summon_stock;
extern SummonCompendium g_summon_comp;
// g_summon_menu is defined in main.c (which we don't compile), so define it
// here. The summon_menu_init() call will set its initial state.
SummonMenu g_summon_menu;
}

class OpalGame : public Object {
    GDCLASS(OpalGame, Object)
    static OpalGame* singleton;
    int get_save_slot_count() const {
    return SAVE_SLOTS;
    }
    bool can_save_now() {
    if (!initialized) return false;
    // Always allowed outside dungeon
    if (!is_in_dungeon()) return true;
    // In dungeon: only safe rooms
    DungeonRoom* r = dungeon_current_room(&ds_sys);
    return r && r->type == ROOM_SAFE;
    }

    // Core systems
    Party        party;
    SkillDB      skilldb;
    ItemDB       itemdb;
    EquipDB      equipdb;
    Inventory    inventory;
    GameState    gs;
    NotifySystem notify;
    BondDB  bonds;
    CombatSystem cs;
    EnemyRegistry enemy_reg;
    NPCRegistry npc_reg;
    QuestRegistry quest_reg;
    // Dungeon-related
    DungeonDB     dungeon_db;
    TimeSystem   time_sys;
    DialogueSystem dialogue;
    DungeonSystem ds_sys;
    // Overworld
    WorldSystem  world_sys;
    SocialStats  social;
    ActivityDB   activity_db;
    ActivityMenu activity_menu;
    EventSystem  event_system;
    Shop         default_shop;
    ShopMenu     shop_menu;
    // Player position in the dungeon (continuous coords)
    float player_x = 0.0f;
    float player_y = 0.0f;
    int   player_w = 24;
    int   player_h = 24;
    // Persistent SDL scancode array — dungeon_move reads from this each frame.
    // Godot sets/clears entries via set_key_down().
    Uint8 key_state[512] = {0};

    bool initialized = false;

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("ping"),              &OpalGame::ping);
        ClassDB::bind_method(D_METHOD("get_version"),       &OpalGame::get_version);
        ClassDB::bind_method(D_METHOD("init_systems"),      &OpalGame::init_systems);
        // Combat
        ClassDB::bind_method(D_METHOD("start_test_battle"), &OpalGame::start_test_battle);
        ClassDB::bind_method(D_METHOD("get_combat_state"),  &OpalGame::get_combat_state);
        ClassDB::bind_method(D_METHOD("send_key", "keycode"), &OpalGame::send_key);
        ClassDB::bind_method(D_METHOD("is_combat_active"),  &OpalGame::is_combat_active);
        ClassDB::bind_method(D_METHOD("combat_won"),        &OpalGame::combat_won);
        ClassDB::bind_method(D_METHOD("update"),            &OpalGame::update);
        // Dungeon
        ClassDB::bind_method(D_METHOD("enter_dungeon", "dungeon_idx"), &OpalGame::enter_dungeon);
        ClassDB::bind_method(D_METHOD("exit_dungeon"),        &OpalGame::exit_dungeon);
        ClassDB::bind_method(D_METHOD("poll_dungeon_cleared_exit"), &OpalGame::poll_dungeon_cleared_exit);
        ClassDB::bind_method(D_METHOD("is_in_dungeon"),      &OpalGame::is_in_dungeon);
        ClassDB::bind_method(D_METHOD("get_dungeon_state"),  &OpalGame::get_dungeon_state);
        ClassDB::bind_method(D_METHOD("get_current_room_tiles"), &OpalGame::get_current_room_tiles);
        ClassDB::bind_method(D_METHOD("dungeon_send_key", "keycode"), &OpalGame::dungeon_send_key);
        ClassDB::bind_method(D_METHOD("set_key_down", "godot_key", "is_down"), &OpalGame::set_key_down);
        ClassDB::bind_method(D_METHOD("get_dungeon_count"),   &OpalGame::get_dungeon_count);
        ClassDB::bind_method(D_METHOD("notify_combat_end", "player_won"), &OpalGame::notify_combat_end);
        ClassDB::bind_method(D_METHOD("cycle_summon", "direction"), &OpalGame::cycle_summon);
        ClassDB::bind_method(D_METHOD("dungeon_set_player_and_update", "px", "py"), &OpalGame::dungeon_set_player_and_update);
        ClassDB::bind_method(D_METHOD("dungeon_tick", "px", "py"), &OpalGame::dungeon_tick);
        ClassDB::bind_method(D_METHOD("dungeon_use_exit", "dir"), &OpalGame::dungeon_use_exit);
        ClassDB::bind_method(D_METHOD("dungeon_start_encounter", "entity_index"), &OpalGame::dungeon_start_encounter);
        // generic flag + grant bridge (chests, triggers, dungeon clears)
        ClassDB::bind_method(D_METHOD("set_flag", "name"),   &OpalGame::set_flag_bridge);
        ClassDB::bind_method(D_METHOD("get_flag", "name"),   &OpalGame::get_flag_bridge);
        ClassDB::bind_method(D_METHOD("give_item", "item_id", "qty"), &OpalGame::give_item_bridge);
        // Overworld
        ClassDB::bind_method(D_METHOD("get_overworld_state"), &OpalGame::get_overworld_state);
        ClassDB::bind_method(D_METHOD("get_available_activities"), &OpalGame::get_available_activities);
        ClassDB::bind_method(D_METHOD("do_activity", "activity_idx"), &OpalGame::do_activity);
        ClassDB::bind_method(D_METHOD("sleep_to_next_day"), &OpalGame::sleep_to_next_day);
        ClassDB::bind_method(D_METHOD("advance_time"), &OpalGame::advance_time);
        // Shop
        ClassDB::bind_method(D_METHOD("get_shop_state"), &OpalGame::get_shop_state);
        ClassDB::bind_method(D_METHOD("buy_item", "item_idx", "quantity"), &OpalGame::buy_item);
        // Inventory / items
	    ClassDB::bind_method(D_METHOD("get_inventory"),
		    &OpalGame::get_inventory);
	    ClassDB::bind_method(D_METHOD("get_item_db"),
		    &OpalGame::get_item_db);
	    ClassDB::bind_method(D_METHOD("get_yen"),
		    &OpalGame::get_yen);
	    ClassDB::bind_method(D_METHOD("use_item", "item_id", "member_idx"),
		    &OpalGame::use_item);
        // Bonds
        ClassDB::bind_method(D_METHOD("get_bonds"),    &OpalGame::get_bonds);
        ClassDB::bind_method(D_METHOD("hang_out", "idx"),   &OpalGame::hang_out);
        ClassDB::bind_method(D_METHOD("tick_quests"), &OpalGame::tick_quests);
        ClassDB::bind_method(D_METHOD("get_quests"), &OpalGame::get_quests);
        ClassDB::bind_method(D_METHOD("debug_dialogue_state"), &OpalGame::debug_dialogue_state);
        // Save / load
        ClassDB::bind_method(D_METHOD("save_to_slot", "slot"), &OpalGame::save_to_slot);
        ClassDB::bind_method(D_METHOD("load_from_slot", "slot"), &OpalGame::load_from_slot);
        ClassDB::bind_method(D_METHOD("get_save_slot_info", "slot"), &OpalGame::get_save_slot_info);
        ClassDB::bind_method(D_METHOD("delete_save_slot", "slot"), &OpalGame::delete_save_slot);
        ClassDB::bind_method(D_METHOD("set_save_directory", "path"), &OpalGame::set_save_directory_bridge);
        ClassDB::bind_method(D_METHOD("get_save_slot_count"), &OpalGame::get_save_slot_count);
        ClassDB::bind_method(D_METHOD("can_save_now"), &OpalGame::can_save_now);
        // Equipment & status
        ClassDB::bind_method(D_METHOD("get_party_status"), &OpalGame::get_party_status);
        ClassDB::bind_method(D_METHOD("get_equipment_db"), &OpalGame::get_equipment_db);
        ClassDB::bind_method(D_METHOD("equip_piece", "member_idx", "piece_id"), &OpalGame::equip_piece);
        ClassDB::bind_method(D_METHOD("unequip_slot", "member_idx", "slot"), &OpalGame::unequip_slot);
        ClassDB::bind_method(D_METHOD("poll_save_load_request"),  &OpalGame::poll_save_load_request);
        // Summon menu
        ClassDB::bind_method(D_METHOD("summon_menu_open"),        &OpalGame::summon_menu_open_bridge);
        ClassDB::bind_method(D_METHOD("summon_menu_close"),       &OpalGame::summon_menu_close_bridge);
        ClassDB::bind_method(D_METHOD("summon_menu_send_key", "key"), &OpalGame::summon_menu_send_key);
        ClassDB::bind_method(D_METHOD("get_summon_menu_state"),   &OpalGame::get_summon_menu_state);
        // Map
        ClassDB::bind_method(D_METHOD("get_floor_map"), &OpalGame::get_floor_map);
        // Notifications
        ClassDB::bind_method(D_METHOD("get_notifications"), &OpalGame::get_notifications);
        ClassDB::bind_method(D_METHOD("tick_notifications"), &OpalGame::tick_notifications);
        ClassDB::bind_method(D_METHOD("tick_events"), &OpalGame::tick_events);
        ClassDB::bind_method(D_METHOD("get_dungeons"), &OpalGame::get_dungeons);
        // Dialogue
        ClassDB::bind_method(D_METHOD("is_dialogue_active"),  &OpalGame::is_dialogue_active);
        ClassDB::bind_method(D_METHOD("get_dialogue_state"),  &OpalGame::get_dialogue_state);
        ClassDB::bind_method(D_METHOD("dialogue_choose", "choice"), &OpalGame::dialogue_choose);
        ClassDB::bind_method(D_METHOD("dialogue_start_node", "node_id"), &OpalGame::dialogue_start_node);
        ClassDB::bind_method(D_METHOD("dialogue_start_convo", "key"), &OpalGame::dialogue_start_convo);
        ClassDB::bind_method(D_METHOD("get_npcs"), &OpalGame::get_npcs);
        ClassDB::bind_method(D_METHOD("npc_interact_id", "id"), &OpalGame::npc_interact_id);
        // Hand-off
        ClassDB::bind_method(D_METHOD("handoff_available", "from_idx"), &OpalGame::handoff_available);
        ClassDB::bind_method(D_METHOD("hand_off", "from_idx", "to_idx"), &OpalGame::hand_off);
    }

public:
    static OpalGame* get_singleton() { return singleton; }
    OpalGame()  { if (!singleton) singleton = this; }
    ~OpalGame() { if (singleton == this) singleton = nullptr; }

    String ping()        { return String("Opal C bridge is working!"); }
    String get_version() { return String("Opal v0.1 — Godot 4 port"); }

    // ── Init ──────────────────────────────────────────────────────────────────
    void init_systems() {
        setbuf(stderr, NULL);  // disable stderr buffering for debug prints

        // ── Core DBs ──────────────────────────────────────────────────────────
        skilldb_init(&skilldb);
        itemdb_init(&itemdb);
        equipdb_init(&equipdb);
        inventory_init(&inventory);
        gamestate_init(&gs);
        notify_init(&notify);
        bonddb_init(&bonds);

        // ── Summons — MUST be initialized before combat ──────────────────────
        // The combat code reads g_summon_stock for Prota's summon-based
        // damage and switching. Without these, summon reads hit garbage.
        summondb_init(&g_summon_db);
        summon_stock_init(&g_summon_stock, &g_summon_db);
        memset(&g_summon_comp, 0, sizeof(SummonCompendium));
        summon_menu_init(&g_summon_menu);
        summon_register(&g_summon_comp, summon_active(&g_summon_stock));

        // ── Party ─────────────────────────────────────────────────────────────
        build_starter_party(&party);
        // Apply Prota's active summon to his stats
        if (party.count > 0)
            summon_apply_to_statblock(&g_summon_stock, &party.members[0]);

        // ── Enemy registries (local + global used by dungeon builders) ─────────
        enemy_registry_init(&enemy_reg);
        enemy_registry_init(&g_enemy_registry);

        // ── Combat ────────────────────────────────────────────────────────────
        combat_init(&cs, &party, &skilldb, &gs, &inventory, &itemdb,
                    &notify, &bonds);

        // ── Time and dungeon ──────────────────────────────────────────────────
        timesystem_init(&time_sys);
        dialogue_init(&dialogue, &gs);
        npc_registry_init(&npc_reg);
        quest_registry_init(&quest_reg);
        dungeon_db_init(&dungeon_db);
        dungeon_init(&ds_sys, &party, &inventory, &gs, &time_sys, &notify,
                     &skilldb, &itemdb, &equipdb, &dialogue, &cs, &dungeon_db);

        // ── Overworld ─────────────────────────────────────────────────────────
        worldsystem_init(&world_sys);
        social_stats_init(&social);
        activitydb_init(&activity_db);
        activitymenu_init(&activity_menu);

        // ── Event system ──────────────────────────────────────────────────────
        eventsystem_init(&event_system);

        // ── Shop ──────────────────────────────────────────────────────────────
        shop_init(&default_shop, "Takemi's Clinic");
        shop_add_item(&default_shop, 0,  -1); // Medicine
        shop_add_item(&default_shop, 1,  -1); // Medicine DX
        shop_add_item(&default_shop, 8,  -1); // Takemedic-All V
        shop_add_item(&default_shop, 10, -1); // Snuff Soul
        shop_add_item(&default_shop, 2,  -1); // Bead
        shop_add_item(&default_shop, 11, -1); // Snuff Soul DX
        shop_add_item(&default_shop, 4,  -1); // Amrita Drop
        shop_add_item(&default_shop, 6,  -1); // Soma
        shopmenu_init(&shop_menu);
        shop_menu.bonds = &bonds;

        initialized = true;
        UtilityFunctions::print("Opal: systems initialised");
    }

    // ══ COMBAT API ════════════════════════════════════════════════════════════

    void start_test_battle() {
        if (!initialized) init_systems();

        EnemyTemplate t1 = enemy_registry_get(&enemy_reg, ENEMY_SHADOW_T1);
        EnemyTemplate t2 = enemy_registry_get(&enemy_reg, ENEMY_SHADOW_T2);

        CombatEnemy enemies[2];
        memset(&enemies[0], 0, sizeof(CombatEnemy));
        memset(&enemies[1], 0, sizeof(CombatEnemy));

        strncpy(enemies[0].stats.name, t1.name, 63);
        enemies[0].stats.hp = t1.hp; enemies[0].stats.max_hp = t1.hp;
        enemies[0].stats.sp = t1.sp; enemies[0].stats.max_sp = t1.sp;
        enemies[0].stats.st = t1.st; enemies[0].stats.ma = t1.ma;
        enemies[0].stats.en = t1.en; enemies[0].stats.ag = t1.ag;
        enemies[0].stats.alive = 1;
        enemies[0].exp_reward  = t1.exp_reward;
        for (int i = 0; i < 10; i++)
            enemies[0].stats.affinities[i] = (Affinity)t1.affinities[i];

        strncpy(enemies[1].stats.name, t2.name, 63);
        enemies[1].stats.hp = t2.hp; enemies[1].stats.max_hp = t2.hp;
        enemies[1].stats.sp = t2.sp; enemies[1].stats.max_sp = t2.sp;
        enemies[1].stats.st = t2.st; enemies[1].stats.ma = t2.ma;
        enemies[1].stats.en = t2.en; enemies[1].stats.ag = t2.ag;
        enemies[1].stats.alive = 1;
        enemies[1].exp_reward  = t2.exp_reward;
        for (int i = 0; i < 10; i++)
            enemies[1].stats.affinities[i] = (Affinity)t2.affinities[i];

        inventory_add(&inventory, 0, 3);
        inventory_add(&inventory, 1, 1);
        inventory_add(&inventory, 6, 1);

        combat_start(&cs, enemies, 2, "test_battle_won");
        UtilityFunctions::print("Opal: battle started");
    }

    Dictionary get_combat_state() {
        Dictionary d;
        if (!initialized) return d;

        d["active"]      = (bool)cs.active;
        d["state"]       = (int)cs.state;
        d["turn_index"]  = cs.turn_index;
        d["menu_sel"]    = cs.menu_selected;
        d["team_attack"]     = (bool)cs.team_attack_available;
        d["just_ended"]  = (bool)cs.just_ended;
        d["won"]         = (bool)cs.won_last_battle;

        // Defensive: compute current actor index with full bounds checking.
        // turn_order has PARTY_MAX + MAX_ENEMIES entries. Party actors are
        // indices [0, PARTY_MAX). Enemy actors are [PARTY_MAX, PARTY_MAX+MAX_ENEMIES).
        // This single value gets reused in several blocks below, so calculate
        // it once and guard everything that reads cs.party->members[].
        const int TURN_MAX = PARTY_MAX + MAX_ENEMIES;
        int actor_idx = -1;
        if (cs.turn_index >= 0 && cs.turn_index < TURN_MAX) {
            actor_idx = cs.turn_order[cs.turn_index];
            if (actor_idx < 0 || actor_idx >= TURN_MAX) actor_idx = -1;
        }
        bool actor_is_party = (actor_idx >= 0 && actor_idx < PARTY_MAX
                               && cs.party && actor_idx < cs.party->count);

        // extra_turn lives on the actor's StatBlock, not CombatSystem
        bool actor_extra_turn = false;
        if (actor_is_party)
            actor_extra_turn = (bool)cs.party->members[actor_idx].extra_turn;
        d["extra_turn"] = actor_extra_turn;

        // Resolved actor party slot (for the HUD to highlight the right member).
        // turn_index is a position in turn_order; this is the actual party index.
        d["active_party_idx"] = actor_is_party ? actor_idx : -1;

        // ── Hand-off state ──────────────────────────────────────────────────
        // Per-member handoff level (0/1/2/3) and whether the current actor can
        // hand-off (must have Extra Turn).
        Array handoff_levels;
        for (int i = 0; i < cs.party->count; i++) {
            handoff_levels.append(cs.party->members[i].handoff_level);
        }
        d["handoff_levels"]    = handoff_levels;
        d["handoff_available"] = actor_is_party && cs.party->members[actor_idx].extra_turn;

        const char* state_names[] = {
            "inactive", "player_turn", "skill_menu", "item_menu",
            "target_select", "enemy_turn", "team_attack", "win",
            "lose", "flee", "skill_learn", "negotiate", "summon_menu"
        };
        int sn_idx = (int)cs.state;
        d["state_name"] = (sn_idx >= 0 && sn_idx <= 12)
            ? String(state_names[sn_idx])
            : String("unknown");

        Array skill_names, skill_sp_costs, skill_elements, skill_descriptions, skill_can_afford, skill_targets;
        if ((cs.state == COMBAT_SKILL_MENU || cs.state == COMBAT_PLAYER_TURN) && actor_is_party) {
            StatBlock* actor = &cs.party->members[actor_idx];
            for (int i = 0; i < actor->skill_count; i++) {
                Skill* sk = skilldb_get(cs.skilldb, actor->skill_ids[i]);
                if (sk) {
                    skill_names.append(String(sk->name));
                    skill_sp_costs.append(sk->sp_cost);
                    skill_elements.append(String(element_name(sk->element)));
                    skill_descriptions.append(String(sk->description));
                    skill_can_afford.append((bool)(actor->sp >= sk->sp_cost));
                    skill_targets.append((int)sk->target);
                } else {
                    skill_names.append(String("???"));
                    skill_sp_costs.append(0);
                    skill_elements.append(String("?"));
                    skill_descriptions.append(String(""));
                    skill_can_afford.append(false);
                    skill_targets.append(0);
                }
            }
        }
        d["skill_names"]        = skill_names;
        d["skill_sp_costs"]     = skill_sp_costs;
        d["skill_elements"]     = skill_elements;
        d["skill_descriptions"] = skill_descriptions;
        d["skill_can_afford"]   = skill_can_afford;
        d["skill_targets"]      = skill_targets;
        d["skill_sel"]          = cs.skill_selected;
        d["target_sel"]         = cs.target_selected;
        d["summon_sel"]        = cs.summon_selected;
        d["ally_target_sel"]    = cs.ally_target_selected;

        // ── Negotiation data (only meaningful in COMBAT_NEGOTIATE state) ──────
        d["neg_phase"]       = cs.neg_phase;
        d["neg_enemy_idx"]   = cs.neg_enemy_idx;
        d["neg_selected"]    = cs.neg_selected;
        d["neg_result"]      = cs.neg_result;
        d["neg_demand_type"] = cs.neg_demand_type;
        d["neg_demand_amount"] = cs.neg_demand_amount;
        d["neg_demand_item"] = cs.neg_demand_item;

        // Personality of the negotiation target (matches C get_personality logic)
        int neg_personality = 0;
        int neg_hp_pct = 100;
        String neg_enemy_name = String("");
        if (cs.state == COMBAT_NEGOTIATE &&
            cs.neg_enemy_idx >= 0 && cs.neg_enemy_idx < cs.enemy_count) {
            StatBlock* sb = &cs.enemies[cs.neg_enemy_idx].stats;
            int max_stat = sb->lu;
            neg_personality = 0;  // upbeat
            if (sb->st > max_stat) { max_stat = sb->st; neg_personality = 1; }  // irritable
            if (sb->en > max_stat) { max_stat = sb->en; neg_personality = 2; }  // timid
            if (sb->ma > max_stat) {                     neg_personality = 3; }  // gloomy
            neg_hp_pct = sb->max_hp > 0 ? (sb->hp * 100) / sb->max_hp : 0;
            neg_enemy_name = String(sb->name);
        }
        d["neg_personality"]    = neg_personality;
        d["neg_enemy_hp_pct"]   = neg_hp_pct;
        d["neg_enemy_name"]     = neg_enemy_name;

        // Resource info for displaying
        d["yen"]             = inventory.yen;
        d["is_boss_combat"] = ds_sys.combat_is_boss != 0;
        // Look up item name only when the demand type is actually an item
        // (type 1). Item ID 0 is a valid item — don't exclude it.
        if (cs.neg_demand_type == 1 && cs.itemdb) {
            Item* it = itemdb_get(cs.itemdb, cs.neg_demand_item);
            d["neg_demand_item_name"] = String(it ? it->name : "?");
            d["neg_demand_item_qty"]  = inventory_count(&inventory, cs.neg_demand_item);
        } else {
            d["neg_demand_item_name"] = String("");
            d["neg_demand_item_qty"]  = 0;
        }

        // ── Summon stock (Prota's summons) ──────────────────────────────────
        Array summon_names, summon_arcanas, summon_levels;
        for (int i = 0; i < g_summon_stock.count; i++) {
            Summon* p = &g_summon_stock.stock[i];
            summon_names.append(String(p->name));
            summon_arcanas.append(String(arcana_name(p->arcana)));
            summon_levels.append(p->level);
        }
        d["summon_names"]   = summon_names;
        d["summon_arcanas"] = summon_arcanas;
        d["summon_levels"]  = summon_levels;
        d["summon_active"]  = g_summon_stock.active_idx;
        d["summon_count"]   = g_summon_stock.count;

        bool targeting_ally = false;
        if (cs.state == COMBAT_ITEM_MENU && cs.inventory && cs.itemdb) {
            if (cs.item_selected >= 0 && cs.item_selected < cs.inventory->slot_count) {
                InventorySlot* slot = &cs.inventory->slots[cs.item_selected];
                Item* item = itemdb_get(cs.itemdb, slot->item_id);
                if (item && item->type != ITEM_BATTLE)
                    targeting_ally = true;
            }
        }
        if (cs.state == COMBAT_SKILL_MENU && actor_is_party) {
            StatBlock* actor = &cs.party->members[actor_idx];
            if (cs.skill_selected >= 0 && cs.skill_selected < actor->skill_count) {
                Skill* sk = skilldb_get(cs.skilldb, actor->skill_ids[cs.skill_selected]);
                if (sk && sk->target == TARGET_ONE_ALLY)
                    targeting_ally = true;
            }
        }
        d["targeting_ally"] = targeting_ally;

        Array item_names, item_descriptions, item_quantities;
        if (cs.inventory && cs.itemdb) {
            for (int i = 0; i < cs.inventory->slot_count; i++) {
                InventorySlot* slot = &cs.inventory->slots[i];
                if (slot->quantity <= 0) continue;
                Item* it = itemdb_get(cs.itemdb, slot->item_id);
                if (it) {
                    item_names.append(String(it->name));
                    item_descriptions.append(String(it->description));
                    item_quantities.append(slot->quantity);
                }
            }
        }
        d["item_names"]        = item_names;
        d["item_descriptions"] = item_descriptions;
        d["item_quantities"]   = item_quantities;
        d["item_sel"]          = cs.item_selected;

        Array party_hp, party_sp, party_names, party_max_hp, party_max_sp, party_alive;
        for (int i = 0; i < party.count; i++) {
            party_hp.append(party.members[i].hp);
            party_sp.append(party.members[i].sp);
            party_max_hp.append(party.members[i].max_hp);
            party_max_sp.append(party.members[i].max_sp);
            party_names.append(String(party.members[i].name));
            party_alive.append((bool)party.members[i].alive);
        }
        d["party_hp"]     = party_hp;
        d["party_sp"]     = party_sp;
        d["party_max_hp"] = party_max_hp;
        d["party_max_sp"] = party_max_sp;
        d["party_names"]  = party_names;
        d["party_alive"]  = party_alive;

        Array party_affinities;
        for (int i = 0; i < party.count; i++) {
            Array per_member;
            for (int el = 0; el < 10; el++) {
                Affinity aff = statblock_get_affinity(&party.members[i], (Element)el);
                if (aff == AFF_NORMAL) continue;
                Dictionary pair;
                pair["element"]  = el;
                pair["affinity"] = (int)aff;
                per_member.append(pair);
            }
            party_affinities.append(per_member);
        }
        d["party_affinities"] = party_affinities;

        Array enemy_hp, enemy_max_hp, enemy_names, enemy_alive, enemy_knocked;
        for (int i = 0; i < cs.enemy_count; i++) {
            enemy_hp.append(cs.enemies[i].stats.hp);
            enemy_max_hp.append(cs.enemies[i].stats.max_hp > 0
                ? cs.enemies[i].stats.max_hp
                : cs.enemies[i].stats.hp + 1);
            enemy_names.append(String(cs.enemies[i].stats.name));
            enemy_alive.append((bool)cs.enemies[i].stats.alive);
            enemy_knocked.append((bool)cs.enemies[i].stats.knocked_down);
        }
        d["enemy_hp"]      = enemy_hp;
        d["enemy_max_hp"]  = enemy_max_hp;
        d["enemy_names"]   = enemy_names;
        d["enemy_alive"]   = enemy_alive;
        d["enemy_knocked"] = enemy_knocked;

        Array enemy_affinities;
        for (int i = 0; i < cs.enemy_count; i++) {
            Array per_enemy;
            for (int el = 0; el < 10; el++) {
                if (!gamestate_is_discovered(&gs, cs.enemies[i].stats.name, el))
                    continue;
                Affinity aff = statblock_get_affinity(&cs.enemies[i].stats, (Element)el);
                Dictionary pair;
                pair["element"]  = el;
                pair["affinity"] = (int)aff;
                per_enemy.append(pair);
            }
            enemy_affinities.append(per_enemy);
        }
        d["enemy_affinities"] = enemy_affinities;

        Array clog;
        int start = cs.log_count > LOG_LINES ? cs.log_count - LOG_LINES : 0;
        for (int i = start; i < cs.log_count && i < start + LOG_LINES; i++)
            clog.append(String(cs.log[i % LOG_LINES]));
        d["clog"] = clog;

        // ── Skill learn data (only meaningful in COMBAT_SKILL_LEARN state) ────
        if (cs.state == COMBAT_SKILL_LEARN &&
            cs.learn_index >= 0 && cs.learn_index < cs.learn_count) {
            PendingLearn* pl = &cs.learn_queue[cs.learn_index];
            StatBlock* m = &cs.party->members[pl->party_member_idx];
            Skill* new_sk = skilldb_get(&skilldb, pl->new_skill_id);
            d["learn_active"]        = true;
            d["learn_member_name"]   = String(m->name);
            d["learn_member_idx"]    = pl->party_member_idx;
            d["learn_new_skill_name"] = String(new_sk ? new_sk->name : "?");
            d["learn_new_skill_desc"] = String(new_sk ? new_sk->description : "");
            d["learn_new_skill_sp"]   = new_sk ? new_sk->sp_cost : 0;
            d["learn_new_skill_elem"] = String(new_sk ? element_name(new_sk->element) : "?");
            d["learn_index"]         = cs.learn_index;
            d["learn_total"]         = cs.learn_count;
            d["learn_choosing_replace"] = (bool)pl->choosing_replace;
            d["learn_replace_selected"] = pl->replace_selected;
            // Existing skill list (for replace mode)
            Array existing_names;
            int max_sk = m->skill_count;
            if (max_sk > 8) max_sk = 8;
            for (int i = 0; i < max_sk; i++) {
                Skill* sk = skilldb_get(&skilldb, m->skill_ids[i]);
                existing_names.append(String(sk ? sk->name : "?"));
            }
            d["learn_existing_skills"] = existing_names;
        } else {
            d["learn_active"] = false;
        }

        return d;
    }

    void set_flag_bridge(const String &name) {
        gamestate_set_flag(&gs, name.utf8().get_data(), 1);
    }

    bool get_flag_bridge(const String &name) {
        return gamestate_get_flag(&gs, name.utf8().get_data()) != 0;
    }

    void give_item_bridge(int item_id, int qty) {
        inventory_add(&inventory, item_id, qty);
    }

    void send_key(int godot_key) {
        if (!initialized || !cs.active) return;
        // Block negotiation (T) during boss fights — bosses can't be negotiated with
        if (godot_key == 84 && ds_sys.combat_is_boss) {
            notify_info(&notify, "You can't negotiate with a boss!");
            return;
        }
        SDL_Event ev;
        memset(&ev, 0, sizeof(SDL_Event));
        ev.type = SDL_KEYDOWN;
        switch (godot_key) {
            case 4194320: ev.key.keysym.sym = SDLK_UP;     break;
            case 4194322: ev.key.keysym.sym = SDLK_DOWN;   break;
            case 4194319: ev.key.keysym.sym = SDLK_LEFT;   break;
            case 4194321: ev.key.keysym.sym = SDLK_RIGHT;  break;
            case 69:      ev.key.keysym.sym = SDLK_e;      break;
            case 4194305: ev.key.keysym.sym = SDLK_ESCAPE; break;
            case 4194309: ev.key.keysym.sym = SDLK_RETURN; break;
            case 89:      ev.key.keysym.sym = SDLK_y;      break;
            case 49:      ev.key.keysym.sym = SDLK_1;      break;
            case 50:      ev.key.keysym.sym = SDLK_2;      break;
            case 51:      ev.key.keysym.sym = SDLK_3;      break;
            case 70:      ev.key.keysym.sym = SDLK_f;      break;
            case 84:      ev.key.keysym.sym = SDLK_t;      break;
            case 88:      ev.key.keysym.sym = SDLK_x;      break;
            default:      return;
        }
        combat_handle_input(&cs, &ev, &gs);
    }

    bool is_combat_active() { return initialized && cs.active; }
    bool combat_won()       { return initialized && cs.won_last_battle; }

    void update() {
        if (!initialized || !cs.active) return;
        combat_update(&cs, &gs);
    }

    // ══ DUNGEON API ═══════════════════════════════════════════════════════════

    int get_dungeon_count() {
        if (!initialized) return 0;
        return dungeon_db.count;
    }

    void enter_dungeon(int dungeon_idx) {
        if (!initialized) init_systems();
        if (dungeon_idx < 0 || dungeon_idx >= dungeon_db.count) return;

        // dungeon_db_get_active picks the highest-indexed dungeon whose entry
        // flag is set but clear flag is not. To control which dungeon we enter,
        // set ONLY that dungeon's entry flag and clear the rest.
        for (int i = 0; i < dungeon_db.count; i++) {
            int is_target = (i == dungeon_idx) ? 1 : 0;
            gamestate_set_flag(&gs, dungeon_db.entries[i].entry_flag, is_target);
        }

        ds_sys.dungeon_count = dungeon_db.count;

        // Only force rebuild if entering a DIFFERENT dungeon than what's loaded.
        // Re-entering the same dungeon must preserve visited rooms, opened
        // chests, defeated enemies, etc.
        if (ds_sys.current_dungeon_idx != dungeon_idx) {
            ds_sys.current_dungeon_idx = -1;  // forces rebuild inside dungeon_enter
        }

        dungeon_enter(&ds_sys, &player_x, &player_y);
        UtilityFunctions::print("Opal: entered dungeon ", dungeon_idx,
                                " (active=", (int)ds_sys.dungeon.active,
                                ", player=(", player_x, ",", player_y, "))");
    }

    void exit_dungeon() {
        if (!initialized) return;
        dungeon_exit(&ds_sys, &player_x, &player_y);
    }

    // Dungeon cleared — mirrors the old main.c per-frame check that the 3D
    // build never ran. After the boss + post-battle dialogue/cooldown, C raises
    // the "dungeon_cleared_exit" flag; this reports it once (and clears it) so the
    // frontend can leave via exit_dungeon() and return to the overworld.
    bool poll_dungeon_cleared_exit() {
        if (!initialized) return false;
        if (gamestate_get_flag(&gs, "dungeon_cleared_exit")) {
            gamestate_set_flag(&gs, "dungeon_cleared_exit", 0);
            notify_success(&notify, "Dungeon cleared! The party escapes safely.");
            return true;
        }
        return false;
    }

    // Called by Godot when combat triggered from the dungeon has finished.
    // Hands the result back to the dungeon system so it can clear in_combat,
    // mark the enemy entity as defeated (or restore on loss), etc.
    void notify_combat_end(bool player_won) {
        if (!initialized) return;
        if (!is_in_dungeon()) return;
        dungeon_on_combat_end(&ds_sys, player_won ? 1 : 0);
    }

    // Cycle Prota's active summon. direction: +1 forward, -1 backward.
    // Only meaningful during Prota's combat turn — applies the new summon's
    // affinities and skills to Prota's StatBlock immediately.
    void cycle_summon(int direction) {
        if (!initialized) return;
        if (g_summon_stock.count < 2) {
            notify_info(&notify, "No other summons to switch to.");
            return;
        }
        int n = g_summon_stock.count;
        int next = (g_summon_stock.active_idx + direction + n) % n;
        summon_switch(&g_summon_stock, next);
        // Apply to Prota's StatBlock if party is initialized
        if (party.count > 0) {
            // Save current HP/SP — apply_to_statblock only overwrites
            // affinities and skills, but be safe
            int prev_hp = party.members[0].hp;
            int prev_sp = party.members[0].sp;
            summon_apply_to_statblock(&g_summon_stock, &party.members[0]);
            party.members[0].hp = prev_hp;
            party.members[0].sp = prev_sp;
        }
        char msg[80];
        snprintf(msg, 80, "Switched to %s.",
                 g_summon_stock.stock[next].name);
        notify_success(&notify, msg);
    }

    // ══ OVERWORLD API ═════════════════════════════════════════════════════════

    Dictionary get_overworld_state() {
        Dictionary d;
        if (!initialized) return d;

        d["day"]            = time_sys.day;
        d["day_of_week"]    = (int)time_sys.day_of_week;
        d["day_name"]       = String(dayofweek_name(time_sys.day_of_week));
        d["slot"]           = (int)time_sys.slot;
        d["slot_name"]      = String(timeslot_name(time_sys.slot));
        d["weather"]        = (int)time_sys.weather;
        d["weather_name"]   = String(weather_name(time_sys.weather));
        d["month"]          = time_sys.month;
        d["date"]           = time_sys.date;
        d["is_school_day"]  = (bool)timesystem_is_school_day(&time_sys);
        d["is_night"]       = (bool)timesystem_is_last_slot(&time_sys);
        d["yen"]            = inventory.yen;

        // Social stats
        Array social_names, social_ranks, social_exp;
        for (int i = 0; i < SOCIAL_COUNT; i++) {
            social_names.append(String(social_stat_name((SocialStatType)i)));
            social_ranks.append(social.rank[i]);
            social_exp.append(social.exp[i]);
        }
        d["social_names"] = social_names;
        d["social_ranks"] = social_ranks;
        d["social_exp"]   = social_exp;

        // Hungerscape availability (dungeon 0 entry flag for now)
        d["hungerscape_unlocked"] = true;  // always unlocked in this test build

        return d;
    }

    // Returns an array of dicts: { idx, name, description, location, available, slot_name }
    Array get_available_activities() {
        Array out;
        if (!initialized) return out;
        for (int i = 0; i < activity_db.count; i++) {
            Activity* act = &activity_db.activities[i];
            Dictionary a;
            a["idx"]         = i;
            a["name"]        = String(act->name);
            a["description"] = String(act->description);
            a["location"]    = String(act->location);
            a["available"]   = (bool)activity_is_available(act, time_sys.slot, &social);
            // Build a stat-gain summary
            String gains = "";
            for (int g = 0; g < act->gain_count; g++) {
                if (g > 0) gains += ", ";
                gains += String(social_stat_name(act->raises[g])) +
                         String(" +") + String::num(act->amounts[g]);
            }
            if (act->yen_reward > 0) {
                if (gains.length() > 0) gains += ", ";
                gains += String("¥") + String::num(act->yen_reward);
            }
            if (act->restores_hp_sp) {
                if (gains.length() > 0) gains += " · ";
                gains += "Restores HP/SP";
            }
            a["effects"]     = gains;
            a["advances_day"] = (bool)act->advances_day;
            out.append(a);
        }
        return out;
    }

    // Execute an activity directly — replicates activitymenu_handle_input's
    // confirm branch without needing SDL events.
    void do_activity(int activity_idx) {
        if (!initialized) return;
        if (activity_idx < 0 || activity_idx >= activity_db.count) return;
        Activity* act = &activity_db.activities[activity_idx];
        if (!activity_is_available(act, time_sys.slot, &social)) return;

        // Stat gains
        for (int i = 0; i < act->gain_count; i++) {
            int old_rank = social.rank[act->raises[i]];
            social_stats_add_exp(&social, act->raises[i], act->amounts[i]);
            int new_rank = social.rank[act->raises[i]];
            char msg[128];
            if (new_rank > old_rank) {
                snprintf(msg, 128, "%s ranked up! (%s)",
                         social_stat_name(act->raises[i]),
                         social_rank_name(act->raises[i], new_rank));
                notify_success(&notify, msg);
            } else {
                snprintf(msg, 128, "%s +%d",
                         social_stat_name(act->raises[i]), act->amounts[i]);
                notify_info(&notify, msg);
            }
        }

        // Yen
        if (act->yen_reward > 0) {
            inventory.yen += act->yen_reward;
            char msg[64];
            snprintf(msg, 64, "Earned %d yen!", act->yen_reward);
            notify_success(&notify, msg);
        }

        // Restore
        if (act->restores_hp_sp) {
            for (int i = 0; i < party.count; i++) {
                party.members[i].hp = party.members[i].max_hp;
                party.members[i].sp = party.members[i].max_sp;
            }
            notify_success(&notify, "HP and SP fully restored!");
        }

        // Time
        if (act->advances_day)
            timesystem_sleep(&time_sys);
        else
            timesystem_advance(&time_sys);
    }

    void sleep_to_next_day() {
        if (!initialized) return;
        timesystem_sleep(&time_sys);
        // Restore party HP/SP on sleep
        for (int i = 0; i < party.count; i++) {
            party.members[i].hp = party.members[i].max_hp;
            party.members[i].sp = party.members[i].max_sp;
        }
        notify_success(&notify, "A new day begins.");
    }

    void advance_time() {
        if (!initialized) return;
        timesystem_advance(&time_sys);
    }

    // ══ SHOP API ══════════════════════════════════════════════════════════════

    Dictionary get_shop_state() {
        Dictionary d;
        if (!initialized) return d;

        d["shop_name"] = String(default_shop.name);
        d["yen"]       = inventory.yen;
        d["entry_count"] = default_shop.entry_count;

        Array names, descs, prices, stocks, types;
        for (int i = 0; i < default_shop.entry_count; i++) {
            ShopEntry* e = &default_shop.entries[i];
            Item* it = itemdb_get(&itemdb, e->item_id);
            names.append(String(it ? it->name : "?"));
            descs.append(String(it ? it->description : ""));
            int price = it ? it->price : 0;
            // Apply bond discount if any
            int discount = bond_get_shop_discount(&bonds);
            if (discount > 0 && price > 0) {
                price = price - (price * discount / 100);
                if (price < 1) price = 1;
            }
            prices.append(price);
            stocks.append(e->stock);
            types.append(it ? (int)it->type : 0);
        }
        d["item_names"]  = names;
        d["item_descs"]  = descs;
        d["item_prices"] = prices;
        d["item_stocks"] = stocks;
        d["item_types"]  = types;
        return d;
    }

    // Buy `quantity` of the item at `item_idx` in the default shop.
    // Returns 0 on success, negative on failure:
    //   -1 = invalid item, -2 = not enough yen
    int buy_item(int item_idx, int quantity) {
        if (!initialized) return -1;
        if (item_idx < 0 || item_idx >= default_shop.entry_count) return -1;
        if (quantity < 1) quantity = 1;

        ShopEntry* e = &default_shop.entries[item_idx];
        Item* it = itemdb_get(&itemdb, e->item_id);
        if (!it) return -1;

        int price = it->price;
        int discount = bond_get_shop_discount(&bonds);
        if (discount > 0) {
            price = price - (price * discount / 100);
            if (price < 1) price = 1;
        }
        int total = price * quantity;
        if (inventory.yen < total) return -2;

        inventory.yen -= total;
        inventory_add(&inventory, it->id, quantity);

        char msg[80];
        snprintf(msg, 80, "Bought %d x %s for %d yen.",
                 quantity, it->name, total);
        notify_success(&notify, msg);
        return 0;
    }

    // ══ INVENTORY / ITEM API ══════════════════════════════════════════════════
 
    // Whether an item does anything from the pause menu. Battle items are the
    // only category that requires an enemy, so they are the only exclusion.
    // This lives here rather than in GDScript because "what can this item do"
    // is game logic — same reasoning as skill_can_afford in get_combat_state.
    static bool item_usable_in_menu(const Item* it) {
    if (!it) return false;
    return it->type != ITEM_BATTLE;
    }
 
    // Stable lowercase identifiers for the ItemType enum, so the menu can branch
    // on category without hardcoding integers that would silently shift if the
    // enum ever gains a member.
    static const char* item_type_name(ItemType t) {
    switch (t) {
    case ITEM_HEALING: return "healing";
    case ITEM_SP: return "sp";
    case ITEM_REVIVAL: return "revival";
    case ITEM_STATUS_CURE: return "status_cure";
    case ITEM_BATTLE: return "battle";
    case ITEM_FULL_HEAL: return "full_heal";
    case ITEM_FULL_SP: return "full_sp";
    case ITEM_FULL_HEAL_ALL: return "full_heal_all";
    case ITEM_FULL_HEAL_SP_ALL: return "full_heal_sp_all";
    }
    return "unknown";
    }
 
    // Current yen. Also available via get_overworld_state and get_shop_state,
    // but the pause menu has no reason to pull either of those just for a number.
    int get_yen() {
    if (!initialized) return 0;
    return inventory.yen;
    }
 
    // The inventory, joined against the item DB so the caller never needs a
    // second lookup. Occupied slots only — empty slots are skipped, so the
    // array index is a display position and "slot" is the real slot number.
    //
    // Each entry:
    //   slot        int     index into inventory.slots
    //   item_id     int
    //   quantity    int
    //   name        String
    //   description String
    //   type        int     raw ItemType
    //   type_name   String  see item_type_name above
    //   power       int     HP/SP restored, or damage dealt
    //   element     String
    //   cures       int     raw StatusEffect; 0 means nothing
    //   target_all  bool    true = applies to the whole party, skip targeting
    //   price       int
    //   usable      bool    usable from the menu right now
    //   reason      String  why not, when usable is false; "" otherwise
    //
    // Per-target judgements — "already at full HP", "that ally isn't down",
    // and the "SP 44 -> 60" preview string — deliberately stay on the GDScript
    // side, where get_party_status() already supplies the target's numbers.
    Array get_inventory() {
    Array out;
    if (!initialized) return out;
    for (int i = 0; i < inventory.slot_count; i++) {
    InventorySlot* s = &inventory.slots[i];
    if (s->quantity <= 0) continue;
 
    Dictionary d;
    d["slot"] = i;
    d["item_id"] = s->item_id;
    d["quantity"] = s->quantity;
 
    Item* it = itemdb_get(&itemdb, s->item_id);
    if (!it) {
    // A slot holding an id the DB doesn't know is a data fault, not a
    // reason to drop the row — showing it is how it gets noticed.
 d["name"] = String("?");
 d["description"] = String("");
 d["type"] = -1;
 d["type_name"] = String("unknown");
 d["power"] = 0;
 d["element"] = String("");
 d["cures"] = 0;
 d["target_all"] = false;
 d["price"] = 0;
 d["usable"] = false;
 d["reason"] = String("unknown item");
 out.append(d);
 continue;
 }
 
 d["name"] = String(it->name);
 d["description"] = String(it->description);
 d["type"] = (int)it->type;
 d["type_name"] = String(item_type_name(it->type));
 d["power"] = it->power;
 d["element"] = String(element_name(it->element));
 d["cures"] = (int)it->cures;
 d["target_all"] = (bool)it->target_all;
 d["price"] = it->price;
 
 bool usable = item_usable_in_menu(it);
 d["usable"] = usable;
 d["reason"] = usable ? String("") : String("battle only");
 
 out.append(d);
 }
 return out;
 }
 
 // The full item database, mirroring get_equipment_db. The menu does not
 // strictly need this now that get_inventory is joined, but opal_pause.gd
 // already probes for it and it costs nothing to answer honestly.
 Array get_item_db() {
 Array out;
 if (!initialized) return out;
 for (int i = 0; i < itemdb.count; i++) {
 Item* it = &itemdb.items[i];
 Dictionary d;
 d["id"] = it->id;
 d["name"] = String(it->name);
 d["description"] = String(it->description);
 d["type"] = (int)it->type;
 d["type_name"] = String(item_type_name(it->type));
 d["power"] = it->power;
 d["element"] = String(element_name(it->element));
 d["cures"] = (int)it->cures;
 d["target_all"] = (bool)it->target_all;
 d["price"] = it->price;
 out.append(d);
 }
 return out;
 }
 
    // Use an item on a party member from outside combat.
 //
 // Returns, matching buy_item's convention of 0 for success:
 //    0  used
 //   -1  no such item in the database
 //   -2  none held
 //   -3  invalid party member
 //   -4  battle item — needs an enemy
 //   -5  the C side refused it (dead target for a heal, full HP, etc.)
 //
 // The decrement belongs to inventory_use_item and is NOT repeated here.
 // Calling inventory_remove alongside it was the original double-removal bug
 // that made quantities fall by two. The count is read either side purely as
 // instrumentation, so a silent no-op shows up in the log instead of looking
 // like a successful use.
 int use_item(int item_id, int member_idx) {
 if (!initialized) return -1;
 
 Item* it = itemdb_get(&itemdb, item_id);
 if (!it) return -1;
 if (inventory_count(&inventory, item_id) <= 0) return -2;
 if (member_idx < 0 || member_idx >= party.count) return -3;
 if (!item_usable_in_menu(it)) return -4;
 
 // target_all items ignore this pointer, but it must still be valid —
 // there is no guarantee the C side checks before dereferencing.
 StatBlock* target = &party.members[member_idx];
 
 int before = inventory_count(&inventory, item_id);
 int ok = inventory_use_item(&inventory, &itemdb, item_id,
 target, &party, &bonds);
 int after = inventory_count(&inventory, item_id);
 
 if (!ok) return -5;
 
 if (after == before) {
 UtilityFunctions::print(
 "Opal: use_item reported success but consumed nothing — id ",
 item_id, " (", it->name, ")");
 } else if (before - after != 1) {
 UtilityFunctions::print(
 "Opal: use_item consumed ", before - after, " of id ", item_id);
 }
 
 return 0;
 }

    // ══ BOND API ═════════════════════════════════════════════════════════

    // Returns array of dicts: { idx, name, arcana, rank, max_rank, points,
    //                          next_rank_points, available, can_hangout }
    Array get_bonds() {
        Array out;
        if (!initialized) return out;
        for (int i = 0; i < bonds.count; i++) {
            Bond* c = &bonds.bonds[i];
            if (!c->active) continue;
            Dictionary d;
            d["idx"]            = i;
            d["name"]           = String(c->name);
            d["arcana"]         = String(c->arcana);
            d["description"]    = String(c->description);
            d["rank"]           = c->current_rank;
            d["max_rank"]       = c->max_rank;
            d["points"]         = c->points;
            int next = c->current_rank < c->max_rank ?
                       c->ranks[c->current_rank].points_required : c->points;
            d["next_rank_points"] = next;
            int band_start = (c->current_rank > 0)
                             ? c->ranks[c->current_rank - 1].points_required : 0;
            d["rank_start_points"] = band_start;
            d["can_hangout"]    = (bool)bond_can_hangout(c, &social);
            // Next-rank perk description (if any)
            String next_perk = String("");
            if (c->current_rank < c->max_rank) {
                BondRank* nr = &c->ranks[c->current_rank];
                next_perk = String(nr->perk.description);
            }
            d["next_perk"] = next_perk;
            out.append(d);
        }
        return out;
    }

    // Hang out with bond at index `idx`. Advances time, adds points,
    // ranks up if threshold met.
    void hang_out(int idx) {
        if (!initialized) return;
        if (idx < 0 || idx >= bonds.count) return;
        Bond* c = &bonds.bonds[idx];
        if (!c->active) return;
        bond_hangout(c, &social, &notify, &time_sys);
    }

    // Per-frame quest check: unlock/complete flags, grant rewards.
    void tick_quests() {
        if (!initialized) return;
        quest_tick(&quest_reg, &gs, &inventory, &bonds, &notify);
    }

    // Returns array of dicts: { id, title, description, unlocked, completed }
    Array get_quests() {
        Array out;
        if (!initialized) return out;
        for (int i = 0; i < quest_reg.count; i++) {
            Quest* q = &quest_reg.quests[i];
            if (!q->active) continue;
            Dictionary d;
            d["id"]          = String(q->id);
            d["title"]       = String(q->title);
            d["description"] = String(q->description);
            d["unlocked"]    = (bool)q->unlocked;
            d["completed"]   = (bool)q->completed;
            out.append(d);
        }
        return out;
    }

    // ══ SAVE / LOAD API ═══════════════════════════════════════════════════════

    // Save current state to slot 0-2. Returns true on success.
    bool save_to_slot(int slot) {
    if (!initialized) return false;
    if (slot < 0 || slot >= SAVE_SLOTS) return false;
    SaveData sd;
    memset(&sd, 0, sizeof(SaveData));
    save_data_from_state(&sd, &party, &inventory, &gs, &time_sys,
                          &social, &bonds,
                          player_x, player_y, 0, &ds_sys);
    save_build_timestamp(&sd, &time_sys);
    // Compute current location name for save slot display
    if (is_in_dungeon()) {
        Dungeon* p = dungeon_get_active(&ds_sys);
        DungeonRoom* r = dungeon_current_room(&ds_sys);
        const char* pname = (p && p->name[0]) ? p->name : "Dungeon";
        const char* rname = (r && r->name[0]) ? r->name : "Unknown";
        snprintf(sd.location_name, 64, "%s - %s", pname, rname);
    } else {
        snprintf(sd.location_name, 64, "Bedroom");
    }
    int result = save_game(slot, &sd);
    if (result) {
        notify_success(&notify, "Game saved.");
        return true;
    }
    notify_info(&notify, "Save failed.");
    return false;
}

    // Load from slot 0-2. Returns true on success.
    // Re-initialises party/inventory/etc. from the save data.
    bool load_from_slot(int slot) {
        if (!initialized) init_systems();
        if (slot < 0 || slot >= SAVE_SLOTS) return false;
        SaveData sd;
        memset(&sd, 0, sizeof(SaveData));
        if (!load_game(slot, &sd)) return false;
        float px = 0, py = 0;
        int room = 0;
        save_data_to_state(&sd, &party, &inventory, &gs, &time_sys,
                            &social, &bonds,
                            &px, &py, &room, &ds_sys);
        player_x = px;
        player_y = py;
        // Re-apply Prota's summon to his StatBlock
        if (party.count > 0)
            summon_apply_to_statblock(&g_summon_stock, &party.members[0]);
        return true;
    }

    // Returns dict with save metadata. If incompatible, returns dict with
// "incompatible": true and "version": (number) — caller should offer to delete.
// If empty, returns dict with "used": false.
Dictionary get_save_slot_info(int slot) {
    Dictionary d;
    if (slot < 0 || slot >= SAVE_SLOTS) return d;

    int v = save_get_version(slot);
    if (v == 0) {
        // No file or bad magic — slot is genuinely empty
        d["used"] = false;
        return d;
    }
    if (v != SAVE_VERSION) {
        d["used"]         = true;
        d["incompatible"] = true;
        d["version"]      = v;
        return d;
    }

    SaveData sd;
    memset(&sd, 0, sizeof(SaveData));
    if (!load_game(slot, &sd)) {
        d["used"] = false;
        return d;
    }
    d["used"]         = (bool)sd.used;
    d["incompatible"] = false;
    d["day"]          = sd.day;
    d["slot"]         = sd.slot;
    d["month"]        = sd.month;
    d["date"]         = sd.date;
    d["yen"]          = sd.yen;
    d["dungeon_idx"]   = sd.dungeon_idx;
    d["timestamp"]    = String(sd.timestamp);
    d["location"]     = String(sd.location_name);
    int level = sd.party_count > 0 ? sd.members[0].level : 1;
    d["level"]         = level;
    d["party_count"]   = sd.party_count;
    return d;
}

    bool delete_save_slot(int slot) {
        if (slot < 0 || slot >= SAVE_SLOTS) return false;
        return delete_save(slot) != 0;
    }

    void set_save_directory_bridge(const String& path) {
    CharString utf = path.utf8();
    save_set_directory(utf.get_data());
}

    // ══ STATUS & EQUIPMENT API ════════════════════════════════════════════════

    // Returns array of party member dicts with full stats and equipment info.
    Array get_party_status() {
        Array out;
        if (!initialized) return out;
        for (int i = 0; i < party.count; i++) {
            StatBlock* sb = &party.members[i];
            Dictionary m;
            m["idx"]    = i;
            m["name"]   = String(sb->name);
            m["level"]  = sb->level;
            m["exp"]    = sb->exp;
            m["exp_next"] = sb->level * 100;  // formula from statblock_add_exp
            m["hp"]     = sb->hp;
            m["max_hp"] = sb->max_hp;
            m["sp"]     = sb->sp;
            m["max_sp"] = sb->max_sp;
            m["st"]     = sb->st;
            m["ma"]     = sb->ma;
            m["en"]     = sb->en;
            m["ag"]     = sb->ag;
            m["lu"]     = sb->lu;
            m["alive"]  = (bool)sb->alive;

            // Equipment slots
            Array eq_slots;
            for (int s = 0; s < EQUIP_SLOT_COUNT; s++) {
                Dictionary slot_d;
                slot_d["slot"]      = s;
                slot_d["slot_name"] = String(equip_slot_name((EquipSlot)s));
                int eid = sb->equipped[s];
                slot_d["piece_id"]  = eid;
                if (eid >= 0) {
                    EquipmentPiece* p = equip_get(&equipdb, eid);
                    if (p) {
                        slot_d["piece_name"] = String(p->name);
                        slot_d["piece_desc"] = String(p->description);
                    } else {
                        slot_d["piece_name"] = String("?");
                        slot_d["piece_desc"] = String("");
                    }
                } else {
                    slot_d["piece_name"] = String("(empty)");
                    slot_d["piece_desc"] = String("");
                }
                eq_slots.append(slot_d);
            }
            m["equipment"] = eq_slots;

            // Skills the member knows
            Array skill_names;
            int sc = sb->skill_count;
            if (sc < 0) sc = 0;
            if (sc > 8) sc = 8;
            for (int s = 0; s < sc; s++) {
                Skill* sk = skilldb_get(&skilldb, sb->skill_ids[s]);
                skill_names.append(String(sk ? sk->name : "?"));
            }
            m["skills"] = skill_names;

            out.append(m);
        }
        return out;
    }

    // Returns full equipment DB as array of dicts.
    Array get_equipment_db() {
        Array out;
        if (!initialized) return out;
        for (int i = 0; i < equipdb.count; i++) {
            EquipmentPiece* p = &equipdb.pieces[i];
            Dictionary d;
            d["id"]         = p->id;
            d["name"]       = String(p->name);
            d["description"] = String(p->description);
            d["slot"]       = (int)p->slot;
            d["slot_name"]  = String(equip_slot_name(p->slot));
            d["price"]      = p->price;
            // Build a stat-mod summary
            String mods = "";
            for (int m = 0; m < p->mod_count; m++) {
                if (m > 0) mods += ", ";
                int v = p->mods[m].value;
                mods += String(equip_mod_name(p->mods[m].type));
                mods += v >= 0 ? String(" +") : String(" ");
                mods += String::num(v);
            }
            d["mods"] = mods;
            out.append(d);
        }
        return out;
    }

    // Equip a piece on a party member. Returns true on success.
    bool equip_piece(int member_idx, int piece_id) {
        if (!initialized) return false;
        if (member_idx < 0 || member_idx >= party.count) return false;
        EquipmentPiece* p = equip_get(&equipdb, piece_id);
        if (!p) return false;
        equip_set(&equipdb, &party.members[member_idx], p->slot, piece_id);
        char msg[80];
        snprintf(msg, 80, "%s equipped %s.",
                 party.members[member_idx].name, p->name);
        notify_info(&notify, msg);
        return true;
    }

    // Unequip slot 0/1/2. Returns true if something was unequipped.
    bool unequip_slot(int member_idx, int slot) {
        if (!initialized) return false;
        if (member_idx < 0 || member_idx >= party.count) return false;
        if (slot < 0 || slot >= EQUIP_SLOT_COUNT) return false;
        int old = party.members[member_idx].equipped[slot];
        if (old < 0) return false;
        equip_set(&equipdb, &party.members[member_idx], (EquipSlot)slot, -1);
        return true;
    }

    // Polled by Godot — returns the pending save/load request (and clears it).
    // 0 = none, 1 = save requested, 2 = load requested
    int poll_save_load_request() {
        if (!initialized) return 0;
        if (gamestate_get_flag(&gs, "request_save")) {
            gamestate_set_flag(&gs, "request_save", 0);
            return 1;
        }
        if (gamestate_get_flag(&gs, "request_load")) {
            gamestate_set_flag(&gs, "request_load", 0);
            return 2;
        }
        return 0;
    }

    // ══ SUMMON MENU API ══════════════════════════════════════════════════════

    void summon_menu_open_bridge() {
        if (!initialized) return;
        summon_menu_open(&g_summon_menu, &g_summon_comp, &inventory);
    }

    void summon_menu_close_bridge() {
        if (!initialized) return;
        summon_menu_close(&g_summon_menu);
    }

    void summon_menu_send_key(int godot_key) {
        if (!initialized) return;
        if (g_summon_menu.state == PMENU_CLOSED) return;
        SDL_Event ev;
        memset(&ev, 0, sizeof(SDL_Event));
        ev.type = SDL_KEYDOWN;
        switch (godot_key) {
            case 69:      ev.key.keysym.sym = SDLK_e;      break;  // E
            case 70:      ev.key.keysym.sym = SDLK_f;      break;  // F
            case 82:      ev.key.keysym.sym = SDLK_r;      break;  // R
            case 67:      ev.key.keysym.sym = SDLK_c;      break;  // C
            case 4194305: ev.key.keysym.sym = SDLK_ESCAPE; break;
            case 4194309: ev.key.keysym.sym = SDLK_RETURN; break;
            case 4194320: ev.key.keysym.sym = SDLK_UP;     break;
            case 4194322: ev.key.keysym.sym = SDLK_DOWN;   break;
            case 4194319: ev.key.keysym.sym = SDLK_LEFT;   break;
            case 4194321: ev.key.keysym.sym = SDLK_RIGHT;  break;
            default:      return;
        }
        summon_menu_handle_input(&g_summon_menu, &ev, &g_summon_stock,
                                  &g_summon_db, &g_summon_comp,
                                  &notify, &inventory);
    }

    // Read the current summon menu state. Returns:
    //   state: "closed" | "browse" | "fuse_a" | "fuse_b" | "confirm" | "compendium"
    //   selected: highlighted row in current list
    //   fuse_idx_a: chosen first parent (only in fuse_b/confirm)
    //   summons: array of dicts with {name, arcana, level, hp, sp, skills:[...], is_active}
    //   compendium: array of dicts with {name, arcana, level}
    //   fuse_result: dict with {name, arcana, level, skills:[...]}
    Dictionary get_summon_menu_state() {
        Dictionary d;
        if (!initialized) return d;

        const char* state_names[] = {
            "closed", "browse", "fuse_a", "fuse_b", "confirm", "compendium"
        };
        int st = (int)g_summon_menu.state;
        if (st < 0 || st > 5) st = 0;
        d["state"]      = String(state_names[st]);
        d["selected"]   = g_summon_menu.selected;
        d["fuse_idx_a"] = g_summon_menu.fuse_idx_a;

        // Owned summons (stock)
        Array summons;
        for (int i = 0; i < g_summon_stock.count; i++) {
            Summon* p = &g_summon_stock.stock[i];
            Dictionary pd;
            pd["name"]      = String(p->name);
            pd["arcana"]    = String(arcana_name(p->arcana));
            pd["level"]     = p->level;
            pd["is_active"] = (i == g_summon_stock.active_idx);
            Array sks;
            for (int s = 0; s < p->skill_count; s++) {
                Skill* sk = skilldb_get(&skilldb, p->skill_ids[s]);
                sks.append(String(sk ? sk->name : "?"));
            }
            pd["skills"] = sks;
            summons.append(pd);
        }
        d["summons"] = summons;

        // Compendium entries
        Array comp_entries;
        for (int i = 0; i < g_summon_comp.count; i++) {
            Summon* e = &g_summon_comp.entries[i];
            Dictionary cd;
            cd["name"]   = String(e->name);
            cd["arcana"] = String(arcana_name(e->arcana));
            cd["level"]  = e->level;
            cd["cost"]   = summon_resummon_cost(e);
            // Check if already in active stock
            bool in_stock = false;
            for (int s = 0; s < g_summon_stock.count; s++) {
                if (strcmp(g_summon_stock.stock[s].name, e->name) == 0) {
                    in_stock = true;
                    break;
                }
            }
            cd["in_stock"] = in_stock;
            // Skills snapshot (so detail pane can show what you'd get)
            Array sks;
            for (int s = 0; s < e->skill_count; s++) {
                Skill* sk = skilldb_get(&skilldb, e->skill_ids[s]);
                sks.append(String(sk ? sk->name : "?"));
            }
            cd["skills"] = sks;
            comp_entries.append(cd);
        }
        d["compendium"] = comp_entries;
        d["yen"]              = inventory.yen;
        d["stock_count"]      = g_summon_stock.count;
        d["stock_max"]        = (int)MAX_PROTA_SUMMONS;

        // Fusion result preview (only valid in PMENU_CONFIRM)
        if (g_summon_menu.state == PMENU_CONFIRM) {
            Summon* fr = &g_summon_menu.fuse_result;
            Dictionary fd;
            fd["name"]   = String(fr->name);
            fd["arcana"] = String(arcana_name(fr->arcana));
            fd["level"]  = fr->level;
            Array fsks;
            for (int s = 0; s < fr->skill_count; s++) {
                Skill* sk = skilldb_get(&skilldb, fr->skill_ids[s]);
                fsks.append(String(sk ? sk->name : "?"));
            }
            fd["skills"] = fsks;
            d["fuse_result"] = fd;
        }

        return d;
    }

    // ══ MAP API ═══════════════════════════════════════════════════════════════

    // Returns layout data for the current floor:
    // {
    //   floor_idx: int,
    //   floor_count: int,
    //   stairs_room: int,        # room ID of stairs (or -1)
    //   current_room: int,       # room ID of current room
    //   grid_w: int,
    //   grid_h: int,
    //   rooms: [
    //     { id, name, type, grid_x, grid_y, visited, cleared,
    //       exits: [n, s, e, w],  # room IDs or -1
    //       is_stairs, is_current
    //     }, ...
    //   ]
    // }
    Dictionary get_floor_map() {
        Dictionary d;
        if (!initialized || !is_in_dungeon()) return d;
        Dungeon* p = dungeon_get_active(&ds_sys);
        DungeonFloor* fl = dungeon_current_floor(&ds_sys);
        DungeonRoom* cur = dungeon_current_room(&ds_sys);
        if (!p || !fl) return d;

        // Replicate the BFS grid layout from minimap_build_grid in C.
        int grid_x[MAX_DUNGEON_ROOMS];
        int grid_y[MAX_DUNGEON_ROOMS];
        int visited_bfs[MAX_DUNGEON_ROOMS];
        for (int i = 0; i < fl->room_count; i++) {
            grid_x[i] = -99;
            grid_y[i] = -99;
            visited_bfs[i] = 0;
        }
        grid_x[0] = 0;
        grid_y[0] = 0;
        visited_bfs[0] = 1;

        int queue_buf[MAX_DUNGEON_ROOMS];
        int qhead = 0, qtail = 0;
        queue_buf[qtail++] = 0;

        const int dxs[] = {0, 0, 1, -1};
        const int dys[] = {-1, 1, 0, 0};

        while (qhead < qtail) {
            int cur_idx = queue_buf[qhead++];
            for (int dir = 0; dir < 4; dir++) {
                int next = fl->rooms[cur_idx].exits[dir];
                if (next < 0 || next >= fl->room_count) continue;
                if (visited_bfs[next]) continue;
                visited_bfs[next] = 1;
                grid_x[next] = grid_x[cur_idx] + dxs[dir];
                grid_y[next] = grid_y[cur_idx] + dys[dir];
                queue_buf[qtail++] = next;
            }
        }

        // Normalise so min is 0
        int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
        for (int i = 0; i < fl->room_count; i++) {
            if (grid_x[i] == -99) continue;
            if (grid_x[i] < min_x) min_x = grid_x[i];
            if (grid_y[i] < min_y) min_y = grid_y[i];
        }
        for (int i = 0; i < fl->room_count; i++) {
            if (grid_x[i] == -99) continue;
            grid_x[i] -= min_x;
            grid_y[i] -= min_y;
            if (grid_x[i] > max_x) max_x = grid_x[i];
            if (grid_y[i] > max_y) max_y = grid_y[i];
        }

        // Build the rooms array
        Array rooms;
        for (int i = 0; i < fl->room_count; i++) {
            DungeonRoom* r = &fl->rooms[i];
            Dictionary rd;
            rd["id"]         = r->id;
            rd["name"]       = String(r->name);
            rd["type"]       = (int)r->type;
            rd["grid_x"]     = grid_x[i];
            rd["grid_y"]     = grid_y[i];
            rd["visited"]    = (bool)r->visited;
            rd["cleared"]    = (bool)r->cleared;
            rd["is_stairs"]  = (bool)(r->id == fl->stairs_room_id);
            rd["is_current"] = (bool)(cur && r->id == cur->id);
            rd["chest_present"] = (bool)(r->chest.item_id != 0 || r->chest.equip_id != 0);
            rd["chest_opened"]  = (bool)r->chest.opened;
            Array exits;
            for (int dir = 0; dir < 4; dir++) exits.append(r->exits[dir]);
            rd["exits"] = exits;
            rooms.append(rd);
        }

        d["floor_idx"]    = p->current_floor;
        d["floor_count"]  = p->floor_count;
        d["stairs_room"]  = fl->stairs_room_id;
        d["current_room"] = cur ? cur->id : -1;
        d["grid_w"]       = max_x + 1;
        d["grid_h"]       = max_y + 1;
        d["rooms"]        = rooms;
        return d;
    }

    // ══ NOTIFICATIONS API ═════════════════════════════════════════════════════

    // Returns active notifications as array of dicts:
    //   { text, timer, fade_alpha, r, g, b }
    // fade_alpha is a float 0..1 — 1.0 normally, fades out in last 60 frames.
    Array get_notifications() {
        Array out;
        if (!initialized) return out;
        for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
            if (!notify.notes[i].active) continue;
            Dictionary d;
            d["text"]  = String(notify.notes[i].text);
            d["timer"] = notify.notes[i].timer;
            float alpha = 1.0f;
            if (notify.notes[i].timer < 60)
                alpha = (float)notify.notes[i].timer / 60.0f;
            d["fade_alpha"] = alpha;
            d["r"] = notify.notes[i].r;
            d["g"] = notify.notes[i].g;
            d["b"] = notify.notes[i].b;
            out.append(d);
        }
        return out;
    }

    // Advances notification timers. Call once per frame from Godot _process.
    void tick_notifications() {
        if (!initialized) return;
        notify_update(&notify);
    }

    // Tick the event system — fires registry events when conditions met.
    // Returns dialogue node ID to start (or -1 if nothing).
    int tick_events() {
        if (!initialized) return -1;
        int force_dialogue = -1;
        eventsystem_update(&event_system, &time_sys, &gs, &notify,
                            &social, &world_sys, &force_dialogue);
        return force_dialogue;
    }

    // Return list of available dungeons with names and unlock status
    Array get_dungeons() {
        Array out;
        if (!initialized) return out;
        for (int i = 0; i < dungeon_db.count; i++) {
            DungeonEntry* e = &dungeon_db.entries[i];
            Dictionary d;
            d["idx"]      = i;
            d["name"]     = String(e->name);
            d["ruler"]    = String("");  // not stored on entry; could be added later
            // Dungeon 0 always unlocked; later dungeons gated by flag
            bool unlocked = (i == 0);
            char unlock_flag[64];
            snprintf(unlock_flag, 64, "dungeon_%d_unlocked", i + 1);
            if (gamestate_get_flag(&gs, unlock_flag)) unlocked = true;
            // Also unlock via entry_flag if specified
            if (e->entry_flag[0] && gamestate_get_flag(&gs, e->entry_flag))
                unlocked = true;
            d["unlocked"] = unlocked;
            d["cleared"]  = (bool)gamestate_get_flag(&gs, e->clear_flag);
            out.append(d);
        }
        return out;
    }

    // ══ DIALOGUE API ══════════════════════════════════════════════════════════

    bool is_dialogue_active() {
        if (!initialized) return false;
        return dialogue.active != 0;
    }

    // Returns the current dialogue node's content:
    //   { speaker, text, choices: [...], current_node }
    // If no dialogue is active, returns an empty dictionary.
    Dictionary get_dialogue_state() {
        Dictionary d;
        if (!initialized || !dialogue.active) return d;
        // Read from whichever buffer is active (keyed conversation vs flat nodes)
        DialogueNode* nodes = dialogue.using_keyed ? dialogue.keyed_nodes : dialogue.nodes;
        int count = dialogue.using_keyed ? dialogue.keyed_node_count : dialogue.node_count;
        if (dialogue.current_node < 0 || dialogue.current_node >= count)
            return d;
        DialogueNode* n = &nodes[dialogue.current_node];
        d["speaker"]      = String(n->speaker);
        d["text"]         = String(n->text);
        d["current_node"] = dialogue.current_node;
        d["choice_count"] = n->choice_count;
        Array choices;
        for (int i = 0; i < n->choice_count; i++) {
            choices.append(String(n->choice_text[i]));
        }
        d["choices"] = choices;
        d["offers_hangout"] = (bool)n->offers_hangout;
        d["bond_name"] = String(n->bond_name);
        return d;
    }

    // Advance dialogue. choice=0 for plain-continue nodes; 0..n-1 for choices.
    void dialogue_choose(int choice) {
        if (!initialized || !dialogue.active) return;
        dialogue_advance(&dialogue, choice, &gs, &bonds, &notify);
        // If a bond hangout just ended, it costs a time slot.
        if (!dialogue.active && dialogue.hangout_pending) {
            dialogue.hangout_pending = 0;
            timesystem_advance(&time_sys);
        }
    }

    Dictionary debug_dialogue_state() {
        Dictionary d;
        if (!initialized) return d;
        d["using_keyed"]      = dialogue.using_keyed;
        d["active"]           = dialogue.active;
        d["current_node"]     = dialogue.current_node;
        d["node_count"]       = dialogue.node_count;
        d["keyed_node_count"] = dialogue.keyed_node_count;
        d["db_count"]         = g_dialogue_db.count;
        d["has_rank1"]  = dialogue_db_find(&g_dialogue_db, "stranger_rank1") ? true : false;
        d["has_filler"] = dialogue_db_find(&g_dialogue_db, "stranger_filler") ? true : false;
        return d;
    }

    // Start dialogue at a specific node. Useful for testing.
    void dialogue_start_node(int node_id) {
        if (!initialized) return;
        dialogue_start(&dialogue, node_id);
    }

    // Start a keyed conversation (Phase 1 keyed dialogue system).
    void dialogue_start_convo(const String& key) {
        if (!initialized) return;
        dialogue_start_key(&dialogue, key.utf8().get_data());
    }

    // ══ NPC API ════════════════════════════════════════════════════════════

    // Returns array of dicts: { id, name, x, y, z, bond_name }
    // The overworld scene uses this to spawn NPC markers at positions.
    Array get_npcs() {
        Array out;
        if (!initialized) return out;
        for (int i = 0; i < npc_reg.count; i++) {
            NPCDefinition* n = &npc_reg.npcs[i];
            if (!n->active) continue;
            Dictionary d;
            d["id"]             = String(n->id);
            d["name"]           = String(n->display_name);
            d["x"]              = n->x;
            d["y"]              = n->y;
            d["z"]              = n->z;
            d["bond_name"] = String(n->bond_name);
            out.append(d);
        }
        return out;
    }

    // Player interacted with an NPC. C resolves first/repeat conversation
    // and starts dialogue. Returns true if a conversation started.
    bool npc_interact_id(const String& id) {
        if (!initialized) return false;
        return npc_interact(&npc_reg, id.utf8().get_data(),
                            &dialogue, &gs, &bonds) != 0;
    }

    // ══ HAND-OFF API ════════════════════════════════════════════════════════

    bool handoff_available(int from_idx) {
        if (!initialized) return false;
        return combat_handoff_available(&cs, from_idx) != 0;
    }

    bool hand_off(int from_idx, int to_idx) {
        if (!initialized) return false;
        return combat_hand_off(&cs, from_idx, to_idx) != 0;
    }

    bool is_in_dungeon() {
        if (!initialized) return false;
        Dungeon* p = dungeon_get_active(&ds_sys);
        return p && p->active;
    }

    // Set key state for dungeon movement. Maps Godot keycodes to SDL scancodes.
    void set_key_down(int godot_key, bool is_down) {
        Uint8 v = is_down ? 1 : 0;
        switch (godot_key) {
            case 4194320: key_state[SDL_SCANCODE_UP]    = v; break; // KEY_UP
            case 4194322: key_state[SDL_SCANCODE_DOWN]  = v; break; // KEY_DOWN
            case 4194319: key_state[SDL_SCANCODE_LEFT]  = v; break; // KEY_LEFT
            case 4194321: key_state[SDL_SCANCODE_RIGHT] = v; break; // KEY_RIGHT
            case 87:      key_state[SDL_SCANCODE_W]     = v; break; // KEY_W
            case 65:      key_state[SDL_SCANCODE_A]     = v; break; // KEY_A
            case 83:      key_state[SDL_SCANCODE_S]     = v; break; // KEY_S
            case 68:      key_state[SDL_SCANCODE_D]     = v; break; // KEY_D
            default: break;
        }
    }

    // Single per-frame dungeon driver. Godot owns movement and passes the player's
    // world position (in C pixel coords). This stores it, checks edge transitions,
    // and runs dungeon logic (encounters, alert, enemies, boss). Combat detection
    // and scene changes are handled by Global reading get_dungeon_state() after.
    void dungeon_tick(float px, float py) {
        if (!initialized) return;
        if (!is_in_dungeon()) return;
        if (cs.active) {
            if (ds_sys.combat_is_boss) dungeon_update(&ds_sys, player_x, player_y);
            return;
        }
        player_x = px;
        player_y = py;
        dungeon_check_edge_transition(&ds_sys, &player_x, &player_y, player_w, player_h);
        dungeon_update(&ds_sys, player_x, player_y);
    }

    bool dungeon_use_exit(int dir) {
        if (!initialized || !is_in_dungeon()) return false;
        if (cs.active) return false;
        return dungeon_use_exit_dir(&ds_sys, dir, &player_x, &player_y) != 0;
    }

    bool dungeon_start_encounter(int entity_index) {
        if (!initialized || !is_in_dungeon()) return false;
        if (cs.active) return false;
        return dungeon_trigger_encounter(&ds_sys, entity_index, player_x, player_y) != 0;
    }

    // Godot-owned dungeon movement: set position from Godot, then run C-side
    // dungeon logic (encounters, transitions, alert, proximity). On a room
    // transition C may relocate player_x/player_y; Godot reads it back after.
    void dungeon_set_player_and_update(float px, float py) {
    if (!initialized || !is_in_dungeon()) return;
    if (cs.active) {
        if (ds_sys.combat_is_boss) dungeon_update(&ds_sys, player_x, player_y);
        return;
    }
    player_x = px;
    player_y = py;
    // Edge transition (walking off room edge into adjacent room)
    dungeon_check_edge_transition(&ds_sys, &player_x, &player_y, player_w, player_h);
    dungeon_update(&ds_sys, player_x, player_y);
}

    // Discrete keypress for dungeon (e.g. confirm to open chest, esc for menu)
    void dungeon_send_key(int godot_key) {
        if (!initialized || !is_in_dungeon()) return;
        SDL_Event ev;
        memset(&ev, 0, sizeof(SDL_Event));
        ev.type = SDL_KEYDOWN;
        switch (godot_key) {
            case 69:      ev.key.keysym.sym = SDLK_e;      break;  // E
            case 84:      ev.key.keysym.sym = SDLK_t;      break;  // T
            case 85:      ev.key.keysym.sym = SDLK_u;      break;  // U
            case 70:      ev.key.keysym.sym = SDLK_f;      break;  // F
            case 32:      ev.key.keysym.sym = SDLK_SPACE;  break;  // SPACE
            case 4194309: ev.key.keysym.sym = SDLK_RETURN; break;  // ENTER
            case 4194305: ev.key.keysym.sym = SDLK_ESCAPE; break;  // ESC
            case 4194320: ev.key.keysym.sym = SDLK_UP;     break;  // UP
            case 4194322: ev.key.keysym.sym = SDLK_DOWN;   break;  // DOWN
            case 4194319: ev.key.keysym.sym = SDLK_LEFT;   break;  // LEFT
            case 4194321: ev.key.keysym.sym = SDLK_RIGHT;  break;  // RIGHT
            default:      return;
        }
        dungeon_handle_input(&ds_sys, &ev, &player_x, &player_y);
    }

    // Returns the current room's tilemap as a PackedByteArray
    // MAP_ROWS * MAP_COLS bytes, each is 0 (floor) or 1 (wall)
    PackedByteArray get_current_room_tiles() {
        PackedByteArray out;
        if (!initialized || !is_in_dungeon()) return out;
        DungeonRoom* room = dungeon_current_room(&ds_sys);
        if (!room) return out;
        out.resize(MAP_ROWS * MAP_COLS);
        for (int r = 0; r < MAP_ROWS; r++) {
            for (int c = 0; c < MAP_COLS; c++) {
                out[r * MAP_COLS + c] = (uint8_t)room->map.tiles[r][c];
            }
        }
        return out;
    }

    // Returns a dictionary describing the current dungeon state
    Dictionary get_dungeon_state() {
        Dictionary d;
        if (!initialized || !is_in_dungeon()) {
            d["active"] = false;
            return d;
        }
        d["active"] = true;

        Dungeon* p = dungeon_get_active(&ds_sys);
        if (!p) return d;

        d["dungeon_name"]   = String(p->name);
        d["dungeon_ruler"]  = String(p->ruler);
        d["alert_level"]   = p->alert_level;
        d["current_floor"] = p->current_floor;
        d["dungeon_idx"]    = ds_sys.current_dungeon_idx;
        d["floor_count"]   = p->floor_count;
        d["in_combat"]     = (bool)ds_sys.in_combat;

        // Player position (continuous, in tile units * TILE_SIZE)
        d["player_x"]     = player_x;
        d["player_y"]     = player_y;
        d["player_tile_x"] = (int)(player_x / TILE_SIZE);
        d["player_tile_y"] = (int)(player_y / TILE_SIZE);

        // Map dimensions
        d["map_cols"]  = MAP_COLS;
        d["map_rows"]  = MAP_ROWS;
        d["tile_size"] = TILE_SIZE;

        // ── Handcrafted-room pipeline bridge ─────────────────────────────
        // Current room
        DungeonRoom* room = dungeon_current_room(&ds_sys);
        DungeonFloor* fl  = dungeon_current_floor(&ds_sys);
        if (room) {
            d["room_id"]          = room->id;
            d["room_name"]        = String(room->name);
            d["room_description"] = String(room->description);
            d["room_type"]        = (int)room->type;
            d["room_visited"]     = (bool)room->visited;
            d["room_cleared"]     = (bool)room->cleared;
            d["chest_present"]    = (bool)(room->chest.item_id != 0 || room->chest.equip_id != 0);
            d["chest_opened"]     = (bool)room->chest.opened;
            d["is_stairs_room"]   = (bool)(fl && room->id == fl->stairs_room_id);
            d["is_safe_room"]     = (bool)(room->type == ROOM_SAFE);
            d["is_chest_room"]    = (bool)(room->type == ROOM_CHEST);
            d["is_boss_room"]     = (bool)(room->type == ROOM_BOSS);

            // Exits — which directions lead somewhere (-1 = no exit)
            Array exits;
            for (int i = 0; i < 4; i++) exits.append(room->exits[i]);
            d["room_exits"] = exits;

            // Entities in this room
            Array entities;
            for (int i = 0; i < room->entities.count; i++) {
                Entity* e = &room->entities.entities[i];
                if (!e->active) continue;
                Dictionary ed;
                ed["type"]     = (int)e->type;
                ed["x"]        = e->x;
                ed["y"]        = e->y;
                ed["width"]    = e->width;
                ed["height"]   = e->height;
                ed["name"]     = String(e->name);
                ed["defeated"] = (bool)e->defeated;
                entities.append(ed);
            }
            d["room_entities"] = entities;
        }

        d["fast_travel_open"] = (bool)ds_sys.fast_travel_open;
        d["fast_travel_selected"] = ds_sys.fast_travel_selected;
        d["safe_menu_open"]   = (bool)ds_sys.safe_menu_open;
        d["safe_menu_selected"] = ds_sys.safe_menu_selected;
        d["chest_open"]       = (bool)ds_sys.chest_open;

        // List of all visited safe rooms across all floors (for fast travel UI)
        Array fast_travel_rooms;
        if (p) {
            for (int fl = 0; fl < p->floor_count; fl++) {
                for (int rm = 0; rm < p->floors[fl].room_count; rm++) {
                    DungeonRoom* r = &p->floors[fl].rooms[rm];
                    if (r->type == ROOM_SAFE && r->visited) {
                        Dictionary ftd;
                        ftd["floor"] = fl;
                        ftd["room"]  = rm;
                        ftd["name"]  = String(r->name);
                        ftd["is_current_floor"] = (fl == p->current_floor);
                        fast_travel_rooms.append(ftd);
                    }
                }
            }
        }
        d["fast_travel_rooms"] = fast_travel_rooms;

        return d;
    }

};


OpalGame* OpalGame::singleton = nullptr;

void initialize_opal(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    ClassDB::register_class<OpalGame>();
}
void uninitialize_opal(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
}

extern "C" {
GDExtensionBool GDE_EXPORT opal_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization* r_initialization)
{
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize_opal);
    init_obj.register_terminator(uninitialize_opal);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}