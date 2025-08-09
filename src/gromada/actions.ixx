module;
#include <cstdint>

export module Gromada.Actions;
import std;

export {
    enum class Action : std::uint8_t;

    constexpr std::string_view to_string(Action action) noexcept;
}

//     event name          index  event arguments                     official comment from the game's maps/header.ini
#define GROMADA_ACTIONS_XMACRO_LIST(ACTION)                                                                                                                     \
ACTION(act_stand          , 0 ,   ()                                ) /**/                                                                                      \
ACTION(act_build          , 1 ,   ()                                ) /**/                                                                                      \
ACTION(act_go             , 2 ,   ()                                ) /**/                                                                                      \
ACTION(act_start_move     , 3 ,   ()                                ) /**/                                                                                      \
ACTION(act_l_rotate       , 4 ,   ()                                ) /**/                                                                                      \
ACTION(act_r_rotate       , 5 ,   ()                                ) /**/                                                                                      \
ACTION(act_open           , 6 ,   ()                                ) /**/                                                                                      \
ACTION(act_close          , 7 ,   ()                                ) /**/                                                                                      \
ACTION(act_fight          , 8 ,   ()                                ) /**/                                                                                      \
ACTION(act_salut          , 9 ,   ()                                ) /**/                                                                                      \
ACTION(act_stand_open     , 10,   ()                                ) /**/                                                                                      \
ACTION(act_load           , 11,   ()                                ) /**/                                                                                      \
ACTION(act_unload         , 12,   ()                                ) /**/                                                                                      \
ACTION(act_wound          , 13,   ()                                ) /**/                                                                                      \
ACTION(act_birth          , 14,   ()                                ) /**/                                                                                      \
ACTION(act_death          , 15,   ()                                ) /**/                                                                                      \
ACTION(act_path_block     , 31,   ((int, dx), (int, dy))            ) /*path is blocked var1-last dx, var2 -last dy*/                                           \
ACTION(act_additem        , 32,   ((int, nvid))                     ) /*var1=nvid*/                                                                             \
ACTION(act_deleteitem     , 33,   ((int, nvid))                     ) /*var1=nvid*/                                                                             \
ACTION(act_askitem        , 34,   ((int, nvid))                     ) /*var1=nvid*/                                                                             \
ACTION(act_changeweapon   , 35,   ((int, nweapon))                  ) /*var1=nweapon*/                                                                          \
ACTION(act_deleteallitem  , 36,   ()                                ) /**/                                                                                      \
ACTION(act_changeaction   , 37,   ((Action, action), (int, dir))    ) /*var1=action var2=direct*/                                                               \
ACTION(act_path_limit     , 38,   ()                                ) /*path is limited of field*/                                                              \
ACTION(act_attack         , 39,   ()                                ) /**/                                                                                      \
ACTION(act_move           , 40,   ()                                ) /**/                                                                                      \
ACTION(act_buildunit      , 41,   ((int, nvid))                     ) /*var1-nvid for building unit*/                                                           \
ACTION(act_drawselected   , 42,   ()                                ) /**/                                                                                      \
ACTION(act_damage         , 43,   ()                                ) /**/                                                                                      \
ACTION(act_select         , 44,   ()                                ) /*вызывает на самом деле act_salut*/                                                      \
ACTION(act_nextcommand    , 45,   ()                                ) /**/                                                                                      \
ACTION(act_init           , 46,   ()                                ) /*вызывается из createunit для инициализации нач. значений*/                              \
ACTION(act_uncom          , 47,   ()                                ) /*запрещает выполнение дальнейших команд com для building*/                               \
ACTION(act_deletetarget   , 48,   ((int, target_id))                ) /*if target ==var1 then target=NULL*/                                                     \
ACTION(act_calltact       , 49,   ()                                ) /**/                                                                                      \
ACTION(act_save           , 51,   ((StreamHandle, stream_id))       ) /*записать информацию относящуюся к данному спрайту в var1*/                              \
ACTION(act_restore        , 52,   ()                                ) /**/                                                                                      \
ACTION(act_destroy        , 54,   ()                                ) /**/                                                                                      \
ACTION(act_coor_attack    , 56,   ()                                ) /**/                                                                                      \
ACTION(act_child          , 57,   ()                                ) /*вызывается из createchild при создании child*/                                          \
ACTION(act_saveframe      , 58,   ((StreamHandle, stream_id))       ) /*записать информацию относящуюся к данному frame в var1*/                                \
ACTION(act_fullsave       , 59,   ((StreamHandle, stream_id))       ) /*записать всю информацию относящуюся к данному sprite, кроме записанной в act_save*/     \
ACTION(act_gettarget      , 60,   ()                                ) /**/                                                                                      \
ACTION(act_cyclecommand   , 61,   ((int, command_index))            ) /*зацикливание стэка комманд*/                                                            \
ACTION(act_changevid      , 62,   ()                                ) /*изменить текущий curvid для спрайта, var2 - new action, if var1==-1 action not changed*/\
ACTION(act_addammo        , 63,   ((int, increment), (int, nweapon))) /*var1-число добавляемых снарядов, var2 - номер оружия*/                                  \
ACTION(act_getammo        , 64,   ((int, nweapon))                  ) /*var1 - номер оружия*/                                                                   \
ACTION(act_gethp          , 65,   ()                                ) /**/                                                                                      \
ACTION(act_clearcom       , 66,   ()                                ) /*снять все команды*/                                                                     \
ACTION(act_setbehave      , 67,   ((int, new_behave))               ) /*var1 -new behave*/                                                                      \
ACTION(act_changecoor     , 68,   ((int, x), (int, y))              ) /*var1-x,var2-y*/                                                                         \
ACTION(act_getlink        , 69,   ()                                ) /*return link sprite*/                                                                    \
ACTION(act_jump           , 70,   ((int, x), (int, y))              ) /*var1-x,var2-y*/                                                                         \
ACTION(act_settext        , 71,   ((StringHandle, text))            ) /*var1-char **/                                                                           \
ACTION(act_gettext        , 72,   ()                                ) /*return-char **/                                                                         \
/* ACTION(act_settextCOUNT   , 32,   ((int, count))                    ) */ /*var1=число выводимых в данный момент символов*/                                   \
ACTION(act_setfile        , 73,   ()                                ) /*var1-(char *)filename*/                                                                 \
ACTION(act_submove        , 74,   ((int, x), (int, y))              ) /*var1-x,var2-y*/                                                                         \
ACTION(act_setarmy        , 75,   ((int, narmy))                    ) /*var1-narmy*/                                                                            \
ACTION(act_getarmy        , 76,   ()                                ) /*return-narmy*/                                                                          \
ACTION(act_setbuildtime   , 77,   ((int, build_time))               ) /*var1-buildtime*/                                                                        \
ACTION(act_getbuildtime   , 78,   ()                                ) /*return-buildtime*/                                                                      \
ACTION(act_setstate       , 79,   ((int, new_state))                ) /*var1-new state*/                                                                        \
ACTION(act_getstate       , 80,   ()                                ) /*return-state*/                                                                          \
ACTION(act_getbehave      , 81,   ()                                ) /*return behave*/                                                                         \
ACTION(act_getaction      , 82,   ()                                ) /*return current action*/                                                                 \
ACTION(act_load_object    , 83,   ((int, target_id))                ) /*var1 loaded object*/                                                                    \
ACTION(act_unload_object  , 84,   ((int, target_id))                ) /*var1-unloaded object*/                                                                  \



// Implementation

enum class Action : std::uint8_t {
#define _ACTION_DECLARE_ENUM_MEMBERS(NAME, VALUE, _PARAMS) NAME = VALUE,
    GROMADA_ACTIONS_XMACRO_LIST(_ACTION_DECLARE_ENUM_MEMBERS)
};

constexpr std::string_view to_string(Action action) noexcept {
#define _ACTION_DECLARE_TO_STRING_SWITCH_CASES(NAME, VALUE, _PARAMS) case Action::NAME: return std::string_view{#NAME};
    switch (action) {
        GROMADA_ACTIONS_XMACRO_LIST(_ACTION_DECLARE_TO_STRING_SWITCH_CASES)
	default:
		return std::string_view{"???"};
	}
}