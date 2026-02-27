//
// View3dView.h
// aommers
//
// copyright 2001, sony online entertainment
//

//-------------------------------------------------------------------

#ifndef INCLUDED_View3dView_H
#define INCLUDED_View3dView_H

//-------------------------------------------------------------------

class GameCamera;
class ObjectList;
class TerrainObject;

//-------------------------------------------------------------------

class View3dView : public CView
{
private:

	GameCamera*    camera;
	TerrainObject* terrain;
	
	real           yaw;
	real           pitch;
	real           moveSpeed;
	real           drawDistance;
	real           nearPlaneDistance;
	real           farPlaneDistance;
	real           timeOfDay;

	uint           timer;
	const uint     milliseconds;
	real           elapsedTime;
	bool           render;
	bool           hooksInstalled;
	bool           needsRedraw;
	bool           environmentInitialized;
	bool           terrainReloadPending;
	bool           terrainFaulted;
	bool           middleMouseDragging;
	bool           rightMouseDragging;
	int            movingUpdateCounter;

	CPoint         lastMousePoint;
	bool           lastMouseValid;

	void           clearTerrain ();
	void           loadTerrain ();
	void           playEnvironmentMusic ();

public:

	void           setDrawDistance (real distance);
	void           setNearPlaneDistance (real distance);
	void           setFarPlaneDistance (real distance);
	void           setMoveSpeed (real speed);
	void           setTimeOfDay (real time);
	real           getDrawDistance () const { return drawDistance; }
	real           getNearPlaneDistance () const { return nearPlaneDistance; }
	real           getFarPlaneDistance () const { return farPlaneDistance; }
	real           getMoveSpeed () const { return moveSpeed; }
	real           getTimeOfDay () const { return timeOfDay; }

protected:

	View3dView();           
	DECLARE_DYNCREATE(View3dView)

	//{{AFX_VIRTUAL(View3dView)
	public:
	virtual void OnInitialUpdate();
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void OnDraw(CDC* pDC);
	//}}AFX_VIRTUAL

protected:

	virtual ~View3dView();

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

	//{{AFX_MSG(View3dView)
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnRefresh();
	afx_msg void OnClearTerrain();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//-------------------------------------------------------------------

//{{AFX_INSERT_LOCATION}}

//-------------------------------------------------------------------

#endif 
