#Requires -Version 5.1
<#
.SYNOPSIS
    LunaChat 一键打包：编译 -> windeployqt -> 拷运行期资源 -> 压缩 zip。

.DESCRIPTION
    产出目录默认 <仓库根>\dist\LunaChat-<Configuration>，内容：
      Client.exe / Server.exe 以及 Qt 运行库、平台/多媒体/数据库插件
      sounds\         四类提示音（message / online / match / activity）
      backgrounds\    聊天与朋友圈背景图
      config.ini      服务端数据库配置模板（首次运行前把密码改成自己的）
      sql\schema.sql  建库脚本
      demo.md         演示脚本（从 docs\demo.md 拷过来，随包分发）

    sounds\ 和 backgrounds\ 缺失不会导致启动失败：SoundPlayer 找不到文件时静默，
    背景图缺失时页面退回纯色背景。

.PARAMETER Configuration
    Debug 或 Release，默认 Release。

.PARAMETER QtBin
    windeployqt.exe 所在目录，默认 D:\qt\5.15.2\msvc2019_64\bin。

.PARAMETER OutDir
    输出目录；留空则用 <仓库根>\dist\LunaChat-<Configuration>。

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\package.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\package.ps1 -Configuration Debug -SkipBuild
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$QtBin = 'D:\qt\5.15.2\msvc2019_64\bin',

    [string]$OutDir = '',

    [switch]$SkipBuild,

    [switch]$NoZip
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
if (-not $OutDir) {
    $OutDir = Join-Path $root (Join-Path 'dist' "LunaChat-$Configuration")
}

function Write-Step([string]$text) {
    Write-Host ''
    Write-Host "==> $text" -ForegroundColor Cyan
}

Write-Host "LunaChat 打包：$Configuration / x64" -ForegroundColor Green
Write-Host "仓库根目录：$root"
Write-Host "输出目录　：$OutDir"

# ---------------------------------------------------------------- 1. 编译
$buildDir = Join-Path $root (Join-Path 'x64' $Configuration)
if (-not $SkipBuild) {
    Write-Step '编译解决方案'

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "找不到 vswhere.exe，请先安装 Visual Studio 2022（含 MSVC v143 工具集）"
    }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if (-not $vsPath) {
        throw 'vswhere 没有找到可用的 Visual Studio 安装'
    }
    $msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'

    & $msbuild (Join-Path $root 'LunaChat.sln') "/p:Configuration=$Configuration" '/p:Platform=x64' /m /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) {
        throw "编译失败（MSBuild 退出码 $LASTEXITCODE）"
    }
}

foreach ($exe in 'Client.exe', 'Server.exe') {
    $path = Join-Path $buildDir $exe
    if (-not (Test-Path $path)) {
        throw "缺少 $path，请先编译 $Configuration|x64（或去掉 -SkipBuild）"
    }
}

# ---------------------------------------------------------------- 2. 输出目录
Write-Step '准备输出目录'

# 只允许清理仓库 dist 目录下的子目录，避免手滑传错 -OutDir 删掉别的东西
$distRoot = (Join-Path $root 'dist') + [System.IO.Path]::DirectorySeparatorChar
$fullOut = [System.IO.Path]::GetFullPath($OutDir)
if (-not $fullOut.StartsWith($distRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "拒绝清理 $fullOut：输出目录必须位于 $distRoot 下"
}
if (Test-Path $fullOut) {
    Remove-Item -LiteralPath $fullOut -Recurse -Force
}
New-Item -ItemType Directory -Path $fullOut -Force | Out-Null

Copy-Item (Join-Path $buildDir 'Client.exe') $fullOut
Copy-Item (Join-Path $buildDir 'Server.exe') $fullOut

# ---------------------------------------------------------------- 3. windeployqt
Write-Step '部署 Qt 运行库与插件'

$windeployqt = Join-Path $QtBin 'windeployqt.exe'
if (-not (Test-Path $windeployqt)) {
    throw "找不到 $windeployqt，请用 -QtBin 指定 Qt 的 bin 目录"
}

$qtMode = if ($Configuration -eq 'Release') { '--release' } else { '--debug' }
$commonArgs = @($qtMode, '--no-translations', '--no-quick-import', "--dir=$fullOut")

# 先部署客户端（会带出 multimedia / imageformats / platforms），再补服务端的 sqldrivers
& $windeployqt @commonArgs (Join-Path $fullOut 'Client.exe')
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt（Client）失败，退出码 $LASTEXITCODE"
}
& $windeployqt @commonArgs (Join-Path $fullOut 'Server.exe')
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt（Server）失败，退出码 $LASTEXITCODE"
}

# Qt 的 qsqlmysql 插件依赖 MySQL 官方客户端库，windeployqt 不会自动带
$mysqlClient = Join-Path $QtBin 'libmysql.dll'
if (-not (Test-Path $mysqlClient)) {
    $mysqlClient = Join-Path $root 'runtime\libmysql.dll'
}
if (Test-Path $mysqlClient) {
    Copy-Item $mysqlClient $fullOut -Force
    Write-Host '    libmysql.dll 已随包（MySQL 驱动依赖）'
} else {
    Write-Warning '缺少 libmysql.dll：目标机器上 Server.exe 会连不上 MySQL，请手动放到程序目录'
}

# ---------------------------------------------------------------- 4. 运行期资源
Write-Step '拷贝运行期资源（sounds / backgrounds / sql / config）'

# 资源优先取仓库里的 runtime\（随仓库维护、和编译配置无关），其次取编译输出目录
$soundsSrc = Join-Path $root 'runtime\sounds'
if (-not (Test-Path $soundsSrc)) {
    $soundsSrc = Join-Path $buildDir 'sounds'
}
$soundsDst = Join-Path $fullOut 'sounds'
New-Item -ItemType Directory -Path $soundsDst -Force | Out-Null
if (Test-Path $soundsSrc) {
    Get-ChildItem $soundsSrc -File | Copy-Item -Destination $soundsDst -Force
    $soundCount = (Get-ChildItem $soundsDst -File).Count
    Write-Host "    sounds\  $soundCount 个文件（来源 $soundsSrc）"
    if ($soundCount -eq 0) {
        Write-Host '    警告：没有提示音文件，运行期 SoundPlayer 会静默' -ForegroundColor Yellow
    }
}

$bgSrc = Join-Path $root 'runtime\backgrounds'
if (-not (Test-Path $bgSrc)) {
    $bgSrc = Join-Path $buildDir 'backgrounds'
}
$bgDst = Join-Path $fullOut 'backgrounds'
New-Item -ItemType Directory -Path $bgDst -Force | Out-Null
if (Test-Path $bgSrc) {
    Get-ChildItem $bgSrc -File | Copy-Item -Destination $bgDst -Force
    Write-Host ("    backgrounds\  " + (Get-ChildItem $bgDst -File).Count + ' 个文件')
}

# 运行时会往这些目录写东西，先建好空目录，避免首次操作报"目录不存在"
foreach ($dir in 'avatars', 'group_avatars', 'moments', 'group_files', 'received') {
    New-Item -ItemType Directory -Path (Join-Path $fullOut $dir) -Force | Out-Null
}

$sqlDst = Join-Path $fullOut 'sql'
New-Item -ItemType Directory -Path $sqlDst -Force | Out-Null
Copy-Item (Join-Path $root 'sql\schema.sql') $sqlDst -Force

# config.ini 含本机数据库密码，只带模板；本地已有配置则原样使用
$configDst = Join-Path $fullOut 'config.ini'
if (Test-Path $configDst) {
    Write-Host '    config.ini 已存在，保留原文件（不覆盖密码）'
} else {
    Copy-Item (Join-Path $root 'Server\config.ini.example') $configDst -Force
    Write-Host '    config.ini 由模板生成，运行 Server.exe 前请填写数据库密码'
}

$demoSrc = Join-Path $root 'docs\demo.md'
if (Test-Path $demoSrc) {
    Copy-Item $demoSrc (Join-Path $fullOut 'demo.md') -Force
}

# ---------------------------------------------------------------- 5. 压缩
$totalMb = [Math]::Round(((Get-ChildItem $fullOut -Recurse -File | Measure-Object Length -Sum).Sum / 1MB), 1)
Write-Step "打包完成：$fullOut（$totalMb MB）"

if (-not $NoZip) {
    $zipPath = "$fullOut.zip"
    if (Test-Path $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Compress-Archive -Path $fullOut -DestinationPath $zipPath -CompressionLevel Optimal
    $zipMb = [Math]::Round(((Get-Item $zipPath).Length / 1MB), 1)
    Write-Host "    压缩包：$zipPath（$zipMb MB）" -ForegroundColor Green
}

Write-Host ''
Write-Host '运行方式：' -ForegroundColor Yellow
Write-Host "  1) 先确保 MySQL 已建库：mysql ... -e `"source $sqlDst\schema.sql`""
Write-Host '  2) 改 config.ini 里的数据库密码'
Write-Host '  3) 启动 Server.exe 与 Client.exe（可开两个客户端演示双人流程）'
