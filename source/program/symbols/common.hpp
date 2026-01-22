#pragma once

// #define _HOSTCHK        (ServerIsLocal() && (!GameIsStartUp() && GameActiveGameCommonDataIsServer()))
#define _HOSTCHK        (ServerIsLocal() && !GameIsStartUp())
#define _SERVERCODE_ON  (sg_bServerCode = (_GameServerCodeEnter() != 0));
#define _SERVERCODE_OFF (GameServerCodeLeave_Tracked(sg_bServerCode));

#include "symbols/base.hpp"
#include "symbols/player_actor.inc"
#include "symbols/hooks.inc"
#include "symbols/game.inc"
#include "symbols/items.inc"
#include "symbols/constructors.hpp"

#undef FUNC_PTR
