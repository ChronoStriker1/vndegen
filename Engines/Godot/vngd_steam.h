// GodotSteam's Steam class and singleton, for games whose own copy of Godot has GodotSteam in it, built in or as an
// extension. VNDegen runs them as Steam would run offline: Steam starts, the player is signed in and owns the game,
// stats load and store at once, achievements and stats last until the game quits, and leaderboard requests fail. The
// members follow GodotSteam's, covering what the library's Steam games call. Both Godot modules include this after their
// own Godot headers.
#pragma once

#include "core/version.h"

#if VERSION_MAJOR >= 4
#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#else
#include "core/class_db.h"
#include "core/engine.h"
#include "core/os/os.h"
#endif

#include <initializer_list>

namespace vngd {

// Set by vngd_start for the game about to start. The module registers the class when it is set.
extern bool steam_wanted;

class Steam : public Object {
	GDCLASS(Steam, Object);

	// What the game set since it started. Games keep their own records and only report to Steam.
	Dictionary achievements;
	Dictionary stats;
	// The app ID the game started Steam with, or 0 when it set its environment's SteamAppId instead.
	uint32_t app_id = 0;

	// What Steam answers a request with, on the next frame as a callback would arrive.
	template <typename... Args>
	void answer(const char *p_signal, Args... p_args) {
		call_deferred("emit_signal", p_signal, p_args...);
	}

public:
	// Steam's API name for the player's language, from the system's locale.
	String language() {
		static const struct {
			const char *code;
			const char *name;
		} names[] = {
			{ "ar", "arabic" }, { "bg", "bulgarian" }, { "cs", "czech" }, { "da", "danish" }, { "de", "german" },
			{ "el", "greek" }, { "en", "english" }, { "fi", "finnish" }, { "fr", "french" }, { "hu", "hungarian" },
			{ "id", "indonesian" }, { "it", "italian" }, { "ja", "japanese" }, { "ko", "koreana" }, { "nb", "norwegian" },
			{ "nl", "dutch" }, { "no", "norwegian" }, { "pl", "polish" }, { "ro", "romanian" }, { "ru", "russian" },
			{ "sv", "swedish" }, { "th", "thai" }, { "tr", "turkish" }, { "uk", "ukrainian" }, { "vi", "vietnamese" },
		};
		const String locale = OS::get_singleton()->get_locale();
		const String code = locale.get_slice("_", 0).to_lower();
		const String country = locale.get_slice("_", 1).to_upper();
		if (code == "zh") {
			return country == "TW" || country == "HK" || country == "MO" ? "tchinese" : "schinese";
		}
		if (code == "pt") {
			return country == "BR" ? "brazilian" : "portuguese";
		}
		if (code == "es") {
			return country == "ES" || country.length() == 0 ? "spanish" : "latam";
		}
		for (const auto &entry : names) {
			if (code == entry.code) {
				return entry.name;
			}
		}
		return "english";
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("steamInit", "retrieve_stats", "app_id", "embed_callbacks"), &Steam::steamInit, DEFVAL(Variant()), DEFVAL(Variant()), DEFVAL(Variant()));
		ClassDB::bind_method(D_METHOD("steamInitEx", "retrieve_stats", "app_id", "embed_callbacks"), &Steam::steamInitEx, DEFVAL(Variant()), DEFVAL(Variant()), DEFVAL(Variant()));
		ClassDB::bind_method(D_METHOD("steamShutdown"), &Steam::nothing);
		ClassDB::bind_method(D_METHOD("run_callbacks"), &Steam::nothing);
		ClassDB::bind_method(D_METHOD("isSteamRunning"), &Steam::yes);
		ClassDB::bind_method(D_METHOD("loggedOn"), &Steam::yes);
		ClassDB::bind_method(D_METHOD("isSubscribed"), &Steam::yes);
		ClassDB::bind_method(D_METHOD("getAppID"), &Steam::getAppID);
		ClassDB::bind_method(D_METHOD("getSteamID"), &Steam::getSteamID);
		ClassDB::bind_method(D_METHOD("getPersonaName"), &Steam::getPersonaName);
		ClassDB::bind_method(D_METHOD("getFriendPersonaName", "steam_id"), &Steam::getFriendPersonaName);
		ClassDB::bind_method(D_METHOD("getCurrentGameLanguage"), &Steam::language);
		ClassDB::bind_method(D_METHOD("getSteamUILanguage"), &Steam::language);

		ClassDB::bind_method(D_METHOD("requestCurrentStats"), &Steam::requestCurrentStats);
		ClassDB::bind_method(D_METHOD("storeStats"), &Steam::storeStats);
		ClassDB::bind_method(D_METHOD("resetAllStats", "achievements_too"), &Steam::resetAllStats, DEFVAL(true));
		ClassDB::bind_method(D_METHOD("getAchievement", "achievement_name"), &Steam::getAchievement);
		ClassDB::bind_method(D_METHOD("setAchievement", "achievement_name"), &Steam::setAchievement);
		ClassDB::bind_method(D_METHOD("clearAchievement", "achievement_name"), &Steam::clearAchievement);
		ClassDB::bind_method(D_METHOD("getStatInt", "stat_name"), &Steam::getStatInt);
		ClassDB::bind_method(D_METHOD("getStatFloat", "stat_name"), &Steam::getStatFloat);
		ClassDB::bind_method(D_METHOD("setStatInt", "stat_name", "value"), &Steam::setStatInt);
		ClassDB::bind_method(D_METHOD("setStatFloat", "stat_name", "value"), &Steam::setStatFloat);

		ClassDB::bind_method(D_METHOD("findLeaderboard", "leaderboard_name"), &Steam::findLeaderboard);
		ClassDB::bind_method(D_METHOD("getLeaderboardName", "leaderboard_handle"), &Steam::getLeaderboardName, DEFVAL(0));
		ClassDB::bind_method(D_METHOD("downloadLeaderboardEntries", "start", "end", "type", "leaderboard_handle"), &Steam::downloadLeaderboardEntries, DEFVAL(0), DEFVAL(0));
		ClassDB::bind_method(D_METHOD("uploadLeaderboardScore", "score", "keep_best", "details", "leaderboard_handle"), &Steam::uploadLeaderboardScore, DEFVAL(true), DEFVAL(Variant()), DEFVAL(0));

		ADD_SIGNAL(MethodInfo("user_stats_received", PropertyInfo(Variant::INT, "game_id"), PropertyInfo(Variant::INT, "result"), PropertyInfo(Variant::INT, "user_id")));
		ADD_SIGNAL(MethodInfo("current_stats_received", PropertyInfo(Variant::INT, "game_id"), PropertyInfo(Variant::INT, "result"), PropertyInfo(Variant::INT, "user_id")));
		ADD_SIGNAL(MethodInfo("user_stats_stored", PropertyInfo(Variant::INT, "game_id"), PropertyInfo(Variant::INT, "result")));
		ADD_SIGNAL(MethodInfo("leaderboard_find_result", PropertyInfo(Variant::INT, "leaderboard_handle"), PropertyInfo(Variant::INT, "found")));
		ADD_SIGNAL(MethodInfo("leaderboard_score_uploaded", PropertyInfo(Variant::BOOL, "success"), PropertyInfo(Variant::INT, "this_handle"), PropertyInfo(Variant::DICTIONARY, "this_score")));
		ADD_SIGNAL(MethodInfo("leaderboard_scores_downloaded", PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::INT, "this_leaderboard_handle"), PropertyInfo(Variant::ARRAY, "leaderboard_entries")));
		ADD_SIGNAL(MethodInfo("steamworks_error", PropertyInfo(Variant::STRING, "failed_signal"), PropertyInfo(Variant::STRING, "io_failure")));

		ClassDB::bind_integer_constant(get_class_static(), StringName(), "LEADERBOARD_DATA_REQUEST_GLOBAL", 0);
		ClassDB::bind_integer_constant(get_class_static(), StringName(), "LEADERBOARD_DATA_REQUEST_GLOBAL_AROUND_USER", 1);
		ClassDB::bind_integer_constant(get_class_static(), StringName(), "LEADERBOARD_DATA_REQUEST_FRIENDS", 2);
		ClassDB::bind_integer_constant(get_class_static(), StringName(), "LEADERBOARD_DATA_REQUEST_USERS", 3);
	}

public:
	// steamInit answers with Steam's k_EResultOK, steamInitEx with k_ESteamAPIInitResult_OK. Steam then loads the
	// player's stats.
	Dictionary steamInit(const Variant &p_a, const Variant &p_b, const Variant &p_c) { return start(1, p_a, p_b, p_c); }
	Dictionary steamInitEx(const Variant &p_a, const Variant &p_b, const Variant &p_c) { return start(0, p_a, p_b, p_c); }
	Dictionary start(int p_status, const Variant &p_a, const Variant &p_b, const Variant &p_c) {
		// GodotSteam versions order the arguments differently. The app ID is the only number among them.
		for (const Variant *argument : { &p_a, &p_b, &p_c }) {
			if (argument->get_type() == Variant::INT && (int64_t)*argument > 0) {
				app_id = (int64_t)*argument;
			}
		}
		requestCurrentStats();
		Dictionary result;
		result["status"] = p_status;
		result["verbal"] = "Steamworks active.";
		return result;
	}
	void nothing() {}
	bool yes() { return true; }
	uint32_t getAppID() { return app_id ? app_id : OS::get_singleton()->get_environment("SteamAppId").to_int(); }
	// The first individual account ID, as a Steam ID.
	uint64_t getSteamID() { return 76561197960265729ULL; }
	String getPersonaName() { return "Player"; }
	String getFriendPersonaName(const Variant &) { return getPersonaName(); }

	bool requestCurrentStats() {
		answer("current_stats_received", getAppID(), 1, getSteamID());
		answer("user_stats_received", getAppID(), 1, getSteamID());
		return true;
	}
	bool storeStats() {
		answer("user_stats_stored", getAppID(), 1);
		return true;
	}
	bool resetAllStats(bool p_achievements_too) {
		stats.clear();
		if (p_achievements_too) {
			achievements.clear();
		}
		return true;
	}
	Dictionary getAchievement(const String &p_name) {
		Dictionary result;
		result["ret"] = true;
		result["achieved"] = achievements.has(p_name);
		return result;
	}
	bool setAchievement(const String &p_name) {
		achievements[p_name] = true;
		return true;
	}
	bool clearAchievement(const String &p_name) {
		achievements.erase(p_name);
		return true;
	}
	int32_t getStatInt(const String &p_name) { return stats.get(p_name, 0); }
	float getStatFloat(const String &p_name) { return stats.get(p_name, 0.0); }
	bool setStatInt(const String &p_name, int32_t p_value) {
		stats[p_name] = p_value;
		return true;
	}
	bool setStatFloat(const String &p_name, float p_value) {
		stats[p_name] = p_value;
		return true;
	}

	void findLeaderboard(const String &) { answer("leaderboard_find_result", 0, 0); }
	String getLeaderboardName(const Variant &) { return ""; }
	void downloadLeaderboardEntries(const Variant &, const Variant &, const Variant &, const Variant &p_handle) {
		answer("leaderboard_scores_downloaded", "", p_handle, Array());
	}
	void uploadLeaderboardScore(const Variant &, const Variant &, const Variant &, const Variant &p_handle) {
		answer("leaderboard_score_uploaded", false, p_handle, Dictionary());
	}
};

} // namespace vngd
