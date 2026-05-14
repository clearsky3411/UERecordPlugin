param(
    [string]$Package = "com.vdjm.recordtest",
    [string]$Device = "100.73.238.68:5555",
    [string]$BuildRoot = "G:\Project\00Main\bg1LAb\build",
    [string]$BuildDate = (Get-Date -Format "yyyyMMdd"),
    [string]$LogPath = "",
    [string]$LogPattern = "PreviewDebug|CarouselWidget DebugSwipe|SwipeFinished|MoveFinished|LogAndroidMedia|Cannot set rate",
    [ValidateSet("None", "Menu", "Dashboard", "Connect", "Start", "StartLog", "Status", "Resolve", "Refresh", "SwipeNext", "SwipePrev", "SwipeSuite", "RecordRefreshSuite", "Log")]
    [string]$Run = "None",
    [int]$RecordSeconds = 3,
    [ValidateRange(1, 60)]
    [int]$DashboardRefreshSeconds = 1,
    [ValidateRange(5, 80)]
    [int]$DashboardLogLines = 18
)

$script:VdjmPreviewDebugPackage = $Package
$script:VdjmPreviewDebugDevice = $Device
$script:VdjmPreviewDebugBuildRoot = $BuildRoot
$script:VdjmPreviewDebugBuildDate = $BuildDate
$script:VdjmPreviewDebugPattern = $LogPattern
$script:VdjmPreviewDebugBuildDir = Join-Path $BuildRoot $BuildDate
$script:VdjmPreviewDebugLogDir = Join-Path $script:VdjmPreviewDebugBuildDir "_logs"
$script:VdjmPreviewDebugLogPath = $LogPath
$script:VdjmPreviewDebugLogJob = $null
$script:VdjmPreviewDebugLastAction = "Ready"

function Initialize-VdjmPreviewDebugPaths {
    if (-not (Test-Path $script:VdjmPreviewDebugBuildDir)) {
        New-Item -ItemType Directory -Path $script:VdjmPreviewDebugBuildDir -Force | Out-Null
    }

    if (-not (Test-Path $script:VdjmPreviewDebugLogDir)) {
        New-Item -ItemType Directory -Path $script:VdjmPreviewDebugLogDir -Force | Out-Null
    }

    if ([string]::IsNullOrWhiteSpace($script:VdjmPreviewDebugLogPath)) {
        $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
        $script:VdjmPreviewDebugLogPath = Join-Path $script:VdjmPreviewDebugLogDir "log_$timestamp.txt"
    }

    $logParent = Split-Path -Parent $script:VdjmPreviewDebugLogPath
    if (-not [string]::IsNullOrWhiteSpace($logParent) -and -not (Test-Path $logParent)) {
        New-Item -ItemType Directory -Path $logParent -Force | Out-Null
    }
}

function Write-VdjmPreviewDebugHeader {
    Clear-Host
    Write-Host "====================================================================" -ForegroundColor Cyan
    Write-Host "  VD-JM PREVIEW DEBUG CONSOLE" -ForegroundColor Cyan
    Write-Host "====================================================================" -ForegroundColor Cyan
    Write-Host ("  Package : {0}" -f $script:VdjmPreviewDebugPackage)
    Write-Host ("  Device  : {0}" -f $script:VdjmPreviewDebugDevice)
    Write-Host ("  Build   : {0}" -f $script:VdjmPreviewDebugBuildDir)
    Write-Host ("  Log     : {0}" -f $script:VdjmPreviewDebugLogPath)
    Write-Host "--------------------------------------------------------------------"
}

function Invoke-VdjmPreviewAdb {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    if ([string]::IsNullOrWhiteSpace($script:VdjmPreviewDebugDevice)) {
        & adb @Arguments
    }
    else {
        & adb -s $script:VdjmPreviewDebugDevice @Arguments
    }
}

function Connect-VdjmPreviewDebugDevice {
    if ([string]::IsNullOrWhiteSpace($script:VdjmPreviewDebugDevice)) {
        Write-Host "[vdjm] device is empty. Skipping adb connect." -ForegroundColor Yellow
        return
    }

    Write-Host "[vdjm] adb connect $script:VdjmPreviewDebugDevice" -ForegroundColor Cyan
    adb connect $script:VdjmPreviewDebugDevice | Out-Host
}

function Invoke-VdjmPreviewDebug {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command
    )

    $fullCommand = $Command.Trim()
    if ($fullCommand -notmatch "^vdjm\.preview\.debug\b") {
        $fullCommand = "vdjm.preview.debug $fullCommand"
    }

    Write-Host "[vdjm] $fullCommand" -ForegroundColor Green
    Invoke-VdjmPreviewAdb -Arguments @(
        "shell",
        "am",
        "broadcast",
        "-a",
        "android.intent.action.RUN",
        "-p",
        $script:VdjmPreviewDebugPackage,
        "-e",
        "cmd",
        $fullCommand) | Out-Host
}

function Clear-VdjmPreviewDebugLogcat {
    Write-Host "[vdjm] adb logcat -c" -ForegroundColor Cyan
    Invoke-VdjmPreviewAdb -Arguments @("logcat", "-c") | Out-Null
}

function Start-VdjmPreviewDebugApp {
    param(
        [switch]$ClearLog,
        [int]$WaitSeconds = 2
    )

    if ($ClearLog) {
        Clear-VdjmPreviewDebugLogcat
    }

    Write-Host "[vdjm] monkey -p $script:VdjmPreviewDebugPackage 1" -ForegroundColor Cyan
    Invoke-VdjmPreviewAdb -Arguments @("shell", "monkey", "-p", $script:VdjmPreviewDebugPackage, "1") | Out-Host
    if ($WaitSeconds -gt 0) {
        Start-Sleep -Seconds $WaitSeconds
    }
}

function Watch-VdjmPreviewDebugLog {
    param(
        [string]$Pattern = $script:VdjmPreviewDebugPattern,
        [string]$OutputPath = $script:VdjmPreviewDebugLogPath
    )

    Initialize-VdjmPreviewDebugPaths
    Write-Host "[vdjm] log path: $OutputPath" -ForegroundColor Green
    Write-Host "[vdjm] pattern : $Pattern" -ForegroundColor DarkGray

    if ([string]::IsNullOrWhiteSpace($script:VdjmPreviewDebugDevice)) {
        adb logcat -s UE |
            Select-String $Pattern |
            Tee-Object -FilePath $OutputPath
    }
    else {
        adb -s $script:VdjmPreviewDebugDevice logcat -s UE |
            Select-String $Pattern |
            Tee-Object -FilePath $OutputPath
    }
}

function Set-VdjmPreviewDebugLastAction {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Action
    )

    $script:VdjmPreviewDebugLastAction = ("{0:HH:mm:ss}  {1}" -f (Get-Date), $Action)
}

function Clear-VdjmPreviewDebugLogFile {
    Initialize-VdjmPreviewDebugPaths
    if (Test-Path $script:VdjmPreviewDebugLogPath) {
        Clear-Content -Path $script:VdjmPreviewDebugLogPath
    }
    else {
        New-Item -ItemType File -Path $script:VdjmPreviewDebugLogPath -Force | Out-Null
    }
}

function Start-VdjmPreviewDebugLogJob {
    param(
        [string]$Pattern = $script:VdjmPreviewDebugPattern,
        [string]$OutputPath = $script:VdjmPreviewDebugLogPath
    )

    Initialize-VdjmPreviewDebugPaths

    if ($script:VdjmPreviewDebugLogJob -and $script:VdjmPreviewDebugLogJob.State -eq "Running") {
        return $script:VdjmPreviewDebugLogJob
    }

    $device = $script:VdjmPreviewDebugDevice
    $jobName = "VdjmPreviewLog_{0}" -f (Get-Date -Format "HHmmss")
    $script:VdjmPreviewDebugLogJob = Start-Job -Name $jobName -ArgumentList $device, $Pattern, $OutputPath -ScriptBlock {
        param(
            [string]$JobDevice,
            [string]$JobPattern,
            [string]$JobOutputPath
        )

        if ([string]::IsNullOrWhiteSpace($JobDevice)) {
            & adb logcat -s UE 2>&1 |
                Select-String -Pattern $JobPattern |
                ForEach-Object {
                    $_.Line | Out-File -FilePath $JobOutputPath -Append -Encoding utf8
                }
        }
        else {
            & adb -s $JobDevice logcat -s UE 2>&1 |
                Select-String -Pattern $JobPattern |
                ForEach-Object {
                    $_.Line | Out-File -FilePath $JobOutputPath -Append -Encoding utf8
                }
        }
    }

    Set-VdjmPreviewDebugLastAction "Log job started: $jobName"
    return $script:VdjmPreviewDebugLogJob
}

function Stop-VdjmPreviewDebugLogJob {
    if ($script:VdjmPreviewDebugLogJob) {
        $job = Get-Job -Id $script:VdjmPreviewDebugLogJob.Id -ErrorAction SilentlyContinue
        if ($job) {
            Stop-Job -Job $job -ErrorAction SilentlyContinue
            Remove-Job -Job $job -Force -ErrorAction SilentlyContinue
        }
        $script:VdjmPreviewDebugLogJob = $null
        Set-VdjmPreviewDebugLastAction "Log job stopped"
    }
}

function Get-VdjmPreviewDebugLogJobState {
    if (-not $script:VdjmPreviewDebugLogJob) {
        return "Stopped"
    }

    $job = Get-Job -Id $script:VdjmPreviewDebugLogJob.Id -ErrorAction SilentlyContinue
    if (-not $job) {
        return "Missing"
    }

    $script:VdjmPreviewDebugLogJob = $job
    return $job.State.ToString()
}

function Get-VdjmPreviewDebugLogTail {
    param(
        [int]$LineCount = 18
    )

    if (-not (Test-Path $script:VdjmPreviewDebugLogPath)) {
        return @("[no log file yet]")
    }

    $lines = @(Get-Content -Path $script:VdjmPreviewDebugLogPath -Tail $LineCount -ErrorAction SilentlyContinue)
    if ($lines.Count -eq 0) {
        return @("[waiting for matching UE log lines]")
    }

    return $lines
}

function Start-VdjmPreviewDebugLogSession {
    Connect-VdjmPreviewDebugDevice
    Clear-VdjmPreviewDebugLogcat
    Start-VdjmPreviewDebugApp -WaitSeconds 2
    Watch-VdjmPreviewDebugLog
}

function Get-VdjmPreviewDebugStatus {
    Invoke-VdjmPreviewDebug "status"
}

function Resolve-VdjmPreviewDebugTargets {
    Invoke-VdjmPreviewDebug "resolve"
}

function Refresh-VdjmPreviewDebugCarousel {
    param(
        [bool]$Force = $false
    )

    Invoke-VdjmPreviewDebug ("refresh " + $Force.ToString().ToLowerInvariant())
}

function Refresh-VdjmPreviewDebugManager {
    param(
        [bool]$Force = $false
    )

    Invoke-VdjmPreviewDebug ("manager.refresh " + $Force.ToString().ToLowerInvariant())
}

function Invoke-VdjmPreviewSwipeNext {
    param(
        [float]$Speed = 0.0
    )

    if ([Math]::Abs($Speed) -gt 0.0001) {
        Invoke-VdjmPreviewDebug ("swipe next " + $Speed.ToString([Globalization.CultureInfo]::InvariantCulture))
    }
    else {
        Invoke-VdjmPreviewDebug "swipe next"
    }
}

function Invoke-VdjmPreviewSwipePrev {
    param(
        [float]$Speed = 0.0
    )

    if ([Math]::Abs($Speed) -gt 0.0001) {
        Invoke-VdjmPreviewDebug ("swipe prev " + $Speed.ToString([Globalization.CultureInfo]::InvariantCulture))
    }
    else {
        Invoke-VdjmPreviewDebug "swipe prev"
    }
}

function Invoke-VdjmPreviewSwipeDelta {
    param(
        [int]$Delta,
        [float]$Speed = 0.0
    )

    Invoke-VdjmPreviewDebug ("swipe delta {0} {1}" -f $Delta, $Speed.ToString([Globalization.CultureInfo]::InvariantCulture))
}

function Lock-VdjmPreviewSwipe {
    param(
        [string]$Reason = "Debug"
    )

    Invoke-VdjmPreviewDebug "lock $Reason"
}

function Unlock-VdjmPreviewSwipe {
    param(
        [string]$Reason = "Debug"
    )

    Invoke-VdjmPreviewDebug "unlock $Reason"
}

function Invoke-VdjmPreviewSwipeSuite {
    param(
        [float]$SoftSpeed = 0.0,
        [float]$HardSpeed = 8.0,
        [int]$PauseSeconds = 2
    )

    Get-VdjmPreviewDebugStatus
    Start-Sleep -Seconds 1
    Resolve-VdjmPreviewDebugTargets
    Start-Sleep -Seconds 1

    Invoke-VdjmPreviewSwipeNext $SoftSpeed
    Start-Sleep -Seconds $PauseSeconds
    Get-VdjmPreviewDebugStatus
    Start-Sleep -Seconds 1

    Invoke-VdjmPreviewSwipePrev $SoftSpeed
    Start-Sleep -Seconds $PauseSeconds
    Get-VdjmPreviewDebugStatus
    Start-Sleep -Seconds 1

    Invoke-VdjmPreviewSwipeDelta 1 $HardSpeed
    Start-Sleep -Seconds $PauseSeconds
    Invoke-VdjmPreviewSwipeDelta -1 $HardSpeed
    Start-Sleep -Seconds $PauseSeconds
    Get-VdjmPreviewDebugStatus
}

function Invoke-VdjmPreviewRecordRefreshSuite {
    param(
        [int]$DurationSeconds = 3,
        [int]$PostStopWaitSeconds = 3
    )

    Get-VdjmPreviewDebugStatus
    Invoke-VdjmPreviewDebug "record start"
    Start-Sleep -Seconds $DurationSeconds
    Invoke-VdjmPreviewDebug "record stop"
    Start-Sleep -Seconds $PostStopWaitSeconds
    Refresh-VdjmPreviewDebugCarousel $true
    Start-Sleep -Seconds 2
    Get-VdjmPreviewDebugStatus
}

function Send-VdjmPreviewCustomCommand {
    $command = Read-Host "vdjm.preview.debug command"
    if (-not [string]::IsNullOrWhiteSpace($command)) {
        Invoke-VdjmPreviewDebug $command
    }
}

function Write-VdjmPreviewDebugDashboard {
    param(
        [int]$LineCount = 18
    )

    Clear-Host
    Write-Host "====================================================================" -ForegroundColor Cyan
    Write-Host "  VD-JM PREVIEW DEBUG LIVE DASHBOARD" -ForegroundColor Cyan
    Write-Host "====================================================================" -ForegroundColor Cyan
    Write-Host ("  Package : {0}" -f $script:VdjmPreviewDebugPackage)
    Write-Host ("  Device  : {0}" -f $script:VdjmPreviewDebugDevice)
    Write-Host ("  LogJob  : {0}" -f (Get-VdjmPreviewDebugLogJobState))
    Write-Host ("  Log     : {0}" -f $script:VdjmPreviewDebugLogPath)
    Write-Host ("  Last    : {0}" -f $script:VdjmPreviewDebugLastAction) -ForegroundColor Yellow
    Write-Host "--------------------------------------------------------------------"
    Write-Host "  Q exit | S status | X resolve | A launch | C clear logs | L restart log"
    Write-Host "  R refresh carousel | M refresh manager | N swipe next | P swipe prev | T record suite"
    Write-Host "--------------------------------------------------------------------"
    Write-Host "  Recent UE log lines" -ForegroundColor Cyan
    Write-Host "--------------------------------------------------------------------"

    foreach ($line in (Get-VdjmPreviewDebugLogTail -LineCount $LineCount)) {
        Write-Host $line
    }
}

function Show-VdjmPreviewDebugDashboard {
    Initialize-VdjmPreviewDebugPaths

    $originalBackground = $Host.UI.RawUI.BackgroundColor
    $originalForeground = $Host.UI.RawUI.ForegroundColor
    $Host.UI.RawUI.BackgroundColor = "DarkBlue"
    $Host.UI.RawUI.ForegroundColor = "Gray"
    Clear-Host

    try {
        Write-Host ""
        Write-Host " ATDT VD-JM-PREVIEW-LIVE" -ForegroundColor Cyan
        Write-Host " CONNECT 9600/ARQ/V32/LAPM" -ForegroundColor Cyan
        Start-Sleep -Milliseconds 400

        Connect-VdjmPreviewDebugDevice
        Clear-VdjmPreviewDebugLogcat
        Clear-VdjmPreviewDebugLogFile
        Start-VdjmPreviewDebugLogJob | Out-Null
        Start-VdjmPreviewDebugApp -WaitSeconds 1
        Set-VdjmPreviewDebugLastAction "Dashboard started"

        $nextRender = Get-Date
        while ($true) {
            if ((Get-Date) -ge $nextRender) {
                Write-VdjmPreviewDebugDashboard -LineCount $DashboardLogLines
                $nextRender = (Get-Date).AddSeconds($DashboardRefreshSeconds)
            }

            if ([Console]::KeyAvailable) {
                $key = [Console]::ReadKey($true)
                $keyChar = $key.KeyChar.ToString().ToUpperInvariant()

                switch ($keyChar) {
                    "Q" {
                        return
                    }
                    "S" {
                        Get-VdjmPreviewDebugStatus
                        Set-VdjmPreviewDebugLastAction "Requested status"
                    }
                    "X" {
                        Resolve-VdjmPreviewDebugTargets
                        Set-VdjmPreviewDebugLastAction "Resolved targets"
                    }
                    "A" {
                        Start-VdjmPreviewDebugApp -WaitSeconds 1
                        Set-VdjmPreviewDebugLastAction "Launched app"
                    }
                    "C" {
                        Stop-VdjmPreviewDebugLogJob
                        Clear-VdjmPreviewDebugLogcat
                        Clear-VdjmPreviewDebugLogFile
                        Start-VdjmPreviewDebugLogJob | Out-Null
                        Set-VdjmPreviewDebugLastAction "Cleared device and file logs"
                    }
                    "L" {
                        Stop-VdjmPreviewDebugLogJob
                        Clear-VdjmPreviewDebugLogFile
                        Start-VdjmPreviewDebugLogJob | Out-Null
                        Set-VdjmPreviewDebugLastAction "Restarted log job"
                    }
                    "R" {
                        Refresh-VdjmPreviewDebugCarousel $true
                        Set-VdjmPreviewDebugLastAction "Requested carousel refresh"
                    }
                    "M" {
                        Refresh-VdjmPreviewDebugManager $true
                        Set-VdjmPreviewDebugLastAction "Requested manager refresh"
                    }
                    "N" {
                        Invoke-VdjmPreviewSwipeNext
                        Set-VdjmPreviewDebugLastAction "Requested swipe next"
                    }
                    "P" {
                        Invoke-VdjmPreviewSwipePrev
                        Set-VdjmPreviewDebugLastAction "Requested swipe prev"
                    }
                    "T" {
                        Invoke-VdjmPreviewRecordRefreshSuite -DurationSeconds $RecordSeconds
                        Set-VdjmPreviewDebugLastAction "Ran record refresh suite"
                    }
                }

                $nextRender = Get-Date
            }

            Start-Sleep -Milliseconds 100
        }
    }
    finally {
        Stop-VdjmPreviewDebugLogJob
        $Host.UI.RawUI.BackgroundColor = $originalBackground
        $Host.UI.RawUI.ForegroundColor = $originalForeground
        Clear-Host
    }
}

function Show-VdjmPreviewDebugMenu {
    Initialize-VdjmPreviewDebugPaths

    $originalBackground = $Host.UI.RawUI.BackgroundColor
    $originalForeground = $Host.UI.RawUI.ForegroundColor
    $Host.UI.RawUI.BackgroundColor = "DarkBlue"
    $Host.UI.RawUI.ForegroundColor = "Gray"
    Clear-Host

    try {
        Write-Host ""
        Write-Host " ATDT VD-JM-PREVIEW" -ForegroundColor Cyan
        Write-Host " CONNECT 9600/ARQ/V32/LAPM" -ForegroundColor Cyan
        Start-Sleep -Milliseconds 400

        while ($true) {
            Write-VdjmPreviewDebugHeader
            Write-Host "  [1] adb connect"
            Write-Host "  [2] clear logcat + launch app"
            Write-Host "  [3] live dashboard"
            Write-Host "  [4] start log session (connect + clear + launch + log)"
            Write-Host "  [5] watch log only"
            Write-Host "  [6] status"
            Write-Host "  [7] resolve targets"
            Write-Host "  [8] swipe suite"
            Write-Host "  [9] record refresh suite"
            Write-Host "  [10] refresh carousel force"
            Write-Host "  [11] swipe next"
            Write-Host "  [12] swipe prev"
            Write-Host "  [13] custom vdjm.preview.debug command"
            Write-Host "  [0] exit"
            Write-Host "--------------------------------------------------------------------"

            $menuSel = Read-Host "select"
            switch ($menuSel) {
                "1" {
                    Connect-VdjmPreviewDebugDevice
                    Read-Host "enter"
                }
                "2" {
                    Start-VdjmPreviewDebugApp -ClearLog
                    Read-Host "enter"
                }
                "3" {
                    Show-VdjmPreviewDebugDashboard
                }
                "4" {
                    Start-VdjmPreviewDebugLogSession
                }
                "5" {
                    Watch-VdjmPreviewDebugLog
                }
                "6" {
                    Get-VdjmPreviewDebugStatus
                    Read-Host "enter"
                }
                "7" {
                    Resolve-VdjmPreviewDebugTargets
                    Read-Host "enter"
                }
                "8" {
                    Invoke-VdjmPreviewSwipeSuite
                    Read-Host "enter"
                }
                "9" {
                    Invoke-VdjmPreviewRecordRefreshSuite -DurationSeconds $RecordSeconds
                    Read-Host "enter"
                }
                "10" {
                    Refresh-VdjmPreviewDebugCarousel $true
                    Read-Host "enter"
                }
                "11" {
                    Invoke-VdjmPreviewSwipeNext
                    Read-Host "enter"
                }
                "12" {
                    Invoke-VdjmPreviewSwipePrev
                    Read-Host "enter"
                }
                "13" {
                    Send-VdjmPreviewCustomCommand
                    Read-Host "enter"
                }
                "0" {
                    return
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

Initialize-VdjmPreviewDebugPaths

switch ($Run) {
    "Menu" {
        Show-VdjmPreviewDebugMenu
    }
    "Dashboard" {
        Show-VdjmPreviewDebugDashboard
    }
    "Connect" {
        Connect-VdjmPreviewDebugDevice
    }
    "Start" {
        Start-VdjmPreviewDebugApp -ClearLog
    }
    "StartLog" {
        Start-VdjmPreviewDebugLogSession
    }
    "Status" {
        Get-VdjmPreviewDebugStatus
    }
    "Resolve" {
        Resolve-VdjmPreviewDebugTargets
    }
    "Refresh" {
        Refresh-VdjmPreviewDebugCarousel $true
    }
    "SwipeNext" {
        Invoke-VdjmPreviewSwipeNext
    }
    "SwipePrev" {
        Invoke-VdjmPreviewSwipePrev
    }
    "SwipeSuite" {
        Invoke-VdjmPreviewSwipeSuite
    }
    "RecordRefreshSuite" {
        Invoke-VdjmPreviewRecordRefreshSuite -DurationSeconds $RecordSeconds
    }
    "Log" {
        Watch-VdjmPreviewDebugLog
    }
    default {
        Write-Host "[vdjm] Loaded preview debug helpers."
        Write-Host "[vdjm] Build dir : $script:VdjmPreviewDebugBuildDir"
        Write-Host "[vdjm] Log path  : $script:VdjmPreviewDebugLogPath"
        Write-Host "[vdjm] Menu      : powershell -ExecutionPolicy Bypass -File .\Tools\VdjmPreviewDebug.ps1 -Run Menu"
        Write-Host "[vdjm] Dashboard : powershell -ExecutionPolicy Bypass -File .\Tools\VdjmPreviewDebug.ps1 -Run Dashboard"
        Write-Host "[vdjm] StartLog  : powershell -ExecutionPolicy Bypass -File .\Tools\VdjmPreviewDebug.ps1 -Run StartLog"
    }
}
