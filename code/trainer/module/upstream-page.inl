/*
* Menyoo PC - Grand Theft Auto V single-player trainer mod
* Copyright (C) 2019  MAFINS
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*/

namespace sub {
	void TimeMenu()
	{
		bool pauseClockToggle = false;
		bool hourPlus = false;
		bool hourMinus = false;
		bool minutePlus = false;
		bool minuteMinus = false;
		bool timeScalePlus = false;
		bool timeScaleMinus = false;

		int hour = GET_CLOCK_HOURS();
		int minute = GET_CLOCK_MINUTES();

		bool presetSunrise = false, presetNoon = false, presetSunset = false, presetNight = false;

		AddTitle("Time");
		AddOption("Clock (System Time)", null, nullFunc, SUB::CLOCK);
		AddToggle("Pause Clock Time", pauseClock, pauseClockToggle);
		AddOptionDescription("Freezes the in-game time.");
		AddToggle("Sync With System Time", syncClock);
		AddOptionDescription("Matches in-game time to your PC clock.");
		AddNumber("Hour", hour, 0, null, hourPlus, hourMinus);
		AddNumber("Minute", minute, 0, null, minutePlus, minuteMinus);
		AddNumber("World Speed", currentTimescale, 1, null, timeScalePlus, timeScaleMinus);
		AddOptionDescription("Game speed. Below 1.0 is slow motion.");
		AddBreak("---Presets---");
		AddOption("Sunrise (06:00)", presetSunrise);
		AddOption("Noon (12:00)", presetNoon);
		AddOption("Sunset (19:00)", presetSunset);
		AddOption("Night (23:00)", presetNight);

		if (presetSunrise) { NETWORK_OVERRIDE_CLOCK_TIME(6, 0, 0); if (pauseClock) { pauseClockH = 6; pauseClockM = 0; } return; }
		if (presetNoon) { NETWORK_OVERRIDE_CLOCK_TIME(12, 0, 0); if (pauseClock) { pauseClockH = 12; pauseClockM = 0; } return; }
		if (presetSunset) { NETWORK_OVERRIDE_CLOCK_TIME(19, 0, 0); if (pauseClock) { pauseClockH = 19; pauseClockM = 0; } return; }
		if (presetNight) { NETWORK_OVERRIDE_CLOCK_TIME(23, 0, 0); if (pauseClock) { pauseClockH = 23; pauseClockM = 0; } return; }

		if (hourPlus) 
		{
			if ((hour + 1) == 24)
			{
				if (GET_CLOCK_MONTH() + 1 == 12 && GET_CLOCK_DAY_OF_MONTH() + 1 == 30)
				{
					SET_CLOCK_DATE(1, 0, GET_CLOCK_YEAR() + 1);
				}
				else if (GET_CLOCK_DAY_OF_MONTH() + 1 == 30)
				{
					SET_CLOCK_DATE(1, GET_CLOCK_MONTH() + 1, GET_CLOCK_YEAR());
				}
				else
				{
					SET_CLOCK_DATE(GET_CLOCK_DAY_OF_MONTH() + 1, GET_CLOCK_MONTH(), GET_CLOCK_YEAR());
				}
				NETWORK_OVERRIDE_CLOCK_TIME(0, minute, GET_CLOCK_SECONDS());
			}
			else
			{
				NETWORK_OVERRIDE_CLOCK_TIME((hour + 1), minute, GET_CLOCK_SECONDS());
			}
			return;
		}
		if (hourMinus) 
		{
			if ((hour - 1) == -1)
			{
				if (GET_CLOCK_MONTH() - 1 == -1 && GET_CLOCK_DAY_OF_MONTH() - 1 == 0)
				{
					SET_CLOCK_DATE(28, 11, GET_CLOCK_YEAR() - 1);
				}
				else if (GET_CLOCK_DAY_OF_MONTH() - 1 == 0)
				{
					SET_CLOCK_DATE(28, GET_CLOCK_MONTH() - 1, GET_CLOCK_YEAR());
				}
				else
				{
					SET_CLOCK_DATE(GET_CLOCK_DAY_OF_MONTH() - 1, GET_CLOCK_MONTH(), GET_CLOCK_YEAR());
				}
				NETWORK_OVERRIDE_CLOCK_TIME(23, minute, GET_CLOCK_SECONDS());
			}
			else
			{
				NETWORK_OVERRIDE_CLOCK_TIME((hour - 1), minute, GET_CLOCK_SECONDS());
			}
			return;
		}

		if (minutePlus) 
		{
			if ((minute + 1) == 60)
			{
				if (hour + 1 == 24)
				{
					if (GET_CLOCK_DAY_OF_MONTH() + 1 == 30)
					{
						SET_CLOCK_DATE(1, GET_CLOCK_MONTH() + 1, GET_CLOCK_YEAR());
					}
					else
					{
						SET_CLOCK_DATE(GET_CLOCK_DAY_OF_MONTH() + 1, GET_CLOCK_MONTH(), GET_CLOCK_YEAR());
					}
					NETWORK_OVERRIDE_CLOCK_TIME(0, 0, GET_CLOCK_SECONDS());
				}
				else
				{
					NETWORK_OVERRIDE_CLOCK_TIME(hour + 1, 0, GET_CLOCK_SECONDS());
				}
			}
			else
			{
				NETWORK_OVERRIDE_CLOCK_TIME(hour, minute + 1, GET_CLOCK_SECONDS());
			}
			return;
		}
		if (minuteMinus) 
		{
			if ((minute - 1) == -1)
			{
				if (hour - 1 == -1)
				{
					if (GET_CLOCK_DAY_OF_MONTH() - 1 == 0)
					{
						SET_CLOCK_DATE(29, GET_CLOCK_MONTH() - 1, GET_CLOCK_YEAR());
					}
					else
					{
						SET_CLOCK_DATE(GET_CLOCK_DAY_OF_MONTH() - 1, GET_CLOCK_MONTH(), GET_CLOCK_YEAR());
					}
					NETWORK_OVERRIDE_CLOCK_TIME(23, 59, GET_CLOCK_SECONDS());
				}
				else
				{
					NETWORK_OVERRIDE_CLOCK_TIME(hour - 1, 59, GET_CLOCK_SECONDS());
				}
			}
			else
			{
				NETWORK_OVERRIDE_CLOCK_TIME(hour, minute - 1, GET_CLOCK_SECONDS());
			}
			return;
		}

		if (timeScalePlus && currentTimescale < 1.1f) 
		{ 
			currentTimescale += 0.1f; 
			SET_TIME_SCALE(currentTimescale); 
			return; 
		}

		if (timeScaleMinus && currentTimescale > 0.0f) 
		{ 
			currentTimescale -= 0.1f; 
			SET_TIME_SCALE(currentTimescale); 
			return; 
		}

		if (pauseClockToggle) 
		{ 
			pauseClockH = GET_CLOCK_HOURS();
			pauseClockM = GET_CLOCK_MINUTES(); 
			return; 
		}
	}

}
