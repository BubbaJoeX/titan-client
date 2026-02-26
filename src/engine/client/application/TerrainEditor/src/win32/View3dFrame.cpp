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
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-------------------------------------------------------------------

View3dView* View3dFrame::getView3dView ()
{
	return dynamic_cast<View3dView*> (GetActiveView ());
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

	CRect barRect;
	m_wndControlBar.GetItemRect (0, &barRect);

	const DWORD lblStyle = WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_RIGHT;
	const DWORD valStyle = WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_LEFT;
	const DWORD sldStyle = WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS;

	const int y = barRect.top + 2;
	const int h = 20;
	int x = barRect.left + 8;

	//-- draw distance: label  |  slider  |  value
	m_labelDrawDist.Create ("Draw Dist:", lblStyle, CRect (x, y, x + 62, y + h), &m_wndControlBar);
	x += 68;

	m_sliderDrawDist.Create (sldStyle, CRect (x, y, x + 120, y + h), &m_wndControlBar, IDC_3DVIEW_SLIDER_DRAWDIST);
	m_sliderDrawDist.SetRange (5, 160);
	m_sliderDrawDist.SetPos (40);
	x += 126;

	m_valueDrawDist.Create ("4000", valStyle, CRect (x, y, x + 42, y + h), &m_wndControlBar);
	x += 56;

	//-- camera speed: label  |  slider  |  value
	m_labelCamSpeed.Create ("Cam Speed:", lblStyle, CRect (x, y, x + 66, y + h), &m_wndControlBar);
	x += 72;

	m_sliderCamSpeed.Create (sldStyle, CRect (x, y, x + 120, y + h), &m_wndControlBar, IDC_3DVIEW_SLIDER_CAMSPEED);
	m_sliderCamSpeed.SetRange (1, 100);
	m_sliderCamSpeed.SetPos (20);
	x += 126;

	m_valueCamSpeed.Create ("2.0", valStyle, CRect (x, y, x + 32, y + h), &m_wndControlBar);
	x += 46;

	//-- time of day: label  |  slider  |  value
	m_labelTimeOfDay.Create ("Time of Day:", lblStyle, CRect (x, y, x + 74, y + h), &m_wndControlBar);
	x += 80;

	m_sliderTimeOfDay.Create (sldStyle, CRect (x, y, x + 120, y + h), &m_wndControlBar, IDC_3DVIEW_SLIDER_TIMEOFDAY);
	m_sliderTimeOfDay.SetRange (0, 240);
	m_sliderTimeOfDay.SetPos (120);
	x += 126;

	m_valueTimeOfDay.Create ("12:00", valStyle, CRect (x, y, x + 38, y + h), &m_wndControlBar);

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
}

//-------------------------------------------------------------------

void View3dFrame::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	View3dView* view = getView3dView ();

	if (view && pScrollBar)
	{
		const int id = pScrollBar->GetDlgCtrlID ();

		if (id == IDC_3DVIEW_SLIDER_DRAWDIST)
			view->setDrawDistance (static_cast<real> (m_sliderDrawDist.GetPos () * 100));
		else if (id == IDC_3DVIEW_SLIDER_CAMSPEED)
			view->setMoveSpeed (static_cast<real> (m_sliderCamSpeed.GetPos ()) / 10.f);
		else if (id == IDC_3DVIEW_SLIDER_TIMEOFDAY)
			view->setTimeOfDay (static_cast<real> (m_sliderTimeOfDay.GetPos ()) / 240.f);
	}

	updateValueLabels ();

	CMDIChildWnd::OnHScroll (nSBCode, nPos, pScrollBar);
}

//-------------------------------------------------------------------
