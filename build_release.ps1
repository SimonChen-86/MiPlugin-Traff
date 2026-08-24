param (
    [string]$Mode = ""
)

# 确保控制台输出编码为 UTF-8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

function Write-Banner {
    Clear-Host
    Write-Host "================================================================================" -ForegroundColor Cyan
    Write-Host "        ⚡  MiPlugin-Traff (v1.1.2) 插件自动化构建与发布控制台  ⚡" -ForegroundColor Yellow
    Write-Host "   TrafficMonitor 小米智能插座/插线板电力监控插件 (原生 C++ / 纯静态 / 零依赖)" -ForegroundColor Cyan
    Write-Host "================================================================================" -ForegroundColor Cyan
}

function Write-Step {
    param([string]$stepNum, [string]$totalSteps, [string]$title)
    Write-Host ""
    Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host " [$stepNum/$totalSteps] ⚡ $title" -ForegroundColor Cyan
    Write-Host "--------------------------------------------------------------------------------" -ForegroundColor DarkGray
}

function Write-Success {
    param([string]$msg)
    Write-Host " [√] $msg" -ForegroundColor Green
}

function Write-Info {
    param([string]$msg)
    Write-Host " [*] $msg" -ForegroundColor Gray
}

function Write-Warn {
    param([string]$msg)
    Write-Host " [!] $msg" -ForegroundColor Yellow
}

function Write-Err {
    param([string]$msg)
    Write-Host " [×] $msg" -ForegroundColor Red
}

# 1. 查找 MSVC 编译器路径
function Get-MSVCPath {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Err "未找到 Visual Studio 安装检测工具 (vswhere.exe)！"
        Write-Warn "请确认已安装 Visual Studio 2019/2022/2026 或 MSVC C++ 构建工具。"
        return $null
    }

    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath -or -not (Test-Path $vsPath)) {
        Write-Err "未检测到支持 C++ 桌面开发的 MSVC 编译器！"
        return $null
    }

    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvars)) {
        Write-Err "未找到 MSVC 环境配置脚本: $vcvars"
        return $null
    }

    return @{
        VSPath = $vsPath
        Vcvars = $vcvars
    }
}

# 2. 编译特定架构 DLL
function Build-Arch {
    param(
        [string]$arch,
        [string]$vcvarsPath,
        [string]$stepNum,
        [string]$totalSteps
    )

    Write-Step $stepNum $totalSteps "正在编译 $arch 架构生产级 Release DLL..."

    $outDir = "release\$arch"
    if (-not (Test-Path $outDir)) {
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    }

    $srcFiles = "src\XiaomiPlugPlugin.cpp src\MiioClient.cpp src\ConfigManager.cpp src\OptionsDlg.cpp"
    $includes = "/I include"
    $libs = "ws2_32.lib bcrypt.lib user32.lib gdi32.lib comctl32.lib advapi32.lib"
    $cflags = "/utf-8 /std:c++17 /EHsc /MT /O2 /GL /Gy /GF /LD /W3 /D UNICODE /D _UNICODE /D NDEBUG $includes"
    $lflags = "/link /LTCG /OPT:REF /OPT:ICF $libs"

    $resFile = "$outDir\Version.res"
    $dllFile = "$outDir\XiaomiPlugPlugin.dll"

    Write-Info "编译 Windows 版本资源文件 ($resFile)..."
    
    # 写入临时批处理脚本执行
    $tempBat = "$outDir\_build.bat"
    $batScript = "@echo off`r`ncall `"$vcvarsPath`" $arch >nul`r`nrc.exe /nologo /fo `"$resFile`" res\Version.rc`r`nif errorlevel 1 exit /b 1`r`ncl.exe /nologo $cflags /Fo$outDir\ $srcFiles /Fe$dllFile `"$resFile`" $lflags`r`nif errorlevel 1 exit /b 1`r`nexit /b 0`r`n"
    Set-Content -Path $tempBat -Value $batScript -Encoding ASCII

    $proc = Start-Process -FilePath "cmd.exe" -ArgumentList "/c `"$tempBat`"" -NoNewWindow -Wait -PassThru
    Remove-Item $tempBat -Force -ErrorAction SilentlyContinue

    if ($proc.ExitCode -ne 0) {
        Write-Err "$arch 架构 Release DLL 编译失败！(ExitCode: $($proc.ExitCode))"
        return $false
    }

    # 清理临时中间文件
    Remove-Item "$outDir\*.obj", "$outDir\*.exp", "$outDir\*.lib", "$outDir\*.res" -Force -ErrorAction SilentlyContinue

    if (Test-Path $dllFile) {
        $fileInfo = Get-Item $dllFile
        $sizeKb = [math]::Round($fileInfo.Length / 1KB, 2)
        $fileLen = $fileInfo.Length
        Write-Success "$arch Release 编译完成！文件体积: $sizeKb KB ($fileLen 字节)"
        Write-Info "输出路径: $dllFile"
        return $true
    } else {
        Write-Err "未找到生成的 DLL 文件: $dllFile"
        return $false
    }
}

# 3. 清理构建缓存
function Clean-Artifacts {
    Write-Step "1" "1" "正在清理编译缓存与中间文件..."
    Remove-Item *.obj, *.exp, *.lib, *.res, release\*\*.obj, release\*\*.exp, release\*\*.lib, release\*\*.res -Force -ErrorAction SilentlyContinue
    Write-Success "编译缓存与中间文件清理完毕！"
}

# 4. 验证生成的 DLL
function Verify-DLLs {
    Write-Step "1" "1" "正在验证生成的 DLL 插件与导出符号..."
    $dllList = @("release\x64\XiaomiPlugPlugin.dll", "release\x86\XiaomiPlugPlugin.dll")
    $allOk = $true

    foreach ($targetDll in $dllList) {
        if (Test-Path $targetDll) {
            $item = Get-Item $targetDll
            $sizeKb = [math]::Round($item.Length / 1KB, 2)
            Write-Success "发现产物: $targetDll ($sizeKb KB)"
            
            try {
                $pyCheck = python -c "import ctypes; dll = ctypes.CDLL(r'$targetDll'); print(bool(dll.TMPluginGetInstance))" 2>$null
                if ($pyCheck -and $pyCheck.Trim() -eq "True") {
                    Write-Success "  └─ [√] 核心接口 TMPluginGetInstance 导出验证通过"
                } else {
                    Write-Warn "  └─ [!] 无法通过 Python 验证导出符号"
                }
            } catch {
                Write-Info "  └─ (未安装 Python，跳过动态符号调用校验)"
            }
        } else {
            Write-Warn "未找到产物: $targetDll (尚未编译)"
            $allOk = $false
        }
    }
    return $allOk
}

# ==================== 主流程控制 ====================

Write-Banner

# 步骤 1: 环境检查
Write-Step "1" "5" "检测 Windows 开发环境与 MSVC C++ 编译器..."
$msvc = Get-MSVCPath
if (-not $msvc) {
    Write-Err "环境检测失败，构建终止。"
    exit 1
}
Write-Success "成功找到 MSVC 编译环境: $($msvc.VSPath)"

# 步骤 2: 交互式选择菜单 (如果未通过命令行传入 Mode)
if ([string]::IsNullOrWhiteSpace($Mode)) {
    Write-Host ""
    Write-Host "========================== [ 请选择构建操作 ] ==========================" -ForegroundColor Yellow
    Write-Host "  [1] 🚀 一键全量构建 (同时编译 x64 和 x86 架构 Release DLL) [推荐]" -ForegroundColor Green
    Write-Host "  [2] ⚡ 仅构建 x64 架构 Release DLL (适用于 64位 TrafficMonitor)" -ForegroundColor Cyan
    Write-Host "  [3] ⚡ 仅构建 x86 架构 Release DLL (适用于 32位 TrafficMonitor)" -ForegroundColor Cyan
    Write-Host "  [4] 🧹 清理编译中间文件与缓存 (*.obj, *.exp, *.lib, *.res)" -ForegroundColor Gray
    Write-Host "  [5] 🔍 运行 DLL 完整性与导出符号自检" -ForegroundColor Magenta
    Write-Host "  [0] 🚪 退出脚本" -ForegroundColor DarkGray
    Write-Host "========================================================================" -ForegroundColor Yellow
    Write-Host ""

    $choice = Read-Host "请输入操作编号 (0-5，回车默认 [1])"
    if ([string]::IsNullOrWhiteSpace($choice)) { $choice = "1" }
} else {
    $choice = $Mode
}

$buildSuccess = $true

switch ($choice) {
    "1" {
        $ok64 = Build-Arch "x64" $msvc.Vcvars "3" "5"
        $ok86 = Build-Arch "x86" $msvc.Vcvars "4" "5"
        $buildSuccess = ($ok64 -and $ok86)
        if ($buildSuccess) {
            Write-Step "5" "5" "构建产物自检与发布汇总"
            Verify-DLLs | Out-Null
        }
    }
    "2" {
        $buildSuccess = Build-Arch "x64" $msvc.Vcvars "3" "4"
        if ($buildSuccess) {
            Write-Step "4" "4" "构建产物自检"
            Verify-DLLs | Out-Null
        }
    }
    "3" {
        $buildSuccess = Build-Arch "x86" $msvc.Vcvars "3" "4"
        if ($buildSuccess) {
            Write-Step "4" "4" "构建产物自检"
            Verify-DLLs | Out-Null
        }
    }
    "4" {
        Clean-Artifacts
        exit 0
    }
    "5" {
        Verify-DLLs | Out-Null
        exit 0
    }
    "0" {
        Write-Info "已取消操作，退出构建系统。"
        exit 0
    }
    default {
        Write-Err "无效的选项: $choice"
        exit 1
    }
}

# 结束汇总提示
Write-Host ""
if ($buildSuccess) {
    $p64 = "release\x64\XiaomiPlugPlugin.dll"
    $p86 = "release\x86\XiaomiPlugPlugin.dll"
    Write-Host "================================================================================" -ForegroundColor Green
    Write-Host "   🎉 恭喜！MiPlugin-Traff 编译成功，所有发布版 DLL 已就绪！" -ForegroundColor Green
    Write-Host "================================================================================" -ForegroundColor Green
    Write-Host "  📂 x64 插件: $p64" -ForegroundColor White
    Write-Host "  📂 x86 插件: $p86" -ForegroundColor White
    Write-Host ""

    if ([string]::IsNullOrWhiteSpace($Mode)) {
        $openFolder = Read-Host "是否立即在文件资源管理器中打开 release 发布目录？(Y/N，默认 Y)"
        if ([string]::IsNullOrWhiteSpace($openFolder) -or $openFolder.Trim().ToUpper() -eq "Y") {
            Start-Process "explorer.exe" (Resolve-Path "release")
        }
        Write-Host ""
        Read-Host "按回车键退出构建控制台..."
    }
} else {
    Write-Host "================================================================================" -ForegroundColor Red
    Write-Host "   ❌ 编译过程中发生错误，请检查上方的编译器错误输出！" -ForegroundColor Red
    Write-Host "================================================================================" -ForegroundColor Red
    if ([string]::IsNullOrWhiteSpace($Mode)) {
        Write-Host ""
        Read-Host "按回车键退出构建控制台..."
    }
    exit 1
}
