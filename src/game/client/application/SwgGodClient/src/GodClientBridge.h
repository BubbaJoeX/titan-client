// ======================================================================
//
// GodClientBridge.h
//
// God Client automation bridge: a localhost-only TCP socket the editor polls
// once per frame (GameWidget::runGameLoop, after the game frame), so tools
// and agents can drive world editing without a human at the mouse. Off
// unless [GodClient] bridgePort is set in SwgGodClient.cfg. Same wire format
// as the game client's DevBridge (one JSON object per line each way), but
// its own port and token file: %LOCALAPPDATA%\SwgTitan\godclient_bridge.json
// ({port, token, pid}), written at start-up and removed at exit. Every
// request carries "token"; a wrong token closes the connection.
//
//   {"id":1,"cmd":"ping","token":"..."}
//   {"id":2,"cmd":"state"}                     scene, camera, cursor, player cell, selection, undo, terrain tool
//   {"id":19,"cmd":"loadscene","terrain":"terrain/tatooine.trn","avatar":"object/creature/player/shared_human_male.iff"}
//        offline single-player scene from the login screen (its Load Scene button); then poll state for ready
//   {"id":3,"cmd":"selection"}                 selected objects: id, template, transform, pending ghost
//   {"id":4,"cmd":"select","ids":["12345"],"add":0}  by network id (replaces the selection unless add)
//   {"id":5,"cmd":"select","template":"object/tangible/.../shared_x.iff","add":0}  every loaded object of a template
//   {"id":6,"cmd":"select","clear":1}
//   {"id":7,"cmd":"objects","center":[x,y,z],"radius":64,"template":"chair","limit":100}  nearest loaded objects
//   {"id":8,"cmd":"create","template":"object/...iff","pos":[x,y,z],"yaw":90,"select":1}  world cell
//   {"id":9,"cmd":"transform","pos":[x,y,z],"yaw":0,"pitch":0,"roll":0,"scale":[1,1,1],"apply":1}
//        ghosts every selected object and sets the given parts (parent space, degrees; a rotation field
//        replaces the whole rotation, missing angles are 0); "copy":1 copies the primary selection's transform,
//        "paste":"position|rotation|all" pastes the copied one; "apply":1 then applies like Ctrl+T
//   {"id":10,"cmd":"apply"}                    move the selection to its ghosts (Ctrl+T / Space)
//   {"id":11,"cmd":"undo","target":"auto"}     "auto" (Ctrl+Z: terrain first while a terrain tool is armed),
//   {"id":12,"cmd":"redo","target":"auto"}     "objects" or "terrain"
//   {"id":13,"cmd":"console","line":"object get 12345","wait":5}  server console, reply matched by message id;
//                                              "/name args" runs a slash command on the primary selection
//   {"id":14,"cmd":"log","since":0}            console lines (server replies, bridge commands)
//   {"id":15,"cmd":"camera"}                   god camera FreeCamera::Info (pivot, distance, yaw, pitch in degrees)
//   {"id":16,"cmd":"camera","pivot":[x,y,z],"distance":40,"yaw":45,"pitch":30,"smooth":0}  any subset
//   {"id":17,"cmd":"screenshot","path":"screenshots/a"}  the 3D view's back buffer (engine adds the extension);
//                                              reply has the full path and "source" (back_buffer, or front_buffer)
//   {"id":18,"cmd":"terrain_stroke","tool":"raise","points":[[x,z],[x,z]],"size":16,"strength":0.5,"spacing":4}
//        one brush stroke through world x/z points (resampled every "spacing" m), one terrain undo step
//
// ======================================================================

#ifndef INCLUDED_GodClientBridge_H
#define INCLUDED_GodClientBridge_H

// ======================================================================

class GodClientBridge
{
public:

	static void update();
	static void remove();

private:

	GodClientBridge();
	GodClientBridge(GodClientBridge const &);
	GodClientBridge & operator=(GodClientBridge const &);
};

// ======================================================================

#endif
