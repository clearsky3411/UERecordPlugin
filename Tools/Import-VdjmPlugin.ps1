param(
    [string]$SourceRepo = "",
    [string]$TargetPluginDir = "",
    [string]$Ref = "main",
    [switch]$CleanTarget,
    [switch]$RunP4Reconcile,
    [switch]$RunOnce,
    [switch]$Force
)

$script:ToolTitle = "VD-JM PLUGIN IMPORT TERMINAL"
$script:PluginName = "VdjmMobileUi"
$script:DefaultSourceRepo = Split-Path -Parent $PSScriptRoot
$script:SourceRepo = if ([string]::IsNullOrWhiteSpace($SourceRepo)) { $script:DefaultSourceRepo } else { $SourceRepo }
$script:TargetPluginDir = $TargetPluginDir
$script:Ref = $Ref
$script:CleanTarget = [bool]$CleanTarget
$script:RunP4Reconcile = [bool]$RunP4Reconcile
$script:LastMessage = "Ready"
$script:ExcludeNames = @(".git", ".vs", "Binaries", "Intermediate", "Saved", "DerivedDataCache")
$script:PreserveTargetNames = @(".p4ignore")

function Set-VdjmImportLastMessage {
    param([string]$Message)

    $script:LastMessage = ("{0:HH:mm:ss}  {1}" -f (Get-Date), $Message)
}

function Get-VdjmImportFullPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Test-VdjmImportCommand {
    param([Parameter(Mandatory = $true)][string]$Name)

    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Test-VdjmImportRemoteRepo {
    param([Parameter(Mandatory = $true)][string]$Repo)

    return $Repo -match "^(https?|ssh)://" -or
        $Repo -match "^[^@\s]+@[^:\s]+:.+\.git$" -or
        $Repo.EndsWith(".git", [System.StringComparison]::OrdinalIgnoreCase)
}

function Test-VdjmImportLocalGitRepo {
    param([Parameter(Mandatory = $true)][string]$Repo)

    if (-not (Test-Path -LiteralPath $Repo)) {
        return $false
    }

    $gitPath = Join-Path $Repo ".git"
    return Test-Path -LiteralPath $gitPath
}

function Test-VdjmImportTargetSafety {
    param(
        [Parameter(Mandatory = $true)][string]$TargetDir,
        [switch]$ThrowOnError
    )

    if ([string]::IsNullOrWhiteSpace($TargetDir)) {
        if ($ThrowOnError) { throw "Target plugin directory is empty." }
        return $false
    }

    $fullTarget = Get-VdjmImportFullPath $TargetDir
    $root = [System.IO.Path]::GetPathRoot($fullTarget)
    if ($fullTarget.TrimEnd("\") -eq $root.TrimEnd("\")) {
        if ($ThrowOnError) { throw "Target plugin directory resolves to a drive root: $fullTarget" }
        return $false
    }

    if ($fullTarget -notmatch "\\Plugins\\[^\\]+$") {
        if ($ThrowOnError) { throw "Target must look like ...\Plugins\PluginName. Current: $fullTarget" }
        return $false
    }

    return $true
}

function Invoke-VdjmImportGit {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)

    & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "git command failed: git $($Arguments -join ' ')"
    }
}

function Copy-VdjmImportDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$SourceDir,
        [Parameter(Mandatory = $true)][string]$TargetDir
    )

    if (-not (Test-Path -LiteralPath $TargetDir)) {
        New-Item -ItemType Directory -Path $TargetDir -Force | Out-Null
    }

    foreach ($child in Get-ChildItem -LiteralPath $SourceDir -Force) {
        if ($script:ExcludeNames -contains $child.Name) {
            continue
        }

        $targetPath = Join-Path $TargetDir $child.Name
        if ($child.PSIsContainer) {
            Copy-VdjmImportDirectory -SourceDir $child.FullName -TargetDir $targetPath
        }
        else {
            Copy-Item -LiteralPath $child.FullName -Destination $targetPath -Force
        }
    }
}

function Clear-VdjmImportTargetDirectory {
    param([Parameter(Mandatory = $true)][string]$TargetDir)

    Test-VdjmImportTargetSafety -TargetDir $TargetDir -ThrowOnError | Out-Null

    if (-not (Test-Path -LiteralPath $TargetDir)) {
        New-Item -ItemType Directory -Path $TargetDir -Force | Out-Null
        return
    }

    foreach ($child in Get-ChildItem -LiteralPath $TargetDir -Force) {
        if ($script:PreserveTargetNames -contains $child.Name) {
            continue
        }

        Remove-Item -LiteralPath $child.FullName -Recurse -Force
    }
}

function New-VdjmImportExport {
    param(
        [Parameter(Mandatory = $true)][string]$Repo,
        [Parameter(Mandatory = $true)][string]$RefName,
        [Parameter(Mandatory = $true)][string]$TempRoot
    )

    if (-not (Test-VdjmImportCommand "git")) {
        throw "git is not available in PATH."
    }

    $exportDir = Join-Path $TempRoot "export"
    New-Item -ItemType Directory -Path $exportDir -Force | Out-Null

    $repoDir = Join-Path $TempRoot "repo"
    if ((Test-VdjmImportRemoteRepo $Repo) -or (Test-VdjmImportLocalGitRepo $Repo)) {
        Invoke-VdjmImportGit -Arguments @("clone", $Repo, $repoDir)
        if (-not [string]::IsNullOrWhiteSpace($RefName)) {
            Invoke-VdjmImportGit -Arguments @("-C", $repoDir, "checkout", $RefName)
        }

        $commit = (& git -C $repoDir rev-parse HEAD).Trim()
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to resolve imported git commit."
        }

        $branch = (& git -C $repoDir rev-parse --abbrev-ref HEAD).Trim()
        if ($LASTEXITCODE -ne 0) {
            $branch = "unknown"
        }

        Copy-VdjmImportDirectory -SourceDir $repoDir -TargetDir $exportDir
        return [pscustomobject]@{
            ExportDir = $exportDir
            SourceKind = "git"
            SourceRepo = $Repo
            Ref = $RefName
            Commit = $commit
            Branch = $branch
        }
    }

    if (-not (Test-Path -LiteralPath $Repo)) {
        throw "Source repo/path does not exist: $Repo"
    }

    Copy-VdjmImportDirectory -SourceDir (Get-VdjmImportFullPath $Repo) -TargetDir $exportDir
    return [pscustomobject]@{
        ExportDir = $exportDir
        SourceKind = "folder"
        SourceRepo = $Repo
        Ref = "working-tree"
        Commit = "not-a-git-export"
        Branch = "not-a-git-export"
    }
}

function Write-VdjmImportStamp {
    param(
        [Parameter(Mandatory = $true)]$ExportInfo,
        [Parameter(Mandatory = $true)][string]$TargetDir
    )

    $docsDir = Join-Path $TargetDir "Docs"
    if (-not (Test-Path -LiteralPath $docsDir)) {
        New-Item -ItemType Directory -Path $docsDir -Force | Out-Null
    }

    $stampPath = Join-Path $docsDir "ImportedFromGit.md"
    $stamp = @"
# VdjmMobileUi Import Stamp

- ImportedAt: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz")
- SourceKind: $($ExportInfo.SourceKind)
- SourceRepo: $($ExportInfo.SourceRepo)
- Ref: $($ExportInfo.Ref)
- Branch: $($ExportInfo.Branch)
- Commit: $($ExportInfo.Commit)
- TargetPluginDir: $TargetDir

This file is generated by Tools/Import-VdjmPlugin.ps1 so a Perforce vendor copy can be traced back to its Git source.
"@
    Set-Content -Path $stampPath -Value $stamp -Encoding UTF8
}

function Invoke-VdjmImportP4Reconcile {
    param([Parameter(Mandatory = $true)][string]$TargetDir)

    if (-not (Test-VdjmImportCommand "p4")) {
        throw "p4 is not available in PATH."
    }

    & p4 reconcile "$TargetDir\..."
    if ($LASTEXITCODE -ne 0) {
        throw "p4 reconcile failed."
    }
}

function Invoke-VdjmPluginImport {
    param(
        [Parameter(Mandatory = $true)][string]$Repo,
        [Parameter(Mandatory = $true)][string]$TargetDir,
        [Parameter(Mandatory = $true)][string]$RefName,
        [bool]$ShouldCleanTarget,
        [bool]$ShouldRunP4,
        [bool]$AssumeYes
    )

    Test-VdjmImportTargetSafety -TargetDir $TargetDir -ThrowOnError | Out-Null
    $fullTarget = Get-VdjmImportFullPath $TargetDir

    if ($ShouldCleanTarget -and -not $AssumeYes) {
        throw "CleanTarget requires -Force in non-interactive mode."
    }

    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("VdjmPluginImport_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    try {
        Write-Host " [SYSTEM] Source export is being prepared..." -ForegroundColor Yellow
        $exportInfo = New-VdjmImportExport -Repo $Repo -RefName $RefName -TempRoot $tempRoot

        if ($ShouldCleanTarget) {
            Write-Host " [SYSTEM] Cleaning target directory..." -ForegroundColor Yellow
            Clear-VdjmImportTargetDirectory -TargetDir $fullTarget
        }
        elseif (-not (Test-Path -LiteralPath $fullTarget)) {
            New-Item -ItemType Directory -Path $fullTarget -Force | Out-Null
        }

        Write-Host " [SYSTEM] Copying plugin files..." -ForegroundColor Yellow
        Copy-VdjmImportDirectory -SourceDir $exportInfo.ExportDir -TargetDir $fullTarget
        Write-VdjmImportStamp -ExportInfo $exportInfo -TargetDir $fullTarget

        if ($ShouldRunP4) {
            Write-Host " [SYSTEM] Running p4 reconcile..." -ForegroundColor Yellow
            Invoke-VdjmImportP4Reconcile -TargetDir $fullTarget
        }

        Set-VdjmImportLastMessage ("Imported {0} @ {1}" -f $exportInfo.SourceRepo, $exportInfo.Commit)
        Write-Host "`n [SYSTEM] Import complete." -ForegroundColor Cyan
        Write-Host ("  Commit : {0}" -f $exportInfo.Commit) -ForegroundColor Green
        Write-Host ("  Target : {0}" -f $fullTarget) -ForegroundColor Green
    }
    finally {
        if (Test-Path -LiteralPath $tempRoot) {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force
        }
    }
}

function Write-VdjmImportIntro {
    Clear-Host
    Write-Host "`n ATDT VDJM-PLUGIN-IMPORT" -ForegroundColor Cyan
    Write-Host " CONNECT 9600/ARQ/V32/LAPM`n" -ForegroundColor Cyan
    Start-Sleep -Milliseconds 450
    Write-Host " 호스트 서버에 접속하고 있습니다..." -ForegroundColor Yellow
    Start-Sleep -Milliseconds 450
}

function Write-VdjmImportHeader {
    Clear-Host
    Write-Host "====================================================================" -ForegroundColor Cyan
    Write-Host "    ██╗   ██╗██████╗      ██╗███╗   ███╗" -ForegroundColor Cyan
    Write-Host "    ██║   ██║██╔══██╗     ██║████╗ ████║" -ForegroundColor Cyan
    Write-Host "    ██║   ██║██║  ██║     ██║██╔████╔██║" -ForegroundColor Cyan
    Write-Host "    ╚██╗ ██╔╝██║  ██║██   ██║██║╚██╔╝██║" -ForegroundColor Cyan
    Write-Host "     ╚████╔╝ ██████╔╝╚█████╔╝██║ ╚═╝ ██║" -ForegroundColor Cyan
    Write-Host "      ╚═══╝  ╚═════╝  ╚════╝ ╚═╝     ╚═╝  VENDOR LINK V1" -ForegroundColor Cyan
    Write-Host "`n            [ $script:ToolTitle ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
}

function Show-VdjmImportSettings {
    $sourceText = if ([string]::IsNullOrWhiteSpace($script:SourceRepo)) { "[설정되지 않음]" } else { $script:SourceRepo }
    $targetText = if ([string]::IsNullOrWhiteSpace($script:TargetPluginDir)) { "[설정되지 않음]" } else { $script:TargetPluginDir }
    $refText = if ([string]::IsNullOrWhiteSpace($script:Ref)) { "[default]" } else { $script:Ref }

    Write-Host "  [1] Git/Folder 원본 설정  : " -NoNewline; Write-Host $sourceText -ForegroundColor Green
    Write-Host "  [2] 회사 Plugin 경로 설정 : " -NoNewline; Write-Host $targetText -ForegroundColor Green
    Write-Host "  [3] Git Ref 설정          : " -NoNewline; Write-Host $refText -ForegroundColor Green
    Write-Host "  [4] Target Clean          : " -NoNewline; Write-Host $script:CleanTarget -ForegroundColor Green
    Write-Host "  [5] p4 reconcile          : " -NoNewline; Write-Host $script:RunP4Reconcile -ForegroundColor Green
    Write-Host "`n  [6] 설정 검증 [Preview]"
    Write-Host "  [7] 플러그인 Import 실행 [Execute]"
    Write-Host "  [8] p4 reconcile만 실행"
    Write-Host "  [0] 접속 종료 [Logoff]`n"
    Write-Host "--------------------------------------------------------------------"
    Write-Host ("  Last: {0}" -f $script:LastMessage) -ForegroundColor DarkGray
    Write-Host "--------------------------------------------------------------------"
}

function Select-VdjmImportFolder {
    param([string]$Description)

    Add-Type -AssemblyName System.Windows.Forms
    $folderBrowser = New-Object System.Windows.Forms.FolderBrowserDialog
    $folderBrowser.Description = $Description
    $folderBrowser.ShowNewFolderButton = $true

    $result = $folderBrowser.ShowDialog()
    if ($result -eq [System.Windows.Forms.DialogResult]::OK) {
        return $folderBrowser.SelectedPath
    }

    return ""
}

function Set-VdjmImportSourceRepoInteractive {
    Clear-Host
    Write-Host "===================================================================="
    Write-Host "                   [ 1. 원본 Git/Folder 설정 ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
    Write-Host " 경로 또는 remote URL을 입력하세요."
    Write-Host " 빈칸으로 엔터를 누르면 폴더 선택창을 엽니다.`n"
    $value = Read-Host "▶ SourceRepo"
    if ([string]::IsNullOrWhiteSpace($value)) {
        $value = Select-VdjmImportFolder "원본 VdjmMobileUi Git 폴더를 선택하세요."
    }

    if (-not [string]::IsNullOrWhiteSpace($value)) {
        $script:SourceRepo = $value
        Set-VdjmImportLastMessage "SourceRepo updated"
    }
}

function Set-VdjmImportTargetInteractive {
    Clear-Host
    Write-Host "===================================================================="
    Write-Host "                   [ 2. 회사 Plugin 경로 설정 ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
    Write-Host " 예: D:\CompanyProject\Plugins\VdjmMobileUi"
    Write-Host " 빈칸으로 엔터를 누르면 폴더 선택창을 엽니다.`n"
    $value = Read-Host "▶ TargetPluginDir"
    if ([string]::IsNullOrWhiteSpace($value)) {
        $value = Select-VdjmImportFolder "회사 프로젝트의 Plugins\VdjmMobileUi 폴더를 선택하세요."
    }

    if (-not [string]::IsNullOrWhiteSpace($value)) {
        $script:TargetPluginDir = $value
        Set-VdjmImportLastMessage "TargetPluginDir updated"
    }
}

function Set-VdjmImportRefInteractive {
    Clear-Host
    Write-Host "===================================================================="
    Write-Host "                   [ 3. Git Ref 설정 ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
    Write-Host " branch, tag, commit hash, HEAD 모두 가능합니다."
    Write-Host " local folder export라면 이 값은 기록용에 가깝습니다.`n"
    $value = Read-Host "▶ Ref"
    if (-not [string]::IsNullOrWhiteSpace($value)) {
        $script:Ref = $value
        Set-VdjmImportLastMessage "Ref updated"
    }
}

function Show-VdjmImportPreview {
    Clear-Host
    Write-Host "===================================================================="
    Write-Host "                   [ 6. Import Preview ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
    Write-Host (" SourceRepo      : {0}" -f $script:SourceRepo)
    Write-Host (" TargetPluginDir : {0}" -f $script:TargetPluginDir)
    Write-Host (" Ref             : {0}" -f $script:Ref)
    Write-Host (" CleanTarget     : {0}" -f $script:CleanTarget)
    Write-Host (" RunP4Reconcile  : {0}" -f $script:RunP4Reconcile)
    Write-Host (" Excludes        : {0}" -f ($script:ExcludeNames -join ", "))
    Write-Host ""

    try {
        Test-VdjmImportTargetSafety -TargetDir $script:TargetPluginDir -ThrowOnError | Out-Null
        Write-Host " [SYSTEM] Target path safety check passed." -ForegroundColor Green
    }
    catch {
        Write-Host (" [ERROR] {0}" -f $_.Exception.Message) -ForegroundColor Red
    }

    Read-Host "`n엔터를 누르면 메인 메뉴로 돌아갑니다..."
}

function Invoke-VdjmImportInteractive {
    Clear-Host
    Write-Host "===================================================================="
    Write-Host "                   [ 7. Import Execute ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
    Write-Host " 아래 대상에 플러그인을 복사합니다." -ForegroundColor Yellow
    Write-Host (" Target: {0}" -f $script:TargetPluginDir) -ForegroundColor Green
    if ($script:CleanTarget) {
        Write-Host "`n [주의] Target Clean이 켜져 있어 대상 폴더 내용이 먼저 정리됩니다." -ForegroundColor Red
        $confirmClean = Read-Host " 계속하려면 CLEAN 입력"
        if ($confirmClean -ne "CLEAN") {
            Set-VdjmImportLastMessage "Import cancelled before clean"
            return
        }
    }

    $confirm = Read-Host " 실행하려면 IMPORT 입력"
    if ($confirm -ne "IMPORT") {
        Set-VdjmImportLastMessage "Import cancelled"
        return
    }

    try {
        Invoke-VdjmPluginImport `
            -Repo $script:SourceRepo `
            -TargetDir $script:TargetPluginDir `
            -RefName $script:Ref `
            -ShouldCleanTarget $script:CleanTarget `
            -ShouldRunP4 $script:RunP4Reconcile `
            -AssumeYes $true
    }
    catch {
        Set-VdjmImportLastMessage ("Import failed: {0}" -f $_.Exception.Message)
        Write-Host ("`n [ERROR] {0}" -f $_.Exception.Message) -ForegroundColor Red
    }

    Read-Host "`n엔터를 누르면 메인 메뉴로 돌아갑니다..."
}

function Invoke-VdjmImportP4OnlyInteractive {
    Clear-Host
    Write-Host "===================================================================="
    Write-Host "                   [ 8. p4 Reconcile ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
    try {
        Invoke-VdjmImportP4Reconcile -TargetDir (Get-VdjmImportFullPath $script:TargetPluginDir)
        Set-VdjmImportLastMessage "p4 reconcile completed"
    }
    catch {
        Set-VdjmImportLastMessage ("p4 reconcile failed: {0}" -f $_.Exception.Message)
        Write-Host (" [ERROR] {0}" -f $_.Exception.Message) -ForegroundColor Red
    }
    Read-Host "`n엔터를 누르면 메인 메뉴로 돌아갑니다..."
}

function Show-VdjmImportMenu {
    $originalBackground = $Host.UI.RawUI.BackgroundColor
    $originalForeground = $Host.UI.RawUI.ForegroundColor
    $Host.UI.RawUI.BackgroundColor = "DarkBlue"
    $Host.UI.RawUI.ForegroundColor = "Gray"

    try {
        Write-VdjmImportIntro

        while ($true) {
            Write-VdjmImportHeader
            Show-VdjmImportSettings
            $menuSel = Read-Host "▶ 원하시는 메뉴 번호를 입력하십시오 (0-8)"

            switch ($menuSel) {
                "1" { Set-VdjmImportSourceRepoInteractive }
                "2" { Set-VdjmImportTargetInteractive }
                "3" { Set-VdjmImportRefInteractive }
                "4" {
                    $script:CleanTarget = -not $script:CleanTarget
                    Set-VdjmImportLastMessage ("CleanTarget = {0}" -f $script:CleanTarget)
                }
                "5" {
                    $script:RunP4Reconcile = -not $script:RunP4Reconcile
                    Set-VdjmImportLastMessage ("RunP4Reconcile = {0}" -f $script:RunP4Reconcile)
                }
                "6" { Show-VdjmImportPreview }
                "7" { Invoke-VdjmImportInteractive }
                "8" { Invoke-VdjmImportP4OnlyInteractive }
                "0" {
                    Clear-Host
                    Write-Host "`n [SYSTEM] 호스트와의 접속을 끊습니다."
                    Write-Host " 이용해 주셔서 감사합니다. 안녕히 가십시오.`n"
                    Write-Host " NO CARRIER`n" -ForegroundColor Red
                    Start-Sleep -Milliseconds 500
                    return
                }
                default {
                    Set-VdjmImportLastMessage "Unknown menu selection"
                }
            }
        }
    }
    finally {
        $Host.UI.RawUI.BackgroundColor = $originalBackground
        $Host.UI.RawUI.ForegroundColor = $originalForeground
        Clear-Host
    }
}

if ($RunOnce) {
    Invoke-VdjmPluginImport `
        -Repo $script:SourceRepo `
        -TargetDir $script:TargetPluginDir `
        -RefName $script:Ref `
        -ShouldCleanTarget $script:CleanTarget `
        -ShouldRunP4 $script:RunP4Reconcile `
        -AssumeYes ([bool]$Force)
}
else {
    Show-VdjmImportMenu
}
