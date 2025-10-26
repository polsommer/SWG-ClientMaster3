//
// View3dView.cpp 
// aommers
//
// copyright 2001, sony online entertainment
//

//-------------------------------------------------------------------

#include "FirstTerrainEditor.h"
#include "View3dView.h"

#include "MapFrame.h"
#include "Resource.h"
#include "TerrainEditorDoc.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/Light.h"
#include "clientGraphics/RenderWorld.h"
#include "clientObject/GameCamera.h"
#include "sharedDebug/Profiler.h"
#include "sharedFile/Iff.h"
#include "sharedFile/TreeFile.h"
#include "sharedMath/VectorArgb.h"
#include "sharedObject/AppearanceTemplate.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/Object.h"
#include "sharedTerrain/TerrainObject.h"

#include <algorithm>
#include <cmath>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//-------------------------------------------------------------------

namespace
{
        const real cs_minimumVerticalClearance = 2.f;
        const real cs_targetHeightBlend        = 0.25f;

        struct OrbitBasis
        {
                Vector forward;
                Vector right;
                Vector up;
        };

        OrbitBasis calculateOrbitBasis(real yaw, real pitch)
        {
                OrbitBasis basis;

                const real cosPitch = static_cast<real>(::cos(pitch));
                basis.forward = Vector(static_cast<real>(::sin(yaw)) * cosPitch, static_cast<real>(::sin(pitch)), static_cast<real>(::cos(yaw)) * cosPitch);
                IGNORE_RETURN(basis.forward.normalize());

                basis.right = Vector::unitY.cross(basis.forward);
                if (!basis.right.normalize())
                        basis.right = Vector::unitX;

                basis.up = basis.right.cross(basis.forward);
                IGNORE_RETURN(basis.up.normalize());

                return basis;
        }
}

//-------------------------------------------------------------------

static inline bool keyDown (int key)
{
        return (GetKeyState (key) & 0x8000) != 0;
}

//-------------------------------------------------------------------

IMPLEMENT_DYNCREATE(View3dView, CView)

//-------------------------------------------------------------------

View3dView::View3dView() :
        CView (),
        camera (0),
        terrain (0),
        ambientLight (0),
        keyLight (0),
        yaw (0),
        pitch (0),
        orbitTarget (Vector::zero),
        orbitDistance (128.f),
        minOrbitDistance (8.f),
        maxOrbitDistance (4096.f),
        verticalOffset (12.f),
        timer (0),
        milliseconds (50),
        elapsedTime (0.f),
        render (false),
        mouseCaptured (false),
        lastMousePositionValid (false),
        lastMousePosition (0, 0)
{
}

//-------------------------------------------------------------------

View3dView::~View3dView()
{
        if (keyLight)
        {
                keyLight->removeFromWorld ();
                delete keyLight;
                keyLight = 0;
        }

        if (ambientLight)
        {
                ambientLight->removeFromWorld ();
                delete ambientLight;
                ambientLight = 0;
        }

        delete camera;
        camera = 0;

        NOT_NULL (terrain);
        terrain->removeFromWorld ();
        delete terrain;
        terrain = 0;
}

//-------------------------------------------------------------------

BEGIN_MESSAGE_MAP(View3dView, CView)
        //{{AFX_MSG_MAP(View3dView)
        ON_WM_MOUSEMOVE()
        ON_WM_MOUSEWHEEL()
        ON_WM_SIZE()
        ON_WM_ERASEBKGND()
        ON_WM_DESTROY()
        ON_WM_TIMER()
        ON_COMMAND(ID_REFRESH, OnRefresh)
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-------------------------------------------------------------------

void View3dView::OnDraw(CDC* pDC)
{
        UNREF (pDC);

        if (!render || !(terrain && terrain->getAppearance ()))
                return;

        NOT_NULL (camera);

        NOT_NULL (terrain);
        const real frameTime = elapsedTime;
        IGNORE_RETURN (terrain->alter (elapsedTime));

        elapsedTime = 0;

        handleKeyboardNavigation (frameTime);
        applyCameraTransform ();

        //-- point the Gl at this window
        Graphics::setViewport (0, 0, camera->getViewportWidth (), camera->getViewportHeight ());

	//-- render a frame
	Graphics::beginScene ();

		Graphics::clearViewport(true, 0xffa1a1a1, true, 1.0f, true, 0);

		//-- render the scene
		camera->renderScene ();

	//-- done rendering the frame
	Graphics::endScene ();
	Graphics::present (m_hWnd, camera->getViewportWidth (), camera->getViewportHeight ());
}

//-------------------------------------------------------------------

#ifdef _DEBUG
void View3dView::AssertValid() const
{
	CView::AssertValid();
}

void View3dView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif //_DEBUG

//-------------------------------------------------------------------

void View3dView::OnInitialUpdate()
{
        CView::OnInitialUpdate();

        timer = SetTimer (1, milliseconds, 0);

        //-- create camera
        camera = new GameCamera ();
        camera->setHorizontalFieldOfView (PI_OVER_3);
        camera->setNearPlane (0.05f);
        camera->setFarPlane (maxOrbitDistance * 2.f);
        camera->addToWorld ();

        ambientLight = new Light (Light::T_ambient, VectorArgb (1.f, 0.45f, 0.45f, 0.45f));
        ambientLight->addToWorld ();
        camera->addChildObject_o (ambientLight);

        keyLight = new Light (Light::T_parallel, VectorArgb (1.f, 0.85f, 0.85f, 0.85f));
        keyLight->yaw_o (-PI_OVER_4);
        keyLight->pitch_o (-PI_OVER_4);
        keyLight->addToWorld ();
        camera->addChildObject_o (keyLight);

        terrain = new TerrainObject ();
        RenderWorld::addObjectNotifications (*terrain);
        terrain->addToWorld ();

        yaw   = 0;
        pitch = 0;

        updateOrbitTargetFromDocument ();
        configureCameraForDocument (true);
        applyCameraTransform ();

        OnRefresh ();
}  //lint !e429  //-- ambientLight/parallelLight has not been freed or returned

//-------------------------------------------------------------------

void View3dView::OnMouseMove(UINT nFlags, CPoint point) 
{
	NOT_NULL (terrain);

        if (!terrain->getAppearance ())
        {
                CView::OnMouseMove(nFlags, point);
                return;
        }

        const bool leftButtonDown   = (nFlags & MK_LBUTTON) != 0;
        const bool rightButtonDown  = (nFlags & MK_RBUTTON) != 0;
        const bool middleButtonDown = (nFlags & MK_MBUTTON) != 0;

        if (!leftButtonDown && !rightButtonDown && !middleButtonDown)
        {
                if (mouseCaptured)
                {
                        ReleaseCapture ();
                        mouseCaptured = false;
                }

                lastMousePositionValid = false;

                CView::OnMouseMove(nFlags, point);
                return;
        }

        if (!mouseCaptured)
        {
                SetCapture ();
                mouseCaptured = true;
        }

        if (!lastMousePositionValid)
        {
                lastMousePosition = point;
                lastMousePositionValid = true;
        }

        const int deltaX = point.x - lastMousePosition.x;
        const int deltaY = point.y - lastMousePosition.y;

        if (leftButtonDown)
        {
                CRect rect;
                GetClientRect (&rect);

                if (rect.Width () > 0 && rect.Height () > 0)
                {
                        const real yawMod = PI_TIMES_2 * static_cast<real> (deltaX) / static_cast<real> (rect.Width ());
                        yaw += yawMod;

                        const real pitchMod = PI_TIMES_2 * static_cast<real> (deltaY) / static_cast<real> (rect.Height ());
                        pitch += pitchMod;
                        const real maxPitch = PI_OVER_2 - 0.01f;
                        pitch = clamp (-maxPitch, pitch, maxPitch);

                        while (yaw > PI_TIMES_2)
                                yaw -= PI_TIMES_2;

                        while (yaw < 0)
                                yaw += PI_TIMES_2;
                }
        }

        if (rightButtonDown || middleButtonDown)
        {
                const OrbitBasis basis = calculateOrbitBasis (yaw, pitch);

                Vector planarRight = basis.right;
                planarRight.y = 0.f;
                if (!planarRight.normalize ())
                        planarRight = basis.right;

                Vector planarForward = basis.forward;
                planarForward.y = 0.f;
                if (!planarForward.normalize ())
                        planarForward = basis.forward;

                real panScalar = orbitDistance * 0.01f;
                if (keyDown (VK_SHIFT))
                        panScalar *= 0.25f;

                orbitTarget -= planarRight * (static_cast<real> (deltaX) * panScalar);
                orbitTarget += planarForward * (static_cast<real> (deltaY) * panScalar);
        }

        lastMousePosition = point;

        Invalidate ();

        CView::OnMouseMove(nFlags, point);
}  //lint !e1746  //-- point could have been made a const reference

//-------------------------------------------------------------------

BOOL View3dView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
        UNREF (nFlags);
        UNREF (pt);

        real zoomBase = std::max (orbitDistance * 0.1f, minOrbitDistance * 0.5f);

        if (keyDown (VK_SHIFT))
                zoomBase *= 0.5f;

        if (keyDown (VK_CONTROL))
                zoomBase *= 0.25f;

        if (zDelta > 0)
                orbitDistance -= zoomBase;
        else if (zDelta < 0)
                orbitDistance += zoomBase;

        orbitDistance = clamp (minOrbitDistance, orbitDistance, maxOrbitDistance);

        Invalidate ();

        return TRUE;
}

//-------------------------------------------------------------------

void View3dView::OnSize(UINT nType, int cx, int cy)
{
	if (camera && cx && cy)
		camera->setViewport(0, 0, cx, cy);

	CView::OnSize(nType, cx, cy);

	Invalidate ();
}

//-------------------------------------------------------------------

BOOL View3dView::OnEraseBkgnd(CDC* pDC) 
{
	NOT_NULL (terrain);

	if (terrain->getAppearance ())
		return TRUE; 

	return CView::OnEraseBkgnd (pDC);	
}

//-------------------------------------------------------------------

void View3dView::OnDestroy()
{
        CView::OnDestroy();

        IGNORE_RETURN (KillTimer (static_cast<int> (timer)));

        // TODO: Add your message handler code here
        if (mouseCaptured)
        {
                ReleaseCapture ();
                mouseCaptured = false;
                lastMousePositionValid = false;
        }

        if (keyLight)
        {
                keyLight->removeFromWorld ();
                delete keyLight;
                keyLight = 0;
        }

        if (ambientLight)
        {
                ambientLight->removeFromWorld ();
                delete ambientLight;
                ambientLight = 0;
        }

        delete camera;
        camera = 0;
}

//-------------------------------------------------------------------

void View3dView::OnTimer(UINT nIDEvent)
{
        // TODO: Add your message handler code here and/or call default
        if (nIDEvent == timer)
                elapsedTime += RECIP (static_cast<float> (milliseconds));

	CView::OnTimer(nIDEvent);

	Invalidate ();
}

//-------------------------------------------------------------------

void View3dView::OnRefresh() 
{
	const TerrainEditorDoc* doc = dynamic_cast<const TerrainEditorDoc*> (GetDocument ());
	NOT_NULL (doc);

	Iff iff (10000);
	doc->save (iff, 0);
        iff.allowNonlinearFunctions ();
        iff.goToTopOfForm ();

        NOT_NULL (terrain);
        terrain->setAppearance (0);

        const AppearanceTemplate* at = AppearanceTemplateList::fetch (&iff);
        terrain->setAppearance (at->createAppearance ());
        AppearanceTemplateList::release (at);

        Vector2d center;
        center.makeZero ();
        if (doc->getMapFrame ())
        {
                center = doc->getMapFrame ()->getCenter ();
                orbitTarget.x = static_cast<real> (center.x);
                orbitTarget.z = static_cast<real> (center.y);
        }

        orbitTarget.y = 0.f;

        configureCameraForDocument (false);
        lastMousePositionValid = false;
        render = true;

        applyCameraTransform ();

        Invalidate ();
}

//-------------------------------------------------------------------

void View3dView::OnSetFocus(CWnd* pOldWnd)
{
        CView::OnSetFocus(pOldWnd);

        render = true;
        lastMousePositionValid = false;
}

//-------------------------------------------------------------------

void View3dView::OnKillFocus(CWnd* pNewWnd)
{
        CView::OnKillFocus(pNewWnd);

        render = false;

        if (mouseCaptured)
        {
                ReleaseCapture ();
                mouseCaptured = false;
                lastMousePositionValid = false;
        }
}

//-------------------------------------------------------------------

void View3dView::applyCameraTransform()
{
        if (!camera)
                return;

        const OrbitBasis basis = calculateOrbitBasis (yaw, pitch);

        camera->resetRotate_o2p ();
        camera->yaw_o (yaw);
        camera->pitch_o (pitch);

        Vector desiredTarget = orbitTarget;

        if (terrain && terrain->getAppearance ())
        {
                real sampledHeight = 0.f;
                if (terrain->getHeight (desiredTarget, sampledHeight))
                        desiredTarget.y = desiredTarget.y * (1.f - cs_targetHeightBlend) + sampledHeight * cs_targetHeightBlend;
        }

        real constrainedDistance = clamp (minOrbitDistance, orbitDistance, maxOrbitDistance);

        const real verticalLift = std::max (verticalOffset, constrainedDistance * 0.15f);
        Vector cameraPosition = desiredTarget - basis.forward * constrainedDistance + basis.up * verticalLift;

        if (terrain && terrain->getAppearance ())
        {
                real sampledCameraHeight = 0.f;
                if (terrain->getHeight (cameraPosition, sampledCameraHeight))
                {
                        const real minHeight = sampledCameraHeight + cs_minimumVerticalClearance;
                        if (cameraPosition.y < minHeight)
                                cameraPosition.y = minHeight;
                }
        }

        orbitTarget = desiredTarget;
        orbitDistance = constrainedDistance;

        camera->setPosition_p (cameraPosition);

        const real adaptiveFov = clamp (0.45f, 1.25f, 0.55f + (orbitDistance * 0.0004f));
        camera->setHorizontalFieldOfView (adaptiveFov);
}

//-------------------------------------------------------------------

void View3dView::handleKeyboardNavigation(real frameTime)
{
        if (!camera)
                return;

        const OrbitBasis basis = calculateOrbitBasis (yaw, pitch);

        Vector planarForward = basis.forward;
        planarForward.y = 0.f;
        if (!planarForward.normalize ())
                planarForward = basis.forward;

        Vector planarRight = basis.right;
        planarRight.y = 0.f;
        if (!planarRight.normalize ())
                planarRight = basis.right;

        real moveSpeed = std::max (orbitDistance * 0.25f, minOrbitDistance) * std::max (frameTime, 0.02f);

        if (keyDown (VK_SHIFT))
                moveSpeed *= 2.f;
        if (keyDown (VK_CONTROL))
                moveSpeed *= 0.35f;

        if (keyDown ('W'))
                orbitTarget += planarForward * moveSpeed;

        if (keyDown ('S'))
                orbitTarget -= planarForward * moveSpeed;

        if (keyDown ('A'))
                orbitTarget -= planarRight * moveSpeed;

        if (keyDown ('D'))
                orbitTarget += planarRight * moveSpeed;

        if (keyDown ('Q'))
                orbitTarget.y += moveSpeed;

        if (keyDown ('E'))
                orbitTarget.y -= moveSpeed;
}

//-------------------------------------------------------------------

void View3dView::updateOrbitTargetFromDocument()
{
        const TerrainEditorDoc* doc = dynamic_cast<const TerrainEditorDoc*> (GetDocument ());
        if (!doc)
                return;

        Vector2d center;
        center.makeZero ();

        if (doc->getMapFrame ())
        {
                center = doc->getMapFrame ()->getCenter ();
                orbitTarget.x = static_cast<real> (center.x);
                orbitTarget.z = static_cast<real> (center.y);
        }

        orbitTarget.y = 0.f;
}

//-------------------------------------------------------------------

void View3dView::configureCameraForDocument(bool resetAngles)
{
        const TerrainEditorDoc* doc = dynamic_cast<const TerrainEditorDoc*> (GetDocument ());

        if (!doc)
                return;

        const real mapWidth = doc->getMapWidthInMeters ();

        if (mapWidth > CONST_REAL (0.0))
        {
                const real recommendedDistance = std::max (minOrbitDistance * CONST_REAL (2.0), mapWidth * CONST_REAL (0.6));
                maxOrbitDistance = std::max (maxOrbitDistance, recommendedDistance * CONST_REAL (1.5));
                orbitDistance = clamp (minOrbitDistance, recommendedDistance, maxOrbitDistance);
                verticalOffset = std::max (verticalOffset, recommendedDistance * CONST_REAL (0.15));
        }

        if (resetAngles)
        {
                yaw = PI_OVER_4;
                pitch = -PI_OVER_4;
        }
}

//-------------------------------------------------------------------

