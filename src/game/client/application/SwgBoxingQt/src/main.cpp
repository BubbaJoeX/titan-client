#include <windows.h>

#include <qapplication.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qfile.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmainwindow.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qsplitter.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qtextstream.h>
#include <qtimer.h>
#include <qtoolbar.h>
#include <qtoolbutton.h>
#include <qpopupmenu.h>
#include <qstatusbar.h>
#include <qwidget.h>
#include <qevent.h>
#include <qdatetime.h>
#include <qsizepolicy.h>
#include <qcursor.h>
#include <qtooltip.h>
#include <qinputdialog.h>
#include <qmap.h>
#include <qobjectlist.h>
#include <qaccel.h>

namespace
{
	struct ProfileRecord
	{
		QString name;
		QString exe;
		QString args;
		QString loginUser;
		QString loginPassword;
		bool    autoLogin;
		bool    startMinimized;
		bool    forceWindowed;
	};

	QString iniEscape(QString const &v)
	{
		QString out = v;
		out.replace("\\", "\\\\");
		out.replace("\n", "\\n");
		out.replace("\r", "\\r");
		out.replace("\t", "\\t");
		return out;
	}

	QString iniUnescape(QString const &v)
	{
		QString out = v;
		out.replace("\\n", "\n");
		out.replace("\\r", "\r");
		out.replace("\\t", "\t");
		out.replace("\\\\", "\\");
		return out;
	}

	typedef QMap<QString, QMap<QString, QString> > IniSectionMap;

	IniSectionMap parseIni(QString const &content)
	{
		IniSectionMap data;
		QString currentSection = "default";
		QStringList lines = QStringList::split('\n', content, true);
		for (QStringList::ConstIterator it = lines.begin(); it != lines.end(); ++it)
		{
			QString line = (*it).stripWhiteSpace();
			if (line.isEmpty() || line.startsWith("#") || line.startsWith(";"))
				continue;
			if (line.startsWith("[") && line.endsWith("]") && line.length() > 2)
			{
				currentSection = line.mid(1, line.length() - 2);
				continue;
			}
			int const eq = line.find('=');
			if (eq <= 0)
				continue;
			QString const key = line.left(eq).stripWhiteSpace();
			QString const value = iniUnescape(line.mid(eq + 1));
			data[currentSection][key] = value;
		}
		return data;
	}

	QString writeIni(IniSectionMap const &data)
	{
		QString out;
		for (IniSectionMap::ConstIterator sec = data.begin(); sec != data.end(); ++sec)
		{
			out += "[";
			out += sec.key();
			out += "]\n";
			QMap<QString, QString> const &keys = sec.data();
			for (QMap<QString, QString>::ConstIterator kv = keys.begin(); kv != keys.end(); ++kv)
			{
				out += kv.key();
				out += "=";
				out += iniEscape(kv.data());
				out += "\n";
			}
			out += "\n";
		}
		return out;
	}

	struct FindWindowContext
	{
		DWORD processId;
		HWND  hwnd;
	};

	BOOL CALLBACK enumWindowsForProcess(HWND hwnd, LPARAM lParam)
	{
		FindWindowContext *ctx = reinterpret_cast<FindWindowContext *>(lParam);
		if (!ctx)
			return FALSE;

		DWORD windowPid = 0;
		::GetWindowThreadProcessId(hwnd, &windowPid);
		if (windowPid != ctx->processId)
			return TRUE;

		if (!::IsWindowVisible(hwnd))
			return TRUE;

		LONG style = ::GetWindowLong(hwnd, GWL_STYLE);
		if ((style & WS_CHILD) != 0)
			return TRUE;

		if (::GetWindow(hwnd, GW_OWNER) != 0)
			return TRUE;

		ctx->hwnd = hwnd;
		return FALSE;
	}

	HWND findMainWindowForProcess(DWORD processId)
	{
		FindWindowContext ctx;
		ctx.processId = processId;
		ctx.hwnd = 0;
		::EnumWindows(enumWindowsForProcess, reinterpret_cast<LPARAM>(&ctx));
		return ctx.hwnd;
	}

	QString buildCommandLine(QString const &exePath, QString const &args)
	{
		QString cmd;
		cmd = "\"";
		cmd += exePath;
		cmd += "\"";
		if (!args.isEmpty())
		{
			cmd += " ";
			cmd += args;
		}
		return cmd;
	}

	QString profileArgsForName(QString const &profileName, bool isLeftPane)
	{
		if (profileName == "Player A")
			return isLeftPane ? "-s Station players -sso" : "-s Station players -sso -localPort 45000";
		if (profileName == "Player B")
			return isLeftPane ? "-s Station players -sso -localPort 45000" : "-s Station players -sso";
		if (profileName == "Local")
			return "-s Local -sso";
		if (profileName == "TC")
			return "-s TC -sso";
		return "-s Station players -sso";
	}

	QString quoteIfNeeded(QString const &v)
	{
		if (v.find(' ') != -1 || v.find('\t') != -1)
		{
			QString out = "\"";
			out += v;
			out += "\"";
			return out;
		}
		return v;
	}

	void appendConfigOverride(QString &args, QString const &section, QString const &key, QString const &value)
	{
		if (args.find("--") == -1)
			args += " --";

		args += " -s ";
		args += section;
		args += " ";
		args += key;
		args += "=";
		args += quoteIfNeeded(value);
	}
}

class ClientPane : public QGroupBox
{
	Q_OBJECT

public:
	ClientPane(QString const &title, bool isLeftPane, QWidget *parent)
	: QGroupBox(title, parent),
	  m_isLeftPane(isLeftPane),
	  m_processStarted(false),
	  m_embedded(false),
	  m_embeddedWindow(0),
	  m_targetWidth(1024),
	  m_targetHeight(768),
	  m_inputToGame(false),
	  m_detachedForInput(false),
	  m_mirrorPeer(0),
	  m_mirrorInputEnabled(false)
	{
		ZeroMemory(&m_processInfo, sizeof(m_processInfo));

		QVBoxLayout *layout = new QVBoxLayout(this, 8, 6);
		layout->setMargin(4);
		layout->setSpacing(4);

		QHBoxLayout *controlRow = new QHBoxLayout(4);
		controlRow->addWidget(new QLabel("Profile:", this));
		m_profile = new QComboBox(false, this);
		controlRow->addWidget(m_profile);
		m_addProfileButton = new QPushButton("+", this);
		m_removeProfileButton = new QPushButton("-", this);
		m_addProfileButton->setFixedWidth(18);
		m_removeProfileButton->setFixedWidth(18);
		controlRow->addWidget(m_addProfileButton);
		controlRow->addWidget(m_removeProfileButton);

		controlRow->addWidget(new QLabel("User:", this));
		m_loginUser = new QLineEdit(this);
		m_loginUser->setMinimumWidth(120);
		controlRow->addWidget(m_loginUser);

		controlRow->addWidget(new QLabel("Pass:", this));
		m_loginPassword = new QLineEdit(this);
		m_loginPassword->setEchoMode(QLineEdit::Password);
		m_loginPassword->setMinimumWidth(120);
		controlRow->addWidget(m_loginPassword);

		m_autoLogin = new QCheckBox("Auto login", this);
		controlRow->addWidget(m_autoLogin);

		m_showAdvanced = new QCheckBox("Show advanced options", this);
		controlRow->addWidget(m_showAdvanced);

		m_launchButton = new QPushButton("Launch", this);
		m_stopButton = new QPushButton("Stop", this);
		m_refreshButton = new QPushButton("Refresh", this);
		m_inputButton = new QPushButton("Take Input", this);
		m_stopButton->setEnabled(false);
		m_refreshButton->setEnabled(false);
		m_inputButton->setEnabled(false);
		controlRow->addWidget(m_launchButton);
		controlRow->addWidget(m_stopButton);
		controlRow->addWidget(m_refreshButton);
		controlRow->addWidget(m_inputButton);

		m_status = new QLabel("Ready", this);
		controlRow->addWidget(m_status, 1);
		layout->addLayout(controlRow);

		m_advancedPanel = new QWidget(this);
		QHBoxLayout *advancedLayout = new QHBoxLayout(m_advancedPanel, 4, 0);
		advancedLayout->addWidget(new QLabel("Client EXE:", m_advancedPanel));
		m_exePath = new QLineEdit(this);
		advancedLayout->addWidget(m_exePath, 2);

		advancedLayout->addWidget(new QLabel("Args:", m_advancedPanel));
		m_args = new QLineEdit(this);
		advancedLayout->addWidget(m_args, 2);

		m_suspendRendering = new QCheckBox("Start minimized", m_advancedPanel);
		advancedLayout->addWidget(m_suspendRendering);

		m_forceWindowed = new QCheckBox("Force embedded window style", m_advancedPanel);
		m_forceWindowed->setChecked(true);
		advancedLayout->addWidget(m_forceWindowed);

		layout->addWidget(m_advancedPanel);
		m_advancedPanel->hide();

		m_container = new QWidget(this);
		m_container->setMinimumSize(320, 240);
		m_container->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
		m_container->setBackgroundMode(QWidget::NoBackground);
		m_container->installEventFilter(this);
		layout->addWidget(m_container, 1);

		m_pollTimer = new QTimer(this);
		connect(m_launchButton, SIGNAL(clicked()), this, SLOT(onLaunch()));
		connect(m_stopButton, SIGNAL(clicked()), this, SLOT(onStop()));
		connect(m_refreshButton, SIGNAL(clicked()), this, SLOT(onRefresh()));
		connect(m_inputButton, SIGNAL(clicked()), this, SLOT(onToggleInput()));
		connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(onPollEmbed()));
		connect(m_profile, SIGNAL(activated(const QString &)), this, SLOT(onProfileChanged(const QString &)));
		connect(m_addProfileButton, SIGNAL(clicked()), this, SLOT(onAddProfile()));
		connect(m_removeProfileButton, SIGNAL(clicked()), this, SLOT(onRemoveProfile()));
		connect(m_showAdvanced, SIGNAL(toggled(bool)), this, SLOT(onShowAdvanced(bool)));
	}

	~ClientPane()
	{
		stopProcess();
	}

	void setDefaults(QString const &exePath, QString const &args)
	{
		m_exePath->setText(exePath);
		m_args->setText(args);
	}

	void setTargetResolution(int width, int height)
	{
		m_targetWidth = width > 0 ? width : 1024;
		m_targetHeight = height > 0 ? height : 768;
	}

	bool isRunning() const
	{
		return m_processStarted;
	}

	void launchClient()
	{
		onLaunch();
	}

	void stopClient()
	{
		onStop();
	}

	void refreshClient()
	{
		onRefresh();
	}

	void releaseInput()
	{
		setInputToGame(false);
	}

	void focusInput()
	{
		setInputToGame(true);
	}

	void setMirrorPeer(ClientPane *peer)
	{
		m_mirrorPeer = peer;
	}

	void setMirrorInputEnabled(bool enabled)
	{
		m_mirrorInputEnabled = enabled;
	}

	void setProfileByName(QString const &name)
	{
		captureCurrentProfileState();
		for (int i = 0; i < m_profile->count(); ++i)
		{
			if (m_profile->text(i) == name)
			{
				m_profile->setCurrentItem(i);
				break;
			}
		}
		onProfileChanged(name);
	}

	QString activeProfileName() const
	{
		return m_profile->currentText();
	}

	QMap<QString, ProfileRecord> getProfiles()
	{
		captureCurrentProfileState();
		return m_profiles;
	}

	void setProfiles(QMap<QString, ProfileRecord> const &profiles, QString const &activeName)
	{
		m_profiles = profiles;
		m_profile->clear();
		for (QMap<QString, ProfileRecord>::ConstIterator it = m_profiles.begin(); it != m_profiles.end(); ++it)
			m_profile->insertItem(it.key());
		if (m_profile->count() == 0)
		{
			ProfileRecord r;
			r.name = "Default";
			r.exe = "D:/titan/exe/win32_rel/SwgTitan_r.exe";
			r.args = "-s Station players";
			r.autoLogin = false;
			r.startMinimized = false;
			r.forceWindowed = true;
			m_profiles[r.name] = r;
			m_profile->insertItem(r.name);
		}
		setProfileByName(activeName.isEmpty() ? m_profile->text(0) : activeName);
	}

	void setChromeVisible(bool visible)
	{
		QObjectList const *childrenList = children();
		if (!childrenList)
			return;

		for (QObjectListIt it(*childrenList); it.current(); ++it)
		{
			QWidget *w = dynamic_cast<QWidget *>(it.current());
			if (!w || w == m_container)
				continue;
			w->setShown(visible);
		}
		setTitle(visible ? (m_isLeftPane ? "Client 1" : "Client 2") : "");
	}


	QString toSettingsString() const
	{
		QString out;
		out += "exe=" + m_exePath->text() + "\n";
		out += "args=" + m_args->text() + "\n";
		out += "profile=" + m_profile->currentText() + "\n";
		out += "loginUser=" + m_loginUser->text() + "\n";
		out += "loginPassword=" + m_loginPassword->text() + "\n";
		out += "autoLogin=" + QString::number(m_autoLogin->isChecked() ? 1 : 0) + "\n";
		out += "startMinimized=" + QString::number(m_suspendRendering->isChecked() ? 1 : 0) + "\n";
		out += "forceWindowed=" + QString::number(m_forceWindowed->isChecked() ? 1 : 0) + "\n";
		return out;
	}

	void fromSettingsLines(QStringList const &lines)
	{
		for (QStringList::ConstIterator it = lines.begin(); it != lines.end(); ++it)
		{
			QString line = *it;
			int eq = line.find('=');
			if (eq <= 0)
				continue;
			QString key = line.left(eq);
			QString val = line.mid(eq + 1);
			if (key == "exe")
				m_exePath->setText(val);
			else if (key == "args")
				m_args->setText(val);
			else if (key == "profile")
			{
				for (int i = 0; i < m_profile->count(); ++i)
				{
					if (m_profile->text(i) == val)
					{
						m_profile->setCurrentItem(i);
						break;
					}
				}
			}
			else if (key == "loginUser")
				m_loginUser->setText(val);
			else if (key == "loginPassword")
				m_loginPassword->setText(val);
			else if (key == "autoLogin")
				m_autoLogin->setChecked(val.toInt() != 0);
			else if (key == "startMinimized")
				m_suspendRendering->setChecked(val.toInt() != 0);
			else if (key == "forceWindowed")
				m_forceWindowed->setChecked(val.toInt() != 0);
		}
	}

protected:
	int getMouseKeyFlagsFromState(int state) const
	{
		int flags = 0;
		if (state & Qt::LeftButton)
			flags |= MK_LBUTTON;
		if (state & Qt::RightButton)
			flags |= MK_RBUTTON;
		if (state & Qt::MidButton)
			flags |= MK_MBUTTON;
		if (state & Qt::ShiftButton)
			flags |= MK_SHIFT;
		if (state & Qt::ControlButton)
			flags |= MK_CONTROL;
		return flags;
	}

	POINT getEmbeddedClientPointFromContainerPos(QPoint const &containerPos) const
	{
		POINT p;
		if (m_detachedForInput)
		{
			QPoint global = m_container->mapToGlobal(containerPos);
			p.x = global.x();
			p.y = global.y();
			::ScreenToClient(m_embeddedWindow, &p);
		}
		else
		{
			p.x = containerPos.x();
			p.y = containerPos.y();
		}
		return p;
	}

	void postMouseMessageToEmbedded(UINT msg, QMouseEvent *event)
	{
		if (!m_embeddedWindow || !::IsWindow(m_embeddedWindow))
			return;
		POINT p = getEmbeddedClientPointFromContainerPos(event->pos());
		int const flags = getMouseKeyFlagsFromState(event->state());
		::PostMessage(m_embeddedWindow, msg, static_cast<WPARAM>(flags), MAKELPARAM(p.x, p.y));
	}

	void postWheelMessageToEmbedded(QWheelEvent *event)
	{
		if (!m_embeddedWindow || !::IsWindow(m_embeddedWindow))
			return;
		QPoint global = m_container->mapToGlobal(event->pos());
		int const flags = getMouseKeyFlagsFromState(event->state());
		WPARAM wp = MAKEWPARAM(flags, static_cast<WORD>(event->delta()));
		LPARAM lp = MAKELPARAM(global.x(), global.y());
		::PostMessage(m_embeddedWindow, WM_MOUSEWHEEL, wp, lp);
	}

	virtual bool eventFilter(QObject *watched, QEvent *event)
	{
		if (watched == m_container && m_embeddedWindow)
		{
			if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
				setInputToGame(true);

			if (m_inputToGame)
			{
				if (event->type() == QEvent::MouseButtonPress)
				{
					QMouseEvent *me = static_cast<QMouseEvent *>(event);
					if (me->button() == Qt::LeftButton)
						postMouseMessageToEmbedded(WM_LBUTTONDOWN, me);
					else if (me->button() == Qt::RightButton)
						postMouseMessageToEmbedded(WM_RBUTTONDOWN, me);
					else if (me->button() == Qt::MidButton)
						postMouseMessageToEmbedded(WM_MBUTTONDOWN, me);
					return true;
				}
				if (event->type() == QEvent::MouseButtonRelease)
				{
					QMouseEvent *me = static_cast<QMouseEvent *>(event);
					if (me->button() == Qt::LeftButton)
						postMouseMessageToEmbedded(WM_LBUTTONUP, me);
					else if (me->button() == Qt::RightButton)
						postMouseMessageToEmbedded(WM_RBUTTONUP, me);
					else if (me->button() == Qt::MidButton)
						postMouseMessageToEmbedded(WM_MBUTTONUP, me);
					return true;
				}
				if (event->type() == QEvent::MouseButtonDblClick)
				{
					QMouseEvent *me = static_cast<QMouseEvent *>(event);
					if (me->button() == Qt::LeftButton)
						postMouseMessageToEmbedded(WM_LBUTTONDBLCLK, me);
					else if (me->button() == Qt::RightButton)
						postMouseMessageToEmbedded(WM_RBUTTONDBLCLK, me);
					else if (me->button() == Qt::MidButton)
						postMouseMessageToEmbedded(WM_MBUTTONDBLCLK, me);
					return true;
				}
				if (event->type() == QEvent::MouseMove)
				{
					postMouseMessageToEmbedded(WM_MOUSEMOVE, static_cast<QMouseEvent *>(event));
					return true;
				}
				if (event->type() == QEvent::Wheel)
				{
					postWheelMessageToEmbedded(static_cast<QWheelEvent *>(event));
					return true;
				}
			}
		}
		return QGroupBox::eventFilter(watched, event);
	}

	virtual void keyPressEvent(QKeyEvent *event)
	{
		if (m_inputToGame && m_embeddedWindow)
		{
			::PostMessage(m_embeddedWindow, WM_KEYDOWN, static_cast<WPARAM>(event->key()), 0);
			if (event->text().length() > 0)
			{
				unsigned short c = event->text().unicode()[0].unicode();
				::PostMessage(m_embeddedWindow, WM_CHAR, static_cast<WPARAM>(c), 0);
			}
			mirrorKeyToPeer(WM_KEYDOWN, static_cast<WPARAM>(event->key()), 0);
			event->accept();
			return;
		}
		QGroupBox::keyPressEvent(event);
	}

	virtual void keyReleaseEvent(QKeyEvent *event)
	{
		if (m_inputToGame && m_embeddedWindow)
		{
			::PostMessage(m_embeddedWindow, WM_KEYUP, static_cast<WPARAM>(event->key()), 0);
			mirrorKeyToPeer(WM_KEYUP, static_cast<WPARAM>(event->key()), 0);
			event->accept();
			return;
		}
		QGroupBox::keyReleaseEvent(event);
	}

	virtual void resizeEvent(QResizeEvent *)
	{
		if (m_embedded && m_embeddedWindow)
		{
			if (m_detachedForInput)
			{
				QPoint gp = m_container->mapToGlobal(QPoint(0, 0));
				::MoveWindow(m_embeddedWindow, gp.x(), gp.y(), m_container->width(), m_container->height(), TRUE);
			}
			else
			{
				::MoveWindow(m_embeddedWindow, 0, 0, m_container->width(), m_container->height(), TRUE);
			}
		}
	}

private slots:
	void onShowAdvanced(bool checked)
	{
		if (checked)
			m_advancedPanel->show();
		else
			m_advancedPanel->hide();
	}

	void onProfileChanged(const QString &value)
	{
		if (!m_profiles.contains(value))
		{
			ProfileRecord r;
			r.name = value;
			r.exe = "D:/titan/exe/win32_rel/SwgTitan_r.exe";
			r.args = profileArgsForName(value, m_isLeftPane);
			r.autoLogin = false;
			r.startMinimized = false;
			r.forceWindowed = true;
			m_profiles[value] = r;
		}

		ProfileRecord const &r = m_profiles[value];
		m_exePath->setText(r.exe);
		m_args->setText(r.args);
		m_loginUser->setText(r.loginUser);
		m_loginPassword->setText(r.loginPassword);
		m_autoLogin->setChecked(r.autoLogin);
		m_suspendRendering->setChecked(r.startMinimized);
		m_forceWindowed->setChecked(r.forceWindowed);
	}

	void onAddProfile()
	{
		bool ok = false;
		QString name = QInputDialog::getText("Add Profile", "Profile name:", QLineEdit::Normal, QString::null, &ok, this);
		if (!ok)
			return;
		name = name.stripWhiteSpace();
		if (name.isEmpty() || m_profiles.contains(name))
			return;
		captureCurrentProfileState();
		ProfileRecord r;
		r.name = name;
		r.exe = m_exePath->text();
		r.args = m_args->text();
		r.loginUser = m_loginUser->text();
		r.loginPassword = m_loginPassword->text();
		r.autoLogin = m_autoLogin->isChecked();
		r.startMinimized = m_suspendRendering->isChecked();
		r.forceWindowed = m_forceWindowed->isChecked();
		m_profiles[name] = r;
		m_profile->insertItem(name);
		setProfileByName(name);
	}

	void onRemoveProfile()
	{
		QString const name = m_profile->currentText();
		if (name.isEmpty() || m_profile->count() <= 1)
			return;
		m_profiles.remove(name);
		m_profile->removeItem(m_profile->currentItem());
		setProfileByName(m_profile->text(0));
	}

	void onLaunch()
	{
		if (m_processStarted)
			return;

		QString exePath = m_exePath->text().stripWhiteSpace();
		if (exePath.isEmpty())
		{
			QMessageBox::warning(this, "Missing executable", "Please set a SWG client executable path.");
			return;
		}

		QString effectiveArgs = m_args->text().stripWhiteSpace();
		appendConfigOverride(effectiveArgs, "ClientGraphics", "screenWidth", QString::number(m_targetWidth));
		appendConfigOverride(effectiveArgs, "ClientGraphics", "screenHeight", QString::number(m_targetHeight));
		appendConfigOverride(effectiveArgs, "ClientGraphics", "windowed", "1");
		appendConfigOverride(effectiveArgs, "ClientGraphics", "constrainMouseCursorToWindow", "0");
		if (m_autoLogin->isChecked() && !m_loginUser->text().isEmpty())
		{
			appendConfigOverride(effectiveArgs, "ClientGame", "loginClientID", m_loginUser->text());
			appendConfigOverride(effectiveArgs, "ClientGame", "loginClientPassword", m_loginPassword->text());
		}

		QString cmd = buildCommandLine(exePath, effectiveArgs);
		STARTUPINFOA si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);

		if (m_suspendRendering->isChecked())
		{
			si.dwFlags = STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_SHOWMINNOACTIVE;
		}

		PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));

		QCString cmdBuf = cmd.local8Bit();
		BOOL ok = ::CreateProcessA(
			0,
			cmdBuf.data(),
			0,
			0,
			FALSE,
			0,
			0,
			0,
			&si,
			&pi);

		if (!ok)
		{
			QMessageBox::critical(this, "Launch failed", "CreateProcess failed for client executable.");
			m_status->setText("Launch failed");
			return;
		}

		m_processInfo = pi;
		::WaitForInputIdle(m_processInfo.hProcess, 5000);
		m_processStarted = true;
		m_embedded = false;
		m_embeddedWindow = 0;
		m_launchButton->setEnabled(false);
		m_stopButton->setEnabled(true);
		m_refreshButton->setEnabled(false);
		m_inputButton->setEnabled(false);
		m_status->setText("Launching...");
		m_pollTimer->start(100, false);
	}

	void onStop()
	{
		stopProcess();
	}

	void onRefresh()
	{
		if (!m_embeddedWindow || !::IsWindow(m_embeddedWindow))
			return;

		::SetWindowPos(m_embeddedWindow, 0, 0, 0, m_container->width(), m_container->height(), SWP_NOZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);
		::MoveWindow(m_embeddedWindow, 0, 0, m_container->width(), m_container->height(), TRUE);
		::RedrawWindow(m_embeddedWindow, 0, 0, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
		::UpdateWindow(m_embeddedWindow);
		m_status->setText("Refreshed");
	}

	void onToggleInput()
	{
		setInputToGame(!m_inputToGame);
	}

	void onPollEmbed()
	{
		if (!m_processStarted || m_embedded)
			return;

		DWORD exitCode = STILL_ACTIVE;
		if (::GetExitCodeProcess(m_processInfo.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
		{
			stopProcess();
			return;
		}

		HWND hwnd = findMainWindowForProcess(m_processInfo.dwProcessId);
		if (!hwnd)
		{
			m_pollTimer->start(100, false);
			return;
		}

		LONG style = ::GetWindowLong(hwnd, GWL_STYLE);
		if (m_forceWindowed->isChecked())
		{
			style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER);
			style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
			::SetWindowLong(hwnd, GWL_STYLE, style);
			::SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
		}
		::SetParent(hwnd, reinterpret_cast<HWND>(m_container->winId()));
		::MoveWindow(hwnd, 0, 0, m_container->width(), m_container->height(), TRUE);
		::ShowWindow(hwnd, SW_SHOW);
		::UpdateWindow(hwnd);

		m_embeddedWindow = hwnd;
		m_embedded = true;
		m_detachedForInput = false;
		m_refreshButton->setEnabled(true);
		m_inputButton->setEnabled(true);
		setInputToGame(false);
		m_status->setText("Running");
	}

private:
	void mirrorKeyToPeer(UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (!m_mirrorInputEnabled || !m_mirrorPeer)
			return;
		m_mirrorPeer->postKeyToEmbedded(msg, wparam, lparam);
	}

	void postKeyToEmbedded(UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (!m_embeddedWindow || !::IsWindow(m_embeddedWindow))
			return;
		::PostMessage(m_embeddedWindow, msg, wparam, lparam);
	}

	void captureCurrentProfileState()
	{
		QString const name = m_profile->currentText();
		if (name.isEmpty())
			return;
		ProfileRecord r;
		r.name = name;
		r.exe = m_exePath->text();
		r.args = m_args->text();
		r.loginUser = m_loginUser->text();
		r.loginPassword = m_loginPassword->text();
		r.autoLogin = m_autoLogin->isChecked();
		r.startMinimized = m_suspendRendering->isChecked();
		r.forceWindowed = m_forceWindowed->isChecked();
		m_profiles[name] = r;
	}

	void setInputToGame(bool enabled)
	{
		if (!m_embeddedWindow || !::IsWindow(m_embeddedWindow))
		{
			m_inputToGame = false;
			m_inputButton->setText("Take Input");
			return;
		}

		m_inputToGame = enabled;
		if (enabled)
		{
			detachEmbeddedForInput();

			HWND hostTop = reinterpret_cast<HWND>(topLevelWidget()->winId());
			DWORD const targetThread = ::GetWindowThreadProcessId(m_embeddedWindow, 0);
			DWORD const hostThread = ::GetCurrentThreadId();
			DWORD const fgThread = ::GetWindowThreadProcessId(::GetForegroundWindow(), 0);

			if (fgThread && fgThread != hostThread)
				::AttachThreadInput(hostThread, fgThread, TRUE);
			if (targetThread && targetThread != hostThread)
				::AttachThreadInput(hostThread, targetThread, TRUE);

			::SetForegroundWindow(hostTop);
			::BringWindowToTop(hostTop);
			::SetActiveWindow(hostTop);
			::EnableWindow(m_embeddedWindow, TRUE);
			::SendMessage(m_embeddedWindow, WM_ACTIVATEAPP, TRUE, 0);
			::SendMessage(m_embeddedWindow, WM_NCACTIVATE, TRUE, 0);
			::SendMessage(m_embeddedWindow, WM_ACTIVATE, MAKEWPARAM(WA_ACTIVE, 0), reinterpret_cast<LPARAM>(hostTop));
			::SetFocus(m_embeddedWindow);
			::SetCapture(m_embeddedWindow);
			::PostMessage(m_embeddedWindow, WM_SETFOCUS, reinterpret_cast<WPARAM>(hostTop), 0);

			if (targetThread && targetThread != hostThread)
				::AttachThreadInput(hostThread, targetThread, FALSE);
			if (fgThread && fgThread != hostThread)
				::AttachThreadInput(hostThread, fgThread, FALSE);

			m_inputButton->setText("Release Input");
			m_status->setText("Input: Game");
		}
		else
		{
			::ReleaseCapture();
			reattachEmbeddedAfterInput();
			HWND hostTop = reinterpret_cast<HWND>(topLevelWidget()->winId());
			::SetForegroundWindow(hostTop);
			::SetActiveWindow(hostTop);
			::SetFocus(reinterpret_cast<HWND>(winId()));
			setFocus();
			m_inputButton->setText("Take Input");
			m_status->setText("Input: Launcher");
		}
	}

	void detachEmbeddedForInput()
	{
		if (m_detachedForInput || !m_embeddedWindow || !::IsWindow(m_embeddedWindow))
			return;

		QPoint gp = m_container->mapToGlobal(QPoint(0, 0));
		LONG style = ::GetWindowLong(m_embeddedWindow, GWL_STYLE);
		style &= ~(WS_CHILD | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER);
		style |= WS_POPUP | WS_VISIBLE;
		::SetWindowLong(m_embeddedWindow, GWL_STYLE, style);
		::SetParent(m_embeddedWindow, 0);
		::SetWindowPos(m_embeddedWindow, HWND_TOP, gp.x(), gp.y(), m_container->width(), m_container->height(), SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		m_detachedForInput = true;
	}

	void reattachEmbeddedAfterInput()
	{
		if (!m_detachedForInput || !m_embeddedWindow || !::IsWindow(m_embeddedWindow))
			return;

		LONG style = ::GetWindowLong(m_embeddedWindow, GWL_STYLE);
		style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER);
		style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
		::SetWindowLong(m_embeddedWindow, GWL_STYLE, style);
		::SetParent(m_embeddedWindow, reinterpret_cast<HWND>(m_container->winId()));
		::SetWindowPos(m_embeddedWindow, 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
		::MoveWindow(m_embeddedWindow, 0, 0, m_container->width(), m_container->height(), TRUE);
		m_detachedForInput = false;
	}

	void stopProcess()
	{
		m_pollTimer->stop();

		if (m_embeddedWindow && ::IsWindow(m_embeddedWindow))
		{
			::PostMessage(m_embeddedWindow, WM_CLOSE, 0, 0);
		}
		m_embeddedWindow = 0;
		m_embedded = false;

		if (m_processStarted)
		{
			DWORD exitCode = STILL_ACTIVE;
			if (::GetExitCodeProcess(m_processInfo.hProcess, &exitCode) && exitCode == STILL_ACTIVE)
			{
				::TerminateProcess(m_processInfo.hProcess, 0);
			}
			if (m_processInfo.hThread)
				::CloseHandle(m_processInfo.hThread);
			if (m_processInfo.hProcess)
				::CloseHandle(m_processInfo.hProcess);
			ZeroMemory(&m_processInfo, sizeof(m_processInfo));
		}

		m_processStarted = false;
		m_inputToGame = false;
		m_detachedForInput = false;
		m_launchButton->setEnabled(true);
		m_stopButton->setEnabled(false);
		m_refreshButton->setEnabled(false);
		m_inputButton->setEnabled(false);
		m_inputButton->setText("Take Input");
		m_status->setText("Stopped");
	}

private:
	bool m_isLeftPane;
	QLabel *m_status;
	QLineEdit *m_exePath;
	QLineEdit *m_args;
	QComboBox *m_profile;
	QPushButton *m_addProfileButton;
	QPushButton *m_removeProfileButton;
	QLineEdit *m_loginUser;
	QLineEdit *m_loginPassword;
	QCheckBox *m_autoLogin;
	QCheckBox *m_showAdvanced;
	QCheckBox *m_suspendRendering;
	QCheckBox *m_forceWindowed;
	QWidget *m_advancedPanel;
	QPushButton *m_launchButton;
	QPushButton *m_stopButton;
	QPushButton *m_refreshButton;
	QPushButton *m_inputButton;
	QWidget *m_container;
	QTimer *m_pollTimer;

	PROCESS_INFORMATION m_processInfo;
	bool m_processStarted;
	bool m_embedded;
	HWND m_embeddedWindow;
	int m_targetWidth;
	int m_targetHeight;
	bool m_inputToGame;
	bool m_detachedForInput;
	ClientPane *m_mirrorPeer;
	bool m_mirrorInputEnabled;
	QMap<QString, ProfileRecord> m_profiles;
};

class BoxingMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	BoxingMainWindow()
	: QMainWindow(0, "SwgBoxingMainWindow"),
	  m_inputFocusClient1Id(-1),
	  m_inputFocusClient2Id(-1),
	  m_inputReleaseBothId(-1),
	  m_inputMirrorId(-1),
	  m_mirrorInputEnabled(false)
	{
		setCaption("Star Wars Galaxies Container");
		int screenW = ::GetSystemMetrics(SM_CXSCREEN);
		int screenH = ::GetSystemMetrics(SM_CYSCREEN);
		setGeometry(0, 0, screenW, screenH);

		QWidget *root = new QWidget(this);
		QVBoxLayout *rootLayout = new QVBoxLayout(root, 2, 2);

		QToolBar *menuToolbar = new QToolBar(this, "menuToolbar");
		menuToolbar->setLabel("Main");
		menuToolbar->setHorizontalStretchable(false);
		menuToolbar->setFixedHeight(22);
		menuToolbar->setMargin(0);
		menuToolbar->setMovingEnabled(false);

		m_fileMenu = new QPopupMenu(this);
		m_fileMenu->insertItem("Launch Both", this, SLOT(onLaunchAll()));
		m_fileMenu->insertItem("Stop Both", this, SLOT(onStopAll()));
		m_fileMenu->insertItem("Refresh Both", this, SLOT(onRefreshAll()));
		m_fileMenu->insertSeparator();
		m_fileMenu->insertItem("Exit", this, SLOT(close()));

		m_profileMenu = new QPopupMenu(this);
		m_profileMenu->insertItem("Default", this, SLOT(onProfileDefault()));
		m_profileMenu->insertItem("Player A / Player B", this, SLOT(onProfileDual()));
		m_profileMenu->insertItem("Local", this, SLOT(onProfileLocal()));
		m_profileMenu->insertItem("TC", this, SLOT(onProfileTc()));

		m_inputMenu = new QPopupMenu(this);
		m_inputMenu->setCheckable(true);
		m_inputFocusClient1Id = m_inputMenu->insertItem("Focus Client 1");
		m_inputFocusClient2Id = m_inputMenu->insertItem("Focus Client 2");
		m_inputMenu->insertSeparator();
		m_inputReleaseBothId = m_inputMenu->insertItem("Release Both");
		m_inputMenu->insertSeparator();
		m_inputMirrorId = m_inputMenu->insertItem("Mirror input to other client");
		m_inputMenu->setItemChecked(m_inputMirrorId, false);
		connect(m_inputMenu, SIGNAL(activated(int)), this, SLOT(onInputMenuActivated(int)));

		QToolButton *fileButton = new QToolButton(menuToolbar);
		fileButton->setTextLabel("File");
		fileButton->setUsesTextLabel(true);
		fileButton->setPopup(m_fileMenu);
		fileButton->setPopupDelay(0);
		fileButton->setAutoRaise(true);
		fileButton->setFixedHeight(20);

		QToolButton *profileButton = new QToolButton(menuToolbar);
		profileButton->setTextLabel("Profile");
		profileButton->setUsesTextLabel(true);
		profileButton->setPopup(m_profileMenu);
		profileButton->setPopupDelay(0);
		profileButton->setAutoRaise(true);
		profileButton->setFixedHeight(20);

		QToolButton *inputButton = new QToolButton(menuToolbar);
		inputButton->setTextLabel("Input");
		inputButton->setUsesTextLabel(true);
		inputButton->setPopup(m_inputMenu);
		inputButton->setPopupDelay(0);
		inputButton->setAutoRaise(true);
		inputButton->setFixedHeight(20);

		statusBar()->hide();

		QSplitter *split = new QSplitter(Qt::Horizontal, root);
		split->setOpaqueResize(true);
		split->setHandleWidth(0);
		rootLayout->addWidget(split, 1);
		setCentralWidget(root);

		m_left = new ClientPane("Client 1", true, split);
		m_right = new ClientPane("Client 2", false, split);
		m_left->setMirrorPeer(m_right);
		m_right->setMirrorPeer(m_left);

		m_left->setDefaults("D:/titan/exe/win32_rel/SwgTitan_r.exe", "-s Station players");
		m_right->setDefaults("D:/titan/exe/win32_rel/SwgTitan_r.exe", "-s Station players");
		m_globalAccel = new QAccel(this, "boxingGlobalAccel");
		int const releaseAccelId = m_globalAccel->insertItem(CTRL + SHIFT + Key_Home);
		m_globalAccel->connectItem(releaseAccelId, this, SLOT(onReleaseInputAll()));
		m_left->setChromeVisible(false);
		m_right->setChromeVisible(false);
		updateScreenFit();
		loadProfiles();
	}

	~BoxingMainWindow()
	{
		saveProfiles();
	}

protected:
	virtual void resizeEvent(QResizeEvent *e)
	{
		QMainWindow::resizeEvent(e);
		updateScreenFit();
	}

private:
	void showToast(QString const &message)
	{
		(void)message;
	}

private slots:
	void onLaunchAll()
	{
		if (!m_left->isRunning())
			m_left->launchClient();
		if (!m_right->isRunning())
			m_right->launchClient();
		showToast("Launched both clients at " + QTime::currentTime().toString());
	}

	void onStopAll()
	{
		if (m_left->isRunning())
			m_left->stopClient();
		if (m_right->isRunning())
			m_right->stopClient();
		showToast("Stopped both clients");
	}

	void onRefreshAll()
	{
		m_left->refreshClient();
		m_right->refreshClient();
		showToast("Refreshed both clients");
	}

	void onReleaseInputAll()
	{
		m_left->releaseInput();
		m_right->releaseInput();
		showToast("Released input to launcher");
	}

	void onInputFocusClient1()
	{
		m_left->focusInput();
		showToast("Focused input on client 1");
	}

	void onInputFocusClient2()
	{
		m_right->focusInput();
		showToast("Focused input on client 2");
	}

	void onInputMenuActivated(int id)
	{
		if (id == m_inputFocusClient1Id)
		{
			onInputFocusClient1();
			return;
		}
		if (id == m_inputFocusClient2Id)
		{
			onInputFocusClient2();
			return;
		}
		if (id == m_inputReleaseBothId)
		{
			onReleaseInputAll();
			return;
		}
		if (id == m_inputMirrorId)
		{
			m_mirrorInputEnabled = !m_mirrorInputEnabled;
			m_inputMenu->setItemChecked(m_inputMirrorId, m_mirrorInputEnabled);
			m_left->setMirrorInputEnabled(m_mirrorInputEnabled);
			m_right->setMirrorInputEnabled(m_mirrorInputEnabled);
			showToast(m_mirrorInputEnabled ? "Mirror input enabled" : "Mirror input disabled");
			return;
		}
	}

	void onProfileDefault()
	{
		m_left->setProfileByName("Default");
		m_right->setProfileByName("Default");
		showToast("Profile: Default");
	}

	void onProfileDual()
	{
		m_left->setProfileByName("Player A");
		m_right->setProfileByName("Player B");
		showToast("Profile: Player A / Player B");
	}

	void onProfileLocal()
	{
		m_left->setProfileByName("Local");
		m_right->setProfileByName("Local");
		showToast("Profile: Local");
	}

	void onProfileTc()
	{
		m_left->setProfileByName("TC");
		m_right->setProfileByName("TC");
		showToast("Profile: TC");
	}

private:
	void updateScreenFit()
	{
		int w = width();
		int h = height();
		int leftW = w / 2;
		int rightW = w - leftW;
		m_left->setTargetResolution(leftW, h);
		m_right->setTargetResolution(rightW, h);
	}

	void loadProfiles()
	{
		QFile f("SwgBoxingQt_profiles.ini");
		QMap<QString, ProfileRecord> profiles;
		QString leftActive = "Default";
		QString rightActive = "Default";

		if (f.open(IO_ReadOnly))
		{
			QString content;
			QTextStream ts(&f);
			while (!ts.atEnd())
			{
				content += ts.readLine();
				content += "\n";
			}
			f.close();

			IniSectionMap const ini = parseIni(content);
			if (ini.contains("state"))
			{
				QMap<QString, QString> const &s = ini["state"];
				if (s.contains("leftActive"))
					leftActive = s["leftActive"];
				if (s.contains("rightActive"))
					rightActive = s["rightActive"];
			}
			for (IniSectionMap::ConstIterator sec = ini.begin(); sec != ini.end(); ++sec)
			{
				if (!sec.key().startsWith("profile."))
					continue;
				QString const profileName = sec.key().mid(8);
				QMap<QString, QString> const &k = sec.data();
				ProfileRecord r;
				r.name = profileName;
				r.exe = k.contains("exe") ? k["exe"] : "D:/titan/exe/win32_rel/SwgTitan_r.exe";
				r.args = k.contains("args") ? k["args"] : "-s Station players";
				r.loginUser = k.contains("loginUser") ? k["loginUser"] : "";
				r.loginPassword = k.contains("loginPassword") ? k["loginPassword"] : "";
				r.autoLogin = k.contains("autoLogin") ? (k["autoLogin"].toInt() != 0) : false;
				r.startMinimized = k.contains("startMinimized") ? (k["startMinimized"].toInt() != 0) : false;
				r.forceWindowed = k.contains("forceWindowed") ? (k["forceWindowed"].toInt() != 0) : true;
				profiles[profileName] = r;
			}
		}

		if (profiles.isEmpty())
		{
			ProfileRecord def;
			def.name = "Default";
			def.exe = "D:/titan/exe/win32_rel/SwgTitan_r.exe";
			def.args = "-s Station players";
			def.autoLogin = false;
			def.startMinimized = false;
			def.forceWindowed = true;
			profiles[def.name] = def;
		}

		m_left->setProfiles(profiles, leftActive);
		m_right->setProfiles(profiles, rightActive);
	}

	void saveProfiles()
	{
		QMap<QString, ProfileRecord> allProfiles = m_left->getProfiles();
		QMap<QString, ProfileRecord> rightProfiles = m_right->getProfiles();
		for (QMap<QString, ProfileRecord>::ConstIterator it = rightProfiles.begin(); it != rightProfiles.end(); ++it)
			allProfiles[it.key()] = it.data();

		IniSectionMap ini;
		ini["state"]["leftActive"] = m_left->activeProfileName();
		ini["state"]["rightActive"] = m_right->activeProfileName();
		ini["meta"]["version"] = "2";

		for (QMap<QString, ProfileRecord>::ConstIterator it = allProfiles.begin(); it != allProfiles.end(); ++it)
		{
			ProfileRecord const &r = it.data();
			QString const sec = "profile." + r.name;
			ini[sec]["exe"] = r.exe;
			ini[sec]["args"] = r.args;
			ini[sec]["loginUser"] = r.loginUser;
			ini[sec]["loginPassword"] = r.loginPassword;
			ini[sec]["autoLogin"] = r.autoLogin ? "1" : "0";
			ini[sec]["startMinimized"] = r.startMinimized ? "1" : "0";
			ini[sec]["forceWindowed"] = r.forceWindowed ? "1" : "0";
		}

		QFile f("SwgBoxingQt_profiles.ini");
		if (!f.open(IO_WriteOnly))
			return;
		QTextStream ts(&f);
		ts << writeIni(ini);
		f.close();
	}

private:
	QPopupMenu *m_fileMenu;
	QPopupMenu *m_profileMenu;
	QPopupMenu *m_inputMenu;
	int m_inputFocusClient1Id;
	int m_inputFocusClient2Id;
	int m_inputReleaseBothId;
	int m_inputMirrorId;
	bool m_mirrorInputEnabled;
	QAccel *m_globalAccel;
	ClientPane *m_left;
	ClientPane *m_right;
};

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	BoxingMainWindow mw;
	app.setMainWidget(&mw);
	mw.show();
	return app.exec();
}

#include "main.moc"
