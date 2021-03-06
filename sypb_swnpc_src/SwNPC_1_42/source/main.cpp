
#include "core.h"

bool g_swnpcRun = false;
int g_numWaypoints = -1;

edict_t *g_hostEntity = null;

int g_callStuck_Pre = -1;
int g_callTakeDamage_Pre = -1;
int g_callKill_Post = -1;
int g_callTakeDamage_Post = -1;

int g_TDP_damageValue = -1;
bool g_TDP_cvOn = false;

int g_sModelIndexBloodDrop = -1;
int g_sModelIndexBloodSpray = -1;
int g_bloodIndex[12];

int g_modelIndexLaser = 0;
int g_modelIndexArrow = 0;

int apiBuffer;

void OnPluginsLoaded()
{
	AllReLoad();

	g_modelIndexLaser = PRECACHE_MODEL("sprites/laserbeam.spr");
	g_modelIndexArrow = PRECACHE_MODEL("sprites/arrow1.spr");

	g_sModelIndexBloodSpray = PRECACHE_MODEL("sprites/bloodspray.spr");
	g_sModelIndexBloodDrop = PRECACHE_MODEL("sprites/blood.spr");

	g_bloodIndex[0] = DECAL_INDEX("{blood1");
	g_bloodIndex[1] = DECAL_INDEX("{blood2");
	g_bloodIndex[2] = DECAL_INDEX("{blood3");
	g_bloodIndex[3] = DECAL_INDEX("{blood4");
	g_bloodIndex[4] = DECAL_INDEX("{blood5");
	g_bloodIndex[5] = DECAL_INDEX("{blood6");
	g_bloodIndex[6] = DECAL_INDEX("{yblood1");
	g_bloodIndex[7] = DECAL_INDEX("{yblood2");
	g_bloodIndex[8] = DECAL_INDEX("{yblood3");
	g_bloodIndex[9] = DECAL_INDEX("{yblood4");
	g_bloodIndex[10] = DECAL_INDEX("{yblood5");
	g_bloodIndex[11] = DECAL_INDEX("{yblood6");

	SyPBDataLoad();

	if (g_swnpcRun)
	{
		// (victimId, attackId, damage)
		g_callTakeDamage_Pre = MF_RegisterForward("SwNPC_TakeDamage_Pre", ET_CONTINUE, FP_CELL, FP_CELL, FP_CELL, FP_DONE);
		// (npcId)
		g_callStuck_Pre = MF_RegisterForward("SwNPC_Stuck_Pre", ET_CONTINUE, FP_CELL, FP_DONE);

		// (victimId, killerId)
		g_callKill_Post = MF_RegisterForward("SwNPC_Kill_Post", ET_IGNORE, FP_CELL, FP_CELL, FP_DONE);
		// (victimId, attackId, damage)
		g_callTakeDamage_Post = MF_RegisterForward("SwNPC_TakeDamage_Post", ET_IGNORE, FP_CELL, FP_CELL, FP_CELL, FP_DONE);
	}
}

void OnAmxxAttach()
{
	MF_AddNatives(swnpc_natives);
}

void AllReLoad(void)
{
	g_swnpcRun = false;
	g_numWaypoints = -1;

	g_callStuck_Pre = -1;
	g_callTakeDamage_Pre = -1;

	g_callKill_Post = -1;
	g_callTakeDamage_Post = -1;

	g_sModelIndexBloodDrop = -1;
	g_sModelIndexBloodSpray = -1;

	for (int i = 0; i < 12; i++)
		g_bloodIndex[i] = -1;

	g_npcManager->RemoveAll();
}

void FN_ClientPutInServer_Post(edict_t *pEntity)
{
	if (g_swnpcRun && !FNullEnt(pEntity) && g_numWaypoints == -1)
	{
		GetWaypointData();
		if (g_numWaypoints <= 0)
			g_swnpcRun = false;
	}

	RETURN_META(MRES_IGNORED);
}

void FN_StartFrame(void)
{
	apiBuffer = 0;
	SyPB_GetHostEntity();

	g_npcManager->Think();
	RETURN_META(MRES_IGNORED);
}

void FN_RemoveEntity(edict_t *c)
{
	NPC *SwNPC = g_npcManager->IsSwNPC(c);
	if (SwNPC != null)
	{
		if (SwNPC->m_needRemove == false)
			RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}

void FN_TraceLine(const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr)
{
	if (IsValidPlayer (pentToSkip) && g_npcManager->IsSwNPC(ptr->pHit) != null)
	{
		if (ptr->iHitgroup == 8)
		{
			ptr->iHitgroup = 1;
			RETURN_META(MRES_HANDLED);
		}
	}

	RETURN_META(MRES_IGNORED);
}

const char *GetModName(void)
{
	static char modName[256];

	GET_GAME_DIR(modName); // ask the engine for the MOD directory path
	int length = strlen(modName); // get the length of the returned string

								  // format the returned string to get the last directory name
	int stop = length - 1;
	while ((modName[stop] == '\\' || modName[stop] == '/') && stop > 0)
		stop--; // shift back any trailing separator

	int start = stop;
	while (modName[start] != '\\' && modName[start] != '/' && start > 0)
		start--; // shift back to the start of the last subdirectory name

	if (modName[start] == '\\' || modName[start] == '/')
		start++; // if we reached a separator, step over it

				 // now copy the formatted string back onto itself character per character
	for (length = start; length <= stop; length++)
		modName[length - start] = modName[length];

	modName[length - start] = 0; // terminate the string

	return &modName[0];
}

void ErrorWindows(char *text)
{
	int button = ::MessageBox(null, text, "SwNPC Error", MB_YESNO | MB_TOPMOST | MB_ICONINFORMATION);
	if (button == IDYES)
		exit(1);
}