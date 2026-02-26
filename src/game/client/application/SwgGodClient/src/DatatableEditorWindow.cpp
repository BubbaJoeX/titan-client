// ======================================================================
//
// DatatableEditorWindow.cpp
// copyright 2024 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "DatatableEditorWindow.h"
#include "DatatableEditorWindow.moc"

#include "MainFrame.h"

#include <qfiledialog.h>
#include <qheader.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qtable.h>

#include <cstdio>
#include <fstream>
#include <sstream>

// ======================================================================

DatatableEditorWindow::DatatableEditorWindow(QWidget * parent, const char * name)
: QDialog(parent, name, false),
  m_headerTable(0),
  m_table(0),
  m_saveButton(0),
  m_compileButton(0),
  m_browseButton(0),
  m_pageFirstButton(0),
  m_pagePrevButton(0),
  m_pageNextButton(0),
  m_pageLastButton(0),
  m_addRowButton(0),
  m_deleteRowButton(0),
  m_addColumnButton(0),
  m_deleteColumnButton(0),
  m_filePathEdit(0),
  m_pageLabel(0),
  m_statusLabel(0),
  m_currentPath(),
  m_currentPage(0),
  m_totalPages(1),
  m_columnNames(),
  m_columnTypes(),
  m_rowData()
{
	setCaption("Datatable Editor");
	resize(1000, 700);

	QVBoxLayout * mainLayout = new QVBoxLayout(this, 6, 6);

	// File path bar
	QHBoxLayout * fileLayout = new QHBoxLayout(0, 0, 4);
	QLabel * fileLabel = new QLabel("File:", this);
	m_filePathEdit = new QLineEdit(this);
	m_filePathEdit->setReadOnly(true);
	m_browseButton = new QPushButton("Open...", this);
	fileLayout->addWidget(fileLabel);
	fileLayout->addWidget(m_filePathEdit, 1);
	fileLayout->addWidget(m_browseButton);
	mainLayout->addLayout(fileLayout);

	// Frozen header table (column names + types, 2 rows)
	m_headerTable = new QTable(this);
	m_headerTable->setSelectionMode(QTable::NoSelection);
	m_headerTable->setFocusStyle(QTable::FollowStyle);
	m_headerTable->setNumRows(2);
	m_headerTable->setNumCols(1);
	m_headerTable->verticalHeader()->hide();
	m_headerTable->setLeftMargin(0);
	m_headerTable->setTopMargin(0);
	m_headerTable->horizontalHeader()->hide();
	m_headerTable->setVScrollBarMode(QScrollView::AlwaysOff);
	m_headerTable->setHScrollBarMode(QScrollView::AlwaysOff);

	int headerRowHeight = m_headerTable->rowHeight(0);
	m_headerTable->setFixedHeight(headerRowHeight * 2 + 4);
	mainLayout->addWidget(m_headerTable);

	// Data table widget
	m_table = new QTable(this);
	m_table->setSelectionMode(QTable::SingleRow);
	m_table->setFocusStyle(QTable::FollowStyle);
	mainLayout->addWidget(m_table, 1);

	// Pagination controls
	QHBoxLayout * pageLayout = new QHBoxLayout(0, 0, 4);
	m_pageFirstButton = new QPushButton("|<", this);
	m_pagePrevButton  = new QPushButton("<", this);
	m_pageLabel       = new QLabel("Page 1 / 1", this);
	m_pageNextButton  = new QPushButton(">", this);
	m_pageLastButton  = new QPushButton(">|", this);

	m_pageFirstButton->setMaximumWidth(40);
	m_pagePrevButton->setMaximumWidth(40);
	m_pageNextButton->setMaximumWidth(40);
	m_pageLastButton->setMaximumWidth(40);

	pageLayout->addStretch(1);
	pageLayout->addWidget(m_pageFirstButton);
	pageLayout->addWidget(m_pagePrevButton);
	pageLayout->addWidget(m_pageLabel);
	pageLayout->addWidget(m_pageNextButton);
	pageLayout->addWidget(m_pageLastButton);
	pageLayout->addStretch(1);
	mainLayout->addLayout(pageLayout);

	// Row/Column editing
	QHBoxLayout * editLayout = new QHBoxLayout(0, 0, 4);
	m_addRowButton      = new QPushButton("Add Row", this);
	m_deleteRowButton   = new QPushButton("Delete Row", this);
	m_addColumnButton   = new QPushButton("Add Column", this);
	m_deleteColumnButton = new QPushButton("Delete Column", this);
	editLayout->addWidget(m_addRowButton);
	editLayout->addWidget(m_deleteRowButton);
	editLayout->addWidget(m_addColumnButton);
	editLayout->addWidget(m_deleteColumnButton);
	editLayout->addStretch(1);
	mainLayout->addLayout(editLayout);

	// Status and action buttons
	m_statusLabel = new QLabel("Open a .tab file to begin editing.", this);
	mainLayout->addWidget(m_statusLabel);

	QHBoxLayout * buttonLayout = new QHBoxLayout(0, 0, 6);
	buttonLayout->addStretch(1);
	m_saveButton    = new QPushButton("Save", this);
	m_compileButton = new QPushButton("Compile", this);
	m_compileButton->setEnabled(false);
	buttonLayout->addWidget(m_saveButton);
	buttonLayout->addWidget(m_compileButton);
	mainLayout->addLayout(buttonLayout);

	IGNORE_RETURN(connect(m_browseButton,       SIGNAL(clicked()), this, SLOT(onBrowse())));
	IGNORE_RETURN(connect(m_saveButton,         SIGNAL(clicked()), this, SLOT(onSave())));
	IGNORE_RETURN(connect(m_compileButton,      SIGNAL(clicked()), this, SLOT(onCompile())));
	IGNORE_RETURN(connect(m_pageFirstButton,    SIGNAL(clicked()), this, SLOT(onPageFirst())));
	IGNORE_RETURN(connect(m_pagePrevButton,     SIGNAL(clicked()), this, SLOT(onPagePrev())));
	IGNORE_RETURN(connect(m_pageNextButton,     SIGNAL(clicked()), this, SLOT(onPageNext())));
	IGNORE_RETURN(connect(m_pageLastButton,     SIGNAL(clicked()), this, SLOT(onPageLast())));
	IGNORE_RETURN(connect(m_addRowButton,       SIGNAL(clicked()), this, SLOT(onAddRow())));
	IGNORE_RETURN(connect(m_deleteRowButton,    SIGNAL(clicked()), this, SLOT(onDeleteRow())));
	IGNORE_RETURN(connect(m_addColumnButton,    SIGNAL(clicked()), this, SLOT(onAddColumn())));
	IGNORE_RETURN(connect(m_deleteColumnButton, SIGNAL(clicked()), this, SLOT(onDeleteColumn())));
}

// ----------------------------------------------------------------------

DatatableEditorWindow::~DatatableEditorWindow()
{
}

// ----------------------------------------------------------------------

void DatatableEditorWindow::loadDatatable(const std::string & tabPath)
{
	if (!parseTabFile(tabPath))
	{
		QMessageBox::warning(this, "Datatable Editor", QString("Could not parse: %1").arg(tabPath.c_str()));
		return;
	}

	m_currentPath = tabPath;
	m_filePathEdit->setText(tabPath.c_str());
	m_currentPage = 0;
	m_compileButton->setEnabled(true);

	refreshTableView();
	m_statusLabel->setText(QString("Loaded: %1 (%2 columns, %3 rows)")
		.arg(tabPath.c_str())
		.arg(static_cast<int>(m_columnNames.size()))
		.arg(static_cast<int>(m_rowData.size())));
}

// ----------------------------------------------------------------------

void DatatableEditorWindow::newDatatable()
{
	m_columnNames.clear();
	m_columnTypes.clear();
	m_rowData.clear();
	m_currentPath.clear();
	m_filePathEdit->clear();
	m_currentPage = 0;
	m_totalPages = 1;
	m_compileButton->setEnabled(false);

	m_columnNames.push_back("column1");
	m_columnTypes.push_back("s");

	std::vector<std::string> emptyRow;
	emptyRow.push_back("");
	m_rowData.push_back(emptyRow);

	refreshTableView();
	m_statusLabel->setText("New datatable created.");
}

// ----------------------------------------------------------------------

bool DatatableEditorWindow::parseTabFile(const std::string & path)
{
	std::ifstream file(path.c_str());
	if (!file.is_open())
		return false;

	m_columnNames.clear();
	m_columnTypes.clear();
	m_rowData.clear();

	std::string line;
	int lineNum = 0;

	while (std::getline(file, line))
	{
		std::vector<std::string> cells;
		std::istringstream iss(line);
		std::string cell;

		while (std::getline(iss, cell, '\t'))
			cells.push_back(cell);

		if (lineNum == 0)
			m_columnNames = cells;
		else if (lineNum == 1)
			m_columnTypes = cells;
		else
			m_rowData.push_back(cells);

		++lineNum;
	}

	file.close();
	return !m_columnNames.empty();
}

// ----------------------------------------------------------------------

void DatatableEditorWindow::refreshTableView()
{
	int numCols = static_cast<int>(m_columnNames.size());
	int totalRows = static_cast<int>(m_rowData.size());
	m_totalPages = (totalRows > 0) ? ((totalRows - 1) / ROWS_PER_PAGE + 1) : 1;

	if (m_currentPage >= m_totalPages)
		m_currentPage = m_totalPages - 1;
	if (m_currentPage < 0)
		m_currentPage = 0;

	int startRow = m_currentPage * ROWS_PER_PAGE;
	int endRow = startRow + ROWS_PER_PAGE;
	if (endRow > totalRows)
		endRow = totalRows;
	int displayRows = endRow - startRow;

	// Frozen header table: row 0 = column names, row 1 = column types
	if (m_headerTable)
	{
		m_headerTable->setNumCols(numCols);
		m_headerTable->setNumRows(2);

		for (int c = 0; c < numCols; ++c)
		{
			m_headerTable->setText(0, c, m_columnNames[static_cast<size_t>(c)].c_str());

			std::string typeStr;
			if (static_cast<size_t>(c) < m_columnTypes.size())
				typeStr = m_columnTypes[static_cast<size_t>(c)];
			m_headerTable->setText(1, c, typeStr.c_str());
		}

		int headerRowHeight = m_headerTable->rowHeight(0);
		m_headerTable->setFixedHeight(headerRowHeight * 2 + 4);
	}

	// Data table: only data rows, paginated
	if (m_table)
	{
		m_table->setNumCols(numCols);
		m_table->setNumRows(displayRows);

		QHeader * hdr = m_table->horizontalHeader();
		for (int c = 0; c < numCols; ++c)
		{
			if (hdr)
				hdr->setLabel(c, m_columnNames[static_cast<size_t>(c)].c_str());
		}

		for (int r = 0; r < displayRows; ++r)
		{
			int dataIdx = startRow + r;
			const std::vector<std::string> & row = m_rowData[static_cast<size_t>(dataIdx)];
			for (int c = 0; c < numCols; ++c)
			{
				std::string val;
				if (static_cast<size_t>(c) < row.size())
					val = row[static_cast<size_t>(c)];
				m_table->setText(r, c, val.c_str());
			}
		}
	}

	m_pageLabel->setText(QString("Page %1 / %2").arg(m_currentPage + 1).arg(m_totalPages));
}

// ----------------------------------------------------------------------

void DatatableEditorWindow::syncHeaderFromTable()
{
	if (!m_headerTable)
		return;

	int numCols = static_cast<int>(m_columnNames.size());

	for (int c = 0; c < numCols; ++c)
	{
		m_columnNames[static_cast<size_t>(c)] = m_headerTable->text(0, c).latin1();
		if (static_cast<size_t>(c) < m_columnTypes.size())
			m_columnTypes[static_cast<size_t>(c)] = m_headerTable->text(1, c).latin1();
	}
}

// ----------------------------------------------------------------------

void DatatableEditorWindow::syncDataFromTable()
{
	if (!m_table)
		return;

	int numCols = static_cast<int>(m_columnNames.size());
	int displayRows = m_table->numRows();
	int startRow = m_currentPage * ROWS_PER_PAGE;

	for (int r = 0; r < displayRows; ++r)
	{
		int dataIdx = startRow + r;
		if (static_cast<size_t>(dataIdx) < m_rowData.size())
		{
			std::vector<std::string> & row = m_rowData[static_cast<size_t>(dataIdx)];
			row.resize(static_cast<size_t>(numCols));
			for (int c = 0; c < numCols; ++c)
				row[static_cast<size_t>(c)] = m_table->text(r, c).latin1();
		}
	}
}

// ----------------------------------------------------------------------

bool DatatableEditorWindow::writeTabFile(const std::string & path)
{
	FILE * fp = fopen(path.c_str(), "w");
	if (!fp)
		return false;

	for (size_t c = 0; c < m_columnNames.size(); ++c)
	{
		if (c > 0) fputc('\t', fp);
		fputs(m_columnNames[c].c_str(), fp);
	}
	fputc('\n', fp);

	for (size_t c = 0; c < m_columnTypes.size(); ++c)
	{
		if (c > 0) fputc('\t', fp);
		fputs(m_columnTypes[c].c_str(), fp);
	}
	fputc('\n', fp);

	for (size_t r = 0; r < m_rowData.size(); ++r)
	{
		const std::vector<std::string> & row = m_rowData[r];
		for (size_t c = 0; c < m_columnNames.size(); ++c)
		{
			if (c > 0) fputc('\t', fp);
			if (c < row.size())
				fputs(row[c].c_str(), fp);
		}
		fputc('\n', fp);
	}
	fclose(fp);
	return true;
}

// ----------------------------------------------------------------------

bool DatatableEditorWindow::runDataTableTool(const std::string & tabPath)
{
	char command[2048];
	snprintf(command, sizeof(command), "DataTableTool -i \"%s\"", tabPath.c_str());

	int result = system(command);
	return (result == 0);
}

// ----------------------------------------------------------------------

void DatatableEditorWindow::onBrowse()
{
	QString path = QFileDialog::getOpenFileName(QString::null, "Tab Files (*.tab);;All Files (*)", this, "browseDatatable", "Open Datatable");
	if (!path.isEmpty())
		loadDatatable(path.latin1());
}

// ----------------------------------------------------------------------

void DatatableEditorWindow::onSave()
{
	syncHeaderFromTable();
	syncDataFromTable();

	std::string savePath = m_currentPath;
	if (savePath.empty())
	{
		QString path = QFileDialog::getSaveFileName(QString::null, "Tab Files (*.tab);;All Files (*)", this, "saveDatatable", "Save Datatable");
		if (path.isEmpty())
			return;
		savePath = path.latin1();
		m_currentPath = savePath;
		m_filePathEdit->setText(savePath.c_str());
	}

	if (writeTabFile(savePath))
	{
		m_compileButton->setEnabled(true);
		m_statusLabel->setText("Datatable saved.");
		emit statusMessage("Datatable saved.");
	}
	else
	{
		QMessageBox::warning(this, "Datatable Editor", "Failed to save file.");
	}
}

// ----------------------------------------------------------------------

void DatatableEditorWindow::onCompile()
{
	if (m_currentPath.empty())
	{
		QMessageBox::warning(this, "Datatable Editor", "Save the datatable first.");
		return;
	}

	m_statusLabel->setText(QString("Compiling: %1").arg(m_currentPath.c_str()));

	if (runDataTableTool(m_currentPath))
	{
		m_statusLabel->setText("Compilation successful.");
		emit statusMessage("Datatable compiled successfully.");
		MainFrame::getInstance().textToConsole(("DatatableEditor: Compiled " + m_currentPath).c_str());
	}
	else
	{
		m_statusLabel->setText("Compilation failed.");
		QMessageBox::warning(this, "Datatable Editor", "DataTableTool failed. Check console.");
	}
}

// ----------------------------------------------------------------------

void DatatableEditorWindow::onPageFirst()
{
	syncDataFromTable();
	m_currentPage = 0;
	refreshTableView();
}

void DatatableEditorWindow::onPagePrev()
{
	syncDataFromTable();
	if (m_currentPage > 0)
		--m_currentPage;
	refreshTableView();
}

void DatatableEditorWindow::onPageNext()
{
	syncDataFromTable();
	if (m_currentPage < m_totalPages - 1)
		++m_currentPage;
	refreshTableView();
}

void DatatableEditorWindow::onPageLast()
{
	syncDataFromTable();
	m_currentPage = m_totalPages - 1;
	refreshTableView();
}

// ----------------------------------------------------------------------

void DatatableEditorWindow::onAddRow()
{
	syncDataFromTable();
	std::vector<std::string> newRow(m_columnNames.size());
	m_rowData.push_back(newRow);
	refreshTableView();
	m_statusLabel->setText(QString("Added row. Total: %1").arg(static_cast<int>(m_rowData.size())));
}

void DatatableEditorWindow::onDeleteRow()
{
	if (m_rowData.empty() || !m_table)
		return;

	int currentRow = m_table->currentRow();
	if (currentRow < 0)
	{
		QMessageBox::warning(this, "Datatable Editor", "Select a data row to delete.");
		return;
	}

	int dataIdx = m_currentPage * ROWS_PER_PAGE + currentRow;
	if (dataIdx >= 0 && static_cast<size_t>(dataIdx) < m_rowData.size())
	{
		m_rowData.erase(m_rowData.begin() + dataIdx);
		refreshTableView();
		m_statusLabel->setText(QString("Deleted row. Total: %1").arg(static_cast<int>(m_rowData.size())));
	}
}

void DatatableEditorWindow::onAddColumn()
{
	syncHeaderFromTable();
	syncDataFromTable();

	m_columnNames.push_back("new_column");
	m_columnTypes.push_back("s");

	for (size_t r = 0; r < m_rowData.size(); ++r)
		m_rowData[r].push_back("");

	refreshTableView();
	m_statusLabel->setText(QString("Added column. Total: %1").arg(static_cast<int>(m_columnNames.size())));
}

void DatatableEditorWindow::onDeleteColumn()
{
	if (m_columnNames.empty() || !m_table)
		return;

	int currentCol = m_table->currentColumn();
	if (currentCol < 0 || static_cast<size_t>(currentCol) >= m_columnNames.size())
	{
		QMessageBox::warning(this, "Datatable Editor", "Select a column to delete.");
		return;
	}

	syncHeaderFromTable();
	syncDataFromTable();

	m_columnNames.erase(m_columnNames.begin() + currentCol);
	if (static_cast<size_t>(currentCol) < m_columnTypes.size())
		m_columnTypes.erase(m_columnTypes.begin() + currentCol);

	for (size_t r = 0; r < m_rowData.size(); ++r)
	{
		if (static_cast<size_t>(currentCol) < m_rowData[r].size())
			m_rowData[r].erase(m_rowData[r].begin() + currentCol);
	}

	refreshTableView();
	m_statusLabel->setText(QString("Deleted column. Total: %1").arg(static_cast<int>(m_columnNames.size())));
}

// ======================================================================
