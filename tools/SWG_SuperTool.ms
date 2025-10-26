-- Star Wars Galaxies Super Tool for 3ds Max 2022
-- Combines importer/exporter workflows with additional automation helpers.
-- This script expects SWG_MSHImporter.ms and SWG_MSHFullExporter.ms to live in the same directory.

global swgSuper_thisScript
global swgSuper_scriptDir
swgSuper_thisScript = getThisScriptFilename()
swgSuper_scriptDir = if swgSuper_thisScript != undefined then getFilenamePath swgSuper_thisScript else (getDir #scripts + "\\")

-- Prevent the legacy scripts from spawning their standalone floaters while we compose the super tool UI.
try(global SWG_Importer_AutoLaunch = false) catch()
try(global SWG_Exporter_AutoLaunch = false) catch()

fn swgSuper_safeFileIn scriptName = (
        local target = swgSuper_scriptDir + scriptName
        if doesFileExist target then (
                fileIn target
        ) else (
                messageBox ("Unable to locate required script: " + target) title:"SWG Super Tool"
        )
)

swgSuper_safeFileIn "SWG_MSHImporter.ms"
swgSuper_safeFileIn "SWG_MSHFullExporter.ms"

-- Global state for the super tool.
global SWG_SuperToolFloater
global SWG_superCurrentRollout
global SWG_superPanelControl
global SWG_superStatusLog
if SWG_superStatusLog == undefined do SWG_superStatusLog = #()

global SWG_superOutputRoot
if SWG_superOutputRoot == undefined do SWG_superOutputRoot = ""

global SWG_superAssetName
if SWG_superAssetName == undefined do SWG_superAssetName = ""

global SWG_superSkeletonRoot

global SWG_superAnimationLibrary
if SWG_superAnimationLibrary == undefined do SWG_superAnimationLibrary = #()

global SWG_superDashboard
global SWG_superUtilities

fn swgSuper_SanitizeAssetName rawName = (
        local name = if rawName == undefined then "" else toLower rawName
        name = substituteString name " " "_"
        name = substituteString name "-" "_"
        name = substituteString name "." "_"
        local allowed = "abcdefghijklmnopqrstuvwxyz0123456789_"
        local result = ""
        for i = 1 to name.count do (
                local ch = name[i]
                if findString allowed ch != undefined then result += ch
        )
        while (result.count > 0 and result[1] == "_") do (
                result = if result.count > 1 then substring result 2 (result.count - 1) else ""
        )
        while findString result "__" != undefined do (
                result = substituteString result "__" "_"
        )
        if result == "" then result = "swg_asset"
        result
)

fn swgSuper_SetOutputRoot path = (
        if path != undefined and path != "" then (
                SWG_superOutputRoot = path
                if SWG_superDashboard != undefined do (
                        SWG_superDashboard.lblOutput.text = "Output: " + path
                )
                swgSuper_UpdateUtilities()
        )
)

fn swgSuper_SetAssetName rawName = (
        local sanitized = swgSuper_SanitizeAssetName rawName
        SWG_superAssetName = sanitized
        if SWG_superDashboard != undefined do (
                if SWG_superDashboard.edtAssetName.text != sanitized then (
                        SWG_superDashboard.edtAssetName.text = sanitized
                )
        )
        swgSuper_UpdateUtilities()
        sanitized
)

fn swgSuper_IsBoneNode node = (
        local isBoneNode = false
        if node != undefined and isValidNode node then (
                try(isBoneNode = isBone node) catch(isBoneNode = false)
                if not isBoneNode then (
                        if isProperty node #isBone then (
                                try(isBoneNode = node.isBone) catch(isBoneNode = false)
                        )
                )
                if not isBoneNode then (
                        local lowerName = toLower node.name
                        if matchPattern lowerName pattern:"*bone*" then isBoneNode = true
                )
        )
        isBoneNode
)

fn swgSuper_FindSkeletonRoot nodes = (
        local fallback = undefined
        if nodes != undefined then (
                for node in nodes do (
                        if node != undefined and isValidNode node then (
                                if swgSuper_IsBoneNode node then (
                                        local lowerName = toLower node.name
                                        if matchPattern lowerName pattern:"*root*" then return node
                                        if matchPattern lowerName pattern:"*pelvis*" then fallback = node
                                        if fallback == undefined then fallback = node
                                )
                        )
                )
        )
        fallback
)

fn swgSuper_SetSkeletonRoot node silent:false = (
        if node != undefined and isValidNode node then (
                SWG_superSkeletonRoot = node
                if not silent then swgSuper_Log ("Skeleton root set to " + node.name)
                swgSuper_UpdateUtilities()
        )
)

fn swgSuper_GatherSkeletonNodes rootNode = (
        local result = #()
        if rootNode == undefined or not isValidNode rootNode then return result
        local stack = #(rootNode)
        while stack.count > 0 do (
                local n = stack[stack.count]
                deleteItem stack stack.count
                if n != undefined and isValidNode n then (
                        if swgSuper_IsBoneNode n then (
                                if (findItem result n) == 0 then append result n
                        )
                        for c in n.children do (
                                append stack c
                        )
                )
        )
        result
)

fn swgSuper_AddOrUpdateClip clipName startFrame endFrame = (
        if clipName == undefined or clipName == "" then return false
        local existingIndex = 0
        for i = 1 to SWG_superAnimationLibrary.count do (
                if toLower SWG_superAnimationLibrary[i][1] == toLower clipName then (
                        existingIndex = i
                        exit
                )
        )
        local record = #(clipName, startFrame as integer, endFrame as integer)
        if existingIndex > 0 then (
                SWG_superAnimationLibrary[existingIndex] = record
        ) else (
                append SWG_superAnimationLibrary record
        )
        swgSuper_UpdateAnimationList()
        true
)

fn swgSuper_AutoConfigureFromScene = (
        local nodes = selection as array
        local candidateName = undefined
        if nodes.count > 0 then candidateName = nodes[1].name
        if candidateName == undefined or candidateName == "" then (
                try(candidateName = getFileNameFile maxFileName) catch(candidateName = undefined)
        )
        if candidateName == undefined or candidateName == "" then candidateName = "swg_asset"
        local sanitized = swgSuper_SetAssetName candidateName
        swgSuper_Log ("Asset tag set to " + sanitized + " via smart configure.")

        if SWG_superOutputRoot == undefined or SWG_superOutputRoot == "" then (
                local defaultPath = undefined
                try(defaultPath = maxFilePath) catch(defaultPath = undefined)
                if defaultPath == undefined or defaultPath == "" then (
                        try(defaultPath = getDir #export) catch(defaultPath = undefined)
                )
                if defaultPath != undefined and defaultPath != "" then (
                        swgSuper_SetOutputRoot defaultPath
                        swgSuper_Log ("Output folder auto-set to " + defaultPath)
                )
        )

        local root = swgSuper_FindSkeletonRoot nodes
        if root == undefined then (
                root = swgSuper_FindSkeletonRoot objects
        )
        if root != undefined then (
                swgSuper_SetSkeletonRoot root
        ) else (
                swgSuper_Log "No skeleton root detected. Select a bone and try again."
        )

        local startFrame = animationRange.start.frame as integer
        local endFrame = animationRange.end.frame as integer
        if swgSuper_AddOrUpdateClip "full_body" startFrame endFrame then (
                swgSuper_Log "Default 'full_body' clip registered."
        )
        swgSuper_Log "Scene smart configuration complete."
)

fn swgSuper_RunExportWizard bakeAnimations = (
        local nodes = selection as array
        if nodes.count == 0 then (
                messageBox "Select the geometry you want to export before running Smart Export." title:"SWG Super Tool"
                return undefined
        )
        swgSuper_Log ("Smart export running on " + (nodes.count as string) + " nodes.")
        if not swgSuper_PrepareGeometry nodes then return undefined

        local skeletonNodes = #()
        if SWG_superSkeletonRoot != undefined and isValidNode SWG_superSkeletonRoot then (
                skeletonNodes = swgSuper_GatherSkeletonNodes SWG_superSkeletonRoot
                if skeletonNodes.count > 0 then (
                        local skeletonReady = swgSuper_PrepareSkeleton skeletonNodes
                        if skeletonReady and bakeAnimations and SWG_superAnimationLibrary.count > 0 then (
                                for clip in SWG_superAnimationLibrary do (
                                        swgSuper_BakeAnimation skeletonNodes clip[2] clip[3]
                                )
                        )
                )
        ) else (
                swgSuper_Log "Smart export skipped skeleton prep because no root is set."
        )

        swgSuper_Log "Triggering exporter dialog."
        swgSuper_RunExporter()
)

fn swgSuper_GetOutputPath extension = (
        if (SWG_superOutputRoot == undefined or SWG_superOutputRoot == "" or SWG_superAssetName == undefined or SWG_superAssetName == "") then (
                "<asset or output path not set>"
        ) else (
                SWG_superOutputRoot + "\\" + SWG_superAssetName + "." + extension
        )
)

fn swgSuper_UpdateAnimationList = (
        if SWG_superUtilities != undefined do (
                local items = for clip in SWG_superAnimationLibrary collect (clip[1] + " [" + (clip[2] as string) + "-" + (clip[3] as string) + "]")
                SWG_superUtilities.lbAnimations.items = items
        )
)

fn swgSuper_UpdateUtilities = (
        if SWG_superUtilities != undefined do (
                SWG_superUtilities.lblMGNPath.text = "MGN: " + swgSuper_GetOutputPath "mgn"
                SWG_superUtilities.lblMSHPath.text = "MSH: " + swgSuper_GetOutputPath "msh"
                SWG_superUtilities.lblSKTPath.text = "SKT: " + swgSuper_GetOutputPath "skt"
                if (SWG_superSkeletonRoot != undefined and isValidNode SWG_superSkeletonRoot) then (
                        SWG_superUtilities.lblSkeletonRoot.text = "Skeleton Root: " + SWG_superSkeletonRoot.name
                ) else (
                        SWG_superUtilities.lblSkeletonRoot.text = "Skeleton Root: <not set>"
                )
                swgSuper_UpdateAnimationList()
        )
        if SWG_superDashboard != undefined do (
                if SWG_superOutputRoot != undefined and SWG_superOutputRoot != "" then (
                        SWG_superDashboard.lblOutput.text = "Output: " + SWG_superOutputRoot
                ) else (
                        SWG_superDashboard.lblOutput.text = "Output: <not set>"
                )
        )
)

fn swgSuper_Log message = (
        append SWG_superStatusLog message
        while SWG_superStatusLog.count > 24 do deleteItem SWG_superStatusLog 1
        if SWG_superDashboard != undefined do (
                local str = ""
                for m in SWG_superStatusLog do str += (m + "\n")
                SWG_superDashboard.logOutput.text = str
                SWG_superDashboard.lblStatus.text = "Status: " + message
        )
)

fn swgSuper_ShowRollout ctrl roll = (
        if ctrl == undefined then return undefined
        if roll == undefined then return undefined
        if SWG_superCurrentRollout != undefined do (
                try(removeSubRollout ctrl SWG_superCurrentRollout) catch()
        )
        addSubRollout ctrl roll
        SWG_superCurrentRollout = roll
        roll
)

fn swgSuper_PrepareGeometry nodes = (
        if nodes.count == 0 then (
                messageBox "Select one or more mesh nodes to prepare." title:"SWG Super Tool"
                false
        ) else (
                local processed = 0
                for obj in nodes do (
                        if obj != undefined and isValidNode obj then (
                                try(resetXForm obj) catch()
                                if (canConvertTo obj Editable_Poly) do (
                                        try(convertToEditablePoly obj) catch()
                                )
                                try(collapseStack obj) catch()
                                processed += 1
                        )
                )
                if processed > 0 then (
                        swgSuper_Log ("Prepared geometry for " + (processed as string) + " nodes.")
                        true
                ) else (
                        swgSuper_Log "No valid geometry nodes were prepared."
                        false
                )
        )
)

fn swgSuper_PrepareSkeleton nodes = (
        if nodes.count == 0 then (
                messageBox "Select the skeleton bones you want to prep." title:"SWG Super Tool"
                false
        ) else (
                local processed = 0
                for b in nodes do (
                        if b != undefined and isValidNode b then (
                                try(if isProperty b #boneScaleType do b.boneScaleType = #none) catch()
                                try(if isProperty b #showLinks do b.showLinks = true) catch()
                                try(if isProperty b #freeze do b.freeze = false) catch()
                                try(if isProperty b #boneObject do b.boneObject = true) catch()
                                processed += 1
                        )
                )
                if processed > 0 then (
                        swgSuper_Log ("Prepared skeleton nodes: " + (processed as string))
                        true
                ) else (
                        swgSuper_Log "No valid skeleton nodes were prepared."
                        false
                )
        )
)

fn swgSuper_BakeAnimation nodes startFrame endFrame = (
        if nodes.count == 0 then (
                messageBox "Select the objects whose animation should be baked." title:"SWG Super Tool"
                false
        ) else (
                local s = amin startFrame endFrame
                local e = amax startFrame endFrame
                local bakedCount = 0
                with animate on (
                        for f = s to e do (
                                sliderTime = f
                                for obj in nodes do (
                                        if obj != undefined and isValidNode obj then (
                                                at time f (
                                                        obj.position = obj.position
                                                        obj.rotation = obj.rotation
                                                        obj.scale = obj.scale
                                                )
                                                if f == s then bakedCount += 1
                                        )
                                )
                        )
                )
                if bakedCount > 0 then (
                        swgSuper_Log ("Baked animation keys for " + (bakedCount as string) + " nodes between " + (s as string) + " and " + (e as string) + ".")
                        true
                ) else (
                        swgSuper_Log "No valid nodes were baked for animation."
                        false
                )
        )
)

fn swgSuper_SetAnimationRange startFrame endFrame = (
        animationRange = interval (startFrame as integer) (endFrame as integer)
        swgSuper_Log ("Animation range set to " + (startFrame as string) + " - " + (endFrame as string))
)

fn swgSuper_RunImporter = (
        if (isValidRollout SWG_importRoll) and (isProperty SWG_importRoll #impMGN) then (
                try(
                        pressButton SWG_importRoll.impMGN
                        swgSuper_Log "Importer dialog opened."
                ) catch(
                        messageBox "Unable to trigger the importer UI." title:"SWG Super Tool"
                )
        ) else (
                messageBox "Importer rollout not loaded." title:"SWG Super Tool"
        )
)

fn swgSuper_RunExporter = (
        if (isValidRollout SWG_exportRoll) and (isProperty SWG_exportRoll #expMSH) then (
                try(
                        pressButton SWG_exportRoll.expMSH
                        swgSuper_Log "Exporter dialog opened."
                ) catch(
                        messageBox "Unable to trigger the exporter UI." title:"SWG Super Tool"
                )
        ) else (
                messageBox "Exporter rollout not loaded." title:"SWG Super Tool"
        )
)

fn swgSuper_RunRoundtrip = (
        local nodes = selection as array
        if swgSuper_PrepareGeometry nodes then (
                if SWG_superPanelControl != undefined then swgSuper_ShowRollout SWG_superPanelControl SWG_exportRoll
                swgSuper_RunExporter()
        )
)

fn swgSuper_ShowStandaloneImporter = (
        if SWG_importFloater != undefined then (
                try(closeRolloutFloater SWG_importFloater) catch()
        )
        SWG_importFloater = newRolloutFloater "SWG Importer" 220 190 50 50
        addRollout SWG_importRoll SWG_importFloater
)

fn swgSuper_ShowStandaloneExporter = (
        if SWG_exportFloater != undefined then (
                try(closeRolloutFloater SWG_exportFloater) catch()
        )
        SWG_exportFloater = newRolloutFloater "SWG Exporter" 420 640 60 60
        addRollout SWG_exportRoll SWG_exportFloater
)

fn swgSuper_AddAnimationClip clipName startFrame endFrame = (
        if clipName == "" then (
                messageBox "Provide a name for the animation clip." title:"SWG Super Tool"
        ) else (
                append SWG_superAnimationLibrary #(clipName, startFrame as integer, endFrame as integer)
                swgSuper_Log ("Registered animation clip '" + clipName + "'.")
                swgSuper_UpdateAnimationList()
        )
)

fn swgSuper_BakeSelectedClips indices = (
        if indices.count == 0 then (
                messageBox "Select one or more clips from the list to bake." title:"SWG Super Tool"
        ) else (
                local nodes = selection as array
                if nodes.count == 0 then (
                        messageBox "Select scene nodes to bake before running this action." title:"SWG Super Tool"
                ) else (
                        for idx in indices do (
                                if idx >= 1 and idx <= SWG_superAnimationLibrary.count then (
                                        local clip = SWG_superAnimationLibrary[idx]
                                        swgSuper_BakeAnimation nodes clip[2] clip[3]
                                )
                        )
                )
        )
)

rollout SWG_superUtilities "Utilities" width:380 height:360 (
        group "Content Paths" (
                label lblMGNPath "MGN: <asset or output path not set>" width:360
                label lblMSHPath "MSH: <asset or output path not set>" width:360
                label lblSKTPath "SKT: <asset or output path not set>" width:360
                button btnCopyMGN "Copy MGN Path" width:110 across:3
                button btnCopyMSH "Copy MSH Path" width:110
                button btnCopySKT "Copy SKT Path" width:110
                button btnOpenFolder "Open Output Folder" width:150 align:#left
        )
        group "Automation" (
                button btnImport "Import Mesh" width:110 across:3
                button btnExport "Export Mesh" width:110
                button btnRoundtrip "Full Roundtrip" width:130
                button btnPrepGeo "Prep Geometry" width:120 across:2
                button btnPrepSkeleton "Prep Skeleton" width:120
                button btnLegacyImporter "Legacy Importer" width:120 across:2
                button btnLegacyExporter "Legacy Exporter" width:120
        )
        group "Skeleton & Animations" (
                label lblSkeletonRoot "Skeleton Root: <not set>" width:360
                button btnSetSkeletonRoot "Use Selection as Skeleton Root" width:200
                editText edtClipName "Clip Name" text:"" width:180 align:#left across:3
                spinner spnClipStart "Start" type:#integer range:[-100000,100000,animationRange.start.frame as integer] align:#left width:80
                spinner spnClipEnd "End" type:#integer range:[-100000,100000,animationRange.end.frame as integer] align:#left width:80
                button btnRegisterClip "Register Clip" width:120 align:#left
                multiListBox lbAnimations "Clips" items:#() height:5 width:350
                button btnBakeClip "Bake Selected Clips" width:160 align:#left
                button btnApplyClipRange "Apply Range From Clip" width:180 align:#right
        )
        on btnCopyMGN pressed do (
                setClipBoardText (swgSuper_GetOutputPath "mgn")
                swgSuper_Log "MGN path copied to clipboard."
        )
        on btnCopyMSH pressed do (
                setClipBoardText (swgSuper_GetOutputPath "msh")
                swgSuper_Log "MSH path copied to clipboard."
        )
        on btnCopySKT pressed do (
                setClipBoardText (swgSuper_GetOutputPath "skt")
                swgSuper_Log "SKT path copied to clipboard."
        )
        on btnOpenFolder pressed do (
                if (SWG_superOutputRoot != undefined and SWG_superOutputRoot != "") then (
                        shellLaunch SWG_superOutputRoot ""
                ) else (
                        messageBox "Set an output folder before opening it." title:"SWG Super Tool"
                )
        )
        on btnImport pressed do swgSuper_RunImporter()
        on btnExport pressed do swgSuper_RunExporter()
        on btnRoundtrip pressed do swgSuper_RunRoundtrip()
        on btnPrepGeo pressed do swgSuper_PrepareGeometry (selection as array)
        on btnPrepSkeleton pressed do swgSuper_PrepareSkeleton (selection as array)
        on btnLegacyImporter pressed do swgSuper_ShowStandaloneImporter()
        on btnLegacyExporter pressed do swgSuper_ShowStandaloneExporter()
        on btnSetSkeletonRoot pressed do (
                if selection.count == 1 then (
                        swgSuper_SetSkeletonRoot selection[1]
                ) else (
                        messageBox "Select exactly one root bone." title:"SWG Super Tool"
                )
        )
        on btnRegisterClip pressed do (
                swgSuper_AddAnimationClip edtClipName.text spnClipStart.value spnClipEnd.value
        )
        on btnBakeClip pressed do (
                swgSuper_BakeSelectedClips (lbAnimations.selection as array)
        )
        on btnApplyClipRange pressed do (
                if lbAnimations.selection.count > 0 then (
                        local idx = lbAnimations.selection[1]
                        if idx >= 1 and idx <= SWG_superAnimationLibrary.count then (
                                local clip = SWG_superAnimationLibrary[idx]
                                swgSuper_SetAnimationRange clip[2] clip[3]
                        )
                ) else (
                        messageBox "Select a clip to apply its range." title:"SWG Super Tool"
                )
        )
        on lbAnimations doubleClicked idx do (
                if idx >= 1 and idx <= SWG_superAnimationLibrary.count then (
                        local clip = SWG_superAnimationLibrary[idx]
                        swgSuper_SetAnimationRange clip[2] clip[3]
                )
        )
        on SWG_superUtilities open do (
                swgSuper_UpdateUtilities()
        )
)

rollout SWG_superDashboard "SWG Content Super Tool" width:420 height:660 (
        label lblTitle "Star Wars Galaxies Content Suite" align:#center
        label lblSubtitle "Importer, exporter, and animation workflow manager" align:#center
        group "Asset" (
                editText edtAssetName "Asset Tag" text:SWG_superAssetName width:240 align:#left
                button btnBrowseOutput "Select Output Folder" width:160 align:#right
                button btnAutoAsset "Smart Configure" width:160 align:#center
                label lblOutput "Output: <not set>" width:360
        )
        group "Automation" (
                button btnPrepGeometry "Prepare Geometry" width:160 across:2
                button btnPrepBones "Prepare Skeleton" width:160
                button btnBakeAnim "Bake Animation" width:160 across:2
                button btnRoundtrip "Roundtrip Export" width:160
                button btnSmartExport "Smart Export" width:160 align:#center
        )
        group "Animation Range" (
                spinner spnStart "Start" type:#integer range:[-100000,100000,animationRange.start.frame as integer] align:#left across:2
                spinner spnEnd "End" type:#integer range:[-100000,100000,animationRange.end.frame as integer]
                button btnApplyRange "Apply Range" width:140
        )
        group "Panels" (
                subRollout srPanels "" width:380 height:360
                button btnShowImporter "Importer" width:120 across:3
                button btnShowExporter "Exporter" width:120
                button btnShowUtilities "Utilities" width:120
        )
        group "Log" (
                label lblStatus "Status: Ready" width:360
                multiLineEditText logOutput "" width:360 height:120 readOnly:true
        )

        on SWG_superDashboard open do (
                SWG_superPanelControl = srPanels
                if SWG_superAssetName != undefined then edtAssetName.text = SWG_superAssetName
                if SWG_superOutputRoot != undefined and SWG_superOutputRoot != "" then (
                        lblOutput.text = "Output: " + SWG_superOutputRoot
                ) else (
                        lblOutput.text = "Output: <not set>"
                )
                swgSuper_ShowRollout srPanels SWG_importRoll
                swgSuper_Log "Ready"
        )
        on btnBrowseOutput pressed do (
                local path = getSavePath caption:"Choose SWG export root"
                if path != undefined do (
                        swgSuper_SetOutputRoot path
                        swgSuper_Log ("Output folder set to " + path)
                )
        )
        on edtAssetName entered text do (
                local sanitized = swgSuper_SetAssetName text
                swgSuper_Log ("Asset tag set to " + sanitized)
        )
        on btnPrepGeometry pressed do (
                swgSuper_PrepareGeometry (selection as array)
        )
        on btnPrepBones pressed do (
                swgSuper_PrepareSkeleton (selection as array)
        )
        on btnBakeAnim pressed do (
                swgSuper_BakeAnimation (selection as array) spnStart.value spnEnd.value
        )
        on btnRoundtrip pressed do swgSuper_RunRoundtrip()
        on btnAutoAsset pressed do (
                swgSuper_AutoConfigureFromScene()
        )
        on btnSmartExport pressed do (
                swgSuper_RunExportWizard true
        )
        on btnApplyRange pressed do (
                swgSuper_SetAnimationRange spnStart.value spnEnd.value
        )
        on btnShowImporter pressed do (
                swgSuper_ShowRollout srPanels SWG_importRoll
                swgSuper_Log "Importer ready"
        )
        on btnShowExporter pressed do (
                swgSuper_ShowRollout srPanels SWG_exportRoll
                swgSuper_Log "Exporter ready"
        )
        on btnShowUtilities pressed do (
                swgSuper_ShowRollout srPanels SWG_superUtilities
                swgSuper_Log "Utilities ready"
        )
)

if SWG_SuperToolFloater != undefined then (
        try(closeRolloutFloater SWG_SuperToolFloater) catch()
)
SWG_SuperToolFloater = newRolloutFloater "SWG Content Super Tool" 440 720 40 40
addRollout SWG_superDashboard SWG_SuperToolFloater

swgSuper_UpdateUtilities()
