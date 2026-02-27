//
// View3dFrame.h
// asommers
//
// copyright 2001, sony online entertainment
//

//-------------------------------------------------------------------

#ifndef INCLUDED_View3dFrame_H
#define INCLUDED_View3dFrame_H

//-------------------------------------------------------------------

class View3dView;

class View3dControlBar : public CToolBar
{
protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
};

class View3dFrame : public CMDIChildWnd
{
private:

	CString            m_windowName;
	CToolBar           m_wndToolBar;
	View3dControlBar   m_wndControlBar;

	CSliderCtrl   m_sliderDrawDist;
	CSliderCtrl   m_sliderCamSpeed;
	CSliderCtrl   m_sliderTimeOfDay;
	CSliderCtrl   m_sliderNearPlane;
	CSliderCtrl   m_sliderFarPlane;
	CStatic       m_labelDrawDist;
	CStatic       m_labelCamSpeed;
	CStatic       m_labelTimeOfDay;
	CStatic       m_labelNearPlane;
	CStatic       m_labelFarPlane;
	CStatic       m_valueDrawDist;
	CStatic       m_valueCamSpeed;
	CStatic       m_valueTimeOfDay;
	CStatic       m_valueNearPlane;
	CStatic       m_valueFarPlane;

	void          applySliderValues ();
	void          updateValueLabels ();
	void          layoutControlBarWidgets ();
	View3dView*   getView3dView ();

private:

	DECLARE_DYNCREATE(View3dFrame)

protected:

	View3dFrame();           

public:

	//{{AFX_VIRTUAL(View3dFrame)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

protected:

	virtual ~View3dFrame();

	//{{AFX_MSG(View3dFrame)
	afx_msg void OnDestroy();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//-------------------------------------------------------------------

//{{AFX_INSERT_LOCATION}}

//-------------------------------------------------------------------

#endif 
