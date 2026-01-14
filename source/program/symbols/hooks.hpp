// Hook function pointer declarations
// These correspond to hook sites in game code that we install trampolines/inline hooks at.
// The actual hook callbacks are defined in the respective hook files (main.cpp, util.hpp, etc).

/* Early hooks (main module). */
FUNC_PTR(main_init,                     void (*)());
FUNC_PTR(gfx_init,                      BOOL (*)());
FUNC_PTR(game_common_data_init,         void (*)(GameCommonData *, const GameParams *));
FUNC_PTR(shell_initialize,              BOOL (*)(uint32, uint32));
FUNC_PTR(sgame_initialize,              SGameID (*)(GameParams *, const OnlineService::PlayerResolveData *));
FUNC_PTR(sinitialize_world,             void (*)(SNO, uintptr_t *));

/* Utility hooks (main module). */
FUNC_PTR(cmd_line_parse,                void (*)(const char *));
FUNC_PTR(var_res_label,                 void (*)() /* Inline hook site */);
FUNC_PTR(move_speed,                    float (*)(ActorCommonData *, Player *, SNO, float));
FUNC_PTR(attack_speed,                  float (*)(AttribGroupID, ActorCommonData *, SNO, uint8 *, int32, int32));
FUNC_PTR(floating_dmg,                  void (*)() /* Inline hook site */);
FUNC_PTR(font_string_get_rendered_size, void (*)() /* Inline hook site */);
FUNC_PTR(font_string_draw_03ff50,       void (*)() /* Inline hook site */);
FUNC_PTR(font_string_draw_0010c8,       void (*)() /* Inline hook site */);
FUNC_PTR(font_string_draw_03e5b0,       void (*)() /* Inline hook site */);

/* Season/event hooks - see symbols/game.hpp for OnConfigFileRetrieved, OnSeasonsFileRetrieved, OnBlacklistFileRetrieved */

/* Lobby hooks (main module). */
FUNC_PTR(lobby_service_create,          void (*)(uint32));
FUNC_PTR(lobby_service_idle_internal,   void (*)(Console::Online::LobbyServiceInternal *));
FUNC_PTR(specifiers_from_modifier,      void (*)() /* Inline hook site */);
FUNC_PTR(eval_mod,                      void (*)() /* Inline hook site */);
FUNC_PTR(dupe_dropped_item,             BOOL (*)(const ACDID, const ACDID, const BOOL));
FUNC_PTR(request_drop_item,             void (*)(ActorCommonData *, const ACDID));

/* Debugging hooks (main module). */
FUNC_PTR(print_challenge_rift_failed,   void (*)() /* Inline hook site */);
FUNC_PTR(challenge_rift_callback,       void (*)(void *, Console::Online::StorageResult *, D3::ChallengeRifts::ChallengeData *, D3::Leaderboard::WeeklyChallengeData *));
FUNC_PTR(pubfile_data_hex,              void (*)() /* Inline hook site */);
FUNC_PTR(print_error_display,           void (*)() /* Inline hook site */);
FUNC_PTR(print_error_string_final,      void (*)() /* Inline hook site */);
