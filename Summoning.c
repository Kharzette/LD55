#include	<stdint.h>
#include	<stdbool.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<assert.h>
#include	<cglm/call.h>
#include	"GrogLibsC/AudioLib/SoundEffect.h"
#include	"GrogLibsC/AudioLib/Audio.h"
#include	"GrogLibsC/MaterialLib/StuffKeeper.h"
#include	"GrogLibsC/MaterialLib/PostProcess.h"
#include	"GrogLibsC/MaterialLib/Material.h"
#include	"GrogLibsC/MaterialLib/MaterialLib.h"
#define	CLAY_IMPLEMENTATION
#include	"GrogLibsC/MaterialLib/UIStuff.h"
#include	"GrogLibsC/UtilityLib/GraphicsDevice.h"
#include	"GrogLibsC/UtilityLib/StringStuff.h"
#include	"GrogLibsC/UtilityLib/ListStuff.h"
#include	"GrogLibsC/UtilityLib/MiscStuff.h"
#include	"GrogLibsC/UtilityLib/GameCamera.h"
#include	"GrogLibsC/UtilityLib/DictionaryStuff.h"
#include	"GrogLibsC/UtilityLib/UpdateTimer.h"
#include	"GrogLibsC/UtilityLib/PrimFactory.h"
#include	"GrogLibsC/UtilityLib/BipedMover.h"
#include	"GrogLibsC/UtilityLib/PlaneMath.h"
#include	"GrogLibsC/TerrainLib/Terrain.h"
#include	"GrogLibsC/MeshLib/AnimLib.h"
#include	"GrogLibsC/MeshLib/Character.h"
#include	"GrogLibsC/MeshLib/CommonPrims.h"
#include	"GrogLibsC/MeshLib/ParticleBoss.h"
#include	"GrogLibsC/InputLib/Input.h"
#include	"GrogLibsC/PhysicsLib/PhysicsStuff.h"


#define	RESX				1280
#define	RESY				720
#define	ROT_RATE			10.0f
#define	UVSCALE_RATE		1.0f
#define	KEYTURN_RATE		0.01f
#define	MOVE_THRESHOLD		0.1f
#define	MOVE_RATE			0.1f
#define	HEIGHT_SCALAR		0.15f
#define	TER_SMOOTH_STEPS	4
#define	RAY_LEN				100.0f
#define	RAY_WIDTH			0.05f
#define	IMPACT_WIDTH		0.2f
#define	NUM_RAYS			1000
#define	MOUSE_TO_ANG		0.001f
#define	MAX_ST_CHARS		256
#define	START_CAM_DIST		5.0f
#define	MAX_CAM_DIST		25.0f
#define	MIN_CAM_DIST		0.25f
#define	PLAYER_RADIUS		(0.25f)
#define	PLAYER_HEIGHT		(1.75f)
#define	PLAYER_EYE_OFFSET	(0.8)
#define	MAX_UI_VERTS		(8192)
#define	MAX_PARTICLES		4096
#define	RAMP_ANGLE			0.7f	//steepness can traverse on foot


//input context, stuff input handlers will need
typedef struct	TestStuff_t
{
	GraphicsDevice	*mpGD;
	Terrain			*mpTer;
	GameCamera		*mpCam;
	UIStuff			*mpUI;
	PhysicsStuff	*mpPhys;
	PhysVCharacter	*mpPhysChar;
	BipedMover		*mpBPM;
	ParticleBoss	*mpPBoss;

	//states
	ID3D11BlendState		*mpNoBlend;
	ID3D11DepthStencilState	*mpDepthEnable;
	ID3D11DepthStencilState	*mpDepthDisable;
	ID3D11SamplerState		*mpPointClamp;

	//layout for skybox
	ID3D11InputLayout	*mpSkyLayout;

	//toggles
	bool	mbMouseLooking;
	bool	mbRunning;
	bool	mbFlyMode;

	//clay pointer stuff
	Clay_Vector2	mScrollDelta;
	Clay_Vector2	mMousePos;

	//jolt stuff
	
	//prims
	PrimObject      *mpSphere;

	//misc data
	vec3	mDanglyForce;
	vec3	mLightDir;
	vec3	mEyePos;
	vec3	mPlayerPos;
	float	mDeltaYaw, mDeltaPitch, mCamDist;
}	TestStuff;

//static forward decs
static void	sSetupKeyBinds(Input *pInp);
static void	sSetupRastVP(GraphicsDevice *pGD);
static void	sSetDefaultCel(GraphicsDevice *pGD, CBKeeper *pCBK);

//clay stuff
static const Clay_RenderCommandArray sCreateLayout(const TestStuff *pTS, vec3 velocity);
static void sHandleClayErrors(Clay_ErrorData errorData);

//material setups
static Material	*sMakeTerrainMat(TestStuff *pTS, const StuffKeeper *pSK);
static Material	*sMakeSkyBoxMat(TestStuff *pTS, const StuffKeeper *pSK);
static Material	*sMakeSphereMat(TestStuff *pTS, const StuffKeeper *pSK);

//input event handlers
static void	sToggleFlyModeEH(void *pContext, const SDL_Event *pEvt);
static void	sLeftMouseDownEH(void *pContext, const SDL_Event *pEvt);
static void	sLeftMouseUpEH(void *pContext, const SDL_Event *pEvt);
static void	sRightMouseDownEH(void *pContext, const SDL_Event *pEvt);
static void	sRightMouseUpEH(void *pContext, const SDL_Event *pEvt);
static void	sKeyMoveForwardEH(void *pContext, const SDL_Event *pEvt);
static void	sKeyMoveBackEH(void *pContext, const SDL_Event *pEvt);
static void	sKeyMoveLeftEH(void *pContext, const SDL_Event *pEvt);
static void	sKeyMoveRightEH(void *pContext, const SDL_Event *pEvt);
static void	sKeyMoveUpEH(void *pContext, const SDL_Event *pEvt);
static void	sKeyMoveDownEH(void *pContext, const SDL_Event *pEvt);
static void	sKeyMoveJumpEH(void *pContext, const SDL_Event *pEvt);
static void	sKeySprintDownEH(void *pContext, const SDL_Event *pEvt);
static void	sKeySprintUpEH(void *pContext, const SDL_Event *pEvt);
static void	sKeyTurnLeftEH(void *pContext, const SDL_Event *pEvt);
static void	sKeyTurnRightEH(void *pContext, const SDL_Event *pEvt);
static void	sKeyTurnUpEH(void *pContext, const SDL_Event *pEvt);
static void	sKeyTurnDownEH(void *pContext, const SDL_Event *pEvt);
static void sMouseMoveEH(void *pContext, const SDL_Event *pEvt);
static void sEscEH(void *pContext, const SDL_Event *pEvt);


int main(void)
{
	printf("Ludum Dare 55!\n");

	PhysicsStuff	*pPhys	=Phys_Create();

	//TODO: ask user when released
	Audio	*pAud	=Audio_Create(0);

	//store a bunch of vars in a struct
	//for ref/modifying by input handlers
	TestStuff	*pTS	=malloc(sizeof(TestStuff));
	memset(pTS, 0, sizeof(TestStuff));

	//start in fly mode?
	pTS->mbFlyMode	=true;
	pTS->mCamDist	=START_CAM_DIST;
	pTS->mpPhys		=pPhys;
	
	//set player on corner near origin
	glm_vec3_scale(GLM_VEC3_ONE, 22.0f, pTS->mPlayerPos);

	//input and key / mouse bindings
	Input	*pInp	=INP_CreateInput();
	sSetupKeyBinds(pInp);

	if(!GD_Init(&pTS->mpGD, "Ludum Dare 55!  Summoning...",
		0, 0, RESX, RESY, true, D3D_FEATURE_LEVEL_11_1))
	{
		printf("Couldn't create GraphicsDevice!\n");
		return	EXIT_FAILURE;
	}

	//turn on border
	GD_SetWindowBordered(pTS->mpGD, true);
	
	sSetupRastVP(pTS->mpGD);

	StuffKeeper	*pSK	=StuffKeeper_Create(pTS->mpGD);
	if(pSK == NULL)
	{
		printf("Couldn't create StuffKeeper!\n");
		GD_Destroy(&pTS->mpGD);
		return	EXIT_FAILURE;
	}

	//grab common states
	pTS->mpDepthEnable	=StuffKeeper_GetDepthStencilState(pSK, "EnableDepth");
	pTS->mpDepthDisable	=StuffKeeper_GetDepthStencilState(pSK, "DisableDepth");
	pTS->mpNoBlend		=StuffKeeper_GetBlendState(pSK, "NoBlend");
	pTS->mpPointClamp	=StuffKeeper_GetSamplerState(pSK, "PointClamp");

	//a terrain chunk
	pTS->mpTer	=Terrain_Create(pTS->mpGD, pPhys, "Blort",
		"Textures/Terrain/HeightMaps/HeightMap.png",
		TER_SMOOTH_STEPS, HEIGHT_SCALAR);

	PrimObject	*pSkyCube	=PF_CreateCube(10.0f, true, pTS->mpGD);
	CBKeeper	*pCBK		=CBK_Create(pTS->mpGD);
	PostProcess	*pPP		=PP_Create(pTS->mpGD, pSK, pCBK);

	//set sky gradient
	{
		vec3	skyHorizon	={	1.0f, 0.5f, 0.0f	};
		vec3	skyHigh		={	1.0f, 0.25f, 0.0f	};

		CBK_SetSky(pCBK, skyHorizon, skyHigh);
		CBK_SetFogVars(pCBK, 50.0f, 300.0f, true);
	}

	pTS->mpSkyLayout	=StuffKeeper_GetInputLayout(pSK, "VPosNormTex0");

	sSetDefaultCel(pTS->mpGD, pCBK);

	PP_SetTargets(pPP, pTS->mpGD, "BackColor", "BackDepth");

	float	aspect	=(float)RESX / (float)RESY;

	//these need align
	__attribute((aligned(32)))	mat4	charMat, camProj, textProj;
	__attribute((aligned(32)))	mat4	viewMat;

	pTS->mEyePos[1]	=0.6f;
	pTS->mEyePos[2]	=4.5f;

	//game camera
	pTS->mpCam	=GameCam_Create(false, 0.1f, 2000.0f, GLM_PI_4f, aspect, 1.0f, 10.0f);

	//biped mover
	pTS->mpBPM	=BPM_Create(pTS->mpCam);

	//physics character
	pTS->mpPhysChar	=Phys_CreateVCharacter(pPhys,
		PLAYER_RADIUS, PLAYER_HEIGHT,
		pTS->mPlayerPos);

	BPM_SetMoveMethod(pTS->mpBPM, pTS->mbFlyMode? BPM_MOVE_FLY : BPM_MOVE_GROUND);

	SoundEffectPlay("synth", pTS->mPlayerPos);

	//3D Projection
	GameCam_GetProjection(pTS->mpCam, camProj);
	CBK_SetProjection(pCBK, camProj);

	//2d projection for text
	glm_ortho(0, RESX, RESY, 0, -1.0f, 1.0f, textProj);

	//set constant buffers to shaders, think I just have to do this once
	CBK_SetCommonCBToShaders(pCBK, pTS->mpGD);
	CBK_SetEmitterToShaders(pCBK, pTS->mpGD);

	pTS->mpPBoss	=PB_Create(pTS->mpGD, pSK, pCBK);

	pTS->mpUI	=UI_Create(pTS->mpGD, pSK, MAX_UI_VERTS);

	UI_AddFont(pTS->mpUI, "MeiryoUI26", 0);

	//clay init
    uint64_t totalMemorySize = Clay_MinMemorySize();
    Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
    Clay_Initialize(clayMemory, (Clay_Dimensions) { (float)RESX, (float)RESY }, (Clay_ErrorHandler) { sHandleClayErrors });
    Clay_SetMeasureTextFunction(UI_MeasureText, pTS->mpUI);

	Clay_SetDebugModeEnabled(false);

	pTS->mLightDir[0]		=0.3f;
	pTS->mLightDir[1]		=-0.7f;
	pTS->mLightDir[2]		=-0.5f;

	glm_vec3_normalize(pTS->mLightDir);
	glm_mat4_identity(charMat);

	uint32_t	emitterID	=PB_CreateEmitter(pTS->mpPBoss, "Particles/Fiery",
		EMIT_SHAPE_BOX, 1.1f, (vec4){1,1,1,0.95f}, 0.01f,
		5.0f, GLM_VEC3_ZERO, MAX_PARTICLES, -0.9f, 0.9f,
		(vec4){-0.01f, 0, 0, 0.01f}, (vec4){-0.02f, 0, 0, 0.01f},
		-0.1f, 0.1f, -0.5f, 0.5f, 12, 25, GLM_VEC3_ZERO);

	PB_EmitterActivate(pTS->mpPBoss, emitterID, true);

	UpdateTimer	*pUT	=UpdateTimer_Create(true, false);

	//TODO: user config file or something?  144 might be too fast for some
//	UpdateTimer_SetFixedTimeStepMilliSeconds(pUT, 6.944444f);	//144hz
	UpdateTimer_SetFixedTimeStepMilliSeconds(pUT, 16.66666f);	//60hz

	//test prims
//	PrimObject	*pSphere	=PF_CreateSphere(GLM_VEC3_ZERO,	0.25f, false, pTS->mpGD);

	//materials
	Material	*pTerMat	=sMakeTerrainMat(pTS, pSK);
	Material	*pSkyBoxMat	=sMakeSkyBoxMat(pTS, pSK);
//	Material	*pSphereMat	=sMakeSphereMat(pTS, pSK);

	//character
	Character	*pChar		=Character_Read(pTS->mpGD, pSK, "Characters/Protag.Character", false);
	AnimLib		*pALib		=AnimLib_Read("Characters/Protag.AnimLib");
	MaterialLib	*pCharMats	=MatLib_Read("Characters/Protag.MatLib", pSK);

	Character_AssignMaterial(pChar, 0, "ProtagHellCel");

	float	animTime		=0.0f;

	pTS->mbRunning		=true;
	while(pTS->mbRunning)
	{
		pTS->mDeltaYaw		=0.0f;
		pTS->mDeltaPitch	=0.0f;

		UpdateTimer_Stamp(pUT);
		for(float secDelta =UpdateTimer_GetUpdateDeltaSeconds(pUT);
			secDelta > 0.0f;
			secDelta =UpdateTimer_GetUpdateDeltaSeconds(pUT))
		{
			//do input here
			//move turn etc
			INP_Update(pInp, pTS);

			{
				vec3	moveVec;

				//check on ground and footing
				bool	bSup		=Phys_VCharacterIsSupported(pTS->mpPhysChar);
				bool	bFooting	=false;
				if(bSup)
				{
					vec3	norm;
					Phys_VCharacterGetGroundNormal(pTS->mpPhysChar, norm);
					bFooting	=PM_IsGroundNormalAng(norm, RAMP_ANGLE);
				}

				bool	bJumped	=BPM_Update(pTS->mpBPM, bSup, bFooting, secDelta, moveVec);
				if(bJumped)
				{
					SoundEffectPlay("jump", pTS->mPlayerPos);
				}

				//moveVec is an amount to move this frame
				//convert to meters per second for phys
				vec3	mpsMove;
				glm_vec3_scale(moveVec, 60, mpsMove);

				vec3	resultVelocity;
				Phys_VCharacterMove(pPhys, pTS->mpPhysChar, mpsMove,
					secDelta, resultVelocity);
				
				bSup	=Phys_VCharacterIsSupported(pTS->mpPhysChar);
				if(bSup)
				{
					//here I think maybe the plane the player is on
					//should be checked for a bad footing situation
					//in which case velocity shouldn't be cleared
					vec3	norm;
					Phys_VCharacterGetGroundNormal(pTS->mpPhysChar, norm);
					if(PM_IsGroundNormalAng(norm, RAMP_ANGLE))
					{
						//if still on the ground, cancel out vertical velocity
						BPM_SetVerticalVelocity(pTS->mpBPM, resultVelocity);
					}
				}
			}

			Phys_VCharacterGetPos(pTS->mpPhysChar, pTS->mPlayerPos);

			Phys_Update(pPhys, secDelta);

			UpdateTimer_UpdateDone(pUT);
		}

		//update materials incase light changed
		MAT_SetLightDirection(pTerMat, pTS->mLightDir);

		//render update
		float	dt	=UpdateTimer_GetRenderUpdateDeltaSeconds(pUT);

		vec3	velocity;
		BPM_GetVelocity(pTS->mpBPM, velocity);

		//update audio
		Audio_Update(pAud, pTS->mPlayerPos, velocity);

		//player moving?
		float	moving	=glm_vec3_norm(velocity);

		if(moving > MOVE_THRESHOLD)
		{
			if(Phys_VCharacterIsSupported(pTS->mpPhysChar))
			{
				animTime	+=dt * moving * 1.0f;
			}
			else
			{
				animTime	+=dt * moving * 0.1f;
			}
			AnimLib_Animate(pALib, "Run", animTime);
		}
		else
		{
			animTime	+=dt;
			AnimLib_Animate(pALib, "Idle", animTime);
		}

		//set no blend, I think post processing turns it on maybe
		GD_OMSetBlendState(pTS->mpGD, pTS->mpNoBlend);
		GD_PSSetSampler(pTS->mpGD, pTS->mpPointClamp, 0);

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
			if(pTS->mbFlyMode)
			{
				GameCam_GetViewMatrixFly(pTS->mpCam, viewMat, pTS->mEyePos);
			}
			else
			{
				GameCam_GetViewMatrixThird(pTS->mpCam, viewMat, pTS->mEyePos);
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
		GD_OMSetDepthStencilState(pTS->mpGD, pTS->mpDepthDisable);

		//draw sky first
		GD_IASetVertexBuffers(pTS->mpGD, pSkyCube->mpVB, pSkyCube->mVertSize, 0);
		GD_IASetIndexBuffers(pTS->mpGD, pSkyCube->mpIB, DXGI_FORMAT_R16_UINT, 0);
		GD_IASetInputLayout(pTS->mpGD, pTS->mpSkyLayout);

		MAT_Apply(pSkyBoxMat, pCBK, pTS->mpGD);
		GD_DrawIndexed(pTS->mpGD, pSkyCube->mIndexCount, 0, 0);

		//turn depth back on
		GD_OMSetDepthStencilState(pTS->mpGD, pTS->mpDepthEnable);

		//terrain draw
		Terrain_DrawMat(pTS->mpTer, pTS->mpGD, pCBK, pTerMat);

		//set mesh draw stuff
		if(!pTS->mbFlyMode)
		{
			//player direction
			GameCam_GetFlatLookMatrix(pTS->mpCam, charMat);

			//drop mesh to ground
			vec3	feetToCenter	={	0.0f, -(PLAYER_HEIGHT * 0.5f), 0.0f	};

			glm_translate(charMat, feetToCenter);

			Material	*pCM	=MatLib_GetMaterial(pCharMats, "ProtagHellCel");
			assert(pCM);
			MAT_SetWorld(pCM, charMat);
			MAT_SetDanglyForce(pCM, pTS->mDanglyForce);
		}

		Character_Draw(pChar, pCharMats, pALib, pTS->mpGD, pCBK);

		PB_UpdateAndDraw(pTS->mpPBoss, dt);

		//set proj for 2D
		CBK_SetProjection(pCBK, textProj);
		CBK_UpdateFrame(pCBK, pTS->mpGD);

		Clay_UpdateScrollContainers(true, pTS->mScrollDelta, dt);

		pTS->mScrollDelta.x	=pTS->mScrollDelta.y	=0.0f;
	
		Clay_RenderCommandArray renderCommands = sCreateLayout(pTS, velocity);
	
		UI_BeginDraw(pTS->mpUI);
	
		UI_ClayRender(pTS->mpUI, renderCommands);
	
		UI_EndDraw(pTS->mpUI);

		//change back to 3D
		CBK_SetProjection(pCBK, camProj);
		CBK_UpdateFrame(pCBK, pTS->mpGD);

		GD_Present(pTS->mpGD);
	}

	Terrain_Destroy(&pTS->mpTer, pPhys);

	Character_Destroy(pChar);

	MatLib_Destroy(&pCharMats);

	Phys_Destroy(&pPhys);

	PB_Destroy(&pTS->mpPBoss);

	GD_Destroy(&pTS->mpGD);

	Audio_Destroy(&pAud);

	return	EXIT_SUCCESS;
}

static void	sToggleFlyModeEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mbFlyMode	=!pTS->mbFlyMode;

	BPM_SetMoveMethod(pTS->mpBPM, pTS->mbFlyMode? BPM_MOVE_FLY : BPM_MOVE_GROUND);
}

static void	sLeftMouseDownEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);
}

static void	sLeftMouseUpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);
}

static void	sRightMouseDownEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	GD_SetMouseRelative(pTS->mpGD, true);

	pTS->mbMouseLooking	=true;
}

static void	sRightMouseUpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	GD_SetMouseRelative(pTS->mpGD, false);

	pTS->mbMouseLooking	=false;
}

static void	sMouseMoveEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	if(pTS->mbMouseLooking)
	{
		pTS->mDeltaYaw		+=(pEvt->motion.xrel * MOUSE_TO_ANG);
		pTS->mDeltaPitch	+=(pEvt->motion.yrel * MOUSE_TO_ANG);
	}
}

static void	sKeyMoveForwardEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputForward(pTS->mpBPM);
}

static void	sKeyMoveBackEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputBack(pTS->mpBPM);
}

static void	sKeyMoveLeftEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputLeft(pTS->mpBPM);
}

static void	sKeyMoveRightEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputRight(pTS->mpBPM);
}

static void	sKeyMoveUpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputUp(pTS->mpBPM);
}

static void	sKeyMoveDownEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputDown(pTS->mpBPM);
}

static void	sKeyMoveJumpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputJump(pTS->mpBPM);
}

static void	sKeySprintDownEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputSprint(pTS->mpBPM, true);
}

static void	sKeySprintUpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	BPM_InputSprint(pTS->mpBPM, false);
}

static void	sKeyTurnLeftEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mDeltaYaw	-=KEYTURN_RATE;
}

static void	sKeyTurnRightEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mDeltaYaw	+=KEYTURN_RATE;
}

static void	sKeyTurnUpEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mDeltaPitch	+=KEYTURN_RATE;
}

static void	sKeyTurnDownEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mDeltaPitch	-=KEYTURN_RATE;
}

static void	sEscEH(void *pContext, const SDL_Event *pEvt)
{
	TestStuff	*pTS	=(TestStuff *)pContext;

	assert(pTS);

	pTS->mbRunning	=false;
}

static Material	*sMakeTerrainMat(TestStuff *pTS, const StuffKeeper *pSK)
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

static void	sSetDefaultCel(GraphicsDevice *pGD, CBKeeper *pCBK)
{
	float	mins[4]	={	0.0f, 0.3f, 0.6f, 1.0f	};
	float	maxs[4]	={	0.3f, 0.6f, 1.0f, 5.0f	};
	float	snap[4]	={	0.3f, 0.5f, 0.9f, 1.4f	};

	CBK_SetCelSteps(pCBK, mins, maxs, snap, 4);

	CBK_UpdateCel(pCBK, pGD);
}

static Material	*sMakeSkyBoxMat(TestStuff *pTS, const StuffKeeper *pSK)
{
	Material	*pRet	=MAT_Create(pTS->mpGD);

	MAT_SetVShader(pRet, "SkyBoxVS", pSK);
	MAT_SetPShader(pRet, "SkyGradientFogPS", pSK);
	MAT_SetWorld(pRet, GLM_MAT4_IDENTITY);

	return	pRet;
}

static Material	*sMakeSphereMat(TestStuff *pTS, const StuffKeeper *pSK)
{
	Material	*pRet	=MAT_Create(pTS->mpGD);

	vec3	light0		={	1.0f, 1.0f, 1.0f	};
	vec3	light1		={	0.2f, 0.3f, 0.3f	};
	vec3	light2		={	0.1f, 0.2f, 0.2f	};

	MAT_SetLights(pRet, light0, light1, light2, pTS->mLightDir);
	MAT_SetVShader(pRet, "WNormWPosVS", pSK);
	MAT_SetPShader(pRet, "TriSolidSpecPS", pSK);
	MAT_SetSolidColour(pRet, GLM_VEC4_ONE);
	MAT_SetSpecular(pRet, GLM_VEC3_ONE, 6.0f);
	MAT_SetWorld(pRet, GLM_MAT4_IDENTITY);

	return	pRet;
}

static void	sSetupKeyBinds(Input *pInp)
{
	//event style bindings
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_F, sToggleFlyModeEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_EVENT, SDLK_ESCAPE, sEscEH);

	//held bindings
	//movement
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_W, sKeyMoveForwardEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_A, sKeyMoveLeftEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_S, sKeyMoveBackEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_D, sKeyMoveRightEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_C, sKeyMoveUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_Z, sKeyMoveDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_SPACE, sKeyMoveJumpEH);

	//key turning
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_Q, sKeyTurnLeftEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_E, sKeyTurnRightEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_R, sKeyTurnUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_HELD, SDLK_T, sKeyTurnDownEH);

	//move data events
	INP_MakeBinding(pInp, INP_BIND_TYPE_MOVE, SDL_EVENT_MOUSE_MOTION, sMouseMoveEH);

	//down/up events
	INP_MakeBinding(pInp, INP_BIND_TYPE_PRESS, SDL_BUTTON_RIGHT, sRightMouseDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_RELEASE, SDL_BUTTON_RIGHT, sRightMouseUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_PRESS, SDL_BUTTON_LEFT, sLeftMouseDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_RELEASE, SDL_BUTTON_LEFT, sLeftMouseUpEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_PRESS, SDLK_LSHIFT, sKeySprintDownEH);
	INP_MakeBinding(pInp, INP_BIND_TYPE_RELEASE, SDLK_LSHIFT, sKeySprintUpEH);
}

static void	sSetupRastVP(GraphicsDevice *pGD)
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


//clay stuffs
static char	sVelString[64];

static Clay_RenderCommandArray	sCreateLayout(const TestStuff *pTS, vec3 velocity)
{
	Clay_BeginLayout();

	sprintf(sVelString, "Velocity: %f, %f, %f", velocity[0], velocity[1], velocity[2]);

	Clay_String	velInfo;

	velInfo.chars	=sVelString;
	velInfo.length	=strlen(sVelString);

	CLAY({.id=CLAY_ID("OuterContainer"), .layout =
		{
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.sizing =
			{
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0)
			},
			.padding = { 8, 8, 8, 8 },
			.childGap = 8
		}})
	{
		CLAY_TEXT(velInfo, CLAY_TEXT_CONFIG({ .fontSize = 26, .textColor = {0, 70, 70, 155} }));

//		sCheckLOS(pTS);
	}

	return	Clay_EndLayout();
}

static bool reinitializeClay = false;

static void sHandleClayErrors(Clay_ErrorData errorData) {
    printf("%s", errorData.errorText.chars);
    if (errorData.errorType == CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED) {
        reinitializeClay = true;
        Clay_SetMaxElementCount(Clay_GetMaxElementCount() * 2);
    } else if (errorData.errorType == CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED) {
        reinitializeClay = true;
        Clay_SetMaxMeasureTextCacheWordCount(Clay_GetMaxMeasureTextCacheWordCount() * 2);
    }
}