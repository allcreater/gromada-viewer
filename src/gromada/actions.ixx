module;
#include <cstdint>

export module Gromada.Actions;
#include <cstdint>

import std;

export enum class Action : std::uint8_t {
	act_stand = 0,
	act_build = 1,
	act_go = 2,
	act_start_move = 3,
	act_l_rotate = 4,
	act_r_rotate = 5,
	act_open = 6,
	act_close = 7,
	act_fight = 8,
	act_salut = 9,
	act_stand_open = 10,
	act_load = 11,
	act_unload = 12,
	act_wound = 13,
	act_birth = 14,
	act_death = 15,
	act_path_block = 31,
	act_additem = 32,
	//act_settextcount = 32,
	act_deleteitem = 33,
	act_askitem = 34,
	act_changeweapon = 35,
	act_deleteallitem = 36,
	act_changeaction = 37,
	act_path_limit = 38,
	act_attack = 39 /* p1 - target */,
	act_move = 40,
	act_buildunit = 41,
	act_drawselected = 42,
	act_damage = 43,
	act_select = 44,
	act_nextcommand = 45,
	act_init = 46,
	act_uncom = 47 /* запрещает выполнение дальнейших команд com для building */,
	act_deletetarget = 48,
	act_calltact = 49,
	act_save = 51,
	act_restore = 52,
	act_destroy = 54,
	act_coor_attack = 56,
	act_child = 57,
	act_saveframe = 58,
	act_fullsave = 59 /* записать всю информацию относящуюся к данному sprite, кроме записанной в act_save */,
	act_gettarget = 60,
	act_cyclecommand = 61 /* безусловный переход к команде номер p1  */,
	act_changevid = 62 /* изменить текущий curvid для спрайта, var2 - new action, if var1==-1 action not changed */,
	act_addammo = 63 /* var1-число добавляемых снарядов, var2 - номер оружия */,
	act_getammo = 64,
	act_gethp = 65,
	act_clearcom = 66,
	act_setbehave = 67,
	act_changecoor = 68 /* var1-x,var2-y */,
	act_getlink = 69,
	act_jump = 70,
	act_settext = 71,
	act_gettext = 72,
	act_setfile = 73,
	act_submove = 74,
	act_setarmy = 75,
	act_getarmy = 76,
	act_setbuildtime = 77,
	act_getbuildtime = 78,
	act_setstate = 79,
	act_getstate = 80,
	act_getbehave = 81,
	act_getaction = 82,
	act_load_object = 83,
	act_unload_object = 84
};


export constexpr std::string_view to_string(Action action) {
	using std::literals::string_view_literals::operator""sv;

	switch (action) {
	case Action::act_stand:
		return "stand"sv;
	case Action::act_build:
		return "build"sv;
	case Action::act_go:
		return "go"sv;
	case Action::act_start_move:
		return "start_move"sv;
	case Action::act_l_rotate:
		return "l_rotate"sv;
	case Action::act_r_rotate:
		return "r_rotate"sv;
	case Action::act_open:
		return "open"sv;
	case Action::act_close:
		return "close"sv;
	case Action::act_fight:
		return "fight"sv;
	case Action::act_salut:
		return "salut"sv;
	case Action::act_stand_open:
		return "stand_open"sv;
	case Action::act_load:
		return "load"sv;
	case Action::act_unload:
		return "unload"sv;
	case Action::act_wound:
		return "wound"sv;
	case Action::act_birth:
		return "birth"sv;
	case Action::act_death:
		return "death"sv;
	case Action::act_path_block:
		return "path_block"sv;
	case Action::act_additem:
		return "additem / settextcount"sv;
	case Action::act_deleteitem:
		return "deleteitem"sv;
	case Action::act_askitem:
		return "askitem"sv;
	case Action::act_changeweapon:
		return "changeweapon"sv;
	case Action::act_deleteallitem:
		return "deleteallitem"sv;
	case Action::act_changeaction:
		return "changeaction"sv;
	case Action::act_path_limit:
		return "path_limit"sv;
	case Action::act_attack:
		return "attack"sv;
	case Action::act_move:
		return "move"sv;
	case Action::act_buildunit:
		return "buildunit"sv;
	case Action::act_drawselected:
		return "drawselected"sv;
	case Action::act_damage:
		return "damage"sv;
	case Action::act_select:
		return "select"sv;
	case Action::act_nextcommand:
		return "nextcommand"sv;
	case Action::act_init:
		return "init"sv;
	case Action::act_uncom:
		return "uncom"sv;
	case Action::act_deletetarget:
		return "deletetarget"sv;
	case Action::act_calltact:
		return "calltact"sv;
	case Action::act_save:
		return "save"sv;
	case Action::act_restore:
		return "restore"sv;
	case Action::act_destroy:
        return "destroy"sv;
	case Action::act_coor_attack:
        return "coor_attack"sv;
	case Action::act_child:
	    return "child"sv;
	case Action::act_saveframe:
	    return "saveframe"sv;
	case Action::act_fullsave:
	    return "fullsave"sv;
	case Action::act_gettarget:
	    return "gettarget"sv;
	case Action::act_cyclecommand:
	    return "cyclecommand"sv;
	case Action::act_changevid:
	    return "changevid"sv;
	case Action::act_addammo:
	    return "addammo"sv;
	case Action::act_getammo:
	    return "getammo"sv;
	case Action::act_gethp:
	    return "gethp"sv;
	case Action::act_clearcom:
	    return "clearcom"sv;
	case Action::act_setbehave:
	    return "setbehave"sv;
	case Action::act_changecoor:
	    return "changecoor"sv;
	case Action::act_getlink:
	    return "getlink"sv;
	case Action::act_jump:
	    return "jump"sv;
	case Action::act_settext:
	    return "settext"sv;
	case Action::act_gettext:
	    return "gettext"sv;
	case Action::act_setfile:
	    return "setfile"sv;
	case Action::act_submove:
	    return "submove"sv;
	case Action::act_setarmy:
	    return "setarmy"sv;
	case Action::act_getarmy:
	    return "getarmy"sv;
	case Action::act_setbuildtime:
	    return "setbuildtime"sv;
	case Action::act_getbuildtime:
	    return "getbuildtime"sv;
	case Action::act_setstate:
	    return "setstate"sv;
	case Action::act_getstate:
	    return "getstate"sv;
	case Action::act_getbehave:
	    return "getbehave"sv;
	case Action::act_getaction:
	    return "getaction"sv;
	case Action::act_load_object:
	    return "load_object"sv;
	case Action::act_unload_object:
	    return "unload_object"sv;
	default:
		return "???"sv;
	}
}