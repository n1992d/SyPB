//
// Copyright (c) 2003-2009, by Yet Another POD-Bot Development Team.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// $Id:$
//

#include <core.h>

ConVar sypb_autovacate ("sypb_autovacate", "-1");
ConVar sypb_quota ("sypb_quota", "0");
ConVar sypb_forceteam ("sypb_forceteam", "any");
/*
ConVar sypb_minskill ("sypb_minskill", "60");
ConVar sypb_maxskill ("sypb_maxskill", "100"); */


ConVar sypb_tagbots ("sypb_tagbots", "0");

ConVar sypb_difficulty("sypb_difficulty", "4");

ConVar sypb_join_after_player ("sypb_join_after_player", "0"); 

BotControl::BotControl (void)
{
   // this is a bot manager class constructor

   m_lastWinner = -1;

   m_economicsGood[TEAM_TERRORIST] = true;
   m_economicsGood[TEAM_COUNTER] = true;

   memset (m_bots, 0, sizeof (m_bots));
   InitQuota ();
}

BotControl::~BotControl (void)
{
   // this is a bot manager class destructor, do not use engine->GetMaxClients () here !!

   for (int i = 0; i < 32; i++)
   {
      if (m_bots[i])
      {
         delete m_bots[i];
         m_bots[i] = null;
      }
   }
}

void BotControl::CallGameEntity (entvars_t *vars)
{
   // this function calls gamedll player() function, in case to create player entity in game

   if (g_isMetamod)
   {
      CALL_GAME_ENTITY (PLID, "player", vars);
      return;
   }

   static EntityPtr_t playerFunction = null;

   if (playerFunction == null)
      playerFunction = (EntityPtr_t) g_gameLib->GetFunctionAddr ("player");

   if (playerFunction != null)
      (*playerFunction) (vars);
}

// SyPB Pro P.20 - Bot Name
int BotControl::CreateBot(String name, int skill, int personality, int team, int member)
{
	// this function completely prepares bot entity (edict) for creation, creates team, skill, sets name etc, and
	// then sends result to bot constructor

#if defined(PRODUCT_DEV_VERSION)
	// SyPB Pro P.34 - Preview DeadLine
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	int year = timeinfo->tm_year + 1900, mon = timeinfo->tm_mon + 1, day = timeinfo->tm_mday;
	int today = (year * 10000) + (mon * 100) + (day);
	int deadline = (PV_VERSION_YEAR * 10000) + (PV_VERSION_MON * 100) + PV_VERSION_DAY;

	if ((deadline - today) < 0)
	{
		m_creationTab.RemoveAll(); // something wrong with waypoints, reset tab of creation
		sypb_quota.SetInt(0); // reset quota

		ChartPrint("[SyPB Preview] This Preview version outdate *****");
		ChartPrint("[SyPB Preview] This Preview version outdate *****");
		ChartPrint("[SyPB Preview] This Preview version outdate *****");
		return -3;
	}
#endif

	// SyPB Pro P.22 - join after player
	if (sypb_join_after_player.GetBool() == true)
	{
		int playerNum = GetHumansNum(1);
		if (playerNum == 0)
			return -3;
	}

	edict_t *bot = null;
	char outputName[33];

	if (g_numWaypoints < 1) // don't allow creating bots with no waypoints loaded
	{
		CenterPrint("Map not waypointed. Can't Create Bot");
		return -1;
	}
	else if (g_waypointsChanged) // don't allow creating bots with changed waypoints (distance tables are messed up)
	{
		CenterPrint("Waypoints has been changed. Load waypoints again...");
		return -1;
	}

	// SyPB Pro P.35 - New skill level
	if (skill < 0 || skill > 100)
	{
		if (sypb_difficulty.GetInt() >= 4)
			skill = 100;
		else if (sypb_difficulty.GetInt() == 3)
			skill = engine->RandomInt(80, 99);
		else if (sypb_difficulty.GetInt() == 2)
			skill = engine->RandomInt(50, 79);
		else if (sypb_difficulty.GetInt() == 1)
			skill = engine->RandomInt(30, 49);
		else if (sypb_difficulty.GetInt() <= 0)
			skill = engine->RandomInt(10, 29);
	}

	if (personality < 0 || personality > 2)
	{
		int randomPrecent = engine->RandomInt(0, 100);

		if (randomPrecent < 50)
			personality = PERSONALITY_NORMAL;
		else
		{
			if (engine->RandomInt(0, randomPrecent) < randomPrecent * 0.5)
				personality = PERSONALITY_CAREFUL;
			else
				personality = PERSONALITY_RUSHER;
		}
	}

	if (name.GetLength() <= 0)  // SyPB Pro P.30 - Set Bot Name
	{
		bool getName = false;
		if (!g_botNames.IsEmpty())
		{
			ITERATE_ARRAY(g_botNames, j)
			{
				if (!g_botNames[j].isUsed)
				{
					getName = true;
					break;
				}
			}
		}

		if (getName)
		{
			bool nameUse = true;

			while (nameUse)
			{
				NameItem &botName = g_botNames.GetRandomElement();
				if (!botName.isUsed)
				{
					nameUse = false;
					botName.isUsed = true;
					sprintf(outputName, "%s", botName.name);
				}
			}
		}
		else
			sprintf(outputName, "[SyPB] bot%i", engine->RandomInt(-9999, 9999)); // just pick ugly random name
	}
	else
		sprintf(outputName, "[SyPB] %s", name);



	if (FNullEnt((bot = (*g_engfuncs.pfnCreateFakeClient) (outputName))))
	{
		CenterPrint("Maximum players reached (%d/%d). Unable to create Bot.", engine->GetMaxClients(), engine->GetMaxClients());
		return -2;
	}

	int index = ENTINDEX(bot) - 1;

	InternalAssert(index >= 0 && index <= 32); // check index
	InternalAssert(m_bots[index] == null); // check bot slot

	m_bots[index] = new Bot(bot, skill, personality, team, member);

	if (m_bots == null)
		TerminateOnMalloc();

	ServerPrint("Connecting SyPB Bot - [%s] Skill [%d]", STRING(bot->v.netname), skill);

	return index;
}


int BotControl::GetIndex (edict_t *ent)
{
   // this function returns index of bot (using own bot array)

   if (FNullEnt (ent))
      return -1;

   int index = ENTINDEX (ent) - 1;

   if (index < 0 || index >= 32)
      return -1;

   if (m_bots[index] != null)
      return index;

   return -1; // if no edict, return -1;
}

Bot *BotControl::GetBot (int index)
{
   // this function finds a bot specified by index, and then returns pointer to it (using own bot array)

   if (index < 0 || index >= 32)
      return null;

   if (m_bots[index] != null)
      return m_bots[index];

   return null; // no bot
}

Bot *BotControl::GetBot (edict_t *ent)
{
   // same as above, but using bot entity

   return GetBot (GetIndex (ent));
}

Bot *BotControl::FindOneValidAliveBot (void)
{
   // this function finds one bot, alive bot :)

   Array <int> foundBots;

   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      if (m_bots[i] != null && IsAlive (m_bots[i]->GetEntity ()))
         foundBots.Push (i);
   }

   if (!foundBots.IsEmpty ())
      return m_bots[foundBots.GetRandomElement ()];

   return null;
}

void BotControl::Think(void)
{

	// this function calls think () function for all available at call moment bots, and
	// try to catch internal error if such shit occurs

	// SyPB Pro P.27 - OS Change
	for (int i = 0; i < engine->GetMaxClients(); i++)
	{
		if (m_bots[i] == null)
			continue;

		// SyPB Pro P.34 - Game think change
		if (m_bots[i]->m_thinkTimer <= engine->GetTime())
		{
			m_bots[i]->m_thinkTimer = engine->GetTime() + 1.0f / 24.9f;

			if (m_bots[i]->m_lastThinkTime <= engine->GetTime())
			{
				m_bots[i]->Think();

				m_bots[i]->m_moveAnglesForRunMove = m_bots[i]->m_moveAngles;
				m_bots[i]->m_moveSpeedForRunMove = m_bots[i]->m_moveSpeed;
				m_bots[i]->m_strafeSpeedForRunMove = m_bots[i]->m_strafeSpeed;

				// SyPB Pro P.35 - New bot action
				if (m_bots[i]->pev->button & IN_JUMP)
					m_bots[i]->m_moveUPSpeedForRunMove = m_bots[i]->pev->maxspeed;
				else if (m_bots[i]->pev->button & IN_DUCK)
					m_bots[i]->m_moveUPSpeedForRunMove = -m_bots[i]->pev->maxspeed;
				else
					m_bots[i]->m_moveUPSpeedForRunMove = 0.0f;
			}

			// SyPB Pro P.35 - Bot think improve
			if (!FNullEnt(m_bots[i]->m_enemy))
			{
				float gameFps = CVAR_GET_FLOAT("fps_max");
				if (gameFps > 60)
					gameFps = 59.9f;
				else if (gameFps < 30)
					gameFps = 29.9f;
				else
					gameFps -= 0.1f;

				m_bots[i]->m_thinkTimer = engine->GetTime() + 1.0f / gameFps;
			}
		}
		// SyPB Pro P.35 - Bot think improve
		else if (!FNullEnt(m_bots[i]->m_enemy))
		{
			m_bots[i]->ChooseAimDirection();
			m_bots[i]->FacePosition();

			if (m_bots[i]->m_wantsToFire && !m_bots[i]->m_isUsingGrenade && m_bots[i]->m_shootTime <= engine->GetTime())
				m_bots[i]->FireWeapon(); 
		}
		else
			m_bots[i]->FacePosition();

		m_bots[i]->pev->angles.ClampAngles();
		m_bots[i]->pev->v_angle.ClampAngles();

		m_bots[i]->RunPlayerMovement(); // run the player movement

		// SyPB Pro P.29 - Bot fps
		//m_bots[i]->Think();
	}
}


void BotControl::AddBot (const String &name, int skill, int personality, int team, int member)
{
   // this function putting bot creation process to queue to prevent engine crashes

   CreateItem queueID;

   // fill the holder
   queueID.name = name;
   queueID.skill = skill;
   queueID.personality = personality;
   queueID.team = team;
   queueID.member = member;

   // put to queue
   m_creationTab.Push (queueID);

   // keep quota number up to date
   if (GetBotsNum () + 1 > sypb_quota.GetInt ())
      sypb_quota.SetInt (GetBotsNum () + 1);
}

void BotControl::AddBot (const String &name, const String &skill, const String &personality, const String &team, const String &member)
{
   // this function is same as the function above, but accept as parameters string instead of integers

   CreateItem queueID;
   const String &any = "*";

   queueID.name = (name.IsEmpty () || (name == any)) ?  String ("\0") : name;
   queueID.skill = (skill.IsEmpty () || (skill == any)) ? -1 : skill;
   queueID.team = (team.IsEmpty () || (team == any)) ? -1 : team;
   queueID.member = (member.IsEmpty () || (member == any)) ? -1 : member;
   queueID.personality = (personality.IsEmpty () || (personality == any)) ? -1 : personality;

   m_creationTab.Push (queueID);

   // keep quota number up to date
   if (GetBotsNum () + 1 > sypb_quota.GetInt ())
      sypb_quota.SetInt (GetBotsNum () + 1);
}

void BotControl::CheckAutoVacate (edict_t * /*ent*/)
{
   // this function sets timer to kick one bot off.

   if (sypb_autovacate.GetBool ())
      RemoveRandom ();
}

// SyPB Pro P.30 - AMXX API
int BotControl::AddBotAPI(const String &name, int skill, int team)
{
	if (g_botManager->GetBotsNum() + 1 > sypb_quota.GetInt())
		sypb_quota.SetInt(g_botManager->GetBotsNum() + 1);

	int resultOfCall = CreateBot(name, skill, -1, team, -1);

	// check the result of creation
	if (resultOfCall == -1)
	{
		m_creationTab.RemoveAll(); // something wrong with waypoints, reset tab of creation
		sypb_quota.SetInt(0); // reset quota

		// SyPB Pro P.23 - SgdWP
		ChartPrint("[SyPB] You can input [sypb sgdwp on] make the new waypoints!!");
	}
	else if (resultOfCall == -2)
	{
		m_creationTab.RemoveAll(); // maximum players reached, so set quota to maximum players
		sypb_quota.SetInt(GetBotsNum());
	}

	m_maintainTime = engine->GetTime() + 0.2f;

	return resultOfCall;  // SyPB Pro P.34 - AMXX API
}

void BotControl::MaintainBotQuota (void)
{
   // this function keeps number of bots up to date, and don't allow to maintain bot creation
   // while creation process in process.

   if (!m_creationTab.IsEmpty () && m_maintainTime < engine->GetTime ())
   {
      CreateItem last = m_creationTab.Pop ();

      int resultOfCall = CreateBot (last.name, last.skill, last.personality, last.team, last.member);

	  // check the result of creation
	  if (resultOfCall == -1)
	  {
		  m_creationTab.RemoveAll(); // something wrong with waypoints, reset tab of creation
		  sypb_quota.SetInt(0); // reset quota

		  // SyPB Pro P.23 - SgdWP
		  ChartPrint("[SyPB] You can input [sypb sgdwp on] make the new waypoints!!");
	  }
	  else if (resultOfCall == -2)
	  {
		  m_creationTab.RemoveAll(); // maximum players reached, so set quota to maximum players
		  sypb_quota.SetInt(GetBotsNum());
	  }

      m_maintainTime = engine->GetTime () + 0.2f;
   }

   // now keep bot number up to date
   if (m_maintainTime < engine->GetTime ())
   {
      int botNumber = GetBotsNum ();
      int humanNumber = GetHumansNum ();

      if (botNumber > sypb_quota.GetInt ())
         RemoveRandom ();

      if (sypb_autovacate.GetBool ())
      {
         if (botNumber < sypb_quota.GetInt () && botNumber < engine->GetMaxClients () - 1)
            AddRandom ();

         if (humanNumber >= engine->GetMaxClients ())
            RemoveRandom ();
      }
      else
      {
         if (botNumber < sypb_quota.GetInt () && botNumber < engine->GetMaxClients ())
            AddRandom ();
      }

      int botQuota = sypb_autovacate.GetBool () ? (engine->GetMaxClients () - 1 - (humanNumber + 1)) : engine->GetMaxClients ();

      // check valid range of quota
      if (sypb_quota.GetInt () > botQuota)
         sypb_quota.SetInt (botQuota);

      else if (sypb_quota.GetInt () < 0)
         sypb_quota.SetInt (0);

      m_maintainTime = engine->GetTime () + 0.25f;
   }
}

void BotControl::InitQuota (void)
{
   m_maintainTime = engine->GetTime () + 2.0f;
   m_creationTab.RemoveAll ();
}

void BotControl::FillServer (int selection, int personality, int skill, int numToAdd)
{
   // this function fill server with bots, with specified team & personality

   if (GetBotsNum () >= engine->GetMaxClients () - GetHumansNum ())
      return;

   if (selection == 1 || selection == 2)
   {
      CVAR_SET_STRING ("mp_limitteams", "0");
      CVAR_SET_STRING ("mp_autoteambalance", "0");
   }
   else
      selection = 5;

   char teamDescs[6][12] =
   {
      "",
      {"Terrorists"},
      {"CTs"},
      "",
      "",
      {"Random"},
   };

   int toAdd = numToAdd == -1 ? engine->GetMaxClients () - (GetHumansNum () + GetBotsNum ()) : numToAdd;

   for (int i = 0; i <= toAdd; i++)
   {
      // since we got constant skill from menu (since creation process call automatic), we need to manually
      // randomize skill here, on given skill there.
      int randomizedSkill = 0;

      if (skill >= 0 && skill <= 20)
         randomizedSkill = engine->RandomInt (0, 20);
      else if (skill >= 20 && skill <= 40)
         randomizedSkill = engine->RandomInt (20, 40);
      else if (skill >= 40 && skill <= 60)
         randomizedSkill = engine->RandomInt (40, 60);
      else if (skill >= 60 && skill <= 80)
         randomizedSkill = engine->RandomInt (60, 80);
      else if (skill >= 80 && skill <= 99)
         randomizedSkill = engine->RandomInt (80, 99);
      else if (skill == 100)
         randomizedSkill = skill;

      AddBot ("", randomizedSkill, personality, selection, -1);
   }

   sypb_quota.SetInt (toAdd);
   CenterPrint ("Fill Server with %s bots...", &teamDescs[selection][0]);
}

void BotControl::RemoveAll (void)
{
   // this function drops all bot clients from server (this function removes only sypb's)`q

   CenterPrint ("Bots are removed from server.");

   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      if (m_bots[i] != null)  // is this slot used?
         m_bots[i]->Kick ();
   }
   m_creationTab.RemoveAll ();

   // reset cvars
   sypb_quota.SetInt (0);
   sypb_autovacate.SetInt (0);
}

void BotControl::RemoveFromTeam (Team team, bool removeAll)
{
   // this function remove random bot from specified team (if removeAll value = 1 then removes all players from team)

   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      if (m_bots[i] != null && team == GetTeam (m_bots[i]->GetEntity ()))
      {
         m_bots[i]->Kick ();

         if (!removeAll)
            break;
      }
   }
}

void BotControl::RemoveMenu(edict_t *ent, int selection)
{
	if ((selection > 4) || (selection < 1))
		return;

	char tempBuffer[1024], buffer[1024];
	memset(tempBuffer, 0, sizeof(tempBuffer));
	memset(buffer, 0, sizeof(buffer));

	int validSlots = (selection == 4) ? (1 << 9) : ((1 << 8) | (1 << 9));
	// SyPB Pro P.30 - Debugs
	for (int i = ((selection - 1) * 8); i < selection * 8; ++i)
	{
		if ((m_bots[i] != null) && !FNullEnt(m_bots[i]->GetEntity()))
		{
			validSlots |= 1 << (i - ((selection - 1) * 8));
			sprintf(buffer, "%s %1.1d. %s%s\n", buffer, i - ((selection - 1) * 8) + 1, STRING(m_bots[i]->pev->netname), GetTeam(m_bots[i]->GetEntity()) == TEAM_COUNTER ? " \\y(CT)\\w" : " \\r(T)\\w");
		}
		else if (!FNullEnt(g_clients[i].ent))
			sprintf(buffer, "%s %1.1d.\\d %s (Not SyPB) \\w\n", buffer, i - ((selection - 1) * 8) + 1, STRING(g_clients[i].ent->v.netname));
		else
			sprintf(buffer, "%s %1.1d.\\d Null \\w\n", buffer, i - ((selection - 1) * 8) + 1);
	}

	sprintf(tempBuffer, "\\ySyPB Remove Menu (%d/4):\\w\n\n%s\n%s 0. Back", selection, buffer, (selection == 4) ? "" : " 9. More...\n");

	switch (selection)
	{
	case 1:
		g_menus[14].validSlots = validSlots & static_cast <unsigned int> (-1);
		g_menus[14].menuText = tempBuffer;

		DisplayMenuToClient(ent, &g_menus[14]);
		break;

	case 2:
		g_menus[15].validSlots = validSlots & static_cast <unsigned int> (-1);
		g_menus[15].menuText = tempBuffer;

		DisplayMenuToClient(ent, &g_menus[15]);
		break;

	case 3:
		g_menus[16].validSlots = validSlots & static_cast <unsigned int> (-1);
		g_menus[16].menuText = tempBuffer;

		DisplayMenuToClient(ent, &g_menus[16]);
		break;

	case 4:
		g_menus[17].validSlots = validSlots & static_cast <unsigned int> (-1);
		g_menus[17].menuText = tempBuffer;

		DisplayMenuToClient(ent, &g_menus[17]);
		break;
	}
}

void BotControl::KillAll (int team)
{
   // this function kills all bots on server (only this dll controlled bots)

   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      if (m_bots[i] != null)
      {
         if (team != -1 && team != GetTeam (m_bots[i]->GetEntity ()))
            continue;

         m_bots[i]->Kill ();
      }
   }
   CenterPrint ("All bots are killed.");
}

void BotControl::RemoveRandom (void)
{
   // this function removes random bot from server (only sypb's)

   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      if (m_bots[i] != null)  // is this slot used?
      {
         m_bots[i]->Kick ();
         break;
      }
   }
}

void BotControl::SetWeaponMode (int selection)
{
   // this function sets bots weapon mode
	// SyPB Pro P.27 - Game Mode Setting
	if (GetGameMod() != 0)
		return;

   int tabMapStandart[7][Const_NumWeapons] =
   {
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Knife only
      {-1,-1,-1, 2, 2, 0, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Pistols only
      {-1,-1,-1,-1,-1,-1,-1, 2, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Shotgun only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1, 2, 1, 2, 0, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 2,-1}, // Machine Guns only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 1, 0, 1, 1,-1,-1,-1,-1,-1,-1}, // Rifles only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 2, 2, 0, 1,-1,-1}, // Snipers only
      {-1,-1,-1, 2, 2, 0, 1, 2, 2, 2, 1, 2, 0, 2, 0, 0, 1, 0, 1, 1, 2, 2, 0, 1, 2, 1}  // Standard
   };

   int tabMapAS[7][Const_NumWeapons] =
   {
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Knife only
      {-1,-1,-1, 2, 2, 0, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Pistols only
      {-1,-1,-1,-1,-1,-1,-1, 1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Shotgun only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1, 1, 1, 1, 0, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1}, // Machine Guns only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0,-1, 1, 0, 1, 1,-1,-1,-1,-1,-1,-1}, // Rifles only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0,-1, 1,-1,-1}, // Snipers only
      {-1,-1,-1, 2, 2, 0, 1, 1, 1, 1, 1, 1, 0, 2, 0,-1, 1, 0, 1, 1, 0, 0,-1, 1, 1, 1}  // Standard
   };

   char modeName[7][12] =
   {
      {"Knife"},
      {"Pistol"},
      {"Shotgun"},
      {"Machine Gun"},
      {"Rifle"},
      {"Sniper"},
      {"Standard"}
   };
   selection--;

   for (int i = 0; i < Const_NumWeapons; i++)
   {
      g_weaponSelect[i].teamStandard = tabMapStandart[selection][i];
      g_weaponSelect[i].teamAS = tabMapAS[selection][i];
   }

   if (selection == 0)
      sypb_knifemode.SetInt (1);
   else
      sypb_knifemode.SetInt (0);

   CenterPrint ("%s weapon mode selected", &modeName[selection][0]);
}

void BotControl::ListBots (void)
{
   // this function list's bots currently playing on the server

   ServerPrintNoTag ("%-3.5s %-9.13s %-17.18s %-3.4s %-3.4s %-3.4s", "index", "name", "personality", "team", "skill", "frags");

   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      edict_t *player = INDEXENT (i);

      // is this player slot valid
      if (IsValidBot (player) != null && GetBot (player) != null)
         ServerPrintNoTag ("[%-3.1d] %-9.13s %-17.18s %-3.4s %-3.1d %-3.1d", i, STRING (player->v.netname), GetBot (player)->m_personality == PERSONALITY_RUSHER ? "rusher" : GetBot (player)->m_personality == PERSONALITY_NORMAL ? "normal" : "careful", GetTeam (player) != 0 ? "CT" : "T", GetBot (player)->m_skill, static_cast <int> (player->v.frags));
   }
}

int BotControl::GetBotsNum (void)
{
   // this function returns number of sypb's playing on the server

   int count = 0;

   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      if (m_bots[i] != null)
         count++;
   }
   return count;
}

int BotControl::GetHumansNum (int mod)
{
   // this function returns number of humans playing on the server

   int count = 0;

   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
	   /*
      if ((g_clients[i].flags & CFLAG_USED) && m_bots[i] == null)
         count++;
		 */

	   if ((g_clients[i].flags & CFLAG_USED) && m_bots[i] == null)
	   {
		   if (mod == 0)
			   count++;
		   else
		   {
			   int team = *((int*)INDEXENT (i+1)->pvPrivateData+OFFSET_TEAM);
			   if (team == (TEAM_COUNTER+1) || team == (TEAM_TERRORIST+1))
				   count++;
		   }
	   }
   }
   return count;
}


Bot *BotControl::GetHighestFragsBot (int team)
{
   Bot *highFragBot = null;

   int bestIndex = 0;
   float bestScore = -1;

   // search bots in this team
   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      highFragBot = g_botManager->GetBot (i);

      if (highFragBot != null && IsAlive (highFragBot->GetEntity ()) && GetTeam (highFragBot->GetEntity ()) == team)
      {
         if (highFragBot->pev->frags > bestScore)
         {
            bestIndex = i;
            bestScore = highFragBot->pev->frags;
         }
      }
   }
   return GetBot (bestIndex);
}

void BotControl::CheckTeamEconomics (int team)
{
   // this function decides is players on specified team is able to buy primary weapons by calculating players
   // that have not enough money to buy primary (with economics), and if this result higher 80%, player is can't
   // buy primary weapons.

   extern ConVar sypb_ecorounds;

   if (!sypb_ecorounds.GetBool ())
   {
      m_economicsGood[team] = true;
      return; // don't check economics while economics disable
   }

   int numPoorPlayers = 0;
   int numTeamPlayers = 0;

   // start calculating
   for (int i = 0; i < engine->GetMaxClients (); i++)
   {
      if (m_bots[i] != null && GetTeam (m_bots[i]->GetEntity ()) == team)
      {
         if (m_bots[i]->m_moneyAmount <= 1500)
            numPoorPlayers++;

         numTeamPlayers++; // update count of team
      }
   }
   m_economicsGood[team] = true;

   if (numTeamPlayers <= 1)
      return;

   // if 80 percent of team have no enough money to purchase primary weapon
   if ((numTeamPlayers * 80) / 100 <= numPoorPlayers)
      m_economicsGood[team] = false;

   // winner must buy something!
   if (m_lastWinner == team)
      m_economicsGood[team] = true;
}

void BotControl::Free (void)
{
   // this function free all bots slots (used on server shutdown)

   for (int i = 0; i < 32; i++)
   {
      if (m_bots[i] != null)
      {
         delete m_bots[i];
         m_bots[i] = null;
      }
   }
}

void BotControl::Free (int index)
{
   // this function frees one bot selected by index (used on bot disconnect)

   delete m_bots[index];
   m_bots[index] = null;
}

Bot::Bot (edict_t *bot, int skill, int personality, int team, int member)
{
   // this function does core operation of creating bot, it's called by CreateBot (),
   // when bot setup completed, (this is a bot class constructor)

   char rejectReason[128];
   int clientIndex = ENTINDEX (bot);

   memset (this, 0, sizeof (Bot));

   pev = VARS (bot);

   if (bot->pvPrivateData != null)
      FREE_PRIVATE (bot);

   bot->pvPrivateData = null;
   bot->v.frags = 0;

   // create the player entity by calling MOD's player function
   BotControl::CallGameEntity (&bot->v);

   // set all info buffer keys for this bot
   char *buffer = GET_INFOKEYBUFFER (bot);

   SET_CLIENT_KEYVALUE (clientIndex, buffer, "model", "");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "rate", "3500.000000");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "cl_updaterate", "20");
   /*
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "cl_lw", "1");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "cl_lc", "1");
   */
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "tracker", "0");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "cl_dlmax", "128");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "friends", "0");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "dm", "0");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "_ah", "0");

   if (sypb_tagbots.GetBool ())
      SET_CLIENT_KEYVALUE (clientIndex, buffer, "*bot", "1");

   SET_CLIENT_KEYVALUE (clientIndex, buffer, "_vgui_menus", "0");

   memset (rejectReason, 0, sizeof (rejectReason)); // reset the reject reason template string
   //MDLL_ClientConnect (bot, "fakeclient", FormatBuffer ("192.168.1.%d", ENTINDEX (bot) + 100), rejectReason);
   // SyPB Pro P.29 - new calling bot
   MDLL_ClientConnect(bot, "BOT", FormatBuffer("127.0.0.%d", ENTINDEX(bot) + 100), rejectReason);

   if (!IsNullString (rejectReason))
   {
      AddLogEntry (true, LOG_WARNING, "Server refused '%s' connection (%s)", STRING (bot->v.netname), rejectReason);
      ServerCommand ("kick \"%s\"", STRING (bot->v.netname)); // kick the bot player if the server refused it

      bot->v.flags |= FL_KILLME;
   }

   if (IsDedicatedServer () && engine->GetDeveloperLevel () > 0)
   {
      if (engine->GetDeveloperLevel () == 2)
      {
          ServerPrint ("Server requiring authentication");
          ServerPrint ("Client '%s' connected", STRING (bot->v.netname));
          ServerPrint ("Adr: 127.0.0.%d:27005", ENTINDEX (bot) + 100);
      }

      ServerPrint ("Verifying and uploading resources...");
      ServerPrint ("Custom resources total 0 bytes");
      ServerPrint ("  Decals:  0 bytes");
      ServerPrint ("----------------------");
      ServerPrint ("Resources to request: 0 bytes");
   }

   MDLL_ClientPutInServer (bot);

   bot->v.flags = 0;
   bot->v.flags |= FL_FAKECLIENT | FL_CLIENT; // set this player as fakeclient

   // initialize all the variables for this bot...
   m_notStarted = true;  // hasn't joined game yet

   m_startAction = CMENU_IDLE;
   m_moneyAmount = 0;
   m_logotypeIndex = engine->RandomInt (0, 5);


   // initialize msec value
   m_msecNum = m_msecDel = 0.0f;
   m_msecInterval = engine->GetTime ();
   m_msecVal = static_cast <uint8_t> (g_pGlobals->frametime * 1000.0f);
   m_msecBuiltin = engine->RandomInt (1, 4);

   // assign how talkative this bot will be
   m_sayTextBuffer.chatDelay = engine->RandomFloat (3.8f, 10.0f);
   m_sayTextBuffer.chatProbability = engine->RandomInt (1, 100);

   m_notKilled = false;
   m_skill = skill;
   m_weaponBurstMode = BURST_DISABLED;

   m_lastThinkTime = engine->GetTime ();
   m_frameInterval = engine->GetTime ();

   bot->v.idealpitch = bot->v.v_angle.x;
   bot->v.ideal_yaw = bot->v.v_angle.y;

   bot->v.yaw_speed = engine->RandomFloat (g_skillTab[m_skill / 20].minTurnSpeed, g_skillTab[m_skill / 20].maxTurnSpeed);
   bot->v.pitch_speed = engine->RandomFloat (g_skillTab[m_skill / 20].minTurnSpeed, g_skillTab[m_skill / 20].maxTurnSpeed);

   switch (personality)
   {
   case 1:
      m_personality = PERSONALITY_RUSHER;
      m_baseAgressionLevel = engine->RandomFloat (0.7f, 1.0f);
      m_baseFearLevel = engine->RandomFloat (0.0f, 0.4f);
      break;

   case 2:
      m_personality = PERSONALITY_CAREFUL;
      m_baseAgressionLevel = engine->RandomFloat (0.0f, 0.4f);
      m_baseFearLevel = engine->RandomFloat (0.7f, 1.0f);
      break;

   default:
      m_personality = PERSONALITY_NORMAL;
      m_baseAgressionLevel = engine->RandomFloat (0.4f, 0.7f);
      m_baseFearLevel = engine->RandomFloat (0.4f, 0.7f);
      break;
   }

   memset (&m_ammoInClip, 0, sizeof (m_ammoInClip));
   memset (&m_ammo, 0, sizeof (m_ammo));

   m_currentWeapon = 0; // current weapon is not assigned at start
   m_voicePitch = engine->RandomInt (166, 250) / 2; // assign voice pitch

   // copy them over to the temp level variables
   m_agressionLevel = m_baseAgressionLevel;
   m_fearLevel = m_baseFearLevel;
   m_nextEmotionUpdate = engine->GetTime () + 0.5f;

   // just to be sure
   m_actMessageIndex = 0;
   m_pushMessageIndex = 0;

   // assign team and class
   m_wantedTeam = team;
   m_wantedClass = member;

   NewRound ();
}

Bot::~Bot (void)
{
   // this is bot destructor

   // SwitchChatterIcon (false); // crash on CTRL+C'ing win32 console hlds
   DeleteSearchNodes ();
   ResetTasks ();

   // free used botname
   ITERATE_ARRAY (g_botNames, j)
   {
      if (strcmp (g_botNames[j].name, STRING (pev->netname)) == 0)
      {
         g_botNames[j].isUsed = false;
         break;
      }
   }
}

void Bot::NewRound (void)
{
   // this function initializes a bot after creation & at the start of each round

   int i = 0;

   // delete all allocated path nodes
   DeleteSearchNodes ();

   // SyPB Pro P.30 - AMXX API
   m_weaponClipAPI = 0;
   m_weaponReloadAPI = false;
   m_lookAtAPI = nullvec;
   m_moveAIAPI = false;
   m_moveTargetEntityAPI = null;
   m_enemyAPI = null;
   m_blockCheckEnemyTime = engine->GetTime();

   // SyPB Pro P.31 - AMXX API
   m_knifeDistance1API = 0;
   m_knifeDistance2API = 0;

   // SyPB Pro P.35 - AMXX API
   m_gunMinDistanceAPI = 0;
   m_gunMaxDistanceAPI = 0;

   m_waypointOrigin = nullvec;
   m_destOrigin = nullvec;
   m_currentWaypointIndex = -1;
   m_currentTravelFlags = 0;
   m_desiredVelocity = nullvec;
   m_prevGoalIndex = -1;
   m_chosenGoalIndex = -1;
   m_loosedBombWptIndex = -1;

   m_moveToC4 = false;
   m_duckDefuse = false;
   m_duckDefuseCheckTime = 0.0f;

   m_prevWptIndex[0] = -1;
   m_prevWptIndex[1] = -1;
   m_prevWptIndex[2] = -1;
   m_prevWptIndex[3] = -1;
   m_prevWptIndex[4] = -1;

   m_navTimeset = engine->GetTime ();

   switch (m_personality)
   {
   case PERSONALITY_NORMAL:
      m_pathType = engine->RandomInt (0, 100) > 50 ? 0 : 1;
      break;

   case PERSONALITY_RUSHER:
      m_pathType = 0;
      break;

   case PERSONALITY_CAREFUL:
      m_pathType = 1;
      break;
   }

   // clear all states & tasks
   m_states = 0;
   ResetTasks ();

   m_isVIP = false;
   m_isLeader = false;
   m_hasProgressBar = false;
   m_canChooseAimDirection = true;

   m_timeTeamOrder = 0.0f;
   m_askCheckTime = 0.0f;
   m_minSpeed = 260.0f;
   m_prevSpeed = 0.0f;
   m_prevOrigin = Vector (9999.0, 9999.0, 9999.0f);
   m_prevTime = engine->GetTime ();
   m_blindRecognizeTime = engine->GetTime ();

   m_viewDistance = 4096.0f;
   m_maxViewDistance = 4096.0f;

   m_pickupItem = null;
   m_itemIgnore = null;
   m_itemCheckTime = 0.0f;

   m_breakableEntity = null;
   m_breakable = nullvec;
   m_timeDoorOpen = 0.0f;

   ResetCollideState ();
   ResetDoubleJumpState ();

   SetMoveTarget (null);

   m_enemy = null;
   m_lastVictim = null;
   m_lastEnemy = null;
   m_lastEnemyOrigin = nullvec;
   m_trackingEdict = null;
   m_timeNextTracking = 0.0f;

   m_buttonPushTime = 0.0f;
   m_enemyUpdateTime = 0.0f;
   m_seeEnemyTime = 0.0f;
   m_shootAtDeadTime = 0.0f;
   m_oldCombatDesire = 0.0f;

   m_avoidGrenade = null;
   m_needAvoidGrenade = 0;

   m_lastDamageType = -1;
   m_voteKickIndex = 0;
   m_lastVoteKick = 0;
   m_voteMap = 0;
   m_doorOpenAttempt = 0;
   m_aimFlags = 0;
   m_burstShotsFired = 0;

   m_position = nullvec;

   m_idealReactionTime = g_skillTab[m_skill / 20].minSurpriseTime;
   m_actualReactionTime = g_skillTab[m_skill / 20].minSurpriseTime;

   m_targetEntity = null;
   m_followWaitTime = 0.0f;
   
   for (i = 0; i < Const_MaxHostages; i++)
      m_hostages[i] = null;

   for (i = 0; i < Chatter_Total; i++)
      m_voiceTimers[i] = -1.0f;

   m_isReloading = false;
   m_reloadState = RSTATE_NONE;

   m_reloadCheckTime = 0.0f;
   m_shootTime = engine->GetTime ();
   m_playerTargetTime = engine->GetTime ();
   m_firePause = 0.0f;
   m_timeLastFired = 0.0f;

   m_grenadeCheckTime = 0.0f;
   m_isUsingGrenade = false;

   m_skillOffset = (100 - m_skill) / 100.0f;
   m_blindButton = 0;
   m_blindTime = 0.0f;
   m_jumpTime = 0.0f;
   m_jumpFinished = false;
   m_isStuck = false;

   m_sayTextBuffer.timeNextChat = engine->GetTime ();
   m_sayTextBuffer.entityIndex = -1;
   m_sayTextBuffer.sayText[0] = 0x0;

   m_buyState = 0;

   if (!m_notKilled) // if bot died, clear all weapon stuff and force buying again
   {
      memset (&m_ammoInClip, 0, sizeof (m_ammoInClip));
      memset (&m_ammo, 0, sizeof (m_ammo));

      m_currentWeapon = 0;
   }

   m_knifeAttackTime = engine->GetTime () + engine->RandomFloat (1.3f, 2.6f);
   m_nextBuyTime = engine->GetTime () + engine->RandomFloat (0.6f, 1.2f);
   
   m_GetNewEnemyTimer = engine->GetTime ();

   m_buyPending = false;
   m_inBombZone = false;

   m_shieldCheckTime = 0.0f;
   m_zoomCheckTime = 0.0f;
   m_strafeSetTime = 0.0f;
   m_combatStrafeDir = 0;
   m_fightStyle = 0;
   m_lastFightStyleCheck = 0.0f;

   m_checkWeaponSwitch = true;
   m_checkKnifeSwitch = true;
   m_buyingFinished = false;

   m_radioEntity = null;
   m_radioOrder = 0;
   m_defendedBomb = false;

   m_timeLogoSpray = engine->GetTime () + engine->RandomFloat (0.5, 2.0f);
   m_spawnTime = engine->GetTime ();
   m_lastChatTime = engine->GetTime ();
   pev->v_angle.y = pev->ideal_yaw;

   m_timeCamping = 0;
   m_campDirection = 0;
   m_nextCampDirTime = 0;
   m_campButtons = 0;

   m_soundUpdateTime = 0.0f;
   m_heardSoundTime = engine->GetTime ();

   // clear its message queue
   for (i = 0; i < 32; i++)
      m_messageQueue[i] = CMENU_IDLE;

   m_actMessageIndex = 0;
   m_pushMessageIndex = 0;

   // and put buying into its message queue
   PushMessageQueue (CMENU_BUY);
   PushTask (TASK_NORMAL, TASKPRI_NORMAL, -1, 0.0, true);
}

void Bot::Kill (void)
{
   // this function kills a bot (not just using ClientKill, but like the CSBot does)
   // base code courtesy of Lazy (from bots-united forums!)

   edict_t *hurtEntity = (*g_engfuncs.pfnCreateNamedEntity) (MAKE_STRING ("trigger_hurt"));

   if (FNullEnt (hurtEntity))
      return;

   hurtEntity->v.classname = MAKE_STRING (g_weaponDefs[m_currentWeapon].className);
   hurtEntity->v.dmg_inflictor = GetEntity ();
   hurtEntity->v.dmg = 9999.0f;
   hurtEntity->v.dmg_take = 1.0f;
   hurtEntity->v.dmgtime = 2.0f;
   hurtEntity->v.effects |= EF_NODRAW;

   (*g_engfuncs.pfnSetOrigin) (hurtEntity, Vector (-4000, -4000, -4000));

   KeyValueData kv;
   kv.szClassName = const_cast <char *> (g_weaponDefs[m_currentWeapon].className);
   kv.szKeyName = "damagetype";
   kv.szValue = FormatBuffer ("%d", (1 << 4));
   kv.fHandled = false;

   MDLL_KeyValue (hurtEntity, &kv);

   MDLL_Spawn (hurtEntity);
   MDLL_Touch (hurtEntity, GetEntity ());

   (*g_engfuncs.pfnRemoveEntity) (hurtEntity);
}

void Bot::Kick (void)
{
   // this function kick off one bot from the server.

   ServerCommand ("kick #%d", GETPLAYERUSERID (GetEntity ()));
   CenterPrint ("Bot '%s' kicked", STRING (pev->netname));

   // balances quota
   if (g_botManager->GetBotsNum () - 1 < sypb_quota.GetInt ())
      sypb_quota.SetInt (g_botManager->GetBotsNum () - 1);
}

void Bot::StartGame (void)
{
   // this function handles the selection of teams & class

   // handle counter-strike stuff here...
   if (m_startAction == CMENU_TEAM)
   {
      m_startAction = CMENU_IDLE;  // switch back to idle

      if (sypb_forceteam.GetString ()[0] == 'C' || sypb_forceteam.GetString ()[0] == 'c' || 
		  sypb_forceteam.GetString()[0] == '2')
         m_wantedTeam = 2;
      else if (sypb_forceteam.GetString ()[0] == 'T' || sypb_forceteam.GetString ()[0] == 't' || 
		  sypb_forceteam.GetString()[0] == '1') // SyPB Pro P.28 - 1=T, 2=CT
         m_wantedTeam = 1;

      if (m_wantedTeam != 1 && m_wantedTeam != 2)
         m_wantedTeam = 5;

      // select the team the bot wishes to join...
      FakeClientCommand (GetEntity (), "menuselect %d", m_wantedTeam);
   }
   else if (m_startAction == CMENU_CLASS)
   {
      m_startAction = CMENU_IDLE;  // switch back to idle

      if (g_gameVersion == CSVER_CZERO) // czero has spetsnaz and militia skins
      {
         if (m_wantedClass < 1 || m_wantedClass > 5)
            m_wantedClass = engine->RandomInt (1, 5); //  use random if invalid
      }
      else
      {
         if (m_wantedClass < 1 || m_wantedClass > 4)
            m_wantedClass = engine->RandomInt (1, 4); // use random if invalid
      }

      // select the class the bot wishes to use...
      FakeClientCommand (GetEntity (), "menuselect %d", m_wantedClass);

      // bot has now joined the game (doesn't need to be started)
      m_notStarted = false;

      // check for greeting other players, since we connected
      if (engine->RandomInt (0, 100) < 20)
         ChatMessage (CHAT_HELLO);
   }
}
