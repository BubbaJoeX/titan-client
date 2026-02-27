//
// View3dFrame.cpp
// asommers 10-9-2000
//
// copyright 2000, verant interactive
//

//-------------------------------------------------------------------

#include "FirstTerrainEditor.h"
#include "View3dFrame.h"
#include "View3dView.h"
#include "Resource.h"
#include "TerrainEditor.h"
#include "TerrainEditorDoc.h"

//-------------------------------------------------------------------

LRESULT View3dControlBar::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_HSCROLL || message == WM_VSCROLL)
	{
		CWnd* parent = GetParent ();
		if (parent)
			return parent->SendMessage (message, wParam, lParam);
	}

	return CToolBar::WindowProc (message, wParam, lParam);
}

//-------------------------------------------------------------------

IMPLEMENT_DYNCREATE(View3dFrame, CMDIChildWnd)

//-------------------------------------------------------------------

View3dFrame::View3dFrame() :
	CMDIChildWnd (),
	m_windowName (),
	m_wndToolBar (),
	m_wndControlBar ()
{
	m_windowName = "3D View";
}

//-------------------------------------------------------------------
	
View3dFrame::~View3dFrame()
{
}

//-------------------------------------------------------------------

BEGIN_MESSAGE_MAP(View3dFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(View3dFrame)
	ON_WM_DESTROY()
	ON_WM_CREATE()
	ON_WM_HSCROLL()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-------------------------------------------------------------------

View3dView* View3dFrame::getView3dView ()
{
	CWnd* const pane = GetDescendantWindow (AFX_IDW_PANE_FIRST, TRUE);
	return dynamic_cast<View3dView*> (pane);
}

//-------------------------------------------------------------------

void View3dFrame::OnDestroy() 
{
	//-- tell document we're being destroyed
	static_cast<TerrainEditorDoc*> (GetActiveDocument ())->setView3dFrame (0);

	TerrainEditorApp* app = static_cast<TerrainEditorApp*>(AfxGetApp());
	app->SaveWindowPosition(this,"View3dFrame");
	CMDIChildWnd::OnDestroy();
}

//-------------------------------------------------------------------

BOOL View3dFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	cs.lpszName = m_windowName;
	cs.style &= ~FWS_ADDTOTITLE;
	cs.style &= ~FWS_PREFIXTITLE;

	return CMDIChildWnd::PreCreateWindow(cs);
}

//-------------------------------------------------------------------

int View3dFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	//-- create tool bar
	if (!m_wndToolBar.CreateEx(this) ||
		!m_wndToolBar.LoadToolBar(IDR_VIEW3D))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;
	}

	m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle() |
		CBRS_TOOLTIPS | CBRS_FLYBY);

	//-- create control bar (a second toolbar with just a wide separator)
	m_wndControlBar.CreateEx (this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS);
	m_wndControlBar.SetBarStyle (m_wndControlBar.GetBarStyle () | CBRS_TOOLTIPS | CBRS_FLYBY);
	m_wndControlBar.GetToolBarCtrl ().SetButtonSize (CSize (1, 26));

	TBBUTTON wideSep;
	memset (&wideSep, 0, sizeof (wideSep));
	wideSep.iBitmap = 800;
	wideSep.fsStyle = TBSTYLE_SEP;
	m_wndControlBar.GetToolBarCtrl ().InsertButton (0, &wideSep);
	m_wndControlBar.GetToolBarCtrl ().AutoSize ();
	m_wndControlBar.ShowWindow (SW_SHOW);

	const DWORD lblStyle = WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_RIGHT;
	const DWORD valStyle = WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_LEFT;
	const DWORD sldStyle = WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS;

	m_labelDrawDist.Create ("Draw Dist:", lblStyle, CRect (0, 0, 0, 0), &m_wndControlBar);
	m_sliderDrawDist.Create (sldStyle, CRect (0, 0, 0, 0), &m_wndControlBar, IDC_3DVIEW_SLIDER_DRAWDIST);
	m_sliderDrawDist.SetRange (5, 640);
	m_sliderDrawDist.SetPos (40);

	m_valueDrawDist.Create ("4000", valStyle, CRect (0, 0, 0, 0), &m_wndControlBar);

	m_labelCamSpeed.Create ("Cam Speed:", lblStyle, CRect (0, 0, 0, 0), &m_wndControlBar);
	m_sliderCamSpeed.Create (sldStyle, CRect (0, 0, 0, 0), &m_wndControlBar, IDC_3DVIEW_SLIDER_CAMSPEED);
	m_sliderCamSpeed.SetRange (1, 100);
	m_sliderCamSpeed.SetPos (20);

	m_valueCamSpeed.Create ("2.0", valStyle, CRect (0, 0, 0, 0), &m_wndControlBar);

	m_labelTimeOfDay.Create ("Time of Day:", lblStyle, CRect (0, 0, 0, 0), &m_wndControlBar);
	m_sliderTimeOfDay.Create (sldStyle, CRect (0, 0, 0, 0), &m_wndControlBar, IDC_3DVIEW_SLIDER_TIMEOFDAY);
	m_sliderTimeOfDay.SetRange (0, 240);
	m_sliderTimeOfDay.SetPos (120);

	m_valueTimeOfDay.Create ("12:00", valStyle, CRect (0, 0, 0, 0), &m_wndControlBar);

	m_labelNearPlane.Create ("Near Plane:", lblStyle, CRect (0, 0, 0, 0), &m_wndControlBar);
	m_sliderNearPlane.Create (sldStyle, CRect (0, 0, 0, 0), &m_wndControlBar, IDC_3DVIEW_SLIDER_NEARPLANE);
	m_sliderNearPlane.SetRange (1, 500);
	m_sliderNearPlane.SetPos (1);
	m_valueNearPlane.Create ("0.1", valStyle, CRect (0, 0, 0, 0), &m_wndControlBar);

	m_labelFarPlane.Create ("Far Plane:", lblStyle, CRect (0, 0, 0, 0), &m_wndControlBar);
	m_sliderFarPlane.Create (sldStyle, CRect (0, 0, 0, 0), &m_wndControlBar, IDC_3DVIEW_SLIDER_FARPLANE);
	m_sliderFarPlane.SetRange (5, 640);
	m_sliderFarPlane.SetPos (40);
	m_valueFarPlane.Create ("4000", valStyle, CRect (0, 0, 0, 0), &m_wndControlBar);

	RecalcLayout ();
	layoutControlBarWidgets ();
	applySliderValues ();

	TerrainEditorApp* app = static_cast<TerrainEditorApp*>(AfxGetApp());
	if(!app->RestoreWindowPosition(this,"View3dFrame"))
	{
		CRect mainRect;
		AfxGetApp()->GetMainWnd ()->GetClientRect (&mainRect);
		mainRect.right  -= 2;
		mainRect.bottom -= 64;
		IGNORE_RETURN (SetWindowPos (&wndTop, mainRect.left, mainRect.top, mainRect.right, mainRect.bottom, SWP_SHOWWINDOW));
	}

	return 0;
}

//-------------------------------------------------------------------

void View3dFrame::updateValueLabels ()
{
	CString text;

	const int drawDist = m_sliderDrawDist.GetPos () * 100;
	text.Format ("%d", drawDist);
	m_valueDrawDist.SetWindowText (text);

	const float camSpeed = static_cast<float> (m_sliderCamSpeed.GetPos ()) / 10.f;
	text.Format ("%.1f", camSpeed);
	m_valueCamSpeed.SetWindowText (text);

	const int todVal = m_sliderTimeOfDay.GetPos ();
	const int hours = todVal / 10;
	const int minutes = (todVal % 10) * 6;
	text.Format ("%02d:%02d", hours, minutes);
	m_valueTimeOfDay.SetWindowText (text);

	const float nearPlane = static_cast<float> (m_sliderNearPlane.GetPos ()) / 10.f;
	text.Format ("%.1f", nearPlane);
	m_valueNearPlane.SetWindowText (text);

	const int farPlane = m_sliderFarPlane.GetPos () * 100;
	text.Format ("%d", farPlane);
	m_valueFarPlane.SetWindowText (text);
}

//-------------------------------------------------------------------

void View3dFrame::applySliderValues ()
{
	View3dView* const view = getView3dView ();
	if (!view)
	{
		updateValueLabels ();
		return;
	}

	real nearPlane = static_cast<real> (m_sliderNearPlane.GetPos ()) / 10.f;
	real farPlane  = static_cast<real> (m_sliderFarPlane.GetPos () * 100);
	real drawDist  = static_cast<real> (m_sliderDrawDist.GetPos () * 100);

	if (farPlane <= nearPlane + 100.f)
	{
		farPlane = nearPlane + 100.f;
		const int sliderPos = static_cast<int> ((farPlane + 50.f) / 100.f);
		m_sliderFarPlane.SetPos (clamp (5, sliderPos, 640));
		farPlane = static_cast<real> (m_sliderFarPlane.GetPos () * 100);
	}

	if (drawDist > farPlane)
	{
		drawDist = farPlane;
		const int sliderPos = static_cast<int> ((drawDist + 50.f) / 100.f);
		m_sliderDrawDist.SetPos (clamp (5, sliderPos, 640));
		drawDist = static_cast<real> (m_sliderDrawDist.GetPos () * 100);
	}

	view->setDrawDistance (drawDist);
	view->setNearPlaneDistance (nearPlane);
	view->setFarPlaneDistance (farPlane);
	view->setMoveSpeed (static_cast<real> (m_sliderCamSpeed.GetPos ()) / 10.f);
	view->setTimeOfDay (static_cast<real> (m_sliderTimeOfDay.GetPos ()) / 240.f);
	updateValueLabels ();
}

//-------------------------------------------------------------------

void View3dFrame::layoutControlBarWidgets ()
{
	CRect barRect;
	m_wndControlBar.GetClientRect (&barRect);
	if (barRect.Width () <= 0 || barRect.Height () <= 0)
		return;

	const int top = 2;
	const int height = 21;
	const int sliderWidth = 132;
	const int spacing = 8;
	const int groupSpacing = 16;
	const int labelWidth = 78;
	const int valueWidth = 46;
	const int rowStep = 24;

	int x = 10;
	int y = top;
	const int groupWidth = labelWidth + spacing + sliderWidth + spacing + valueWidth + groupSpacing;
	const int rightLimit = barRect.right - 8;

	#define PLACE_GROUP(lbl, sld, val) \
		do { \
			if (x + groupWidth > rightLimit) { x = 10; y += rowStep; } \
			(lbl).MoveWindow (x, y, labelWidth, height); \
			x += labelWidth + spacing; \
			(sld).MoveWindow (x, y, sliderWidth, height); \
			x += sliderWidth + spacing; \
			(val).MoveWindow (x, y, valueWidth, height); \
			x += valueWidth + groupSpacing; \
		} while (0)

	PLACE_GROUP (m_labelDrawDist, m_sliderDrawDist, m_valueDrawDist);
	PLACE_GROUP (m_labelCamSpeed, m_sliderCamSpeed, m_valueCamSpeed);
	PLACE_GROUP (m_labelTimeOfDay, m_sliderTimeOfDay, m_valueTimeOfDay);
	PLACE_GROUP (m_labelNearPlane, m_sliderNearPlane, m_valueNearPlane);
	PLACE_GROUP (m_labelFarPlane, m_sliderFarPlane, m_valueFarPlane);

	#undef PLACE_GROUP
}

//-------------------------------------------------------------------

void View3dFrame::OnSize(UINT nType, int cx, int cy)
{
	CMDIChildWnd::OnSize (nType, cx, cy);

	if (cx <= 0 || cy <= 0)
		return;

	int toolbarHeight = 0;
	if (m_wndToolBar.GetSafeHwnd () && m_wndToolBar.IsWindowVisible ())
	{
		CRect toolRect;
		m_wndToolBar.GetWindowRect (&toolRect);
		toolbarHeight = toolRect.Height ();
	}

	const int controlBarHeight = 78;
	if (m_wndControlBar.GetSafeHwnd ())
	{
		m_wndControlBar.MoveWindow (0, toolbarHeight, cx, controlBarHeight, TRUE);
		layoutControlBarWidgets ();
	}

	CWnd* const pane = GetDescendantWindow (AFX_IDW_PANE_FIRST, TRUE);
	if (pane && pane->GetSafeHwnd ())
	{
		const int paneTop = toolbarHeight + controlBarHeight;
		const int paneHeight = cy > paneTop ? (cy - paneTop) : 0;
		pane->MoveWindow (0, paneTop, cx, paneHeight, TRUE);
	}
}

//-------------------------------------------------------------------

void View3dFrame::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (pScrollBar)
	{
		const int id = pScrollBar->GetDlgCtrlID ();
		if (id == IDC_3DVIEW_SLIDER_DRAWDIST || id == IDC_3DVIEW_SLIDER_CAMSPEED || id == IDC_3DVIEW_SLIDER_TIMEOFDAY ||
			id == IDC_3DVIEW_SLIDER_NEARPLANE || id == IDC_3DVIEW_SLIDER_FARPLANE)
			applySliderValues ();
	}

	CMDIChildWnd::OnHScroll (nSBCode, nPos, pScrollBar);
}

//-------------------------------------------------------------------
