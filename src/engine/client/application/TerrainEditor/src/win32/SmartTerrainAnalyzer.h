#pragma once

//----------------------------------------------------------------------
//
// SmartTerrainAnalyzer
// A lightweight analytic helper that inspects the currently loaded
// terrain generator and produces human readable insights for the
// modernised Terrain Editor experience.
//
//----------------------------------------------------------------------

#include "FirstTerrainEditor.h"

class TerrainEditorDoc;

class SmartTerrainAnalyzer
{
public:
        struct Insight
        {
                CString headline;
                CString detail;
                float   confidence;
        };

public:
        static CString runAudit(const TerrainEditorDoc &doc);
};

//----------------------------------------------------------------------
