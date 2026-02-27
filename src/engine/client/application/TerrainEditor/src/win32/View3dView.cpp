//
// View3dView.cpp 
// aommers
//
// copyright 2001, sony online entertainment
//

//-------------------------------------------------------------------

#include "FirstTerrainEditor.h"
#include "View3dView.h"

#include "MapFrame.h"
#include "Resource.h"
#include "TerrainEditorDoc.h"
#include "TerrainEditorLog.h"
#include "clientAudio/Audio.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/Light.h"
#include "clientGraphics/RenderWorld.h"
#include "clientGraphics/ShaderPrimitiveSorter.h"
#include "clientObject/GameCamera.h"
#include "clientTerrain/ClientProceduralTerrainAppearance.h"
#include "clientTerrain/ClientTerrainSorter.h"
#include "clientTerrain/EnvironmentBlock.h"
#include "clientTerrain/GroundEnvironment.h"
#include "sharedDebug/Profiler.h"
#include "sharedFile/Iff.h"
#include "sharedFile/TreeFile.h"
#include "sharedMath/VectorArgb.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplate.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/Object.h"
#include "sharedTerrain/FloraManager.h"
#include "sharedTerrain/ProceduralTerrainAppearance.h"
#include "sharedTerrain/TerrainAppearance.h"
#include "sharedTerrain/TerrainObject.h"
#include "sharedTerrain/TerrainQuadTree.h"
#include <float.h>

//-------------------------------------------------------------------

static inline bool keyDown (int key)
{
	return (GetKeyState (key) & 0x8000) != 0;
}

static bool isFinitePosition (const Vector& v)
{
	return _finite (v.x) != 0 && _finite (v.y) != 0 && _finite (v.z) != 0;
}

static void clampCameraToTerrainBounds (GameCamera* camera, TerrainObject* terrain)
{
	if (!camera || !terrain)
		return;

	Vector position = camera->getPosition_p ();
	if (!isFinitePosition (position))
	{
		position.makeZero ();
		camera->setPosition_p (position);
		return;
	}

	const real halfMap = terrain->getMapWidthInMeters () * 0.5f;
	const real margin = 16.f;
	const real limit = halfMap > margin ? (halfMap - margin) : halfMap;

	position.x = clamp (-limit, position.x, limit);
	position.z = clamp (-limit, position.z, limit);
	position.y = clamp (-2048.f, position.y, 2048.f);
	camera->setPosition_p (position);
}

struct TerrainQualityState
{
	int   chunkBudget;
	real  lodThreshold;
	real  highLodThreshold;
	bool  moving;
	DWORD lastApplyMs;
};

static TerrainQualityState s_terrainQualityState = { -1, -1.f, -1.f, false, 0 };

static int clampChunkBudgetForMemoryPressure (int requestedChunkBudget)
{
	MEMORYSTATUSEX memoryStatus;
	memset (&memoryStatus, 0, sizeof (memoryStatus));
	memoryStatus.dwLength = sizeof (memoryStatus);
	if (!GlobalMemoryStatusEx (&memoryStatus))
		return requestedChunkBudget;

	// Scale down chunk budget under memory pressure to avoid allocator spikes and AVs.
	if (memoryStatus.dwMemoryLoad >= 85 || memoryStatus.ullAvailPhys < (1ULL << 30))
		return min (requestedChunkBudget, 8192);
	if (memoryStatus.dwMemoryLoad >= 75 || memoryStatus.ullAvailPhys < (2ULL << 30))
		return min (requestedChunkBudget, 12288);
	if (memoryStatus.dwMemoryLoad >= 65 || memoryStatus.ullAvailPhys < (3ULL << 30))
		return min (requestedChunkBudget, 16384);

	return requestedChunkBudget;
}

static void applyEditorTerrainChunkQualityProfile (real farPlaneDistance, bool moving, bool force)
{
	int chunkBudget = moving
		? clamp (6144, static_cast<int> (farPlaneDistance * 1.5f), 16384)
		: clamp (8192, static_cast<int> (farPlaneDistance * 2.5f), 32768);
	chunkBudget = clampChunkBudgetForMemoryPressure (chunkBudget);

	const real lodThreshold = moving ? 4.5f : 6.5f;
	const real highLodThreshold = moving ? 5.5f : 7.5f;

	const DWORD now = GetTickCount ();
	if (!force)
	{
		if (s_terrainQualityState.chunkBudget == chunkBudget &&
			s_terrainQualityState.lodThreshold == lodThreshold &&
			s_terrainQualityState.highLodThreshold == highLodThreshold)
		{
			return;
		}

		// Avoid rapid oscillation while input state changes frame-to-frame.
		if ((now - s_terrainQualityState.lastApplyMs) < 250 && s_terrainQualityState.moving == moving)
			return;
	}

	ProceduralTerrainAppearance::setMaximumNumberOfChunksAllowed (chunkBudget);
	TerrainObject::setLevelOfDetailThreshold (lodThreshold);
	TerrainObject::setHighLevelOfDetailThreshold (highLodThreshold);

	s_terrainQualityState.chunkBudget = chunkBudget;
	s_terrainQualityState.lodThreshold = lodThreshold;
	s_terrainQualityState.highLodThreshold = highLodThreshold;
	s_terrainQualityState.moving = moving;
	s_terrainQualityState.lastApplyMs = now;
}

//-------------------------------------------------------------------

IMPLEMENT_DYNCREATE(View3dView, CView)

//-------------------------------------------------------------------

View3dView::View3dView() :
	CView (),
	camera (0),
	terrain (0),
	yaw (0),
	pitch (0),
	moveSpeed (2.f),
	drawDistance (4000.f),
	nearPlaneDistance (0.1f),
	farPlaneDistance (4000.f),
	timeOfDay (0.5f),
	timer (0),
	milliseconds (50),
	elapsedTime (0.f),
	render (false),
	hooksInstalled (false),
	needsRedraw (true),
	environmentInitialized (false),
	terrainReloadPending (false),
	terrainFaulted (false),
	middleMouseDragging (false),
	rightMouseDragging (false),
	movingUpdateCounter (0),
	lastMousePoint (0, 0),
	lastMouseValid (false)
{
}

//-------------------------------------------------------------------

View3dView::~View3dView()
{
	clearTerrain ();

	if (camera)
	{
		camera->removeFromWorld ();
		delete camera;
		camera = 0;
	}

	if (terrain)
	{
		terrain->removeFromWorld ();
		delete terrain;
		terrain = 0;
	}
}

//-------------------------------------------------------------------

void View3dView::clearTerrain ()
{
	if (environmentInitialized)
	{
		GroundEnvironment& env = GroundEnvironment::getInstance ();
		env.setReferenceCamera (0);
		env.setReferenceObject (0);
		env.setClientProceduralTerrainAppearance (0, 0);
		environmentInitialized = false;
	}

	FloraManager::setReferenceObject (0);
	ClientProceduralTerrainAppearance::setReferenceCamera (0);

	if (hooksInstalled)
	{
		CellProperty* const worldCell = CellProperty::getWorldCellProperty ();
		if (worldCell)
		{
			worldCell->removePreDrawRenderHookFunction (&ClientTerrainSorter::draw);
			worldCell->removeExitRenderHookFunction (&ClientTerrainSorter::clear);
		}
		hooksInstalled = false;
	}

	if (terrain)
	{
		TerrainAppearance* const terrainAppearance = dynamic_cast<TerrainAppearance*> (terrain->getAppearance ());
		if (terrainAppearance)
			terrainAppearance->removeAllReferenceObjects ();

		terrain->setAppearance (0);
	}
}

//-------------------------------------------------------------------

void View3dView::loadTerrain ()
{
	const TerrainEditorDoc* doc = dynamic_cast<const TerrainEditorDoc*> (GetDocument ());
	if (!doc || !terrain || !camera)
		return;

	TerrainEditorLog::write ("loadTerrain: begin");
	clearTerrain ();
	TerrainEditorLog::write ("loadTerrain: cleared existing terrain");

	Iff iff (10000);
	doc->save (iff, 0);
	iff.allowNonlinearFunctions ();
	iff.goToTopOfForm ();
	TerrainEditorLog::write ("loadTerrain: document saved into iff");

	const AppearanceTemplate* at = AppearanceTemplateList::fetch (&iff);
	if (!at)
	{
		TerrainEditorLog::write ("loadTerrain: AppearanceTemplateList::fetch returned null");
		terrainFaulted = true;
		return;
	}

	Appearance* appearance = at->createAppearance ();
	if (!appearance)
	{
		TerrainEditorLog::write ("loadTerrain: createAppearance returned null");
		AppearanceTemplateList::release (at);
		terrainFaulted = true;
		return;
	}

	terrain->setAppearance (appearance);
	AppearanceTemplateList::release (at);
	TerrainEditorLog::write ("loadTerrain: appearance created and assigned");

	TerrainAppearance* const terrainAppearance = dynamic_cast<TerrainAppearance*> (appearance);
	if (terrainAppearance)
	{
		terrainAppearance->removeAllReferenceObjects ();
		terrainAppearance->addReferenceObject (camera);
	}
	TerrainEditorLog::write ("loadTerrain: reference object added");

	ClientProceduralTerrainAppearance::setReferenceCamera (camera);

	CellProperty* const worldCell = CellProperty::getWorldCellProperty ();
	if (worldCell)
	{
		worldCell->addPreDrawRenderHookFunction (&ClientTerrainSorter::draw);
		worldCell->addExitRenderHookFunction (&ClientTerrainSorter::clear);
		hooksInstalled = true;
	}

	ClientProceduralTerrainAppearance* const cpta = dynamic_cast<ClientProceduralTerrainAppearance*> (appearance);
	if (cpta)
	{
		GroundEnvironment& env = GroundEnvironment::getInstance ();
		env.setClientProceduralTerrainAppearance (cpta, 86400.f);
		env.setReferenceCamera (camera);
		env.setReferenceObject (camera);
		env.setWeatherIndex (0);
		env.setEnableFog (true);
		env.setPaused (true);
		env.setTime (timeOfDay * 86400.f, true);
		environmentInitialized = true;
	}
	TerrainEditorLog::write ("loadTerrain: environment bound");

	Vector2d center;
	center.makeZero ();
	if (doc->getMapFrame ())
	{
		center = doc->getMapFrame ()->getCenter ();
		camera->setPosition_p (Vector (center.x, 0.f, center.y));
	}

	FloraManager::setReferenceObject (camera);
	TerrainQuadTree::setUseDistance2d (false);
	applyEditorTerrainChunkQualityProfile (farPlaneDistance, false, true);
	playEnvironmentMusic ();

	terrainFaulted = false;
	needsRedraw = true;
	Invalidate ();
	TerrainEditorLog::write ("loadTerrain: end");
}

//-------------------------------------------------------------------

BEGIN_MESSAGE_MAP(View3dView, CView)
	//{{AFX_MSG_MAP(View3dView)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_COMMAND(ID_REFRESH, OnRefresh)
	ON_COMMAND(ID_BUTTON_STOP, OnClearTerrain)
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-------------------------------------------------------------------

void View3dView::OnDraw(CDC* pDC)
{
	UNREF (pDC);

	if (!terrain || !camera)
		return;

	if (!terrain->getAppearance ())
		return;

	bool moved = false;
	bool qualityMoving = middleMouseDragging || rightMouseDragging;

	if (render)
	{
		const real speed = moveSpeed * (keyDown (VK_SHIFT) ? 4.f : 1.f);

		if (keyDown ('W') || keyDown (VK_UP))
			{ camera->move_o (Vector::unitZ * speed); clampCameraToTerrainBounds (camera, terrain); moved = true; qualityMoving = true; }

		if (keyDown ('S') || keyDown (VK_DOWN))
			{ camera->move_o (Vector::negativeUnitZ * speed); clampCameraToTerrainBounds (camera, terrain); moved = true; qualityMoving = true; }

		if (keyDown ('A') || keyDown (VK_LEFT))
			{ camera->move_o (Vector::negativeUnitX * speed); clampCameraToTerrainBounds (camera, terrain); moved = true; qualityMoving = true; }

		if (keyDown ('D') || keyDown (VK_RIGHT))
			{ camera->move_o (Vector::unitX * speed); clampCameraToTerrainBounds (camera, terrain); moved = true; qualityMoving = true; }

		if (keyDown ('E') || keyDown (VK_PRIOR))
			{ camera->move_o (Vector::unitY * speed); clampCameraToTerrainBounds (camera, terrain); moved = true; qualityMoving = true; }

		if (keyDown ('Q') || keyDown (VK_NEXT))
			{ camera->move_o (Vector::negativeUnitY * speed); clampCameraToTerrainBounds (camera, terrain); moved = true; qualityMoving = true; }
	}

	if (moved)
		needsRedraw = true;

	if (elapsedTime > 0.f)
	{
		real timeStep = elapsedTime;
		elapsedTime = 0.f;
		if (timeStep > 0.1f)
			timeStep = 0.1f;

		applyEditorTerrainChunkQualityProfile (farPlaneDistance, qualityMoving, false);

		const bool doHeavyTerrainUpdate = !qualityMoving || ((++movingUpdateCounter % 3) == 0);
		if (!qualityMoving)
			movingUpdateCounter = 0;

		if (doHeavyTerrainUpdate)
		{
			terrain->getAppearance ()->setRenderedThisFrame ();
			IGNORE_RETURN (terrain->alter (timeStep));
		}

		ClientProceduralTerrainAppearance::setReferenceCamera (camera);
		FloraManager::setReferenceObject (camera);
		TerrainQuadTree::setUseDistance2d (false);

		if (environmentInitialized && doHeavyTerrainUpdate)
		{
			GroundEnvironment& env = GroundEnvironment::getInstance ();
			env.setReferenceCamera (camera);
			env.setReferenceObject (camera);
			env.alter (timeStep);
		}

		ShaderPrimitiveSorter::alter (timeStep);
		needsRedraw = true;
	}

	if (!needsRedraw)
		return;

	needsRedraw = false;

	CRect rect;
	GetClientRect (&rect);
	if (rect.Width () <= 0 || rect.Height () <= 0)
		return;

	Graphics::setViewport (0, 0, rect.Width (), rect.Height ());

	Graphics::beginScene ();

		Graphics::clearViewport (true, 0xff4080c0, true, 1.0f, true, 0);

		camera->renderScene ();

	Graphics::endScene ();
	Graphics::present (m_hWnd, rect.Width (), rect.Height ());
}

//-------------------------------------------------------------------

#ifdef _DEBUG
void View3dView::AssertValid() const
{
	CView::AssertValid();
}

void View3dView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif //_DEBUG

//-------------------------------------------------------------------

void View3dView::OnInitialUpdate() 
{
	CView::OnInitialUpdate();

	timer = SetTimer (1, milliseconds, 0);

	//-- create camera
	camera = new GameCamera ();
	camera->setHorizontalFieldOfView (PI_OVER_3);
	camera->setNearPlane (nearPlaneDistance);
	camera->setFarPlane (farPlaneDistance);
	camera->addToWorld ();

	terrain = new TerrainObject ();
	RenderWorld::addObjectNotifications (*terrain);
	terrain->addToWorld ();

	yaw   = 0;
	pitch = 0;

	loadTerrain ();
}

//-------------------------------------------------------------------

void View3dView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	UNREF (pSender);
	UNREF (lHint);
	UNREF (pHint);

	terrainReloadPending = true;
	needsRedraw = true;
}

//-------------------------------------------------------------------

void View3dView::OnMouseMove(UINT nFlags, CPoint point) 
{
	if (!camera || !terrain || !terrain->getAppearance ())
	{
		CView::OnMouseMove (nFlags, point);
		return;
	}

	if (lastMouseValid)
	{
		const int dx = point.x - lastMousePoint.x;
		const int dy = point.y - lastMousePoint.y;

		if (nFlags & MK_RBUTTON)
		{
			CRect rect;
			GetClientRect (&rect);
			if (rect.Width () > 0 && rect.Height () > 0)
			{
				yaw   += PI_TIMES_2 * static_cast<real> (dx) / rect.Width ();
				pitch += PI_TIMES_2 * static_cast<real> (dy) / rect.Height ();
				pitch  = clamp (-PI_OVER_2 * 0.95f, pitch, PI_OVER_2 * 0.95f);

				camera->resetRotate_o2p ();
				camera->yaw_o (yaw);
				camera->pitch_o (pitch);

				Invalidate ();
			}
		}
		else if (nFlags & MK_MBUTTON)
		{
			const real panSpeed = moveSpeed * 0.5f;
			camera->move_o (Vector::negativeUnitX * static_cast<real> (dx) * panSpeed * 0.1f);
			camera->move_o (Vector::unitY * static_cast<real> (dy) * panSpeed * 0.1f);
			clampCameraToTerrainBounds (camera, terrain);
			Invalidate ();
		}
	}

	lastMousePoint = point;
	lastMouseValid = true;

	CView::OnMouseMove (nFlags, point);
}

//-------------------------------------------------------------------

void View3dView::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetCapture ();
	lastMousePoint = point;
	lastMouseValid = true;
	CView::OnLButtonDown (nFlags, point);
}

//-------------------------------------------------------------------

void View3dView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (!(nFlags & (MK_RBUTTON | MK_MBUTTON)))
		ReleaseCapture ();
	CView::OnLButtonUp (nFlags, point);
}

//-------------------------------------------------------------------

void View3dView::OnRButtonDown(UINT nFlags, CPoint point)
{
	SetCapture ();
	lastMousePoint = point;
	lastMouseValid = true;
	rightMouseDragging = true;
	CView::OnRButtonDown (nFlags, point);
}

//-------------------------------------------------------------------

void View3dView::OnRButtonUp(UINT nFlags, CPoint point)
{
	if (!(nFlags & (MK_LBUTTON | MK_MBUTTON)))
		ReleaseCapture ();
	rightMouseDragging = false;
	CView::OnRButtonUp (nFlags, point);
}

//-------------------------------------------------------------------

void View3dView::OnMButtonDown(UINT nFlags, CPoint point)
{
	SetCapture ();
	lastMousePoint = point;
	lastMouseValid = true;
	middleMouseDragging = true;
	CView::OnMButtonDown (nFlags, point);
}

//-------------------------------------------------------------------

void View3dView::OnMButtonUp(UINT nFlags, CPoint point)
{
	if (!(nFlags & (MK_LBUTTON | MK_RBUTTON)))
		ReleaseCapture ();
	middleMouseDragging = false;
	CView::OnMButtonUp (nFlags, point);
}

//-------------------------------------------------------------------

BOOL View3dView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	UNREF (nFlags);
	UNREF (pt);

	if (camera)
	{
		const real zoomAmount = static_cast<real> (zDelta) / 120.f * moveSpeed * 3.f;
		camera->move_o (Vector::unitZ * zoomAmount);
		clampCameraToTerrainBounds (camera, terrain);
		Invalidate ();
	}

	return TRUE;
}

//-------------------------------------------------------------------

void View3dView::OnSize(UINT nType, int cx, int cy) 
{
	if (camera && cx && cy)
		camera->setViewport(0, 0, cx, cy);

	CView::OnSize(nType, cx, cy);

	Invalidate ();
}

//-------------------------------------------------------------------

BOOL View3dView::OnEraseBkgnd(CDC* pDC) 
{
	NOT_NULL (terrain);

	if (terrain->getAppearance ())
		return TRUE; 

	return CView::OnEraseBkgnd (pDC);	
}

//-------------------------------------------------------------------

void View3dView::OnDestroy()
{
	clearTerrain ();

	CView::OnDestroy();
	IGNORE_RETURN(KillTimer(static_cast<int>(timer)));
}

//-------------------------------------------------------------------

void View3dView::OnTimer(UINT nIDEvent) 
{
	if (nIDEvent == timer)
	{
		if (terrainReloadPending)
		{
			terrainReloadPending = false;
			if (terrainFaulted)
			{
				TerrainEditorLog::write ("OnTimer: terrain reload suppressed after prior fault, use Refresh to retry");
			}
			else
			{
				loadTerrain ();
			}
		}

		elapsedTime += RECIP (static_cast<float> (milliseconds));

		bool moving = render && (
			keyDown ('W') || keyDown ('S') || keyDown ('A') || keyDown ('D') ||
			keyDown ('Q') || keyDown ('E') ||
			keyDown (VK_UP) || keyDown (VK_DOWN) || keyDown (VK_LEFT) || keyDown (VK_RIGHT) ||
			keyDown (VK_PRIOR) || keyDown (VK_NEXT));

		if (moving || needsRedraw)
			Invalidate ();
	}

	CView::OnTimer(nIDEvent);
}

//-------------------------------------------------------------------

void View3dView::OnRefresh() 
{
	terrainFaulted = false;
	loadTerrain ();
}

//-------------------------------------------------------------------

void View3dView::OnClearTerrain()
{
	terrainFaulted = false;
	terrainReloadPending = false;
	clearTerrain ();
	needsRedraw = true;
	Invalidate ();
	TerrainEditorLog::write ("clearTerrain: user requested viewport clear");
}

//-------------------------------------------------------------------

void View3dView::OnSetFocus(CWnd* pOldWnd) 
{
	CView::OnSetFocus(pOldWnd);
	render = true;	
}

//-------------------------------------------------------------------

void View3dView::OnKillFocus(CWnd* pNewWnd) 
{
	CView::OnKillFocus(pNewWnd);
	render = false;
	lastMouseValid = false;
	middleMouseDragging = false;
	rightMouseDragging = false;
}

//-------------------------------------------------------------------

BOOL View3dView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_KEYUP)
	{
		switch (pMsg->wParam)
		{
			case 'W': case 'A': case 'S': case 'D':
			case 'Q': case 'E':
			case VK_SHIFT:
				Invalidate ();
				return TRUE;
		}
	}

	return CView::PreTranslateMessage (pMsg);
}

//-------------------------------------------------------------------

void View3dView::setDrawDistance (real distance)
{
	drawDistance = clamp (500.f, distance, 64000.f);

	if (farPlaneDistance < drawDistance)
		farPlaneDistance = drawDistance;

	if (camera)
	{
		camera->setFarPlane (farPlaneDistance);
		applyEditorTerrainChunkQualityProfile (farPlaneDistance, false, true);
		needsRedraw = true;
		Invalidate ();
	}
}

//-------------------------------------------------------------------

void View3dView::setNearPlaneDistance (real distance)
{
	nearPlaneDistance = clamp (0.1f, distance, 200.f);

	if (nearPlaneDistance >= farPlaneDistance - 1.f)
		nearPlaneDistance = farPlaneDistance - 1.f;

	if (nearPlaneDistance < 0.1f)
		nearPlaneDistance = 0.1f;

	if (camera)
	{
		camera->setNearPlane (nearPlaneDistance);
		needsRedraw = true;
		Invalidate ();
	}
}

//-------------------------------------------------------------------

void View3dView::setFarPlaneDistance (real distance)
{
	farPlaneDistance = clamp (500.f, distance, 64000.f);

	if (farPlaneDistance <= nearPlaneDistance + 1.f)
		farPlaneDistance = nearPlaneDistance + 1.f;

	if (drawDistance > farPlaneDistance)
		drawDistance = farPlaneDistance;

	if (camera)
	{
		camera->setFarPlane (farPlaneDistance);
		applyEditorTerrainChunkQualityProfile (farPlaneDistance, false, true);
		needsRedraw = true;
		Invalidate ();
	}
}

//-------------------------------------------------------------------

void View3dView::setMoveSpeed (real speed)
{
	moveSpeed = clamp (0.1f, speed, 10.f);
	needsRedraw = true;
}

//-------------------------------------------------------------------

void View3dView::setTimeOfDay (real time)
{
	timeOfDay = clamp (0.f, time, 1.f);

	if (environmentInitialized)
		GroundEnvironment::getInstance ().setTime (timeOfDay * 86400.f, true);

	needsRedraw = true;
	Invalidate ();
}

//-------------------------------------------------------------------

void View3dView::playEnvironmentMusic ()
{
	if (!environmentInitialized)
		return;

	const EnvironmentBlock* const block = GroundEnvironment::getInstance ().getCurrentEnvironmentBlock ();
	if (!block)
		return;

	const CrcString* templateName = block->getFirstMusicSoundTemplateName ();
	if (templateName && templateName->getString () && *templateName->getString ())
	{
		Audio::playSound (templateName->getString ());
		return;
	}

	templateName = block->getSunriseMusicSoundTemplateName ();
	if (templateName && templateName->getString () && *templateName->getString ())
		Audio::playSound (templateName->getString ());
}

//-------------------------------------------------------------------

