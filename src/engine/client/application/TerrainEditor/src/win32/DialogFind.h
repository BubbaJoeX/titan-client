//
// DialogFind.h
// asommers
//
// copyright 2001, sony online entertainment
//

//-------------------------------------------------------------------

#ifndef INCLUDED_DialogFind_H
#define INCLUDED_DialogFind_H

//-------------------------------------------------------------------

#include "Resource.h"
#include "TerrainGeneratorHelper.h"

//-------------------------------------------------------------------

class DialogFind : public CDialog
{
private:

	ArrayList<TerrainGeneratorHelper::LayerItemQueryType> & query;

public:

	explicit DialogFind(ArrayList<TerrainGeneratorHelper::LayerItemQueryType> & newQuery, CWnd * pParent = NULL);
	bool getClear() const;
	CString const & getName() const;

protected:

	//{{AFX_DATA(DialogFind)
	enum { IDD = IDD_DIALOG_FIND };
	CListBox m_source;
	CListBox m_destination;
	BOOL     m_clear;
	CString  m_name;
	//}}AFX_DATA

	//{{AFX_VIRTUAL(DialogFind)
	protected:
	virtual void DoDataExchange(CDataExchange * pDX);
	virtual void OnOK();
	//}}AFX_VIRTUAL

protected:

	//{{AFX_MSG(DialogFind)
	virtual BOOL OnInitDialog();
	afx_msg void OnDblclkListDestination();
	afx_msg void OnDblclkListSource();
	afx_msg void OnTodestination();
	afx_msg void OnTosource();
	afx_msg void OnCheckClear();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

inline bool DialogFind::getClear() const
{
	return m_clear == TRUE;
}

inline CString const & DialogFind::getName() const
{
	return m_name;
}

//-------------------------------------------------------------------

//{{AFX_INSERT_LOCATION}}

//-------------------------------------------------------------------

#endif
