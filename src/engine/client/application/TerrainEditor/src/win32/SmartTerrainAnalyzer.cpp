#include "FirstTerrainEditor.h"
#include "SmartTerrainAnalyzer.h"

#include "TerrainEditorDoc.h"
#include "sharedTerrain/TerrainGenerator.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
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

        static float clampScore(float score)
        {
                return std::max(0.0f, std::min(100.0f, score));
        }

        static float clamp01(float value)
        {
                return std::max(0.0f, std::min(1.0f, value));
        }

        static float normalise(float value, float reference)
        {
                if (reference <= 0.0f)
                        return 0.0f;

                return clamp01(value / reference);
        }

        static CString formatPercent(float value)
        {
                CString formatted;
                formatted.Format(_T("%0.0f%%"), clamp01(value) * 100.0f);
                return formatted;
        }
}

//----------------------------------------------------------------------

SmartTerrainAnalyzer::AuditReport SmartTerrainAnalyzer::analyze(const TerrainEditorDoc &doc)
{
        AuditReport report = {};

        const TerrainGenerator *const generator = doc.getTerrainGenerator();
        if (!generator)
        {
                report.hasGlobalWaterTable = doc.getUseGlobalWaterTable();
                report.environmentCycleMinutes = doc.getEnvironmentCycleTime();
                return report;
        }

        LayerStats stats;
        const int topLevelLayers = generator->getNumberOfLayers();
        for (int i = 0; i < topLevelLayers; ++i)
        {
                const TerrainGenerator::Layer *const layer = generator->getLayer(i);
                if (layer)
                        collectLayerStats(*layer, 0, stats);
        }

        report.totalLayers = stats.totalLayers;
        report.activeLayers = stats.activeLayers;
        report.inactiveLayers = stats.inactiveLayers;
        report.totalBoundaries = stats.totalBoundaries;
        report.totalFilters = stats.totalFilters;
        report.totalAffectors = stats.totalAffectors;
        report.hierarchyDepth = stats.maxDepth + 1;

        report.shaderFamilies = generator->getShaderGroup().getNumberOfFamilies();
        report.floraFamilies = generator->getFloraGroup().getNumberOfFamilies();
        report.radialFamilies = generator->getRadialGroup().getNumberOfFamilies();
        report.environmentFamilies = generator->getEnvironmentGroup().getNumberOfFamilies();
        report.fractalFamilies = generator->getFractalGroup().getNumberOfFamilies();
        report.bitmapFamilies = generator->getBitmapGroup().getNumberOfFamilies();

        report.hasGlobalWaterTable = doc.getUseGlobalWaterTable();
        report.environmentCycleMinutes = doc.getEnvironmentCycleTime();

        report.dormantLayers = stats.dormantLayers;
        report.emptyLayers = stats.emptyLayers;
        report.boundaryFreeLayers = stats.boundaryFreeLayers;
        report.heroLayers = stats.heroLayers;

        const float activeRatio = (report.totalLayers > 0) ? static_cast<float>(report.activeLayers) / static_cast<float>(report.totalLayers) : 0.0f;
        const float boundaryDensity = (report.totalLayers > 0) ? static_cast<float>(report.totalBoundaries) / static_cast<float>(report.totalLayers) : 0.0f;
        const float filterDensity = (report.totalLayers > 0) ? static_cast<float>(report.totalFilters) / static_cast<float>(report.totalLayers) : 0.0f;
        const float affectorDensity = (report.totalLayers > 0) ? static_cast<float>(report.totalAffectors) / static_cast<float>(report.totalLayers) : 0.0f;

        float structureScore = 0.0f;
        if (report.totalLayers > 0)
        {
                structureScore += 30.0f * clamp01(activeRatio);
                structureScore += 15.0f * normalise(boundaryDensity, 2.0f);
                structureScore += 15.0f * normalise(filterDensity, 3.0f);
                structureScore += 20.0f * normalise(affectorDensity, 4.0f);
                structureScore += 10.0f * normalise(static_cast<float>(report.hierarchyDepth), 6.0f);
                structureScore += 10.0f * normalise(static_cast<float>(report.heroLayers.size()), 5.0f);
        }
        report.structureScore = clampScore(structureScore);

        float ecosystemScore = 0.0f;
        ecosystemScore += 20.0f * normalise(static_cast<float>(report.shaderFamilies), 6.0f);
        ecosystemScore += 15.0f * normalise(static_cast<float>(report.floraFamilies), 5.0f);
        ecosystemScore += 10.0f * normalise(static_cast<float>(report.radialFamilies), 4.0f);
        ecosystemScore += 10.0f * normalise(static_cast<float>(report.environmentFamilies), 4.0f);
        ecosystemScore += 20.0f * normalise(static_cast<float>(report.fractalFamilies), 8.0f);
        ecosystemScore += 10.0f * normalise(static_cast<float>(report.bitmapFamilies), 6.0f);
        ecosystemScore += report.hasGlobalWaterTable ? 5.0f : 0.0f;
        ecosystemScore += generator->hasPassableAffectors() ? 10.0f : 0.0f;
        report.ecosystemScore = clampScore(ecosystemScore);

        float workflowScore = 40.0f;
        const int environmentCycle = report.environmentCycleMinutes;
        if (environmentCycle >= 45 && environmentCycle <= 120)
                workflowScore += 20.0f;
        else if (environmentCycle >= 30 && environmentCycle < 45)
                workflowScore += 10.0f;
        else if (environmentCycle > 120 && environmentCycle <= 180)
                workflowScore += 5.0f;
        else
                workflowScore -= 5.0f;

        const float collidableRange = doc.getCollidableMaximumDistance() - doc.getCollidableMinimumDistance();
        workflowScore += 8.0f * normalise(collidableRange, std::max(1.0f, doc.getCollidableTileSize() * 2.0f));

        const float nonCollidableRange = doc.getNonCollidableMaximumDistance() - doc.getNonCollidableMinimumDistance();
        workflowScore += 6.0f * normalise(nonCollidableRange, std::max(1.0f, doc.getNonCollidableTileSize() * 2.0f));

        const float radialRange = doc.getRadialMaximumDistance() - doc.getRadialMinimumDistance();
        workflowScore += 6.0f * normalise(radialRange, std::max(1.0f, doc.getRadialTileSize() * 4.0f));

        const float farRadialRange = doc.getFarRadialMaximumDistance() - doc.getFarRadialMinimumDistance();
        workflowScore += 6.0f * normalise(farRadialRange, std::max(1.0f, doc.getFarRadialTileSize() * 4.0f));

        const float tileWidth = doc.getTileWidthInMeters();
        if (tileWidth >= 2.0f && tileWidth <= 8.0f)
                workflowScore += 6.0f;
        else if (tileWidth > 8.0f)
                workflowScore -= 4.0f;

        const std::set<uint32> uniqueSeeds = {
                doc.getCollidableSeed(),
                doc.getNonCollidableSeed(),
                doc.getRadialSeed(),
                doc.getFarRadialSeed()
        };
        switch (uniqueSeeds.size())
        {
        case 4:
                workflowScore += 12.0f;
                break;
        case 3:
                workflowScore += 8.0f;
                break;
        case 2:
                workflowScore += 4.0f;
                break;
        default:
                workflowScore -= 4.0f;
                break;
        }

        if (doc.getLastAverageChunkGenerationTime() > CONST_REAL(0))
        {
                const float averageTime = static_cast<float>(doc.getLastAverageChunkGenerationTime());
                workflowScore += (averageTime < 1.0f) ? 6.0f : ((averageTime < 2.5f) ? 3.0f : -2.0f);
        }

        report.workflowScore = clampScore(workflowScore);

        report.foresightScore = clampScore((report.structureScore + report.ecosystemScore + report.workflowScore) / 3.0f);

        if (!stats.dormantLayers.empty())
        {
                Insight insight;
                insight.headline = _T("Dormant layers detected");
                insight.detail.Format(_T("%d layer(s) are inactive: %s."), static_cast<int>(stats.dormantLayers.size()), joinNames(stats.dormantLayers).GetString());
                insight.confidence = 0.85f;
                report.insights.push_back(insight);

                BlueprintAction action;
                action.label = _T("Reactivate or archive dormant layers");
                action.rationale = insight.detail + _T(" Prioritise them or convert them into templates.");
                action.predictedImpact = 0.55f;
                action.confidence = 0.8f;
                report.blueprint.push_back(action);
        }

        if (!stats.emptyLayers.empty())
        {
                Insight insight;
                insight.headline = _T("Unwired layers");
                insight.detail.Format(_T("Layers without affectors: %s."), joinNames(stats.emptyLayers).GetString());
                insight.confidence = 0.7f;
                report.insights.push_back(insight);

                BlueprintAction action;
                action.label = _T("Wire empty layers with AI presets");
                action.rationale = _T("Let the AI assistant seed starter affectors for the listed layers to accelerate block-outs.");
                action.predictedImpact = 0.6f;
                action.confidence = 0.7f;
                report.blueprint.push_back(action);
        }

        if (!stats.boundaryFreeLayers.empty())
        {
                Insight insight;
                insight.headline = _T("Unguided sculpting layers");
                insight.detail.Format(_T("Layers %s operate without boundaries/filters."), joinNames(stats.boundaryFreeLayers).GetString());
                insight.confidence = 0.65f;
                report.insights.push_back(insight);

                BlueprintAction action;
                action.label = _T("Generate smart boundaries");
                action.rationale = _T("Use the AI spline tools to draft falloff masks around the highlighted layers for cleaner blending.");
                action.predictedImpact = 0.58f;
                action.confidence = 0.75f;
                report.blueprint.push_back(action);
        }

        if (!stats.heroLayers.empty())
        {
                Insight insight;
                insight.headline = _T("Hero layers carry heavy logic");
                insight.detail.Format(_T("High-complexity layers: %s."), joinNames(stats.heroLayers).GetString());
                insight.confidence = 0.75f;
                report.insights.push_back(insight);

                BlueprintAction action;
                action.label = _T("Snapshot hero layers");
                action.rationale = _T("Create AI-authored variants and backups before major tweaks to the hero stack.");
                action.predictedImpact = 0.45f;
                action.confidence = 0.8f;
                report.blueprint.push_back(action);
        }

        if (report.shaderFamilies < 3)
        {
                Insight insight;
                insight.headline = _T("Limited shader diversity");
                insight.detail.Format(_T("Only %d shader families are referenced – expand the palette for richer biomes."), report.shaderFamilies);
                insight.confidence = 0.8f;
                report.insights.push_back(insight);

                BlueprintAction action;
                action.label = _T("Synthesize new shader families");
                action.rationale = _T("Leverage the generative material mixer to author a broader shader set and update layer assignments.");
                action.predictedImpact = 0.62f;
                action.confidence = 0.78f;
                report.blueprint.push_back(action);
        }

        if (report.fractalFamilies < 4)
        {
                Insight insight;
                insight.headline = _T("Fractal library is compact");
                insight.detail.Format(_T("%d fractal families detected – diversify noise patterns for terrain variety."), report.fractalFamilies);
                insight.confidence = 0.7f;
                report.insights.push_back(insight);

                BlueprintAction action;
                action.label = _T("Train fractal variations");
                action.rationale = _T("Ask the AI toolkit to evolve new multifractal presets from in-game reference worlds.");
                action.predictedImpact = 0.57f;
                action.confidence = 0.72f;
                report.blueprint.push_back(action);
        }

        if (!report.hasGlobalWaterTable)
        {
                Insight insight;
                insight.headline = _T("Global water table disabled");
                insight.detail = _T("Enable the water table to unlock AI-calculated shorelines and reflections.");
                insight.confidence = 0.8f;
                report.insights.push_back(insight);

                BlueprintAction action;
                action.label = _T("Bootstrap water simulation");
                action.rationale = _T("Activate the global water table so the AI can propose river deltas and tidal flats.");
                action.predictedImpact = 0.5f;
                action.confidence = 0.82f;
                report.blueprint.push_back(action);
        }

        if (uniqueSeeds.size() < 4)
        {
                Insight insight;
                insight.headline = _T("Seed values recycled");
                insight.detail = _T("Random seeds for flora/radial passes repeat – randomise them to avoid visible tiling.");
                insight.confidence = 0.75f;
                report.insights.push_back(insight);

                BlueprintAction action;
                action.label = _T("Regenerate procedural seeds");
                action.rationale = _T("Use the AI seed foundry to roll distinct seeds per flora band and preview distribution heatmaps.");
                action.predictedImpact = 0.52f;
                action.confidence = 0.77f;
                report.blueprint.push_back(action);
        }

        if (report.workflowScore < 55.0f)
        {
                Insight insight;
                insight.headline = _T("Workflow optimisations available");
                insight.detail = _T("Tune cycle time, seeds and tile sizing to boost iteration speed.");
                insight.confidence = 0.7f;
                report.insights.push_back(insight);

                BlueprintAction action;
                action.label = _T("Enable guided iteration mode");
                action.rationale = _T("Let the AI orchestrate bake queues, background previews and autosaves while you sculpt.");
                action.predictedImpact = 0.6f;
                action.confidence = 0.74f;
                report.blueprint.push_back(action);
        }

        if (report.structureScore < 60.0f && report.totalLayers > 0)
        {
                Insight insight;
                insight.headline = _T("Structure could be reinforced");
                insight.detail = _T("Balance boundaries/filters across active layers to tighten control.");
                insight.confidence = 0.68f;
                report.insights.push_back(insight);

                BlueprintAction action;
                action.label = _T("Auto-balance layer stack");
                action.rationale = _T("Let the AI arranger propose boundary+filter combos per layer depth.");
                action.predictedImpact = 0.65f;
                action.confidence = 0.7f;
                report.blueprint.push_back(action);
        }

        return report;
}

//----------------------------------------------------------------------

static CString buildBlueprintOutput(const SmartTerrainAnalyzer::AuditReport &report)
{
        if (report.blueprint.empty())
                return CString(_T("- Blueprint queue is empty – the terrain already follows best practices.\r\n"));

        CString result;
        for (size_t i = 0; i < report.blueprint.size(); ++i)
        {
                const SmartTerrainAnalyzer::BlueprintAction &action = report.blueprint[i];
                CString line;
                line.Format(_T("%d. %s (impact %s, confidence %s)\r\n   %s\r\n"),
                        static_cast<int>(i + 1),
                        action.label.GetString(),
                        formatPercent(action.predictedImpact).GetString(),
                        formatPercent(action.confidence).GetString(),
                        action.rationale.GetString());
                result += line;
        }

        return result;
}

//----------------------------------------------------------------------

CString SmartTerrainAnalyzer::runAudit(const TerrainEditorDoc &doc)
{
        const AuditReport report = analyze(doc);

        const TerrainGenerator *const generator = doc.getTerrainGenerator();
        if (!generator)
                return CString(_T("[Terrain Intelligence]\r\nNo terrain data is loaded."));

        CString output;
        output += _T("[Terrain Intelligence Audit]\r\n");

        CString scoreLine;
        scoreLine.Format(_T("Foresight score: %0.1f/100 %s\r\n"), report.foresightScore, buildGauge(report.foresightScore).GetString());
        output += scoreLine;

        CString subScoreLine;
        subScoreLine.Format(_T("Structure %0.1f/100 • Ecosystem %0.1f/100 • Workflow %0.1f/100\r\n"), report.structureScore, report.ecosystemScore, report.workflowScore);
        output += subScoreLine;

        CString mapLine;
        mapLine.Format(_T("Map: %0.0fm square • chunk width %0.0fm • tile width %.2fm\r\n"), doc.getMapWidthInMeters(), doc.getChunkWidthInMeters(), doc.getTileWidthInMeters());
        output += mapLine;

        CString layerLine;
        layerLine.Format(_T("Layers: %s, %s active (%0.0f%%), %s resting • depth %d\r\n"),
                formatPlural(report.totalLayers, _T("layer")).GetString(),
                formatPlural(report.activeLayers, _T("layer")).GetString(),
                (report.totalLayers > 0) ? (100.0f * static_cast<float>(report.activeLayers) / static_cast<float>(report.totalLayers)) : 0.0f,
                formatPlural(report.inactiveLayers, _T("layer")).GetString(),
                report.hierarchyDepth);
        output += layerLine;

        CString compositionLine;
        compositionLine.Format(_T("Composition: %s, %s, %s affectors\r\n"),
                formatPlural(report.totalBoundaries, _T("boundary")).GetString(),
                formatPlural(report.totalFilters, _T("filter")).GetString(),
                formatPlural(report.totalAffectors, _T("affector")).GetString());
        output += compositionLine;

        CString libraryLine;
        libraryLine.Format(_T("Libraries: %d shader • %d flora • %d radial • %d environment • %d fractal • %d bitmap families\r\n"),
                report.shaderFamilies, report.floraFamilies, report.radialFamilies, report.environmentFamilies, report.fractalFamilies, report.bitmapFamilies);
        output += libraryLine;

        CString workflowLine;
        workflowLine.Format(_T("Workflow: environment cycle %d min • global water %s\r\n"),
                report.environmentCycleMinutes,
                report.hasGlobalWaterTable ? _T("enabled") : _T("disabled"));
        output += workflowLine;

        output += _T("\r\nAI Insights:\r\n");
        if (report.insights.empty())
        {
                output += _T("- The AI assistant found no blockers – keep iterating!\r\n");
        }
        else
        {
                for (size_t i = 0; i < report.insights.size(); ++i)
                {
                        const Insight &insight = report.insights[i];
                        CString line;
                        line.Format(_T("- %s (%s confidence)\r\n   %s\r\n"),
                                insight.headline.GetString(),
                                formatPercent(insight.confidence).GetString(),
                                insight.detail.GetString());
                        output += line;
                }
        }

        output += _T("\r\nAI Blueprint:\r\n");
        output += buildBlueprintOutput(report);

        const real totalGenerationTime = doc.getLastTotalChunkGenerationTime();
        if (totalGenerationTime > CONST_REAL(0))
        {
                CString perfLine;
                perfLine.Format(_T("\r\nPerf tracker: last bake %.2fs (avg %.2fs).\r\n"),
                        totalGenerationTime,
                        doc.getLastAverageChunkGenerationTime());
                output += perfLine;
        }

        output += _T("\r\nNext experiments:\r\n");
        output += _T("• Deploy the AI blueprint queue to auto-stage terrain polish passes.\r\n");
        output += _T("• Ask the blueprint runner to materialise suggested shader/flora families.\r\n");
        output += _T("• Combine the audit with console filters to jump directly to recommended layers.\r\n");

        return output;
}

//----------------------------------------------------------------------
