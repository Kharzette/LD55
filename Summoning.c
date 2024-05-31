#include	<d3d11_1.h>
#include	<stdint.h>
#include	<stdbool.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<assert.h>
#include	<x86intrin.h>
#include	<SDL3/SDL.h>
#include	<SDL3/SDL_keycode.h>
#include	<cglm/call.h>
#include	"GrogLibsC/AudioLib/SoundEffect.h"
#include	"GrogLibsC/AudioLib/Audio.h"
#include	"GrogLibsC/MaterialLib/StuffKeeper.h"
#include	"GrogLibsC/MaterialLib/CBKeeper.h"
#include	"GrogLibsC/MaterialLib/PostProcess.h"
#include	"GrogLibsC/MaterialLib/Material.h"
#include	"GrogLibsC/MaterialLib/MaterialLib.h"
#include	"GrogLibsC/MaterialLib/ScreenText.h"
#include	"GrogLibsC/UtilityLib/GraphicsDevice.h"
#include	"GrogLibsC/UtilityLib/StringStuff.h"
#include	"GrogLibsC/UtilityLib/ListStuff.h"
#include	"GrogLibsC/UtilityLib/MiscStuff.h"
#include	"GrogLibsC/UtilityLib/GameCamera.h"
#include	"GrogLibsC/UtilityLib/DictionaryStuff.h"
#include	"GrogLibsC/UtilityLib/UpdateTimer.h"
#include	"GrogLibsC/UtilityLib/PrimFactory.h"
#include	"GrogLibsC/UtilityLib/PlaneMath.h"
#include	"GrogLibsC/TerrainLib/Terrain.h"
#include	"GrogLibsC/MeshLib/Mesh.h"
#include	"GrogLibsC/MeshLib/AnimLib.h"
#include	"GrogLibsC/MeshLib/Character.h"
#include	"GrogLibsC/MeshLib/CommonPrims.h"
#include	"GrogLibsC/InputLib/Input.h"
#include	"GrogLibsC/UtilityLib/BipedMover.h"


#define	RESX			1280
#define	RESY			720
#define	ROT_RATE		10.0f
#define	UVSCALE_RATE	1.0f
#define	KEYTURN_RATE	0.01f
#define	MOVE_RATE		0.1f
#define	HEIGHT_SCALAR	0.15f
#define	RAY_LEN			100.0f
#define	RAY_WIDTH		0.05f
#define	IMPACT_WIDTH	0.2f
#define	NUM_RAYS		1000
#define	MOUSE_TO_ANG	0.001f
#define	MAX_ST_CHARS	256
#define	START_CAM_DIST	5.0f
#define	MAX_CAM_DIST	25.0f
#define	MIN_CAM_DIST	0.25f

//should match CommonFunctions.hlsli
#define	MAX_BONES		55


//input context, stuff input handlers will need
typedef struct	TestStuff_t
{
	GraphicsDevice	*mpGD;
	Terrain			*mpTer;
	GameCamera		*mpCam;
	ScreenText		*mpST;
	BipedMover		*mpBPM;

	//toggles
	bool	mbMouseLooking;
	bool	mbRunning;
	bool	mbFlyMode;

	//character movement
	vec3	mCharMoveVec;

	//misc data
	vec3	mDanglyForce;
	vec3	mLightDir;
	vec3	mEyePos;
	vec3	mPlayerPos;
	float	mDeltaYaw, mDeltaPitch, mCamDist;
}	TestStuff;

//static forward decs
static void		SetupKeyBinds(Input *pInp);
static void		SetupRastVP(GraphicsDevice *pGD);
static void		SetupDebugStrings(TestStuff *pTS, const StuffKeeper *pSK);
static void		MoveCharacter(TestStuff *pTS, const vec3 moveVec);
static DictSZ	*sLoadCharacterMeshParts(GraphicsDevice *pGD, StuffKeeper *pSK, const Character *pChar);

//material setups
static Material	*MakeTerrainMat(TestStuff *pTS, const StuffKeeper *pSK);
static Material	*MakeSkyBoxMat(TestStuff *pTS, const StuffKeeper *pSK);

//input event handlers
static void	ToggleFlyModeEH(void *pContext, const SDL_Event *pEvt);
static void	LeftMouseDownEH(void *pContext, const SDL_Event *pEvt);
static void	LeftMouseUpEH(void *pContext, const SDL_Event *pEvt);
static void	RightMouseDownEH(void *pContext, const SDL_Event *pEvt);
static void	RightMouseUpEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveForwardEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveBackEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveLeftEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveRightEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveUpEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveDownEH(void *pContext, const SDL_Event *pEvt);
static void	KeyMoveJumpEH(void *pContext, const SDL_Event *pEvt);
static void	KeySprintDownEH(void *pContext, const SDL_Event *pEvt);
static void	KeySprintUpEH(void *pContext, const SDL_Event *pEvt);
static void	KeyTurnLeftEH(void *pContext, const SDL_Event *pEvt);
static void	KeyTurnRightEH(void *pContext, const SDL_Event *pEvt);
static void	KeyTurnUpEH(void *pContext, const SDL_Event *pEvt);
static void	KeyTurnDownEH(void *pContext, const SDL_Event *pEvt);
static void MouseMoveEH(void *pContext, const SDL_Event *pEvt);
static void EscEH(void *pContext, const SDL_Event *pEvt);


int main(void)
{
	printf("Ludum Dare 55!\n");

	//TODO: ask user when released
	Audio	*pAud	=Audio_Create(2);

	//store a bunch of vars in a struct
	//for ref/modifying by input handlers
	TestStuff	*pTS	=malloc(sizeof(TestStuff));
	memset(pTS, 0, sizeof(TestStuff));

	//start in fly mode?
	pTS->mbFlyMode	=false;
	pTS->mCamDist	=START_CAM_DIST;
	
	//set player on corner near origin
	glm_vec3_scale(GLM_VEC3_ONE, 22.0f, pTS->mPlayerPos);

	//input and key / mouse bindings
	Input	*pInp	=INP_CreateInput();
	SetupKeyBinds(pInp);

	if(GD_Init(&pTS->mpGD, "Ludum Dare 55!  Summoning...", 0, 0, RESX, RESY, true, D3D_FEATURE_LEVEL_11_1) == false)
	{
		printf("Couldn't create GraphicsDevice!\n");
		return	EXIT_FAILURE;
	}

	SetupRastVP(pTS->mpGD);

	StuffKeeper	*pSK	=StuffKeeper_Create(pTS->mpGD);
	if(pSK == NULL)
	{
		printf("Couldn't create StuffKeeper!\n");
		GD_Destroy(&pTS->mpGD);
		return	EXIT_FAILURE;
	}

	//a terrain chunk
	pTS->mpTer	=Terrain_Create(pTS->mpGD, "Blort", "Textures/Terrain/HeightMaps/HeightMap.png", 4, HEIGHT_SCALAR);

	PrimObject	*pSkyCube	=PF_CreateCube(10.0f, true, pTS->mpGD);
	CBKeeper	*pCBK		=CBK_Create(pTS->mpGD);
	PostProcess	*pPP		=PP_Create(pTS->mpGD, pSK, pCBK);

	SetupDebugStrings(pTS, pSK);

	//set sky gradient
	{
		vec3	skyHorizon	={	1.0f, 0.5f, 0.0f	};
		vec3	skyHigh		={	1.0f, 0.25f, 0.0f	};

		CBK_SetSky(pCBK, skyHorizon, skyHigh);
		CBK_SetFogVars(pCBK, 50.0f, 300.0f, true);
	}

	PP_SetTargets(pPP, pTS->mpGD, "BackColor", "BackDepth");

	float	aspect	=(float)RESX / (float)RESY;

	mat4	charMat;

	pTS->mEyePos[1]	=0.6f;
	pTS->mEyePos[2]	=4.5f;

	//game camera
	pTS->mpCam	=GameCam_Create(false, 0.1f, 2000.0f, GLM_PI_4f, aspect, 1.0f, 10.0f);

	//biped mover
	pTS->mpBPM	=BPM_Create(pTS->mpCam);

	SoundEffectPlay("synth(3)", pTS->mPlayerPos);

	//3D Projection
	mat4	camProj;
	GameCam_GetProjection(pTS->mpCam, camProj);
	CBK_SetProjection(pCBK, camProj);

	//2d projection for text
	mat4	textProj;
	glm_ortho(0, RESX, RESY, 0, -1.0f, 1.0f, textProj);

	//set constant buffers to shaders, think I just have to do this once
	CBK_SetCommonCBToShaders(pCBK, pTS->mpGD);

	pTS->mLightDir[0]		=0.3f;
	pTS->mLightDir[1]		=-0.7f;
	pTS->mLightDir[2]		=-0.5f;

	glm_vec3_normalize(pTS->mLightDir);
	glm_mat4_identity(charMat);

	UpdateTimer	*pUT	=UpdateTimer_Create(true, false);

	//TODO: user config file or something?  144 might be too fast for some
	UpdateTimer_SetFixedTimeStepMilliSeconds(pUT, 6.944444f);	//144hz

	//materials
	Material	*pTerMat	=MakeTerrainMat(pTS, pSK);
	Material	*pSkyBoxMat	=MakeSkyBoxMat(pTS, pSK);

	//character
	Character	*pChar	=Character_Read("Characters/Protag.Character");
	AnimLib		*pALib		=AnimLib_Read("Characters/Protag.AnimLib");
	MaterialLib	*pCharMats	=MatLib_Read("Characters/Protag.MatLib", pSK);
	DictSZ		*pMeshes	=sLoadCharacterMeshParts(pTS->mpGD, pSK, pChar);

	float	animTime		=0.0f;
	float	maxDT			=0.0f;

	pTS->mbRunning	=true;
	while(pTS->mbRunning)
	{
		pTS->mDeltaYaw		=0.0f;
		pTS->mDeltaPitch	=0.0f;

		UpdateTimer_Stamp(pUT);
		for(float secDelta =UpdateTimer_GetUpdateDeltaSeconds(pUT);
			secDelta > 0.0f;
			secDelta =UpdateTimer_GetUpdateDeltaSeconds(pUT))
		{
			//zero out charmove
			glm_vec3_zero(pTS->mCharMoveVec);

			//do input here
			//move turn etc
			INP_Update(pInp, pTS);

			bool	bJumped	=BPM_Update(pTS->mpBPM, secDelta, pTS->mCharMoveVec);
			if(bJumped)
			{
				SoundEffectPlay("jump", pTS->mPlayerPos);
			}

			MoveCharacter(pTS, pTS->mCharMoveVec);

			UpdateTimer_UpdateDone(pUT);
		}

		//update materials incase light changed
		MAT_SetLightDirection(pTerMat, pTS->mLightDir);

		//render update
		float	dt	=UpdateTimer_GetRenderUpdateDeltaSeconds(pUT);

		//update audio
		Audio_Update(pAud, pTS->mPlayerPos, pTS->mCharMoveVec);

		//player moving?
		float	moving	=glm_vec3_norm(pTS->mCharMoveVec);

		if(moving > 0.0f)
		{
			if(BPM_IsGoodFooting(pTS->mpBPM))
			{
				animTime	+=dt * moving * 200.0f;
			}
			else
			{
				animTime	+=dt * moving * 10.0f;
			}
			AnimLib_Animate(pALib, "LD55ProtagRun", animTime);
		}
		else
		{
			animTime	+=dt;
			AnimLib_Animate(pALib, "LD55ProtagIdle", animTime);
		}

		{
			if(dt > maxDT)
			{
				maxDT	=dt;
			}

			char	timeStr[32];

//			sprintf(timeStr, "maxDT: %f", maxDT);
			sprintf(timeStr, "animTime: %f", animTime);

			ST_ModifyStringText(pTS->mpST, 69, timeStr);

			sprintf(timeStr, "charMove: %2.2f %2.2f %2.2f",
				pTS->mCharMoveVec[0], pTS->mCharMoveVec[1], pTS->mCharMoveVec[2]);

			ST_ModifyStringText(pTS->mpST, 70, timeStr);
		}

		//update strings
		ST_Update(pTS->mpST, pTS->mpGD);

		//set no blend, I think post processing turns it on maybe
		GD_OMSetBlendState(pTS->mpGD, StuffKeeper_GetBlendState(pSK, "NoBlending"));
		GD_PSSetSampler(pTS->mpGD, StuffKeeper_GetSamplerState(pSK, "PointWrap"), 0);

		//camera update
		if(pTS->mbFlyMode)
		{
			GameCam_UpdateRotation(pTS->mpCam, pTS->mEyePos, pTS->mDeltaPitch,
									pTS->mDeltaYaw, 0.0f);
		}
		else
		{
			glm_vec3_copy(pTS->mPlayerPos, pTS->mEyePos);
			GameCam_UpdateRotationSecondary(pTS->mpCam, pTS->mPlayerPos, dt,
											pTS->mDeltaPitch, pTS->mDeltaYaw, 0.0f,
											(moving > 0)? true : false);
		}

		//set CB view
		{
			mat4	viewMat;
			if(pTS->mbFlyMode)
			{
				GameCam_GetViewMatrixFly(pTS->mpCam, viewMat, pTS->mEyePos);
			}
			else
			{
				GameCam_GetViewMatrixThird(pTS->mpCam, viewMat, pTS->mEyePos);

				mat4	headMat, guraMat;
				GameCam_GetLookMatrix(pTS->mpCam, headMat);
				GameCam_GetFlatLookMatrix(pTS->mpCam, guraMat);

				//drop mesh to ground
				vec3	feetToCenter	={	0.0f, -0.25f, 0.0f	};
				glm_translate(guraMat, feetToCenter);

				Material	*pCM	=MatLib_GetMaterial(pCharMats, "ProtagHell");
				MAT_SetWorld(pCM, guraMat);
			}

			CBK_SetView(pCBK, viewMat, pTS->mEyePos);

			//set the skybox world mat to match eye pos
			glm_translate_make(viewMat, pTS->mEyePos);
			MAT_SetWorld(pSkyBoxMat, viewMat);
		}

		PP_ClearDepth(pPP, pTS->mpGD, "BackDepth");
		PP_ClearTarget(pPP, pTS->mpGD, "BackColor");

		//update frame CB
		CBK_UpdateFrame(pCBK, pTS->mpGD);

		//turn depth off for sky
		GD_OMSetDepthStencilState(pTS->mpGD, StuffKeeper_GetDepthStencilState(pSK, "DisableDepth"));

		//draw sky first
		GD_IASetVertexBuffers(pTS->mpGD, pSkyCube->mpVB, pSkyCube->mVertSize, 0);
		GD_IASetIndexBuffers(pTS->mpGD, pSkyCube->mpIB, DXGI_FORMAT_R16_UINT, 0);

		MAT_Apply(pSkyBoxMat, pCBK, pTS->mpGD);
		GD_DrawIndexed(pTS->mpGD, pSkyCube->mIndexCount, 0, 0);

		//turn depth back on
		GD_OMSetDepthStencilState(pTS->mpGD, StuffKeeper_GetDepthStencilState(pSK, "EnableDepth"));

		//terrain draw
		Terrain_DrawMat(pTS->mpTer, pTS->mpGD, pCBK, pTerMat);

		//set mesh draw stuff
		if(pTS->mbFlyMode)
		{
			glm_translate_make(charMat, pTS->mPlayerPos);
			Material	*pCM	=MatLib_GetMaterial(pCharMats, "Protag");
			MAT_SetWorld(pCM, charMat);
		}
		GD_PSSetSampler(pTS->mpGD, StuffKeeper_GetSamplerState(pSK, "PointClamp"), 0);

		Character_Draw(pChar, pMeshes, pCharMats, pALib, pTS->mpGD, pCBK);

		//set proj for 2D
		CBK_SetProjection(pCBK, textProj);
		CBK_UpdateFrame(pCBK, pTS->mpGD);

		ST_Draw(pTS->mpST, pTS->mpGD, pCBK);

		//change back to 3D
		CBK_SetProjection(pCBK, camProj);
		CBK_UpdateFrame(pCBK, pTS->mpGD);

		GD_Present(pTS->mpGD);
	}

	Audio_Destroy(&pAud);

	GD_Destroy(&pTS->mpGD);

	return	EXIT_SUCCESS;
}

static void	ToggleFlyModeEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mbFlyMode	=!pTS->mbFlyMode;
}

static void	LeftMouseDownEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);
}

static void	LeftMouseUpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);
}

static void	RightMouseDownEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	SDL_SetRelativeMouseMode(SDL_TRUE);

	pTS->mbMouseLooking	=true;
}

static void	RightMouseUpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	SDL_SetRelativeMouseMode(SDL_FALSE);

	pTS->mbMouseLooking	=false;
}

static void	MouseMoveEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	if(pTS->mbMouseLooking)
	{
		pTS->mDeltaYaw		+=(pEvt->motion.xrel * MOUSE_TO_ANG);
		pTS->mDeltaPitch	+=(pEvt->motion.yrel * MOUSE_TO_ANG);
	}
}

static void	KeyMoveForwardEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputForward(pTS->mpBPM);
}

static void	KeyMoveBackEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputBack(pTS->mpBPM);
}

static void	KeyMoveLeftEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputLeft(pTS->mpBPM);
}

static void	KeyMoveRightEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputRight(pTS->mpBPM);
}

static void	KeyMoveUpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputUp(pTS->mpBPM);
}

static void	KeyMoveDownEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputDown(pTS->mpBPM);
}

static void	KeyMoveJumpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputJump(pTS->mpBPM);
}

static void	KeySprintDownEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputSprint(pTS->mpBPM, true);
}

static void	KeySprintUpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputSprint(pTS->mpBPM, false);
}

static void	KeyTurnLeftEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mDeltaYaw	-=KEYTURN_RATE;
}

static void	KeyTurnRightEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mDeltaYaw	+=KEYTURN_RATE;
}

static void	KeyTurnUpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mDeltaPitch	+=KEYTURN_RATE;
}

static void	KeyTurnDownEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mDeltaPitch	-=KEYTURN_RATE;
}

static void	EscEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mbRunning	=false;
}

static Material	*MakeTerrainMat(TestStuff *pTS, const StuffKeeper *pSK)
{
	Material	*pRet	=MAT_Create(pTS->mpGD);

	vec3	light0		={	1.0f, 1.0f, 1.0f	};
	vec3	light1		={	0.2f, 0.3f, 0.3f	};
	vec3	light2		={	0.1f, 0.2f, 0.2f	};

	MAT_SetLights(pRet, light0, light1, light2, pTS->mLightDir);
	MAT_SetVShader(pRet, "WNormWPosTexFactVS", pSK);
	MAT_SetPShader(pRet, "TriTexFact8PS", pSK);
	MAT_SetSolidColour(pRet, GLM_VEC4_ONE);
	MAT_SetSRV0(pRet, "Terrain/TerAtlasLudum", pSK);
	MAT_SetSpecular(pRet, GLM_VEC3_ONE, 3.0f);
	MAT_SetWorld(pRet, GLM_MAT4_IDENTITY);

	//srv is set in the material, but need input layout set
	Terrain_SetSRVAndLayout(pTS->mpTer, NULL, pSK);

	return	pRet;
}

static Material	*MakeSkyBoxMat(TestStuff *pTS, const StuffKeeper *pSK)
{
	Material	*pRet	=MAT_Create(pTS->mpGD);

	MAT_SetVShader(pRet, "SkyBoxVS", pSK);
	MAT_SetPShader(pRet, "SkyGradientFogPS", pSK);
	MAT_SetWorld(pRet, GLM_MAT4_IDENTITY);

	return	pRet;
}

static void	SetupKeyBinds(Input *pInp)
{
	//event style bindings
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_f, ToggleFlyModeEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_ESCAPE, EscEH);

	//held bindings
	//movement
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_w, KeyMoveForwardEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_a, KeyMoveLeftEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_s, KeyMoveBackEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_d, KeyMoveRightEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_c, KeyMoveUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_z, KeyMoveDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_SPACE, KeyMoveJumpEH);

	//key turning
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_q, KeyTurnLeftEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_e, KeyTurnRightEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_r, KeyTurnUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_t, KeyTurnDownEH);

	//move data events
	INP_MakeBinding(pInp, INP_BIND_TYPE_MOVE, SDL_EVENT_MOUSE_MOTION, MouseMoveEH);

	//down/up events
	INP_MakeBinding(pInp, INP_BIND_TYPE_PRESS, SDL_BUTTON_RIGHT, RightMouseDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_RELEASE, SDL_BUTTON_RIGHT, RightMouseUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_PRESS, SDL_BUTTON_LEFT, LeftMouseDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_RELEASE, SDL_BUTTON_LEFT, LeftMouseUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_PRESS, SDLK_LSHIFT, KeySprintDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_RELEASE, SDLK_LSHIFT, KeySprintUpEH);
}

static void	SetupRastVP(GraphicsDevice *pGD)
{
	D3D11_RASTERIZER_DESC	rastDesc;
	rastDesc.AntialiasedLineEnable	=false;
	rastDesc.CullMode				=D3D11_CULL_BACK;
	rastDesc.FillMode				=D3D11_FILL_SOLID;
	rastDesc.FrontCounterClockwise	=true;
	rastDesc.MultisampleEnable		=false;
	rastDesc.DepthBias				=0;
	rastDesc.DepthBiasClamp			=0;
	rastDesc.DepthClipEnable		=true;
	rastDesc.ScissorEnable			=false;
	rastDesc.SlopeScaledDepthBias	=0;
	ID3D11RasterizerState	*pRast	=GD_CreateRasterizerState(pGD, &rastDesc);

	D3D11_VIEWPORT	vp;

	vp.Width	=RESX;
	vp.Height	=RESY;
	vp.MaxDepth	=1.0f;
	vp.MinDepth	=0.0f;
	vp.TopLeftX	=0;
	vp.TopLeftY	=0;

	GD_RSSetViewPort(pGD, &vp);
	GD_RSSetState(pGD, pRast);
	GD_IASetPrimitiveTopology(pGD, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

static void SetupDebugStrings(TestStuff *pTS, const StuffKeeper *pSK)
{
	//debug screen text
	pTS->mpST	=ST_Create(pTS->mpGD, pSK, MAX_ST_CHARS, "CGA", "CGA");

	vec2	embiggen	={	2.0f, 2.0f	};
	vec2	topLeftPos	={	5.0f, 5.0f	};
	vec2	nextLine	={	0.0f, 20.0f	};
	vec4	red			={	1.0f, 0.0f, 0.0f, 1.0f	};
	__attribute_maybe_unused__
	vec4	blue		={	0.0f, 0.0f, 1.0f, 1.0f	};
	vec4	green		={	0.0f, 1.0f, 0.0f, 1.0f	};
	vec4	magenta		={	1.0f, 0.0f, 1.0f, 1.0f	};
	__attribute_maybe_unused__
	vec4	cyan		={	0.0f, 1.0f, 1.0f, 1.0f	};


	ST_AddString(pTS->mpST, "Timing thing", 69, green, topLeftPos, embiggen);

	glm_vec2_add(topLeftPos, nextLine, topLeftPos);
	ST_AddString(pTS->mpST, "Movestuff", 70, red, topLeftPos, embiggen);

	glm_vec2_add(topLeftPos, nextLine, topLeftPos);
	ST_AddString(pTS->mpST, "Pos Storage", 71, magenta, topLeftPos, embiggen);
}

static void	MoveCharacter(TestStuff *pTS, const vec3 moveVec)
{
	if(pTS->mbFlyMode)
	{
		glm_vec3_add(pTS->mEyePos, moveVec, pTS->mEyePos);
	}
	else
	{
		vec3	end, newPos;
		glm_vec3_add(pTS->mPlayerPos, moveVec, end);

		int	footing	=Terrain_MoveSphere(pTS->mpTer, pTS->mPlayerPos, end, 0.25f, newPos);
		
		BPM_SetFooting(pTS->mpBPM, footing);

		//watch for a glitchy move
		float	dist	=glm_vec3_distance(pTS->mPlayerPos, newPos);
		if(dist > 10.0f)
		{
			printf("Glitchy Move: %f %f %f to %f %f %f\n",
				pTS->mPlayerPos[0], pTS->mPlayerPos[1], pTS->mPlayerPos[2], 
				end[0], end[1], end[2]);
		}

		glm_vec3_copy(newPos, pTS->mPlayerPos);

		ST_ModifyStringText(pTS->mpST, 70, "Moved!");
	}
}

static DictSZ *sLoadCharacterMeshParts(GraphicsDevice *pGD, StuffKeeper *pSK, const Character *pChar)
{
	StringList	*pParts	=Character_GetPartList(pChar);

	DictSZ		*pMeshes;
	UT_string	*szMeshPath;

	DictSZ_New(&pMeshes);
	utstring_new(szMeshPath);

	const StringList	*pCur	=SZList_Iterate(pParts);
	while(pCur != NULL)
	{
		utstring_printf(szMeshPath, "Characters/%s.mesh", SZList_IteratorVal(pCur));

		Mesh	*pMesh	=Mesh_Read(pGD, pSK, utstring_body(szMeshPath), false);

		DictSZ_Add(&pMeshes, SZList_IteratorValUT(pCur), pMesh);

		pCur	=SZList_IteratorNext(pCur);
	}

	utstring_done(szMeshPath);
	SZList_Clear(&pParts);

	return	pMeshes;
}