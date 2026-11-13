// Derived from Menyoo PC, Copyright(C)2019 MAFINS, GPL-3.0-or-later.
#pragma once
namespace menyoo_vr {
template<class Stats>bool maxAllStoryStats(Stats&stats){
 bool complete=true;
			const char* spChars[] = { "SP0_", "SP1_", "SP2_" };
			for (auto& ch : spChars)
			{
				auto setInt = [&](const char* name, int val) {
					if (!stats.setInt(std::string(ch) + name, val)) complete = false;
				};
				auto setFloat = [&](const char* name, float val) {
					if (!stats.setFloat(std::string(ch) + name, val)) complete = false;
				};

				setFloat("DIST_RUNNING", 20000.0f);
				setInt("TIME_SWIMMING", 100);
				setInt("TIME_DRIVING_BICYCLE", 100);
				setInt("UNARMED_HITS", 2500);
				if (!stats.increment(std::string(ch) + "TIME_UNDERWATER", 6000.0f)) complete = false;
				setInt("NUMBER_NEAR_MISS", 6000);
				setInt("TIME_DRIVING_PLANE", 1000);
				setInt("TIME_DRIVING_HELI", 1000);
				setInt("PLANE_LANDINGS", 100);
				setInt("HITS_MISSION", 5000);
				setInt("HITS_PEDS_VEHICLES", 10000);
				setFloat("DIST_WALK_ST", 5000.0f);
				setInt("KILLS_STEALTH", 200);
				setInt("SPECIAL_ABILITY_UNLOCKED", 100);

				setInt("STAMINA", 100);
				setInt("STRENGTH", 100);
				setInt("LUNG_CAPACITY", 100);
				setInt("WHEELIE_ABILITY", 100);
				setInt("FLYING_ABILITY", 100);
				setInt("SHOOTING_ABILITY", 100);
				setInt("STEALTH_ABILITY", 100);

				if (!stats.setBool(std::string(ch) + "STAMINA_MAXED", true)) complete = false;
				if (!stats.setBool(std::string(ch) + "STRENGTH_MAXED", true)) complete = false;
				if (!stats.setBool(std::string(ch) + "LUNG_CAPACITY_MAXED", true)) complete = false;
				if (!stats.setBool(std::string(ch) + "WHEELIE_ABILITY_MAXED", true)) complete = false;
				if (!stats.setBool(std::string(ch) + "FLYING_ABILITY_MAXED", true)) complete = false;
				if (!stats.setBool(std::string(ch) + "SHOOTING_ABILITY_MAXED", true)) complete = false;
				if (!stats.setBool(std::string(ch) + "STEALTH_ABILITY_MAXED", true)) complete = false;
			}

 return complete&&stats.autoSave();
}
}
