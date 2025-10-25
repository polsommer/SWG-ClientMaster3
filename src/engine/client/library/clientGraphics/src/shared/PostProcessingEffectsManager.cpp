// ======================================================================
//
// PostProcessingEffectsManager.cpp
// Copyright 2004, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#include "clientGraphics/FirstClientGraphics.h"
#include "clientGraphics/PostProcessingEffectsManager.h"

#include "clientGraphics/DynamicVertexBuffer.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/GraphicsOptionTags.h"
#include "clientGraphics/LensPostProcessing.h"
#include "clientGraphics/ShaderCapability.h"
#include "clientGraphics/ShaderTemplateList.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/Texture.h"
#include "clientGraphics/TextureList.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedDebug/PerformanceTimer.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/FloatMath.h"
#include "sharedMath/VectorRgba.h"
#include "sharedUtility/LocalMachineOptionManager.h"

// ======================================================================

namespace PostProcessingEffectsManagerNamespace
{
void deviceRestored();
void deviceLost();

bool ms_enable = true;
bool ms_enabled;
bool ms_enableLensArtifacts = true;
bool ms_enableColorGrading = true;

Texture * ms_primaryBuffer;
Texture * ms_secondaryBuffer;
Texture * ms_tertiaryBuffer;
StaticShader * ms_copyShader;

StaticShader * ms_heatCompositingShader = NULL;

bool ms_antialiasEnabled = false;

float ms_lensArtifactBudgetMilliseconds = 4.0f;
int ms_lensArtifactOverBudgetCount = 0;
int const ms_lensArtifactOverBudgetLimit = 3;
bool ms_lensArtifactsAutoDisabled = false;

float ms_chromaticAberrationStrength = 0.0025f;
float ms_lensFlareStrength = 0.12f;
float ms_lensStreakStrength = 0.06f;
float ms_vignetteStrength = 0.18f;
float ms_colorGradeStrength = 0.8f;
float ms_colorGradeContrast = 1.12f;
float ms_colorGradeSaturation = 1.08f;
float ms_colorGradeTintStrength = 0.35f;
}

using namespace PostProcessingEffectsManagerNamespace;

// ======================================================================

void PostProcessingEffectsManager::install()
{
        InstallTimer const installTimer("PostProcessingEffectsManager::install");

        ExitChain::add(PostProcessingEffectsManager::remove, "PostProcessingEffectsManager::remove");

        LocalMachineOptionManager::registerOption(ms_enable, "ClientGraphics/PostProcessingEffectsManager", "enable");
        LocalMachineOptionManager::registerOption(ms_enableLensArtifacts, "ClientGraphics/PostProcessingEffects", "enableLensArtifacts");
        LocalMachineOptionManager::registerOption(ms_antialiasEnabled, "ClientGraphics/Antialias", "enable");
        LocalMachineOptionManager::registerOption(ms_enableColorGrading, "ClientGraphics/PostProcessingEffects", "enableColorGrading");

#ifdef _DEBUG
        DebugFlags::registerFlag(ms_enable, "ClientGraphics/PostProcessingEffectsManager", "enabled");
        DebugFlags::registerFlag(ms_enableLensArtifacts, "ClientGraphics/PostProcessingEffects", "enableLensArtifacts");
        DebugFlags::registerFlag(ms_enableColorGrading, "ClientGraphics/PostProcessingEffects", "enableColorGrading");
#endif

        ms_chromaticAberrationStrength = ConfigFile::getKeyFloat("ClientGraphics/PostProcessingEffects", "chromaticAberrationStrength", ms_chromaticAberrationStrength);
        ms_lensFlareStrength = ConfigFile::getKeyFloat("ClientGraphics/PostProcessingEffects", "lensFlareStrength", ms_lensFlareStrength);
        ms_lensStreakStrength = ConfigFile::getKeyFloat("ClientGraphics/PostProcessingEffects", "lensStreakStrength", ms_lensStreakStrength);
        ms_vignetteStrength = ConfigFile::getKeyFloat("ClientGraphics/PostProcessingEffects", "vignetteStrength", ms_vignetteStrength);
        ms_colorGradeStrength = ConfigFile::getKeyFloat("ClientGraphics/PostProcessingEffects", "colorGradeStrength", ms_colorGradeStrength);
        ms_colorGradeContrast = ConfigFile::getKeyFloat("ClientGraphics/PostProcessingEffects", "colorGradeContrast", ms_colorGradeContrast);
        ms_colorGradeSaturation = ConfigFile::getKeyFloat("ClientGraphics/PostProcessingEffects", "colorGradeSaturation", ms_colorGradeSaturation);
        ms_colorGradeTintStrength = ConfigFile::getKeyFloat("ClientGraphics/PostProcessingEffects", "colorGradeTintStrength", ms_colorGradeTintStrength);
        ms_lensArtifactBudgetMilliseconds = ConfigFile::getKeyFloat("ClientGraphics/PostProcessingEffects", "lensArtifactBudgetMs", ms_lensArtifactBudgetMilliseconds);
        if (ms_lensArtifactBudgetMilliseconds < 0.0f)
                ms_lensArtifactBudgetMilliseconds = 0.0f;

        if (ms_enable)
                enable();
        setAntialiasEnabled(ms_antialiasEnabled);
}

// ----------------------------------------------------------------------

void PostProcessingEffectsManager::remove()
{
	disable();
}

//----------------------------------------------------------------------

bool PostProcessingEffectsManager::isSupported()
{
	return GraphicsOptionTags::get(TAG(P,O,S,T)) && Graphics::getShaderCapability() >= ShaderCapability(2,0);
}

// ----------------------------------------------------------------------

bool PostProcessingEffectsManager::isEnabled()
{
	return ms_enable;
}

// ----------------------------------------------------------------------

void PostProcessingEffectsManager::setEnabled(bool const enable)
{
	ms_enable = enable;
}

// ----------------------------------------------------------------------

void PostProcessingEffectsManager::enable()
{
	if (!ms_enabled)
	{
		if (PostProcessingEffectsManager::isSupported())
		{
			Graphics::addDeviceLostCallback(PostProcessingEffectsManagerNamespace::deviceLost);
			Graphics::addDeviceRestoredCallback(PostProcessingEffectsManagerNamespace::deviceRestored);
			deviceRestored();
			ms_enabled = true;
		}
		else
		{
			ms_enable = false;
			ms_enabled = false;
		}
	}
}

// ----------------------------------------------------------------------

void PostProcessingEffectsManager::disable()
{
	if (ms_enabled)
	{
		deviceLost();
		Graphics::removeDeviceLostCallback(deviceLost);
		Graphics::removeDeviceRestoredCallback(deviceRestored);
		
		ms_enable = false;
		ms_enabled = false;
	}
}

// ----------------------------------------------------------------------

void PostProcessingEffectsManagerNamespace::deviceRestored()
{
	TextureFormat formats[] = { TF_ARGB_8888 };
	
	ms_primaryBuffer   = TextureList::fetch(TCF_renderTarget, Graphics::getFrameBufferMaxWidth(), Graphics::getFrameBufferMaxHeight(), 1, formats, sizeof(formats) / sizeof(formats[0]));
	ms_secondaryBuffer = TextureList::fetch(TCF_renderTarget, Graphics::getFrameBufferMaxWidth(), Graphics::getFrameBufferMaxHeight(), 1, formats, sizeof(formats) / sizeof(formats[0]));
	ms_tertiaryBuffer  = TextureList::fetch(TCF_renderTarget, Graphics::getFrameBufferMaxWidth(), Graphics::getFrameBufferMaxHeight(), 1, formats, sizeof(formats) / sizeof(formats[0]));
	ms_copyShader            = dynamic_cast<StaticShader *>(ShaderTemplateList::fetchModifiableShader("shader/2d_texture.sht"));
	ms_heatCompositingShader = dynamic_cast<StaticShader *>(ShaderTemplateList::fetchModifiableShader("shader/2d_heat_composite.sht"));
}

// ----------------------------------------------------------------------

void PostProcessingEffectsManagerNamespace::deviceLost()
{
	if (ms_primaryBuffer)
	{
		ms_primaryBuffer->release();
		ms_primaryBuffer = NULL;
	}
	
	if (ms_secondaryBuffer)
	{
		ms_secondaryBuffer->release();
		ms_secondaryBuffer = NULL;
	}

	if (ms_tertiaryBuffer)
	{
		ms_tertiaryBuffer->release();
		ms_tertiaryBuffer = NULL;
	}

	if (ms_copyShader)
	{
		ms_copyShader->release();
		ms_copyShader= NULL;
	}

	if (ms_heatCompositingShader)
	{
		ms_heatCompositingShader->release();
		ms_heatCompositingShader= NULL;
	}
}

// ----------------------------------------------------------------------

void PostProcessingEffectsManager::preSceneRender()
{
	// handle switching between PostProcessingEffectsManager enabled & disabled
	if (ms_enabled && !ms_enable)
		disable();
	else if (!ms_enabled && ms_enable)
		enable();
		
	if (ms_enabled)
	{
		Graphics::setRenderTarget(ms_primaryBuffer, CF_none, 0);
		DEBUG_FATAL(ms_primaryBuffer->getWidth() != Graphics::getCurrentRenderTargetWidth(),("rendertarget and big widths do not match"));
		DEBUG_FATAL(ms_primaryBuffer->getHeight() != Graphics::getCurrentRenderTargetHeight(),("rendertarget and big heights do not match"));
	}
	
	Graphics::setViewport(0, 0, Graphics::getCurrentRenderTargetWidth(), Graphics::getCurrentRenderTargetHeight(), 0.0f, 1.0f);
}

// ----------------------------------------------------------------------

void PostProcessingEffectsManager::postSceneRender()
{
        if (ms_enabled)
        {
                if (ms_enableLensArtifacts && ms_lensArtifactsAutoDisabled)
                {
                        ms_lensArtifactsAutoDisabled = false;
                        ms_lensArtifactOverBudgetCount = 0;
                }

                VertexBufferFormat format;
                format.setPosition();
                format.setTransformed();
                format.setNumberOfTextureCoordinateSets(1);
                format.setTextureCoordinateSetDimension(0, 2);

                int destinationWidth = ms_primaryBuffer->getWidth();
                int destinationHeight = ms_primaryBuffer->getHeight();

                if (ms_enableLensArtifacts)
                {
                        PerformanceTimer timer;
                        timer.start();

                        ms_secondaryBuffer->copyFrom(0, *ms_primaryBuffer, 0, 0, destinationWidth, destinationHeight, 0, 0, destinationWidth, destinationHeight);
                        LensPostProcessing::apply(*ms_primaryBuffer, *ms_secondaryBuffer, ms_chromaticAberrationStrength, ms_lensFlareStrength, ms_lensStreakStrength, ms_vignetteStrength, ms_enableColorGrading, ms_colorGradeStrength, ms_colorGradeContrast, ms_colorGradeSaturation, ms_colorGradeTintStrength);

                        timer.stop();
                        float const elapsedMs = timer.getElapsedTime() * 1000.0f;
                        if (elapsedMs > ms_lensArtifactBudgetMilliseconds)
                        {
                                ++ms_lensArtifactOverBudgetCount;
                                if (ms_lensArtifactOverBudgetCount >= ms_lensArtifactOverBudgetLimit && !ms_lensArtifactsAutoDisabled)
                                {
                                        DEBUG_REPORT_LOG(true, ("PostProcessingEffectsManager: disabling lens artifacts after %.2fms exceeded %.2fms budget for %d consecutive frames.\n", elapsedMs, ms_lensArtifactBudgetMilliseconds, ms_lensArtifactOverBudgetLimit));
                                        ms_enableLensArtifacts = false;
                                        ms_lensArtifactsAutoDisabled = true;
                                }
                        }
                        else
                        {
                                ms_lensArtifactOverBudgetCount = 0;
                        }
                }
                else
                {
                        ms_lensArtifactOverBudgetCount = 0;
                }

                // copy the back buffer to the frame buffer
                DynamicVertexBuffer vertexBuffer(format);
		
		vertexBuffer.lock(4);
		{
			VertexBufferWriteIterator v = vertexBuffer.begin();
			
			v.setPosition(Vector(0.0f - 0.5f, 0.0f - 0.5f, 1.f));
			v.setOoz(1.f);
			v.setTextureCoordinates(0, 0.0f, 0.0f);
			++v;
			
			v.setPosition(Vector(static_cast<float>(destinationWidth) - 0.5f, 0.0f - 0.5f, 1.f));
			v.setOoz(1.f);
			v.setTextureCoordinates(0, 1.0f, 0.0f);
			++v;
			
			v.setPosition(Vector(static_cast<float>(destinationWidth) - 0.5f, static_cast<float>(destinationHeight) - 0.5f, 1.f));
			v.setOoz(1.f);
			v.setTextureCoordinates(0, 1.0f, 1.0f);
			++v;
			
			v.setPosition(Vector(0.0f - 0.5f, static_cast<float>(destinationHeight) - 0.5f, 1.f));
			v.setOoz(1.f);
			v.setTextureCoordinates(0, 0.0f, 1.0f);
		}
		vertexBuffer.unlock();
		
		ms_copyShader->setTexture(TAG(M,A,I,N), *ms_primaryBuffer);
		Graphics::setRenderTarget(NULL, CF_none, 0);
		Graphics::setViewport(0, 0, destinationWidth, destinationHeight, 0.0f, 1.0f);
		Graphics::setVertexBuffer(vertexBuffer);
		Graphics::setStaticShader(*ms_copyShader);

		GlFillMode const fillMode = Graphics::getFillMode();
		Graphics::setFillMode(GFM_solid);

		Graphics::drawTriangleFan();

		Graphics::setFillMode(fillMode);

	}
}

//----------------------------------------------------------------------

Texture * PostProcessingEffectsManager::getPrimaryBuffer()
{
	return ms_primaryBuffer;
}

//----------------------------------------------------------------------

Texture * PostProcessingEffectsManager::getSecondaryBuffer()
{
	return ms_secondaryBuffer;
}

//----------------------------------------------------------------------

Texture * PostProcessingEffectsManager::getTertiaryBuffer()
{
	return ms_tertiaryBuffer;
}

//----------------------------------------------------------------------

void PostProcessingEffectsManager::swapBuffers()
{
	Texture * tmp = ms_secondaryBuffer;
	ms_secondaryBuffer = ms_primaryBuffer;
	ms_primaryBuffer = tmp;
}

//----------------------------------------------------------------------

StaticShader * PostProcessingEffectsManager::getHeatCompositingShader()
{
	return ms_heatCompositingShader;
}

//----------------------------------------------------------------------

void PostProcessingEffectsManager::setAntialiasEnabled(bool enabled)
{
	if(enabled && !Graphics::supportsAntialias())
		enabled = false;
	ms_antialiasEnabled = enabled;
	Graphics::setAntialiasEnabled(enabled);
}

//----------------------------------------------------------------------

bool PostProcessingEffectsManager::getAntialiasEnabled()
{
	return ms_antialiasEnabled;
}

// ======================================================================
