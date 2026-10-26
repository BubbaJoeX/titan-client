// ======================================================================
//
// DevBridge.h
//
// Developer bridge: a localhost-only TCP socket the client polls once per
// frame, so tools (an MCP server, a test script) can drive the client and
// read its state without screenshots. Off unless [ClientGame]
// devBridgePort is set; development builds only. One JSON object per line in
// each direction; every request carries "token", read from
// %LOCALAPPDATA%\SwgTitan\devbridge.json (port, token, pid) which the client
// writes at start-up (a wrong token closes the connection):
//
//   {"id":1,"cmd":"ping","token":"..."}
//   {"id":2,"cmd":"state"}                         player / camera / cell, mover (speed, altitude)
//   {"id":3,"cmd":"exec","line":"/warpme 0 5 0"}   run as if typed in chat
//   {"id":4,"cmd":"screenshot","path":"shots/a"}   engine adds the extension; taken from the back buffer just
//                                                  before the next Present, so covered / unfocused windows work
//                                                  ("now":1 = the old immediate front-buffer grab)
//   {"id":14,"cmd":"ui","op":"tree","path":"/Opt"} UI automation: tree / get / set / press / open / close / scale /
//                                                  fontscale / font (see DevBridge.cpp)
//   {"id":5,"cmd":"log","since":0}                 console output lines
//   {"id":6,"cmd":"walk","to":[x,y,z],"face":[x,y,z],"speed":6}  walk there frame by frame
//                                                  under real collision; replies on arrival or "blocked"
//   {"id":8,"cmd":"settle"}                        drop the post-warp float, stand on the floor
//   {"id":7,"cmd":"objects","radius":150}          objects near the player
//   {"id":9,"cmd":"terrain","points":[[x,z],...]}  terrain heights (building layers applied)
//   {"id":10,"cmd":"menu"}                         offline test menu actions (DevTestMenu) + saved points
//   {"id":11,"cmd":"menu","action":"flight.xwing"} run one; each console line with its output
//   {"id":12,"cmd":"menu","open":1}                open the in-game test menu (/devtest menu)
//   {"id":13,"cmd":"camera","pivot":[x,y,z],"distance":2,"yaw":180,"pitch":5}  free camera orbiting a point;
//        "mode":"free" + "pos"/"look", "mode":"orbit" + distance/pitch/yaw/target, "mode":"info", "mode":"chase"
//   {"id":15,"cmd":"menuselect","oid":"<oid>","item":"SERVER_MENU1"}  pick a radial item (name or number) on an
//                                                  object, as clicking it does; the server answers as usual
//
// ======================================================================

#ifndef INCLUDED_DevBridge_H
#define INCLUDED_DevBridge_H

// ======================================================================

class DevBridge
{
public:

	static void update();
	static void beforePresent();          // Game loop, after endScene: deferred screenshots
	static void remove();

private:

	DevBridge();
	DevBridge(DevBridge const &);
	DevBridge & operator=(DevBridge const &);
};

// ======================================================================

#endif
