-- Star Wars Galaxies Exporter for Autodesk 3ds Max 2022
-- ------------------------------------------------------
-- This MaxScript replicates the Star Wars Galaxies Maya exporter workflow in 3ds Max.
-- It provides rollout UI, validation, configuration persistence, export automation,
-- and publishing hooks so artists can build game-ready assets directly from Max.
--
-- The exporter relies on native 3ds Max capabilities for geometry extraction
-- and uses the SWG command line toolchain (swg-tool) for final asset conversion.
-- ------------------------------------------------------

global SWGExporter

struct SWGExporterSettings
(
    version,
    projectRoot,
    appearancePath,
    meshPath,
    skeletonPath,
    animationPath,
    shaderPath,
    collisionPath,
    logPath,
    publishPath,
    exportPreset,
    convertWithSwgTool,
    publishAfterExport
)

struct SWGExporterState
(
    rollout,
    settings,
    settingsFile,
    logFile,
    mutex = false
)

fn swg_getUserScriptsDirectory =
(
    local scriptsDir = getDir #userScripts
    if scriptsDir == undefined or scriptsDir == "" do
    (
        scriptsDir = getDir #scripts
    )
    scriptsDir
)

fn swg_getSettingsFile =
(
    (swg_getUserScriptsDirectory()) + "/SWGExporterSettings.ini"
)

fn swg_defaultSettings =
(
    SWGExporterSettings 
    (
        version = "1.0.0",
        projectRoot = getDir #maxroot,
        appearancePath = getDir #export + "/appearance",
        meshPath = getDir #export + "/mesh",
        skeletonPath = getDir #export + "/skeleton",
        animationPath = getDir #export + "/animation",
        shaderPath = getDir #export + "/shader",
        collisionPath = getDir #export + "/collision",
        logPath = swg_getUserScriptsDirectory() + "/logs",
        publishPath = swg_getUserScriptsDirectory() + "/publish",
        exportPreset = "GameExporter",
        convertWithSwgTool = true,
        publishAfterExport = false
    )
)

fn swg_ensureDirectory path =
(
    if (doesFileExist path) then
    (
        if (getFileAttribute path #isdir) then true else false
    )
    else
    (
        makeDir path
    )
)

fn swg_log state message =
(
    if state.logFile == undefined do return undefined
    local dotNetDateTime = dotNetClass "System.DateTime"
    local timestamp = dotNetDateTime.Now.ToString "yyyy-MM-dd HH:mm:ss"
    local line = timestamp + " - " + message + "\n"
    local stream = openFile state.logFile mode:"at"
    if stream != undefined then
    (
        format "%" line to:stream
        close stream
    )
)

fn swg_loadSettings state =
(
    local file = state.settingsFile
    if not (doesFileExist file) then
    (
        state.settings = swg_defaultSettings()
        swg_saveSettings state
        return state.settings
    )

    local settings = swg_defaultSettings()

    settings.projectRoot = getINISetting file "paths" "projectRoot" default:settings.projectRoot
    settings.appearancePath = getINISetting file "paths" "appearancePath" default:settings.appearancePath
    settings.meshPath = getINISetting file "paths" "meshPath" default:settings.meshPath
    settings.skeletonPath = getINISetting file "paths" "skeletonPath" default:settings.skeletonPath
    settings.animationPath = getINISetting file "paths" "animationPath" default:settings.animationPath
    settings.shaderPath = getINISetting file "paths" "shaderPath" default:settings.shaderPath
    settings.collisionPath = getINISetting file "paths" "collisionPath" default:settings.collisionPath
    settings.logPath = getINISetting file "paths" "logPath" default:settings.logPath
    settings.publishPath = getINISetting file "paths" "publishPath" default:settings.publishPath
    settings.exportPreset = getINISetting file "export" "preset" default:settings.exportPreset
    settings.convertWithSwgTool = (getINISetting file "export" "convertWithSwgTool" default:(settings.convertWithSwgTool as string)) as boolean
    settings.publishAfterExport = (getINISetting file "export" "publishAfterExport" default:(settings.publishAfterExport as string)) as boolean

    state.settings = settings
    state
)

fn swg_saveSettings state =
(
    local file = state.settingsFile
    setINISetting file "paths" "projectRoot" state.settings.projectRoot
    setINISetting file "paths" "appearancePath" state.settings.appearancePath
    setINISetting file "paths" "meshPath" state.settings.meshPath
    setINISetting file "paths" "skeletonPath" state.settings.skeletonPath
    setINISetting file "paths" "animationPath" state.settings.animationPath
    setINISetting file "paths" "shaderPath" state.settings.shaderPath
    setINISetting file "paths" "collisionPath" state.settings.collisionPath
    setINISetting file "paths" "logPath" state.settings.logPath
    setINISetting file "paths" "publishPath" state.settings.publishPath
    setINISetting file "export" "preset" state.settings.exportPreset
    setINISetting file "export" "convertWithSwgTool" (state.settings.convertWithSwgTool as string)
    setINISetting file "export" "publishAfterExport" (state.settings.publishAfterExport as string)
)

fn swg_buildLogFile state assetName =
(
    swg_ensureDirectory state.settings.logPath
    local dotNetDateTime = dotNetClass "System.DateTime"
    local stamp = dotNetDateTime.Now.ToString "yyyyMMdd_HHmmss"
    state.logFile = state.settings.logPath + "/" + assetName + "_" + stamp + ".log"
    state.logFile
)

fn swg_selectDirectory caption startPath =
(
    local folder = getSavePath caption caption startPath
    if folder == undefined then startPath else folder
)

fn swg_collectNodes includeHidden:off =
(
    local nodes = #()
    if selection.count > 0 then
    (
        for obj in selection do append nodes obj
    )
    else
    (
        for obj in geometry do if (includeHidden or obj.isHidden == false) do append nodes obj
    )
    nodes
)

fn swg_validateNaming nodes =
(
    local issues = #()
    for obj in nodes do
    (
        if matchPattern obj.name pattern:"*_LOD*" ignoreCase:true == false and
           matchPattern obj.name pattern:"hp_*" ignoreCase:true == false and
           matchPattern obj.name pattern:"col_*" ignoreCase:true == false then
        (
            append issues ("Node '" + obj.name + "' does not match SWG naming conventions.")
        )
    )
    issues
)

fn swg_validateTransforms nodes =
(
    local issues = #()
    for obj in nodes do
    (
        local scl = obj.scale
        if (length (scl - [1,1,1]) > 0.001) do append issues ("Node '" + obj.name + "' has non-uniform scale.")

        local rot = (obj.rotation as eulerAngles)
        if (abs rot.x > 0.1 or abs rot.y > 0.1 or abs rot.z > 0.1) do append issues ("Node '" + obj.name + "' has baked rotation; freeze transforms before export.")
    )
    issues
)

fn swg_validateScene state =
(
    local nodes = swg_collectNodes includeHidden:on
    local issues = #()
    join issues (swg_validateNaming nodes)
    join issues (swg_validateTransforms nodes)

    if issues.count == 0 then
    (
        swg_log state "Scene validation passed."
        messageBox "Validation complete. No issues found." title:"SWG Exporter"
        true
    )
    else
    (
        local message = "Validation completed with warnings:\n\n"
        for issue in issues do message += issue + "\n"
        swg_log state message
        messageBox message title:"SWG Exporter" beep:true
        false
    )
)

fn swg_exportSelection preset filePath =
(
    if selection.count == 0 do select (swg_collectNodes())
    if selection.count == 0 then
    (
        messageBox "No objects available to export." title:"SWG Exporter"
        return false
    )

    try
    (
        exportFile filePath #noPrompt selectedOnly:true using:FBXEXP
        true
    )
    catch
    (
        messageBox ("Export failed. Check that the FBX exporter is installed.\n" + getCurrentException()) title:"SWG Exporter"
        false
    )
)

fn swg_runSwgTool state exportType sourceFile targetFile =
(
    local pythonExe = getEnv "SWG_PYTHON"
    if pythonExe == undefined or pythonExe == "" do pythonExe = "python"

    local swgTool = state.settings.projectRoot + "/tools/swg-tool.py"
    if not (doesFileExist swgTool) then
    (
        swg_log state "swg-tool.py not found. Skipping conversion."
        return false
    )

    local args = ("\"" + swgTool + "\" export --type " + exportType + " --input \"" + sourceFile + "\" --output \"" + targetFile + "\"")
    local command = pythonExe

    local proc = dotNetObject "System.Diagnostics.Process"
    proc.StartInfo.FileName = command
    proc.StartInfo.Arguments = args
    proc.StartInfo.WorkingDirectory = state.settings.projectRoot
    proc.StartInfo.UseShellExecute = false
    proc.StartInfo.RedirectStandardOutput = true
    proc.StartInfo.RedirectStandardError = true
    proc.StartInfo.CreateNoWindow = true
    proc.Start()
    proc.WaitForExit()

    local output = proc.StandardOutput.ReadToEnd()
    local errorOutput = proc.StandardError.ReadToEnd()
    if output != "" do swg_log state output
    if errorOutput != "" do swg_log state ("ERROR: " + errorOutput)

    proc.ExitCode == 0
)

fn swg_publishAssets state files =
(
    if files.count == 0 do return true

    local changelistFile = state.settings.publishPath + "/swg_exporter_cl.txt"
    swg_ensureDirectory state.settings.publishPath
    local stream = createFile changelistFile
    for file in files do format "%\n" file to:stream
    close stream
    swg_log state ("Wrote changelist file: " + changelistFile)
    true
)

fn swg_performExport state exportType =
(
    if state.mutex then
    (
        messageBox "An export is already in progress." title:"SWG Exporter"
        return false
    )

    state.mutex = true

    local assetName = (if selection.count > 0 then selection[1].name else (maxFileName as string))
    if assetName == undefined or assetName == "" do assetName = "swg_asset"
    local tempDir = getDir #temp + "/swg_exporter"
    swg_ensureDirectory tempDir

    local exportDirectory = case exportType of
    (
        #static: state.settings.meshPath
        #skeletal: state.settings.skeletonPath
        #animation: state.settings.animationPath
        #portal: state.settings.appearancePath
        #collision: state.settings.collisionPath
        default: state.settings.meshPath
    )
    swg_ensureDirectory exportDirectory

    local tempFile = tempDir + "/" + assetName + "_" + (exportType as string) + ".fbx"
    local finalExtension = case exportType of
    (
        #static: ".msh"
        #skeletal: ".skm"
        #animation: ".ska"
        #portal: ".cmp"
        #collision: ".clm"
        default: ".msh"
    )
    local finalFile = exportDirectory + "/" + assetName + finalExtension

    swg_buildLogFile state assetName
    swg_log state ("Export started for " + assetName + " as " + (exportType as string))

    if swg_exportSelection state.settings.exportPreset tempFile then
    (
        local exportedFiles = #()
        if state.settings.convertWithSwgTool then
        (
            if swg_runSwgTool state (exportType as string) tempFile finalFile then
            (
                append exportedFiles finalFile
                swg_log state ("SWG conversion finished: " + finalFile)
                if doesFileExist tempFile do deleteFile tempFile
            )
            else
            (
                append exportedFiles tempFile
                swg_log state "Conversion failed; leaving FBX file for manual inspection."
            )
        )
        else
        (
            if doesFileExist finalFile do deleteFile finalFile
            dotNetClass "System.IO.File".Copy tempFile finalFile true
            if doesFileExist tempFile do deleteFile tempFile
            append exportedFiles finalFile
        )

        if state.settings.publishAfterExport then swg_publishAssets state exportedFiles
        state.mutex = false
        local summary = ""
        for f in exportedFiles do summary += f + "\n"
        messageBox ("Export completed:\n" + summary) title:"SWG Exporter"
        true
    )
    else
    (
        state.mutex = false
        false
    )
)

fn swg_updateRolloutFromSettings ro state =
(
    ro.edtProjectRoot.text = state.settings.projectRoot
    ro.edtAppearance.text = state.settings.appearancePath
    ro.edtMesh.text = state.settings.meshPath
    ro.edtSkeleton.text = state.settings.skeletonPath
    ro.edtAnimation.text = state.settings.animationPath
    ro.edtShader.text = state.settings.shaderPath
    ro.edtCollision.text = state.settings.collisionPath
    ro.edtLog.text = state.settings.logPath
    ro.edtPublish.text = state.settings.publishPath
    ro.chkConvert.checked = state.settings.convertWithSwgTool
    ro.chkPublish.checked = state.settings.publishAfterExport
    local idx = findItem ro.cmbPreset.items state.settings.exportPreset
    if idx == 0 do idx = 1
    ro.cmbPreset.selection = idx
)

fn swg_applyRolloutToSettings ro state =
(
    state.settings.projectRoot = ro.edtProjectRoot.text
    state.settings.appearancePath = ro.edtAppearance.text
    state.settings.meshPath = ro.edtMesh.text
    state.settings.skeletonPath = ro.edtSkeleton.text
    state.settings.animationPath = ro.edtAnimation.text
    state.settings.shaderPath = ro.edtShader.text
    state.settings.collisionPath = ro.edtCollision.text
    state.settings.logPath = ro.edtLog.text
    state.settings.publishPath = ro.edtPublish.text
    state.settings.convertWithSwgTool = ro.chkConvert.checked
    state.settings.publishAfterExport = ro.chkPublish.checked
    if ro.cmbPreset.selection > 0 do state.settings.exportPreset = ro.cmbPreset.items[ro.cmbPreset.selection]
    swg_saveSettings state
)

if SWGExporter != undefined do
(
    if SWGExporter.rollout != undefined do try(destroyDialog SWGExporter.rollout)catch()
    SWGExporter.rollout = undefined
)

fn swg_createRollout =
(
    rollout ro "SWG Exporter" width:400
    (
        fn swg_getState =
        (
            if SWGExporter == undefined then
            (
                messageBox "SWG Exporter is not initialized. Please run swg_install()." title:"SWG Exporter"
                undefined
            )
            else
            (
                SWGExporter
            )
        )

        fn swg_beforeAction =
        (
            local state = swg_getState()
            if state != undefined do swg_applyRolloutToSettings ro state
            state
        )

        group "Project"
        (
            edittext edtProjectRoot "Project Root:" text:"" fieldWidth:280
            button btnBrowseProject "Browse" width:80
        )
        group "Output Paths"
        (
            edittext edtAppearance "Appearance:" text:"" fieldWidth:280
            button btnBrowseAppearance "Browse" width:80

            edittext edtMesh "Meshes:" text:"" fieldWidth:280
            button btnBrowseMesh "Browse" width:80

            edittext edtSkeleton "Skeletons:" text:"" fieldWidth:280
            button btnBrowseSkeleton "Browse" width:80

            edittext edtAnimation "Animations:" text:"" fieldWidth:280
            button btnBrowseAnimation "Browse" width:80

            edittext edtShader "Shaders:" text:"" fieldWidth:280
            button btnBrowseShader "Browse" width:80

            edittext edtCollision "Collision:" text:"" fieldWidth:280
            button btnBrowseCollision "Browse" width:80

            edittext edtLog "Logs:" text:"" fieldWidth:280
            button btnBrowseLog "Browse" width:80

            edittext edtPublish "Publish:" text:"" fieldWidth:280
            button btnBrowsePublish "Browse" width:80
        )

        group "Export"
        (
            dropdownlist cmbPreset "FBX Preset" items:#("GameExporter", "Default", "Previous")
            checkbox chkConvert "Convert with swg-tool" checked:true
            checkbox chkPublish "Publish after export" checked:false

            button btnValidate "Validate Scene" width:180
            button btnExportStatic "Export Static Mesh" width:180
            button btnExportSkeletal "Export Skeletal Mesh" width:180
            button btnExportAnimation "Export Animation" width:180
            button btnExportPortal "Export Portal/Cell" width:180
            button btnExportCollision "Export Collision" width:180
        )

        on ro open do
        (
            local state = swg_getState()
            if state != undefined do swg_updateRolloutFromSettings ro state
        )

        on btnBrowseProject pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtProjectRoot.text = swg_selectDirectory "Project Root" state.settings.projectRoot
        )
        on btnBrowseAppearance pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtAppearance.text = swg_selectDirectory "Appearance" state.settings.appearancePath
        )
        on btnBrowseMesh pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtMesh.text = swg_selectDirectory "Meshes" state.settings.meshPath
        )
        on btnBrowseSkeleton pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtSkeleton.text = swg_selectDirectory "Skeletons" state.settings.skeletonPath
        )
        on btnBrowseAnimation pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtAnimation.text = swg_selectDirectory "Animations" state.settings.animationPath
        )
        on btnBrowseShader pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtShader.text = swg_selectDirectory "Shader" state.settings.shaderPath
        )
        on btnBrowseCollision pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtCollision.text = swg_selectDirectory "Collision" state.settings.collisionPath
        )
        on btnBrowseLog pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtLog.text = swg_selectDirectory "Logs" state.settings.logPath
        )
        on btnBrowsePublish pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtPublish.text = swg_selectDirectory "Publish" state.settings.publishPath
        )

        on btnValidate pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_validateScene state
        )

        on btnExportStatic pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_performExport state #static
        )

        on btnExportSkeletal pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_performExport state #skeletal
        )

        on btnExportAnimation pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_performExport state #animation
        )

        on btnExportPortal pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_performExport state #portal
        )

        on btnExportCollision pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_performExport state #collision
        )
    )
    ro
)

fn swg_showExporterDialog =
(
    swg_install()
    if SWGExporter.rollout == undefined do SWGExporter.rollout = swg_createRollout()
    if SWGExporter.rollout != undefined do swg_updateRolloutFromSettings SWGExporter.rollout SWGExporter
    if SWGExporter.rollout != undefined do try(destroyDialog SWGExporter.rollout)catch()
    if SWGExporter.rollout != undefined do createDialog SWGExporter.rollout style:#(#style_titlebar, #style_sysmenu, #style_toolwindow)
)

fn swg_install =
(
    local firstInstall = false
    if SWGExporter == undefined then
    (
        SWGExporter = SWGExporterState()
        SWGExporter.settingsFile = swg_getSettingsFile()
        swg_ensureDirectory (getFilenamePath SWGExporter.settingsFile)
        swg_loadSettings SWGExporter
        firstInstall = true
    )
    if SWGExporter.rollout == undefined do SWGExporter.rollout = swg_createRollout()
    if firstInstall do format "[SWG Exporter] Installed.\n"
    true
)

fn swg_uninstall =
(
    if SWGExporter != undefined do
    (
        if SWGExporter.rollout != undefined do try(destroyDialog SWGExporter.rollout)catch()
        SWGExporter.rollout = undefined
        SWGExporter = undefined
    )
    if macros.isRegistered "Star Wars Galaxies" "SWGExporter" do macros.delete "Star Wars Galaxies" "SWGExporter"
    format "[SWG Exporter] Uninstalled.\n"
    true
)

if macros.isRegistered "Star Wars Galaxies" "SWGExporter" do macros.delete "Star Wars Galaxies" "SWGExporter"

macroScript SWGExporter category:"Star Wars Galaxies" tooltip:"SWG Exporter"
(
    on execute do swg_showExporterDialog()
)

swg_install()
