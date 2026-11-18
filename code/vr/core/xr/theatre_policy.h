#pragma once
// Settings > Camera > Cutscenes and the scene test that admits the stereo theatre. GTAVXR-owned policy,
// VS2012-compatible, no game or runtime headers.
namespace gx_theatre
{
enum View
{
    CURRENT_CAMERA = 0, // the headset moves the view inside the cutscene, as before this option existed
    STEREO_THEATRE = 1, // the cutscene camera films a stereo pair shown on a screen fixed in the room
    VIEW_COUNT
};
const View DefaultView = STEREO_THEATRE;

inline int sanitize(int view)
{
    return view == STEREO_THEATRE ? STEREO_THEATRE : CURRENT_CAMERA;
}
// Cutscenes are authored for a 16:9 frame.
const double AuthoredAspect = 16.0 / 9.0;

// The camera director the game renders this update.
enum Director
{
    NO_DIRECTOR,
    CUTSCENE_DIRECTOR,  // the cutscene director (it also films a player seated in a vehicle)
    GAMEPLAY_DIRECTOR,  // gameplay cameras: on foot, the cockpit view and the chase view while driving
    CINEMATIC_DIRECTOR, // cinematic cameras, including the in-vehicle first-person cinematic camera
    OTHER_DIRECTOR      // scripted, synced-scene, switch and any other director
};
// What the game reports this update. Each field has one producer in native_theatre.inl (TheatreScene).
struct Scene
{
    bool playback;     // CutSceneManager reports confirmed playback
    unsigned director; // Director: the dominant rendered director
    bool blendingOut;  // the cutscene director is blending out to the first-person camera
    bool scope;        // this pair is a scope frame
    bool pause;        // pause render phases
    bool online;       // a network session exists (fail-closed)
    Scene() : playback(false), director(NO_DIRECTOR), blendingOut(false), scope(false), pause(false), online(false)
    {
    }
};
// Only a cutscene that its manager is playing back and that its own director renders. The vehicle views
// (cockpit, chase) and scripted cameras are other directors, so they never pass; a player sitting in a
// vehicle inside a cutscene does.
inline bool cutscene(const Scene& s)
{
    return s.playback && s.director == CUTSCENE_DIRECTOR && !s.blendingOut && !s.scope && !s.pause && !s.online;
}
// Vertical field of view the eye projections reproduce within the game's own projection check. A shot
// outside it (an extreme telephoto or fisheye lens) is shown with the current VR camera.
inline bool lens(double verticalFovDegrees)
{
    return verticalFovDegrees >= 5.0 && verticalFovDegrees <= 120.0;
}
// bridgeExport: the loaded bridge offers the theatre. Without it the choice has no effect.
inline bool wanted(int view, bool bridgeExport, const Scene& s, double verticalFovDegrees)
{
    return sanitize(view) == STEREO_THEATRE && bridgeExport && cutscene(s) && lens(verticalFovDegrees);
}
}
