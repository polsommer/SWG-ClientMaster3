#include "FirstTerrainEditor.h"
#include "SmartTerrainAnalyzer.h"

#include "TerrainEditorDoc.h"
#include "sharedTerrain/TerrainGenerator.h"

#include <algorithm>
#include <numeric>
#include <vector>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace
{
        struct LayerStats
        {
                int totalLayers;
                int activeLayers;
                int inactiveLayers;
                int totalBoundaries;
                int totalFilters;
                int totalAffectors;
                int maxDepth;

                std::vector<CString> dormantLayers;
                std::vector<CString> emptyLayers;
                std::vector<CString> boundaryFreeLayers;
                std::vector<CString> heroLayers;

                LayerStats()
                        : totalLayers(0)
                        , activeLayers(0)
                        , inactiveLayers(0)
                        , totalBoundaries(0)
                        , totalFilters(0)
                        , totalAffectors(0)
                        , maxDepth(0)
                {
                }
        };

        static CString makeLayerName(const TerrainGenerator::Layer &layer)
        {
                const char *const rawName = layer.getName();
                if (rawName && *rawName)
                        return CString(rawName);

                CString generatedName;
                generatedName.Format(_T("Layer_%p"), reinterpret_cast<const void*>(&layer));
                return generatedName;
        }

        static void collectLayerStats(const TerrainGenerator::Layer &layer, int depth, LayerStats &stats)
        {
                stats.totalLayers++;
                stats.maxDepth = std::max(stats.maxDepth, depth);

                const CString layerName = makeLayerName(layer);

                if (layer.isActive())
                        stats.activeLayers++;
                else
                        stats.inactiveLayers++, stats.dormantLayers.push_back(layerName);

                const int boundaryCount = layer.getNumberOfBoundaries();
                const int filterCount = layer.getNumberOfFilters();
                const int affectorCount = layer.getNumberOfAffectors();
                const int subLayerCount = layer.getNumberOfLayers();

                stats.totalBoundaries += boundaryCount;
                stats.totalFilters += filterCount;
                stats.totalAffectors += affectorCount;

                if (affectorCount == 0 && subLayerCount == 0)
                        stats.emptyLayers.push_back(layerName);

                if (boundaryCount == 0 && filterCount == 0 && affectorCount > 0)
                        stats.boundaryFreeLayers.push_back(layerName);

                if (affectorCount >= 6 || (filterCount >= 4 && boundaryCount >= 4))
                        stats.heroLayers.push_back(layerName);

                for (int i = 0; i < subLayerCount; ++i)
                {
                        const TerrainGenerator::Layer *const child = layer.getLayer(i);
                        if (child)
                                collectLayerStats(*child, depth + 1, stats);
                }
        }

        static CString joinNames(const std::vector<CString> &names)
        {
                if (names.empty())
                        return CString();

                CString joined;
                for (size_t i = 0; i < names.size(); ++i)
                {
                        if (i > 0)
                        {
                                if (i + 1 == names.size())
                                        joined += _T(" and ");
                                else
                                        joined += _T(", ");
                        }
                        joined += names[i];
                }

                return joined;
        }

        static CString buildGauge(float score)
        {
                const int segments = 20;
                const int filled = static_cast<int>(std::max(0.0f, std::min(1.0f, score / 100.0f)) * segments + 0.5f);

                CString gauge(_T("["));
                for (int i = 0; i < segments; ++i)
                        gauge += (i < filled) ? _T('#') : _T('.');
                gauge += _T(']');
                return gauge;
        }

        static CString formatPlural(int value, const TCHAR *noun)
        {
                CString formatted;
                formatted.Format(_T("%d %s%s"), value, noun, (value == 1) ? _T("") : _T("s"));
                return formatted;
        }
}

//----------------------------------------------------------------------

CString SmartTerrainAnalyzer::runAudit(const TerrainEditorDoc &doc)
{
        const TerrainGenerator *const generator = doc.getTerrainGenerator();
        if (!generator)
                return CString(_T("[Terrain Intelligence]\r\nNo terrain data is loaded."));

        LayerStats stats;
        const int topLevelLayers = generator->getNumberOfLayers();
        for (int i = 0; i < topLevelLayers; ++i)
        {
                const TerrainGenerator::Layer *const layer = generator->getLayer(i);
                if (layer)
                        collectLayerStats(*layer, 0, stats);
        }

        const float mapWidth = doc.getMapWidthInMeters();
        const float chunkWidth = doc.getChunkWidthInMeters();
        const float tileWidth = doc.getTileWidthInMeters();

        const int shaderFamilies = generator->getShaderGroup().getNumberOfFamilies();
        const int floraFamilies = generator->getFloraGroup().getNumberOfFamilies();
        const int radialFamilies = generator->getRadialGroup().getNumberOfFamilies();
        const int environmentFamilies = generator->getEnvironmentGroup().getNumberOfFamilies();

        float score = 40.0f;
        if (stats.activeLayers > 0)
                score += std::min(20.0f, static_cast<float>(stats.activeLayers) * 2.0f);
        else
                score -= 15.0f;

        if (stats.inactiveLayers == 0)
                score += 5.0f;
        else
                score -= std::min(10.0f, static_cast<float>(stats.inactiveLayers) * 1.5f);

        if (stats.totalFilters + stats.totalBoundaries > 0)
                score += 5.0f;

        score += std::min(10.0f, static_cast<float>(stats.heroLayers.size()) * 2.5f);

        if (shaderFamilies >= 4)
                score += 5.0f;
        else if (shaderFamilies <= 1)
                score -= 5.0f;

        if (floraFamilies >= 3)
                score += 3.0f;

        if (!generator->hasPassableAffectors())
                score -= 4.0f;

        if (doc.getUseGlobalWaterTable())
                score += 2.0f;

        if (stats.totalLayers == 0)
                score = 5.0f;

        score = std::max(0.0f, std::min(100.0f, score));

        CString output;
        output += _T("[Terrain Intelligence Audit]\r\n");

        CString scoreLine;
        scoreLine.Format(_T("Foresight score: %0.1f/100 %s\r\n"), score, buildGauge(score).GetString());
        output += scoreLine;

        CString mapLine;
        mapLine.Format(_T("Map: %0.0fm square • chunk width %0.0fm • tile width %.2fm\r\n"), mapWidth, chunkWidth, tileWidth);
        output += mapLine;

        CString layerLine;
        layerLine.Format(_T("Layers: %s, %s active (%0.0f%%), %s resting • depth %d\r\n"),
                formatPlural(stats.totalLayers, _T("layer")).GetString(),
                formatPlural(stats.activeLayers, _T("layer")).GetString(),
                (stats.totalLayers > 0) ? (100.0f * static_cast<float>(stats.activeLayers) / static_cast<float>(stats.totalLayers)) : 0.0f,
                formatPlural(stats.inactiveLayers, _T("layer")).GetString(),
                stats.maxDepth + 1);
        output += layerLine;

        CString compositionLine;
        compositionLine.Format(_T("Composition: %s, %s, %s affectors\r\n"),
                formatPlural(stats.totalBoundaries, _T("boundary")).GetString(),
                formatPlural(stats.totalFilters, _T("filter")).GetString(),
                formatPlural(stats.totalAffectors, _T("affector")).GetString());
        output += compositionLine;

        CString libraryLine;
        libraryLine.Format(_T("Libraries: %d shader families • %d flora families • %d radial sets • %d environment profiles\r\n"),
                shaderFamilies, floraFamilies, radialFamilies, environmentFamilies);
        output += libraryLine;

        output += _T("\r\nOpportunities:\r\n");

        if (!stats.dormantLayers.empty())
        {
                CString message;
                message.Format(_T("- Reactivate or archive dormant layers: %s\r\n"), joinNames(stats.dormantLayers).GetString());
                output += message;
        }
        else
        {
                output += _T("- Every layer is live – great job keeping the stack lean.\r\n");
        }

        if (!stats.emptyLayers.empty())
        {
                CString message;
                message.Format(_T("- These layers have no affectors: %s. Consider wiring them up or pruning them.\r\n"),
                        joinNames(stats.emptyLayers).GetString());
                output += message;
        }

        if (!stats.boundaryFreeLayers.empty())
        {
                CString message;
                message.Format(_T("- Layers %s sculpt without boundaries/filters – add guidance to focus their influence.\r\n"),
                        joinNames(stats.boundaryFreeLayers).GetString());
                output += message;
        }

        if (!stats.heroLayers.empty())
        {
                CString message;
                message.Format(_T("- Hero layers %s carry complex logic – snapshot them before large edits.\r\n"),
                        joinNames(stats.heroLayers).GetString());
                output += message;
        }

        if (!generator->hasPassableAffectors())
        {
                output += _T("- No passable affectors detected. Add one to guide navigation meshes.\r\n");
        }

        if (shaderFamilies < 2)
        {
                output += _T("- Expand the shader library for richer biome blending.\r\n");
        }

        if (!doc.getUseGlobalWaterTable())
        {
                output += _T("- Global water table disabled – enable it to unlock reflective previews if your world needs water.\r\n");
        }

        const real totalGenerationTime = doc.getLastTotalChunkGenerationTime();
        if (totalGenerationTime > CONST_REAL(0))
        {
                CString perfLine;
                perfLine.Format(_T("- Recent chunk generation took %.2fs (avg %.2fs). Profile heavy layers if editing feels sluggish.\r\n"),
                        totalGenerationTime,
                        doc.getLastAverageChunkGenerationTime());
                output += perfLine;
        }

        output += _T("\r\nNext experiments:\r\n");
        output += _T("• Use the AI audit regularly to track progress while modernising legacy maps.\r\n");
        output += _T("• Combine terrain intelligence with the console's search tools to jump straight to the suggested layers.\r\n");

        return output;
}

//----------------------------------------------------------------------
