# Read toolpaths.mk and regenerate .vscode configs for an MSPM0 project.
# Usage (from project root):
#   mingw32-make apply-paths
# Or:
#   powershell -File ../common/scripts/apply_toolpaths.ps1 -ProjectRoot .
param(
    [string]$ProjectRoot = "",
    [string]$ToolpathsFile = ""
)

$ErrorActionPreference = "Stop"

if (-not $ProjectRoot) {
    $ProjectRoot = (Get-Location).Path
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

if (-not $ToolpathsFile) {
    # Prefer repo root (parent of project) toolpaths.mk
    $candidate = Join-Path (Split-Path -Parent $ProjectRoot) "toolpaths.mk"
    if (Test-Path -LiteralPath $candidate) {
        $ToolpathsFile = $candidate
    } else {
        $ToolpathsFile = Join-Path $ProjectRoot "toolpaths.mk"
    }
}
$ToolpathsFile = (Resolve-Path -LiteralPath $ToolpathsFile).Path

function Read-ToolPaths([string]$path) {
    $map = @{}
    Get-Content -LiteralPath $path | ForEach-Object {
        $line = $_.Trim()
        if (-not $line -or $line.StartsWith("#")) { return }
        if ($line -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)\s*$') {
            $map[$Matches[1]] = $Matches[2].Trim().Trim('"').Trim("'")
        }
    }
    return $map
}

function To-Fwd([string]$p) { if (-not $p) { return $p }; return ($p -replace '\\', '/') }
function To-Back([string]$p) { if (-not $p) { return $p }; return ($p -replace '/', '\') }

$p = Read-ToolPaths $ToolpathsFile
$req = @('GCC_PATH','SDK','SYSCONFIG_ROOT','JLINK_ROOT','MAKE_BIN')
foreach ($k in $req) {
    if (-not $p.ContainsKey($k) -or -not $p[$k]) { throw "Missing $k in toolpaths.mk" }
}

$gcc  = To-Fwd $p['GCC_PATH']
$sdk  = To-Fwd $p['SDK']
$sys  = To-Fwd $p['SYSCONFIG_ROOT']
$jl   = To-Fwd $p['JLINK_ROOT']
$make = To-Fwd $p['MAKE_BIN']
$cmake = if ($p['CMAKE_BIN']) { To-Fwd $p['CMAKE_BIN'] } else { '' }
$ninja = if ($p['NINJA_BIN']) { To-Fwd $p['NINJA_BIN'] } else { '' }
$ocd   = if ($p['OPENOCD_BIN']) { To-Fwd $p['OPENOCD_BIN'] } else { '' }

$gccB  = To-Back $gcc
$sdkB  = To-Back $sdk
$sysB  = To-Back $sys
$jlB   = To-Back $jl
$makeB = To-Back $make

$svd = To-Fwd $p['SVD_FILE']
if (-not $svd) {
    $extRoot = Join-Path $env:USERPROFILE ".vscode\extensions"
    $hit = Get-ChildItem -LiteralPath $extRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like 'ti-development-tools.cortex-debug-dp-mspm0*' } |
        Sort-Object Name -Descending | Select-Object -First 1
    if ($hit) {
        $cand = Join-Path $hit.FullName "data\MSPM0G350X.svd"
        if (Test-Path -LiteralPath $cand) { $svd = To-Fwd $cand }
    }
}
if (-not $svd) {
    $svd = ""
}

$pathParts = @(
    $makeB,
    (Join-Path $gccB "bin"),
    $sysB,
    $jlB
)
if ($cmake) { $pathParts += (To-Back $cmake) }
if ($ninja) { $pathParts += (To-Back $ninja) }
if ($ocd)   { $pathParts += (To-Back $ocd) }
$pathParts += '${env:Path}'
$pathEnv = ($pathParts -join ';')

$taskPath  = ($makeB + ';' + (Join-Path $gccB 'bin') + ';' + $sysB + ';' + $jlB + ';${env:Path}')
$flashPath = ($makeB + ';' + (Join-Path $gccB 'bin') + ';' + $jlB + ';${env:Path}')
$sysPath   = ($sysB + ';' + $makeB + ';${env:Path}')
$cleanPath = ($makeB + ';${env:Path}')

# Project-specific include dirs for IntelliSense
$includePath = [System.Collections.Generic.List[string]]::new()
$includePath.Add('${workspaceFolder}/**') | Out-Null
$includePath.Add('${workspaceFolder}') | Out-Null
foreach ($rel in @('Function/Inc', 'Hardware/Inc', 'Hardware')) {
    $full = Join-Path $ProjectRoot ($rel -replace '/', '\')
    if (Test-Path -LiteralPath $full) {
        $includePath.Add(('${workspaceFolder}/' + $rel)) | Out-Null
    }
}
$includePath.Add("$sdk/source") | Out-Null
$includePath.Add("$sdk/source/third_party/CMSIS/Core/Include") | Out-Null
$includePath.Add("$gcc/arm-none-eabi/include") | Out-Null

# Detect existing Keil tasks/launch to merge (e.g. gimbal)
$vscode = Join-Path $ProjectRoot ".vscode"
New-Item -ItemType Directory -Path $vscode -Force | Out-Null

$existingTasksPath  = Join-Path $vscode "tasks.json"
$existingLaunchPath = Join-Path $vscode "launch.json"
$keilTasks = @()
$keilLaunch = @()

function Get-JsonObject([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    $raw = Get-Content -LiteralPath $path -Raw -Encoding UTF8
    # Strip BOM / comments-ish by parsing leniently
    try { return ($raw | ConvertFrom-Json) } catch { return $null }
}

$oldTasks = Get-JsonObject $existingTasksPath
if ($oldTasks -and $oldTasks.tasks) {
    foreach ($t in $oldTasks.tasks) {
        if ($t.label -and ($t.label -like 'Keil*' -or $t.command -eq 'UV4')) {
            $keilTasks += $t
        }
    }
}
$oldLaunch = Get-JsonObject $existingLaunchPath
if ($oldLaunch -and $oldLaunch.configurations) {
    foreach ($c in $oldLaunch.configurations) {
        # Keep configs that target Keil .axf or are named Keil
        $exe = [string]$c.executable
        $name = [string]$c.name
        if ($exe -like '*.axf' -or $name -like '*Keil*' -or ($c.toolchainPrefix -eq 'armclang')) {
            $keilLaunch += $c
        }
    }
}

# settings.json
$settings = [ordered]@{
    "files.associations" = [ordered]@{ "*.h" = "c"; "*.c" = "c" }
    "cortex-debug.armToolchainPath" = "$gcc/bin"
    "cortex-debug.gdbPath" = "$gcc/bin/arm-none-eabi-gdb.exe"
    "cortex-debug.JLinkGDBServerPath" = "$jl/JLinkGDBServerCL.exe"
    "terminal.integrated.env.windows" = [ordered]@{ Path = $pathEnv }
}
$settings | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $vscode "settings.json") -Encoding utf8

# c_cpp_properties.json
$cpp = [ordered]@{
    configurations = @(
        [ordered]@{
            name = "MSPM0G3507"
            includePath = @($includePath.ToArray())
            defines = @("__MSPM0G3507__", "DeviceFamily_MSPM0G1X0X_G3X0X")
            compilerPath = "$gcc/bin/arm-none-eabi-gcc.exe"
            cStandard = "c99"
            intelliSenseMode = "gcc-arm"
        }
    )
    version = 4
}
$cpp | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $vscode "c_cpp_properties.json") -Encoding utf8

# launch.json — GCC primary + optional Keil configs
$gccLaunch = @(
    [ordered]@{
        name = "Debug (GCC J-Link)"
        cwd = '${workspaceFolder}'
        executable = '${workspaceFolder}/build/app.out'
        request = "launch"
        type = "cortex-debug"
        runToEntryPoint = "main"
        servertype = "jlink"
        device = "MSPM0G3507"
        interface = "swd"
        serverpath = "$jl/JLinkGDBServerCL.exe"
        gdbPath = "$gcc/bin/arm-none-eabi-gdb.exe"
        armToolchainPath = "$gcc/bin"
        svdFile = $svd
        preLaunchTask = "build (GCC)"
    }
    [ordered]@{
        name = "Attach (GCC J-Link)"
        cwd = '${workspaceFolder}'
        executable = '${workspaceFolder}/build/app.out'
        request = "attach"
        type = "cortex-debug"
        servertype = "jlink"
        device = "MSPM0G3507"
        interface = "swd"
        serverpath = "$jl/JLinkGDBServerCL.exe"
        gdbPath = "$gcc/bin/arm-none-eabi-gdb.exe"
        armToolchainPath = "$gcc/bin"
        svdFile = $svd
    }
)

$allLaunch = @($gccLaunch)
foreach ($k in $keilLaunch) { $allLaunch += $k }

$launch = [ordered]@{
    version = "0.2.0"
    configurations = $allLaunch
}
$launch | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $vscode "launch.json") -Encoding utf8

# tasks.json — GCC default + optional Keil
$gccTasks = @(
    [ordered]@{
        label = "build (GCC)"
        type = "shell"
        command = "mingw32-make"
        args = @("-j8")
        options = [ordered]@{ cwd = '${workspaceFolder}'; env = [ordered]@{ Path = $taskPath } }
        group = [ordered]@{ kind = "build"; isDefault = $true }
        problemMatcher = @('$gcc')
    }
    [ordered]@{
        label = "clean (GCC)"
        type = "shell"
        command = "mingw32-make"
        args = @("clean")
        options = [ordered]@{ cwd = '${workspaceFolder}'; env = [ordered]@{ Path = $cleanPath } }
        problemMatcher = @()
    }
    [ordered]@{
        label = "syscfg"
        type = "shell"
        command = "mingw32-make"
        args = @("syscfg")
        options = [ordered]@{ cwd = '${workspaceFolder}'; env = [ordered]@{ Path = $sysPath } }
        problemMatcher = @()
    }
    [ordered]@{
        label = "syscfg-gui"
        type = "shell"
        command = (To-Back "$sys/sysconfig_gui.bat")
        args = @(
            "--product"
            "$sdk/.metadata/product.json"
            "--compiler"
            "gcc"
            "--output"
            '${workspaceFolder}'
            '${workspaceFolder}/empty.syscfg'
        )
        options = [ordered]@{ cwd = '${workspaceFolder}' }
        problemMatcher = @()
        presentation = [ordered]@{ reveal = "silent"; panel = "shared" }
    }
    [ordered]@{
        label = "flash (GCC)"
        type = "shell"
        command = "mingw32-make"
        args = @("flash")
        options = [ordered]@{ cwd = '${workspaceFolder}'; env = [ordered]@{ Path = $flashPath } }
        problemMatcher = @()
        dependsOn = @("build (GCC)")
    }
    [ordered]@{
        label = "apply-paths"
        type = "shell"
        command = "powershell"
        args = @(
            "-NoProfile"
            "-ExecutionPolicy"
            "Bypass"
            "-File"
            '${workspaceFolder}/../common/scripts/apply_toolpaths.ps1'
            "-ProjectRoot"
            '${workspaceFolder}'
            "-ToolpathsFile"
            '${workspaceFolder}/../toolpaths.mk'
        )
        options = [ordered]@{ cwd = '${workspaceFolder}' }
        problemMatcher = @()
    }
)

# If Keil tasks exist, clear their isDefault so GCC remains default build
$mergedKeilTasks = @()
foreach ($t in $keilTasks) {
    # Convert PSCustomObject to ordered hashtable-like via JSON roundtrip
    $j = $t | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    if ($j.group) {
        $j.group | Add-Member -NotePropertyName isDefault -NotePropertyValue $false -Force
    }
    $mergedKeilTasks += $j
}

$allTasks = @($gccTasks) + $mergedKeilTasks
$tasks = [ordered]@{
    version = "2.0.0"
    tasks = $allTasks
}
$tasks | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $vscode "tasks.json") -Encoding utf8

# extensions.json
$ext = [ordered]@{
    recommendations = @(
        "ms-vscode.cpptools"
        "ms-vscode.cpptools-extension-pack"
        "marus25.cortex-debug"
        "ti-development-tools.cortex-debug-dp-mspm0"
        "ms-vscode.vscode-serial-monitor"
        "ms-vscode.hexeditor"
        "dan-c-underwood.arm"
    )
}
$ext | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $vscode "extensions.json") -Encoding utf8

# flash.jlink
@'
r
h
loadfile build/app.out
r
g
qc
'@ | Set-Content -LiteralPath (Join-Path $vscode "flash.jlink") -Encoding ascii

Write-Host "Applied toolpaths from $ToolpathsFile"
Write-Host "  Project: $ProjectRoot"
Write-Host "  GCC : $gcc"
Write-Host "  SDK : $sdk"
Write-Host "  SYS : $sys"
Write-Host "  JLink: $jl"
Write-Host "  SVD : $svd"
if ($keilTasks.Count -gt 0 -or $keilLaunch.Count -gt 0) {
    Write-Host "  Merged Keil tasks: $($keilTasks.Count), launch: $($keilLaunch.Count)"
}
Write-Host "Updated: $vscode"
