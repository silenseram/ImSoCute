#include <Windows.h>
#include <iostream>
#include "csgo.hpp"
#include "MemMan.h"
#include <iomanip>
#include "MathHelp.h"

#define AIM_FOV 10 // TODO config or gui

using namespace hazedumper::netvars;
using namespace hazedumper::signatures;

typedef struct GlowObjectDefinition_t {
	float r;
	float g;
	float b;
	float a;
	uint8_t unk1[16];
	bool m_bRenderWhenOccluded;
	bool m_bRenderWhenUnoccluded;
	bool m_bFullBloom;
	int GlowStyle;
	uint8_t unk2[10];
} GlowObjectDefinition_t;

Memory MemClass;
PModule bClient;
PModule bEngine;


Vector getBonePos(int pTargetIn, int bone)
{
	const int matrix = MemClass.readMem<int>(pTargetIn + m_dwBoneMatrix);
	return Vector(
		MemClass.readMem<float>(matrix + 0x30 * bone + 0xC),
		MemClass.readMem<float>(matrix + 0x30 * bone + 0x1C),
		MemClass.readMem<float>(matrix + 0x30 * bone + 0x2C)
	);
}

/*Set new ViewAngle*/
void setViewAngle(DWORD EngineBase, Vector angle)
{

	ClampAngles(angle);
	NormalizeAngles(angle);

	MemClass.writeMem(EngineBase + dwClientState_ViewAngles, angle);
}

/*Calc angle to target bone*/
Vector AngelToTarget(int pLocal, int pTargetIn, int boneIndex)
{
	const Vector position = MemClass.readMem<Vector>(pLocal + m_vecOrigin);
	const Vector vecView = MemClass.readMem<Vector>(pLocal + m_vecViewOffset);

	const Vector myView = position + vecView;
	const Vector aimView = getBonePos(pTargetIn, boneIndex);

	Vector dst = CalcAngle(myView, aimView).ToVector();

	NormalizeAngles(dst);

	return dst;
}

/*get the best target*/
int getTarget(int pLocal, int clientState)
{
	int currentTarget = -1;
	float misDist = 360.f;

	Vector _viewAngels = MemClass.readMem<Vector>(clientState + dwClientState_ViewAngles);

	const int playerTeam = MemClass.readMem<int>(pLocal + m_iTeamNum);

	for (int i = 0; i <= 64; i++)
	{
		const int target = MemClass.readMem<int>(bClient.dwBase + dwEntityList + (i - 1) * 0x10);
		if (!target || target < 0)
			continue;

		const int targetHealth = MemClass.readMem<int>(target + m_iHealth);
		if (!targetHealth || targetHealth < 0)
			continue;

		const int targetTeam = MemClass.readMem<int>(target + m_iTeamNum);

		if (!targetTeam || targetTeam == playerTeam)
			continue;

		const bool targetDormant = MemClass.readMem<bool>(target + m_bDormant);
		if (targetDormant)
			continue;

		Vector angleToTarget = AngelToTarget(pLocal, target, 8/*head bone index head_0*/);
		float fov = GetFov(_viewAngels.ToQAngle(), angleToTarget.ToQAngle());
		if (fov < misDist && fov <= AIM_FOV)/*sort by fov*/
		{
			misDist = fov;
			currentTarget = target;
		}
	}
	return currentTarget;
}

GlowObjectDefinition_t getGlowEnemyColor(GlowObjectDefinition_t glow, int hp) {
	glow.r = hp * -0.01 + 1;
	glow.g = hp * 0.01;
	glow.b = 0.0;
	glow.a = 1;
	return glow;
}

GlowObjectDefinition_t getGlowTeamColor(GlowObjectDefinition_t glow, int hp) {
	glow.r = 0;
	glow.g = 0;
	glow.b = 1.0;
	glow.a = 0.7;
	return glow;
}

void SetEntityGlow(int entity, int team, int glowObject, int playerTeam) {

	int glowIndex = MemClass.readMem<int>(entity + m_iGlowIndex);
	int hp = MemClass.readMem<int>(entity + m_iHealth);
	int glowArray = MemClass.readMem<int>(bClient.dwBase + dwGlowObjectManager);

	GlowObjectDefinition_t glow = MemClass.readMem<GlowObjectDefinition_t>(glowArray + 0x38 * glowIndex + 0x4);

	//fix trash in glow
	glow.r = 0.0;
	glow.g = 2.0;
	glow.b = 0.0;
	glow.a = 0.5;
		
	if (team == playerTeam)
		glow = getGlowTeamColor(glow, hp);
	else
		glow = getGlowEnemyColor(glow, hp);

	glow.m_bRenderWhenOccluded = true;
	glow.m_bRenderWhenUnoccluded = false;

	MemClass.writeMem<GlowObjectDefinition_t>(glowArray + glowIndex * 0x38 + 0x4, glow);

}

void HandleGlow() {
	int glowObject = MemClass.readMem<int>(bClient.dwBase + dwGlowObjectManager);
	int localPlayer = MemClass.readMem<int>(bClient.dwBase + dwLocalPlayer);

	int playerTeam = MemClass.readMem<int>(localPlayer + m_iTeamNum);
	int glowCount = MemClass.readMem<int>(bClient.dwBase + dwGlowObjectManager + 0x4);

	for (int i = 0; i < glowCount; i++) {
		int entity = MemClass.readMem<int>(bClient.dwBase + dwEntityList + i * 0x10);

		if (entity != NULL) {
			int entityTeam = MemClass.readMem<int>(entity + m_iTeamNum);
			int entityHp = MemClass.readMem<int>(entity + m_iHealth);

			if (!entityTeam || !entityHp)
				continue;
			if (entityTeam != playerTeam) {
				SetEntityGlow(entity, entityTeam, glowObject, playerTeam);
			}
		}

	}
}

void HandleAim() {
	int pLocal = MemClass.readMem<int>(bClient.dwBase + dwLocalPlayer);
	int pEngine = MemClass.readMem<int>(bEngine.dwBase + dwClientState);

	int target = getTarget(pLocal, pEngine);/*Get The best target addr*/

	if (!pEngine)
		return;
	if (target == -1)
		return;

	Vector s = AngelToTarget(pLocal, target, 8);

	setViewAngle(pEngine, AngelToTarget(pLocal, target, 8));
}

int main()
{
	bool isAimActive = true; // TODO

	while (!MemClass.Attach("csgo.exe", PROCESS_ALL_ACCESS)) { std::cout << "finding csgo.exe -_-\n";	}
	std::cout << "by writer :3" << std::endl;

	bClient = MemClass.GetModule("client_panorama.dll");
	bEngine = MemClass.GetModule("engine.dll");

	while (1){
		int localPlayer = MemClass.readMem<int>(bClient.dwBase + dwLocalPlayer);
		if (localPlayer == NULL)
			continue;

		int playerTeam = MemClass.readMem<int>(localPlayer + m_iTeamNum);

		HandleGlow();
		if (isAimActive)
			if (GetAsyncKeyState(VK_LBUTTON))
				HandleAim();

		Sleep(1);
	}


	return 0;
}