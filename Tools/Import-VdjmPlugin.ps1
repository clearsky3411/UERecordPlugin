param(
    [string]$ProfileName = "default",
    [string]$SourceRepo = "",
    [string]$TargetPluginDir = "",
    [string]$TargetPluginsRoot = "",
    [string]$TargetPluginName = "VdjmMobileUi",
    [string]$Ref = "main",
    [ValidateSet("Menu", "Gui", "Preview", "Import", "Undo", "Profiles", "None")]
    [string]$Run = "Menu",
    [ValidateSet("BackupThenMirror", "OverwriteOnly")]
    [string]$ImportMode = "BackupThenMirror",
    [bool]$IncludeContent = $true,
    [bool]$IncludeConfig = $true,
    [switch]$IncludeBinaries,
    [switch]$RunP4Reconcile,
    [switch]$Force
)

$script:InitialBoundParameters = @{} + $PSBoundParameters
$script:ToolTitle = "VD-JM PLUGIN IMPORT MANAGER"
$script:PluginName = "VdjmMobileUi"
$script:DefaultSourceRepo = Split-Path -Parent $PSScriptRoot
$script:StateRoot = Join-Path $env:LOCALAPPDATA "VdjmPluginImporter"
$script:ProfileRoot = Join-Path $script:StateRoot "profiles"
$script:TransactionRoot = Join-Path $script:StateRoot "transactions"
$script:BackupRoot = Join-Path $script:StateRoot "backups"
$script:LastMessage = "Ready"
$script:AlwaysExcludeNames = @(".git", ".vs", "Saved", "DerivedDataCache")
$script:PreserveTargetNames = @(".p4ignore")
$script:GeneratedRelativePaths = @("Docs/ImportedFromGit.md")
$script:CurrentProfile = $null

function Initialize-VdjmImportState {
    foreach ($path in @($script:StateRoot, $script:ProfileRoot, $script:TransactionRoot, $script:BackupRoot)) {
        if (-not (Test-Path -LiteralPath $path)) {
            New-Item -ItemType Directory -Path $path -Force | Out-Null
        }
    }
}

function Set-VdjmImportLastMessage {
    param([string]$Message)

    $script:LastMessage = ("{0:HH:mm:ss}  {1}" -f (Get-Date), $Message)
}

function Get-VdjmImportFullPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Get-VdjmImportProfilePath {
    param([Parameter(Mandatory = $true)][string]$Name)

    $safeName = ($Name -replace '[\\/:*?"<>|]', "_").Trim()
    if ([string]::IsNullOrWhiteSpace($safeName)) {
        $safeName = "default"
    }

    return Join-Path $script:ProfileRoot ("{0}.json" -f $safeName)
}

function New-VdjmImportDefaultProfile {
    param([string]$Name = "default")

    return [pscustomobject]@{
        profile_name = $Name
        source_repo = $script:DefaultSourceRepo
        source_ref = "main"
        target_plugins_root = ""
        target_plugin_name = $script:PluginName
        target_plugin_dir = ""
        import_mode = "BackupThenMirror"
        include_content = $true
        include_config = $true
        include_binaries = $false
        run_p4_reconcile = $false
        last_transaction_id = ""
        history = @()
    }
}

function Repair-VdjmImportProfile {
    param(
        [Parameter(Mandatory = $true)]$Profile,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $defaultProfile = New-VdjmImportDefaultProfile -Name $Name
    foreach ($property in $defaultProfile.PSObject.Properties) {
        if ($Profile.PSObject.Properties.Name -notcontains $property.Name) {
            $Profile | Add-Member -NotePropertyName $property.Name -NotePropertyValue $property.Value -Force
        }
    }

    if ([string]::IsNullOrWhiteSpace($Profile.profile_name)) {
        $Profile.profile_name = $Name
    }
    if ($null -eq $Profile.history) {
        $Profile.history = @()
    }

    return $Profile
}

function Read-VdjmImportProfile {
    param([Parameter(Mandatory = $true)][string]$Name)

    Initialize-VdjmImportState
    $profilePath = Get-VdjmImportProfilePath $Name
    if (Test-Path -LiteralPath $profilePath) {
        $profile = Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
    }
    else {
        $profile = New-VdjmImportDefaultProfile -Name $Name
    }

    return Repair-VdjmImportProfile -Profile $profile -Name $Name
}

function Save-VdjmImportProfile {
    param([Parameter(Mandatory = $true)]$Profile)

    Initialize-VdjmImportState
    $profilePath = Get-VdjmImportProfilePath $Profile.profile_name
    $Profile | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $profilePath -Encoding UTF8
    return $profilePath
}

function Update-VdjmImportProfileFromParameters {
    param([Parameter(Mandatory = $true)]$Profile)

    if ($script:InitialBoundParameters.ContainsKey("SourceRepo") -and -not [string]::IsNullOrWhiteSpace($SourceRepo)) {
        $Profile.source_repo = $SourceRepo
    }
    if ($script:InitialBoundParameters.ContainsKey("TargetPluginDir") -and -not [string]::IsNullOrWhiteSpace($TargetPluginDir)) {
        $Profile.target_plugin_dir = $TargetPluginDir
    }
    if ($script:InitialBoundParameters.ContainsKey("TargetPluginsRoot") -and -not [string]::IsNullOrWhiteSpace($TargetPluginsRoot)) {
        $Profile.target_plugins_root = $TargetPluginsRoot
    }
    if ($script:InitialBoundParameters.ContainsKey("TargetPluginName") -and -not [string]::IsNullOrWhiteSpace($TargetPluginName)) {
        $Profile.target_plugin_name = $TargetPluginName
    }
    if ($script:InitialBoundParameters.ContainsKey("Ref") -and -not [string]::IsNullOrWhiteSpace($Ref)) {
        $Profile.source_ref = $Ref
    }

    if ($script:InitialBoundParameters.ContainsKey("ImportMode")) {
        $Profile.import_mode = $ImportMode
    }
    if ($script:InitialBoundParameters.ContainsKey("IncludeContent")) {
        $Profile.include_content = [bool]$IncludeContent
    }
    if ($script:InitialBoundParameters.ContainsKey("IncludeConfig")) {
        $Profile.include_config = [bool]$IncludeConfig
    }
    if ($script:InitialBoundParameters.ContainsKey("IncludeBinaries")) {
        $Profile.include_binaries = $true
    }
    if ($script:InitialBoundParameters.ContainsKey("RunP4Reconcile")) {
        $Profile.run_p4_reconcile = [bool]$RunP4Reconcile
    }
    return $Profile
}

function Get-VdjmImportTargetPluginDir {
    param([Parameter(Mandatory = $true)]$Profile)

    if (-not [string]::IsNullOrWhiteSpace($Profile.target_plugin_dir)) {
        return Get-VdjmImportFullPath $Profile.target_plugin_dir
    }

    if ([string]::IsNullOrWhiteSpace($Profile.target_plugins_root)) {
        return ""
    }

    $pluginName = if ([string]::IsNullOrWhiteSpace($Profile.target_plugin_name)) { $script:PluginName } else { $Profile.target_plugin_name }
    return Get-VdjmImportFullPath (Join-Path $Profile.target_plugins_root $pluginName)
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

    return Test-Path -LiteralPath (Join-Path $Repo ".git")
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

function Get-VdjmImportRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$BaseDir,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $baseFull = Get-VdjmImportFullPath $BaseDir
    $pathFull = Get-VdjmImportFullPath $Path
    $baseFull = $baseFull.TrimEnd("\") + "\"
    if ($pathFull.StartsWith($baseFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $pathFull.Substring($baseFull.Length).Replace("\", "/")
    }

    return $pathFull.Replace("\", "/")
}

function Get-VdjmImportExcludeNames {
    param([Parameter(Mandatory = $true)]$Profile)

    $excludeNames = @($script:AlwaysExcludeNames)
    if (-not [bool]$Profile.include_binaries) {
        $excludeNames += "Binaries"
    }
    if (-not [bool]$Profile.include_content) {
        $excludeNames += "Content"
    }
    if (-not [bool]$Profile.include_config) {
        $excludeNames += "Config"
    }

    return $excludeNames
}

function Test-VdjmImportExcludedRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string[]]$ExcludeNames
    )

    $parts = $RelativePath -split "[/\\]"
    $normalizedRelativePath = $RelativePath.Replace("\", "/")
    if ($script:GeneratedRelativePaths -contains $normalizedRelativePath) {
        return $true
    }

    foreach ($part in $parts) {
        if (($ExcludeNames -contains $part) -or ($script:PreserveTargetNames -contains $part)) {
            return $true
        }
    }

    return $false
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
        [Parameter(Mandatory = $true)][string]$TargetDir,
        [Parameter(Mandatory = $true)][string[]]$ExcludeNames
    )

    if (-not (Test-Path -LiteralPath $TargetDir)) {
        New-Item -ItemType Directory -Path $TargetDir -Force | Out-Null
    }

    foreach ($child in Get-ChildItem -LiteralPath $SourceDir -Force) {
        $relativePath = Get-VdjmImportRelativePath -BaseDir $SourceDir -Path $child.FullName
        if (Test-VdjmImportExcludedRelativePath -RelativePath $relativePath -ExcludeNames $ExcludeNames) {
            continue
        }

        $targetPath = Join-Path $TargetDir $child.Name
        if ($child.PSIsContainer) {
            Copy-VdjmImportDirectory -SourceDir $child.FullName -TargetDir $targetPath -ExcludeNames $ExcludeNames
        }
        else {
            Copy-Item -LiteralPath $child.FullName -Destination $targetPath -Force
        }
    }
}

function New-VdjmImportExport {
    param(
        [Parameter(Mandatory = $true)]$Profile,
        [Parameter(Mandatory = $true)][string]$TempRoot
    )

    $repo = [string]$Profile.source_repo
    $refName = [string]$Profile.source_ref
    $excludeNames = Get-VdjmImportExcludeNames -Profile $Profile
    $exportDir = Join-Path $TempRoot "export"
    New-Item -ItemType Directory -Path $exportDir -Force | Out-Null

    $repoDir = Join-Path $TempRoot "repo"
    if ((Test-VdjmImportRemoteRepo $repo) -or (Test-VdjmImportLocalGitRepo $repo)) {
        if (-not (Test-VdjmImportCommand "git")) {
            throw "git is not available in PATH."
        }

        Invoke-VdjmImportGit -Arguments @("clone", $repo, $repoDir)
        if (-not [string]::IsNullOrWhiteSpace($refName)) {
            Invoke-VdjmImportGit -Arguments @("-C", $repoDir, "checkout", $refName)
        }

        $commit = (& git -C $repoDir rev-parse HEAD).Trim()
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to resolve imported git commit."
        }

        $branch = (& git -C $repoDir rev-parse --abbrev-ref HEAD).Trim()
        if ($LASTEXITCODE -ne 0) {
            $branch = "unknown"
        }

        Copy-VdjmImportDirectory -SourceDir $repoDir -TargetDir $exportDir -ExcludeNames $excludeNames
        return [pscustomobject]@{
            export_dir = $exportDir
            source_kind = "git"
            source_repo = $repo
            ref = $refName
            commit = $commit
            branch = $branch
        }
    }

    if (-not (Test-Path -LiteralPath $repo)) {
        throw "Source repo/path does not exist: $repo"
    }

    Copy-VdjmImportDirectory -SourceDir (Get-VdjmImportFullPath $repo) -TargetDir $exportDir -ExcludeNames $excludeNames
    return [pscustomobject]@{
        export_dir = $exportDir
        source_kind = "folder"
        source_repo = $repo
        ref = "working-tree"
        commit = "not-a-git-export"
        branch = "not-a-git-export"
    }
}

function Get-VdjmImportFileManifest {
    param(
        [Parameter(Mandatory = $true)][string]$RootDir,
        [Parameter(Mandatory = $true)][string[]]$ExcludeNames
    )

    $manifest = @{}
    if (-not (Test-Path -LiteralPath $RootDir)) {
        return $manifest
    }

    foreach ($file in Get-ChildItem -LiteralPath $RootDir -File -Recurse -Force) {
        $relativePath = Get-VdjmImportRelativePath -BaseDir $RootDir -Path $file.FullName
        if (Test-VdjmImportExcludedRelativePath -RelativePath $relativePath -ExcludeNames $ExcludeNames) {
            continue
        }

        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $manifest[$relativePath] = [pscustomobject]@{
            relative_path = $relativePath
            full_path = $file.FullName
            hash = $hash
            length = $file.Length
            last_write_time_utc = $file.LastWriteTimeUtc.ToString("o")
        }
    }

    return $manifest
}

function Get-VdjmImportTransactionPath {
    param([Parameter(Mandatory = $true)][string]$TransactionId)

    return Join-Path $script:TransactionRoot ("{0}.json" -f $TransactionId)
}

function Read-VdjmImportTransaction {
    param([Parameter(Mandatory = $true)][string]$TransactionId)

    $path = Get-VdjmImportTransactionPath $TransactionId
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Transaction not found: $TransactionId"
    }

    return Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
}

function Save-VdjmImportTransaction {
    param([Parameter(Mandatory = $true)]$Transaction)

    Initialize-VdjmImportState
    $path = Get-VdjmImportTransactionPath $Transaction.transaction_id
    $Transaction | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $path -Encoding UTF8
    return $path
}

function Get-VdjmImportLastHashMap {
    param([Parameter(Mandatory = $true)]$Profile)

    $hashMap = @{}
    if ([string]::IsNullOrWhiteSpace($Profile.last_transaction_id)) {
        return $hashMap
    }

    try {
        $transaction = Read-VdjmImportTransaction -TransactionId $Profile.last_transaction_id
        foreach ($operation in @($transaction.operations)) {
            if (-not [string]::IsNullOrWhiteSpace($operation.after_hash)) {
                $hashMap[$operation.path] = $operation.after_hash
            }
            elseif ($operation.operation -eq "deleted") {
                $hashMap[$operation.path] = ""
            }
        }
    }
    catch {
        return $hashMap
    }

    return $hashMap
}

function New-VdjmImportPlan {
    param(
        [Parameter(Mandatory = $true)]$Profile,
        [Parameter(Mandatory = $true)]$ExportInfo
    )

    $targetDir = Get-VdjmImportTargetPluginDir -Profile $Profile
    Test-VdjmImportTargetSafety -TargetDir $targetDir -ThrowOnError | Out-Null

    $excludeNames = Get-VdjmImportExcludeNames -Profile $Profile
    $sourceManifest = Get-VdjmImportFileManifest -RootDir $ExportInfo.export_dir -ExcludeNames $excludeNames
    $targetManifest = Get-VdjmImportFileManifest -RootDir $targetDir -ExcludeNames $excludeNames
    $lastHashMap = Get-VdjmImportLastHashMap -Profile $Profile
    $operations = @()
    $unchangedCount = 0
    $staleCount = 0
    $localChangeCount = 0

    foreach ($path in ($sourceManifest.Keys | Sort-Object)) {
        $sourceFile = $sourceManifest[$path]
        $targetFile = $targetManifest[$path]
        $operation = "added"
        $beforeHash = ""
        $afterHash = $sourceFile.hash
        $targetLocalChanged = $false

        if ($null -ne $targetFile) {
            $beforeHash = $targetFile.hash
            if ($beforeHash -eq $afterHash) {
                ++$unchangedCount
                continue
            }

            $operation = "modified"
            if ($lastHashMap.ContainsKey($path) -and $lastHashMap[$path] -ne "" -and $lastHashMap[$path] -ne $beforeHash) {
                $targetLocalChanged = $true
                ++$localChangeCount
            }
        }

        $operations += [pscustomobject]@{
            path = $path
            operation = $operation
            source_path = $sourceFile.full_path
            target_path = Join-Path $targetDir ($path -replace "/", "\")
            before_hash = $beforeHash
            after_hash = $afterHash
            backup_path = ""
            target_local_changed = $targetLocalChanged
            length = $sourceFile.length
        }
    }

    foreach ($path in ($targetManifest.Keys | Sort-Object)) {
        if ($sourceManifest.ContainsKey($path)) {
            continue
        }

        $targetFile = $targetManifest[$path]
        if ($Profile.import_mode -eq "BackupThenMirror") {
            $targetLocalChanged = $false
            if ($lastHashMap.ContainsKey($path) -and $lastHashMap[$path] -ne "" -and $lastHashMap[$path] -ne $targetFile.hash) {
                $targetLocalChanged = $true
                ++$localChangeCount
            }

            $operations += [pscustomobject]@{
                path = $path
                operation = "deleted"
                source_path = ""
                target_path = $targetFile.full_path
                before_hash = $targetFile.hash
                after_hash = ""
                backup_path = ""
                target_local_changed = $targetLocalChanged
                length = $targetFile.length
            }
        }
        else {
            ++$staleCount
        }
    }

    return [pscustomobject]@{
        profile_name = $Profile.profile_name
        target_dir = $targetDir
        source_count = $sourceManifest.Count
        target_count = $targetManifest.Count
        unchanged_count = $unchangedCount
        stale_count = $staleCount
        local_change_count = $localChangeCount
        operations = @($operations)
        export_info = $ExportInfo
    }
}

function New-VdjmImportBackupPath {
    param(
        [Parameter(Mandatory = $true)][string]$BackupDir,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )

    return Join-Path $BackupDir ($RelativePath -replace "/", "\")
}

function Backup-VdjmImportTargetFile {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$BackupPath
    )

    $backupParent = Split-Path -Parent $BackupPath
    if (-not (Test-Path -LiteralPath $backupParent)) {
        New-Item -ItemType Directory -Path $backupParent -Force | Out-Null
    }

    Copy-Item -LiteralPath $SourcePath -Destination $BackupPath -Force
}

function Write-VdjmImportStamp {
    param(
        [Parameter(Mandatory = $true)]$Profile,
        [Parameter(Mandatory = $true)]$Transaction,
        [Parameter(Mandatory = $true)][string]$TargetDir
    )

    $docsDir = Join-Path $TargetDir "Docs"
    if (-not (Test-Path -LiteralPath $docsDir)) {
        New-Item -ItemType Directory -Path $docsDir -Force | Out-Null
    }

    $stampPath = Join-Path $docsDir "ImportedFromGit.md"
    $stamp = @"
# VdjmMobileUi Import Stamp

- ImportedAt: $($Transaction.imported_at)
- Profile: $($Profile.profile_name)
- TransactionId: $($Transaction.transaction_id)
- SourceKind: $($Transaction.source.kind)
- SourceRepo: $($Transaction.source.repo)
- Ref: $($Transaction.source.ref)
- Branch: $($Transaction.source.branch)
- Commit: $($Transaction.source.commit)
- TargetPluginDir: $TargetDir
- ImportMode: $($Profile.import_mode)
- IncludeContent: $($Profile.include_content)
- IncludeConfig: $($Profile.include_config)
- IncludeBinaries: $($Profile.include_binaries)
- BackupDir: $($Transaction.backup_dir)

This file is generated by Tools/Import-VdjmPlugin.ps1 so a Perforce vendor copy can be traced back to its source.
"@
    Set-Content -LiteralPath $stampPath -Value $stamp -Encoding UTF8
}

function Get-VdjmImportStampPath {
    param([Parameter(Mandatory = $true)][string]$TargetDir)

    return Join-Path (Join-Path $TargetDir "Docs") "ImportedFromGit.md"
}

function Remove-VdjmImportEmptyDirectories {
    param([Parameter(Mandatory = $true)][string]$RootDir)

    if (-not (Test-Path -LiteralPath $RootDir)) {
        return
    }

    $directories = @(Get-ChildItem -LiteralPath $RootDir -Directory -Recurse -Force | Sort-Object -Property FullName -Descending)
    foreach ($directory in $directories) {
        if (@(Get-ChildItem -LiteralPath $directory.FullName -Force -ErrorAction SilentlyContinue).Count -eq 0) {
            Remove-Item -LiteralPath $directory.FullName -Force
        }
    }

    if (@(Get-ChildItem -LiteralPath $RootDir -Force -ErrorAction SilentlyContinue).Count -eq 0) {
        Remove-Item -LiteralPath $RootDir -Force
    }
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

function Invoke-VdjmPluginImportPlan {
    param(
        [Parameter(Mandatory = $true)]$Profile,
        [Parameter(Mandatory = $true)]$Plan,
        [bool]$AssumeYes
    )

    if ($Plan.local_change_count -gt 0 -and -not $AssumeYes) {
        throw "Target local changes detected. Re-run with -Force or confirm in the menu/GUI."
    }

    $transactionId = Get-Date -Format "yyyyMMdd_HHmmss"
    $backupDir = Join-Path (Join-Path $script:BackupRoot $Profile.profile_name) $transactionId
    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
    if (-not (Test-Path -LiteralPath $Plan.target_dir)) {
        New-Item -ItemType Directory -Path $Plan.target_dir -Force | Out-Null
    }

    $appliedOperations = @()
    foreach ($operation in @($Plan.operations)) {
        $targetPath = [string]$operation.target_path
        $backupPath = ""
        if (($operation.operation -eq "modified" -or $operation.operation -eq "deleted") -and (Test-Path -LiteralPath $targetPath)) {
            $backupPath = New-VdjmImportBackupPath -BackupDir $backupDir -RelativePath $operation.path
            Backup-VdjmImportTargetFile -SourcePath $targetPath -BackupPath $backupPath
        }

        if ($operation.operation -eq "added" -or $operation.operation -eq "modified") {
            $targetParent = Split-Path -Parent $targetPath
            if (-not (Test-Path -LiteralPath $targetParent)) {
                New-Item -ItemType Directory -Path $targetParent -Force | Out-Null
            }
            Copy-Item -LiteralPath $operation.source_path -Destination $targetPath -Force
        }
        elseif ($operation.operation -eq "deleted") {
            if (Test-Path -LiteralPath $targetPath) {
                Remove-Item -LiteralPath $targetPath -Force
            }
        }

        $operation.backup_path = $backupPath
        $appliedOperations += $operation
    }

    $stampPath = Get-VdjmImportStampPath -TargetDir $Plan.target_dir
    $stampBackupPath = ""
    if (Test-Path -LiteralPath $stampPath) {
        $stampBackupPath = New-VdjmImportBackupPath -BackupDir $backupDir -RelativePath "Docs/ImportedFromGit.md"
        Backup-VdjmImportTargetFile -SourcePath $stampPath -BackupPath $stampBackupPath
    }

    $transaction = [pscustomobject]@{
        transaction_id = $transactionId
        profile_name = $Profile.profile_name
        imported_at = (Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz")
        undone = $false
        undone_at = ""
        target_dir = $Plan.target_dir
        backup_dir = $backupDir
        stamp_backup_path = $stampBackupPath
        source = [pscustomobject]@{
            kind = $Plan.export_info.source_kind
            repo = $Plan.export_info.source_repo
            ref = $Plan.export_info.ref
            branch = $Plan.export_info.branch
            commit = $Plan.export_info.commit
        }
        settings = [pscustomobject]@{
            import_mode = $Profile.import_mode
            include_content = $Profile.include_content
            include_config = $Profile.include_config
            include_binaries = $Profile.include_binaries
            run_p4_reconcile = $Profile.run_p4_reconcile
        }
        counts = [pscustomobject]@{
            added = @($appliedOperations | Where-Object { $_.operation -eq "added" }).Count
            modified = @($appliedOperations | Where-Object { $_.operation -eq "modified" }).Count
            deleted = @($appliedOperations | Where-Object { $_.operation -eq "deleted" }).Count
            unchanged = $Plan.unchanged_count
            stale = $Plan.stale_count
            local_changes = $Plan.local_change_count
        }
        operations = @($appliedOperations)
    }

    Save-VdjmImportTransaction -Transaction $transaction | Out-Null
    $Profile.last_transaction_id = $transaction.transaction_id
    $history = @($Profile.history)
    $history += [pscustomobject]@{
        transaction_id = $transaction.transaction_id
        imported_at = $transaction.imported_at
        commit = $transaction.source.commit
        target_dir = $transaction.target_dir
    }
    if ($history.Count -gt 50) {
        $history = $history[($history.Count - 50)..($history.Count - 1)]
    }
    $Profile.history = @($history)
    Save-VdjmImportProfile -Profile $Profile | Out-Null
    Write-VdjmImportStamp -Profile $Profile -Transaction $transaction -TargetDir $Plan.target_dir

    if ([bool]$Profile.run_p4_reconcile) {
        Invoke-VdjmImportP4Reconcile -TargetDir $Plan.target_dir
    }

    return $transaction
}

function Invoke-VdjmPluginImport {
    param(
        [Parameter(Mandatory = $true)]$Profile,
        [bool]$AssumeYes
    )

    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("VdjmPluginImport_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
    try {
        Write-Host " [SYSTEM] Source export is being prepared..." -ForegroundColor Yellow
        $exportInfo = New-VdjmImportExport -Profile $Profile -TempRoot $tempRoot
        Write-Host " [SYSTEM] Comparing source and target..." -ForegroundColor Yellow
        $plan = New-VdjmImportPlan -Profile $Profile -ExportInfo $exportInfo
        Write-Host (" [SYSTEM] Plan: +{0} ~{1} -{2} unchanged={3} localChanges={4}" -f `
            @($plan.operations | Where-Object { $_.operation -eq "added" }).Count,
            @($plan.operations | Where-Object { $_.operation -eq "modified" }).Count,
            @($plan.operations | Where-Object { $_.operation -eq "deleted" }).Count,
            $plan.unchanged_count,
            $plan.local_change_count) -ForegroundColor Cyan
        $transaction = Invoke-VdjmPluginImportPlan -Profile $Profile -Plan $plan -AssumeYes $AssumeYes
        Set-VdjmImportLastMessage ("Imported transaction {0}" -f $transaction.transaction_id)
        return $transaction
    }
    finally {
        if (Test-Path -LiteralPath $tempRoot) {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force
        }
    }
}

function Undo-VdjmPluginImport {
    param(
        [Parameter(Mandatory = $true)]$Profile,
        [string]$TransactionId = ""
    )

    if ([string]::IsNullOrWhiteSpace($TransactionId)) {
        $TransactionId = $Profile.last_transaction_id
    }
    if ([string]::IsNullOrWhiteSpace($TransactionId)) {
        throw "No transaction id is available for undo."
    }

    $transaction = Read-VdjmImportTransaction -TransactionId $TransactionId
    if ([bool]$transaction.undone) {
        throw "Transaction is already undone: $TransactionId"
    }

    foreach ($operation in (@($transaction.operations) | Sort-Object -Property path -Descending)) {
        $targetPath = [string]$operation.target_path
        if ($operation.operation -eq "added") {
            if (Test-Path -LiteralPath $targetPath) {
                Remove-Item -LiteralPath $targetPath -Force
            }
        }
        elseif ($operation.operation -eq "modified" -or $operation.operation -eq "deleted") {
            if ([string]::IsNullOrWhiteSpace($operation.backup_path) -or -not (Test-Path -LiteralPath $operation.backup_path)) {
                throw "Missing backup file for undo: $($operation.path)"
            }

            $targetParent = Split-Path -Parent $targetPath
            if (-not (Test-Path -LiteralPath $targetParent)) {
                New-Item -ItemType Directory -Path $targetParent -Force | Out-Null
            }
            Copy-Item -LiteralPath $operation.backup_path -Destination $targetPath -Force
        }
    }

    $stampPath = Get-VdjmImportStampPath -TargetDir $transaction.target_dir
    if (-not [string]::IsNullOrWhiteSpace($transaction.stamp_backup_path) -and (Test-Path -LiteralPath $transaction.stamp_backup_path)) {
        $stampParent = Split-Path -Parent $stampPath
        if (-not (Test-Path -LiteralPath $stampParent)) {
            New-Item -ItemType Directory -Path $stampParent -Force | Out-Null
        }
        Copy-Item -LiteralPath $transaction.stamp_backup_path -Destination $stampPath -Force
    }
    elseif (Test-Path -LiteralPath $stampPath) {
        Remove-Item -LiteralPath $stampPath -Force
    }

    Remove-VdjmImportEmptyDirectories -RootDir $transaction.target_dir

    $transaction.undone = $true
    $transaction.undone_at = (Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz")
    Save-VdjmImportTransaction -Transaction $transaction | Out-Null
    Set-VdjmImportLastMessage ("Undone transaction {0}" -f $TransactionId)
    return $transaction
}

function New-VdjmImportPreviewPlan {
    param([Parameter(Mandatory = $true)]$Profile)

    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("VdjmPluginPreview_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
    try {
        $exportInfo = New-VdjmImportExport -Profile $Profile -TempRoot $tempRoot
        return New-VdjmImportPlan -Profile $Profile -ExportInfo $exportInfo
    }
    finally {
        if (Test-Path -LiteralPath $tempRoot) {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force
        }
    }
}

function Write-VdjmImportPlanSummary {
    param(
        [Parameter(Mandatory = $true)]$Plan,
        [int]$MaxLines = 40
    )

    $added = @($Plan.operations | Where-Object { $_.operation -eq "added" }).Count
    $modified = @($Plan.operations | Where-Object { $_.operation -eq "modified" }).Count
    $deleted = @($Plan.operations | Where-Object { $_.operation -eq "deleted" }).Count
    Write-Host (" Source files : {0}" -f $Plan.source_count)
    Write-Host (" Target files : {0}" -f $Plan.target_count)
    Write-Host (" Added        : {0}" -f $added) -ForegroundColor Green
    Write-Host (" Modified     : {0}" -f $modified) -ForegroundColor Yellow
    Write-Host (" Deleted      : {0}" -f $deleted) -ForegroundColor Red
    Write-Host (" Unchanged    : {0}" -f $Plan.unchanged_count)
    Write-Host (" Stale kept   : {0}" -f $Plan.stale_count)
    Write-Host (" Local change : {0}" -f $Plan.local_change_count) -ForegroundColor Magenta
    Write-Host ""

    $shown = 0
    foreach ($operation in @($Plan.operations)) {
        if ($shown -ge $MaxLines) {
            Write-Host (" ... {0} more operations" -f ($Plan.operations.Count - $shown)) -ForegroundColor DarkGray
            break
        }

        $color = "Gray"
        if ($operation.operation -eq "added") { $color = "Green" }
        elseif ($operation.operation -eq "modified") { $color = "Yellow" }
        elseif ($operation.operation -eq "deleted") { $color = "Red" }
        $localFlag = if ($operation.target_local_changed) { " LOCAL" } else { "" }
        Write-Host ("  {0,-8} {1}{2}" -f $operation.operation, $operation.path, $localFlag) -ForegroundColor $color
        ++$shown
    }
}

function Write-VdjmImportIntro {
    Clear-Host
    Write-Host "`n ATDT VDJM-PLUGIN-IMPORT" -ForegroundColor Cyan
    Write-Host " CONNECT 9600/ARQ/V32/LAPM`n" -ForegroundColor Cyan
    Start-Sleep -Milliseconds 350
    Write-Host " 호스트 서버에 접속하고 있습니다..." -ForegroundColor Yellow
    Start-Sleep -Milliseconds 350
}

function Write-VdjmImportHeader {
    Clear-Host
    Write-Host "====================================================================" -ForegroundColor Cyan
    Write-Host "    ██╗   ██╗██████╗      ██╗███╗   ███╗" -ForegroundColor Cyan
    Write-Host "    ██║   ██║██╔══██╗     ██║████╗ ████║" -ForegroundColor Cyan
    Write-Host "    ██║   ██║██║  ██║     ██║██╔████╔██║" -ForegroundColor Cyan
    Write-Host "    ╚██╗ ██╔╝██║  ██║██   ██║██║╚██╔╝██║" -ForegroundColor Cyan
    Write-Host "     ╚████╔╝ ██████╔╝╚█████╔╝██║ ╚═╝ ██║" -ForegroundColor Cyan
    Write-Host "      ╚═══╝  ╚═════╝  ╚════╝ ╚═╝     ╚═╝  VENDOR LINK V2" -ForegroundColor Cyan
    Write-Host "`n            [ $script:ToolTitle ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
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

function Show-VdjmImportSettings {
    param([Parameter(Mandatory = $true)]$Profile)

    $targetDir = Get-VdjmImportTargetPluginDir -Profile $Profile
    Write-Host "  [1] Profile 선택/생성     : " -NoNewline; Write-Host $Profile.profile_name -ForegroundColor Green
    Write-Host "  [2] Git/Folder 원본 설정  : " -NoNewline; Write-Host $Profile.source_repo -ForegroundColor Green
    Write-Host "  [3] 회사 Plugins root     : " -NoNewline; Write-Host $(if ($Profile.target_plugins_root) { $Profile.target_plugins_root } else { "[선택되지 않음]" }) -ForegroundColor Green
    Write-Host "  [4] Target Plugin dir     : " -NoNewline; Write-Host $(if ($targetDir) { $targetDir } else { "[설정되지 않음]" }) -ForegroundColor Green
    Write-Host "  [5] Git Ref               : " -NoNewline; Write-Host $Profile.source_ref -ForegroundColor Green
    Write-Host "  [6] Import Mode           : " -NoNewline; Write-Host $Profile.import_mode -ForegroundColor Green
    Write-Host "  [7] Include Binaries      : " -NoNewline; Write-Host $Profile.include_binaries -ForegroundColor Green
    Write-Host "  [8] Include Content       : " -NoNewline; Write-Host $Profile.include_content -ForegroundColor Green
    Write-Host "  [9] p4 reconcile          : " -NoNewline; Write-Host $Profile.run_p4_reconcile -ForegroundColor Green
    Write-Host "`n  [10] 설정 저장 [Save Profile]"
    Write-Host "  [11] 변경사항 미리보기 [Preview]"
    Write-Host "  [12] Import 실행 [Backup + Apply]"
    Write-Host "  [13] 마지막 Import Undo"
    Write-Host "  [14] GUI 열기"
    Write-Host "  [0] 접속 종료 [Logoff]`n"
    Write-Host "--------------------------------------------------------------------"
    Write-Host ("  Last transaction: {0}" -f $(if ($Profile.last_transaction_id) { $Profile.last_transaction_id } else { "[none]" })) -ForegroundColor DarkGray
    Write-Host ("  Last: {0}" -f $script:LastMessage) -ForegroundColor DarkGray
    Write-Host "--------------------------------------------------------------------"
}

function Show-VdjmImportProfilesInteractive {
    Clear-Host
    Write-Host "===================================================================="
    Write-Host "                   [ 1. Profile 선택/생성 ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
    $profiles = @(Get-ChildItem -LiteralPath $script:ProfileRoot -Filter "*.json" -ErrorAction SilentlyContinue)
    for ($i = 0; $i -lt $profiles.Count; ++$i) {
        Write-Host ("  [{0}] {1}" -f ($i + 1), [System.IO.Path]::GetFileNameWithoutExtension($profiles[$i].Name)) -ForegroundColor Cyan
    }
    Write-Host "`n 빈칸이 아닌 새 이름을 입력하면 profile을 생성/로드합니다."
    $selection = Read-Host "▶ Profile"
    if ([string]::IsNullOrWhiteSpace($selection)) {
        return
    }

    $selectedNumber = 0
    if ([int]::TryParse($selection, [ref]$selectedNumber) -and $selectedNumber -gt 0 -and $selectedNumber -le $profiles.Count) {
        $name = [System.IO.Path]::GetFileNameWithoutExtension($profiles[$selectedNumber - 1].Name)
        $script:CurrentProfile = Read-VdjmImportProfile -Name $name
        Set-VdjmImportLastMessage "Profile loaded: $name"
        return
    }

    $script:CurrentProfile = Read-VdjmImportProfile -Name $selection
    $script:CurrentProfile.profile_name = $selection
    Set-VdjmImportLastMessage "Profile selected: $selection"
}

function Set-VdjmImportSourceRepoInteractive {
    param([Parameter(Mandatory = $true)]$Profile)

    Clear-Host
    Write-Host "===================================================================="
    Write-Host "                   [ 2. 원본 Git/Folder 설정 ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
    Write-Host " 경로 또는 remote URL을 입력하세요. 빈칸이면 폴더 선택창을 엽니다.`n"
    $value = Read-Host "▶ SourceRepo"
    if ([string]::IsNullOrWhiteSpace($value)) {
        $value = Select-VdjmImportFolder "원본 VdjmMobileUi Git 폴더를 선택하세요."
    }
    if (-not [string]::IsNullOrWhiteSpace($value)) {
        $Profile.source_repo = $value
        Set-VdjmImportLastMessage "SourceRepo updated"
    }
}

function Set-VdjmImportPluginsRootInteractive {
    param([Parameter(Mandatory = $true)]$Profile)

    $value = Select-VdjmImportFolder "회사 프로젝트의 Plugins 폴더를 선택하세요."
    if (-not [string]::IsNullOrWhiteSpace($value)) {
        $Profile.target_plugins_root = $value
        $Profile.target_plugin_dir = ""
        if ([string]::IsNullOrWhiteSpace($Profile.target_plugin_name)) {
            $Profile.target_plugin_name = $script:PluginName
        }
        Set-VdjmImportLastMessage "Target Plugins root updated"
    }
}

function Set-VdjmImportTargetDirInteractive {
    param([Parameter(Mandatory = $true)]$Profile)

    $value = Select-VdjmImportFolder "회사 프로젝트의 Plugins\VdjmMobileUi 폴더를 선택하세요. 없으면 만들 위치를 선택해도 됩니다."
    if (-not [string]::IsNullOrWhiteSpace($value)) {
        $Profile.target_plugin_dir = $value
        Set-VdjmImportLastMessage "Target plugin dir updated"
    }
}

function Set-VdjmImportRefInteractive {
    param([Parameter(Mandatory = $true)]$Profile)

    $value = Read-Host "▶ Ref / branch / tag / commit"
    if (-not [string]::IsNullOrWhiteSpace($value)) {
        $Profile.source_ref = $value
        Set-VdjmImportLastMessage "Ref updated"
    }
}

function Show-VdjmImportPreviewInteractive {
    param([Parameter(Mandatory = $true)]$Profile)

    Clear-Host
    Write-Host "===================================================================="
    Write-Host "                   [ 11. Import Preview ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
    try {
        $plan = New-VdjmImportPreviewPlan -Profile $Profile
        Write-VdjmImportPlanSummary -Plan $plan -MaxLines 80
    }
    catch {
        Write-Host (" [ERROR] {0}" -f $_.Exception.Message) -ForegroundColor Red
    }
    Read-Host "`n엔터를 누르면 메인 메뉴로 돌아갑니다..."
}

function Invoke-VdjmImportInteractive {
    param([Parameter(Mandatory = $true)]$Profile)

    Clear-Host
    Write-Host "===================================================================="
    Write-Host "                   [ 12. Import Execute ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
    try {
        $plan = New-VdjmImportPreviewPlan -Profile $Profile
        Write-VdjmImportPlanSummary -Plan $plan -MaxLines 40
        if ($plan.local_change_count -gt 0) {
            Write-Host "`n [주의] Target local changes detected. 이 파일들은 Perforce 쪽에서 직접 수정되었을 수 있습니다." -ForegroundColor Magenta
        }

        $confirm = Read-Host "`n 실행하려면 IMPORT 입력"
        if ($confirm -ne "IMPORT") {
            Set-VdjmImportLastMessage "Import cancelled"
            return
        }

        $transaction = Invoke-VdjmPluginImport -Profile $Profile -AssumeYes $true
        Write-Host ("`n [SYSTEM] Import complete. Transaction={0}" -f $transaction.transaction_id) -ForegroundColor Cyan
        Write-Host (" Backup: {0}" -f $transaction.backup_dir) -ForegroundColor Green
    }
    catch {
        Set-VdjmImportLastMessage ("Import failed: {0}" -f $_.Exception.Message)
        Write-Host ("`n [ERROR] {0}" -f $_.Exception.Message) -ForegroundColor Red
    }
    Read-Host "`n엔터를 누르면 메인 메뉴로 돌아갑니다..."
}

function Invoke-VdjmUndoInteractive {
    param([Parameter(Mandatory = $true)]$Profile)

    Clear-Host
    Write-Host "===================================================================="
    Write-Host "                   [ 13. Undo Last Import ]" -ForegroundColor Yellow
    Write-Host "====================================================================`n"
    Write-Host (" Last transaction: {0}" -f $Profile.last_transaction_id) -ForegroundColor Green
    $confirm = Read-Host " 되돌리려면 UNDO 입력"
    if ($confirm -ne "UNDO") {
        Set-VdjmImportLastMessage "Undo cancelled"
        return
    }

    try {
        $transaction = Undo-VdjmPluginImport -Profile $Profile
        Write-Host ("`n [SYSTEM] Undo complete. Transaction={0}" -f $transaction.transaction_id) -ForegroundColor Cyan
    }
    catch {
        Set-VdjmImportLastMessage ("Undo failed: {0}" -f $_.Exception.Message)
        Write-Host ("`n [ERROR] {0}" -f $_.Exception.Message) -ForegroundColor Red
    }
    Read-Host "`n엔터를 누르면 메인 메뉴로 돌아갑니다..."
}

function Show-VdjmImportGui {
    param([Parameter(Mandatory = $true)]$Profile)

    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing

    $form = New-Object System.Windows.Forms.Form
    $form.Text = "VD-JM Plugin Import Manager"
    $form.Size = New-Object System.Drawing.Size(980, 700)
    $form.StartPosition = "CenterScreen"

    $font = New-Object System.Drawing.Font("Consolas", 9)
    $form.Font = $font

    $labelWidth = 120
    $textLeft = 135
    $top = 15

    function Add-Label($text, $y) {
        $label = New-Object System.Windows.Forms.Label
        $label.Text = $text
        $label.Location = New-Object System.Drawing.Point(10, $y)
        $label.Size = New-Object System.Drawing.Size($labelWidth, 22)
        $form.Controls.Add($label)
        return $label
    }

    function Add-TextBox($text, $y) {
        $box = New-Object System.Windows.Forms.TextBox
        $box.Text = $text
        $box.Location = New-Object System.Drawing.Point($textLeft, $y)
        $box.Size = New-Object System.Drawing.Size(660, 22)
        $form.Controls.Add($box)
        return $box
    }

    Add-Label "Profile" $top | Out-Null
    $profileBox = Add-TextBox $Profile.profile_name $top
    $top += 30
    Add-Label "Source" $top | Out-Null
    $sourceBox = Add-TextBox $Profile.source_repo $top
    $browseSource = New-Object System.Windows.Forms.Button
    $browseSource.Text = "..."
    $browseSource.Location = New-Object System.Drawing.Point(805, $top)
    $browseSource.Size = New-Object System.Drawing.Size(35, 23)
    $form.Controls.Add($browseSource)
    $top += 30
    Add-Label "Target Plugin" $top | Out-Null
    $targetBox = Add-TextBox (Get-VdjmImportTargetPluginDir -Profile $Profile) $top
    $browseTarget = New-Object System.Windows.Forms.Button
    $browseTarget.Text = "..."
    $browseTarget.Location = New-Object System.Drawing.Point(805, $top)
    $browseTarget.Size = New-Object System.Drawing.Size(35, 23)
    $form.Controls.Add($browseTarget)
    $top += 30
    Add-Label "Ref" $top | Out-Null
    $refBox = Add-TextBox $Profile.source_ref $top
    $top += 35

    $includeBinariesBox = New-Object System.Windows.Forms.CheckBox
    $includeBinariesBox.Text = "Include Binaries"
    $includeBinariesBox.Checked = [bool]$Profile.include_binaries
    $includeBinariesBox.Location = New-Object System.Drawing.Point(10, $top)
    $includeBinariesBox.Size = New-Object System.Drawing.Size(160, 24)
    $form.Controls.Add($includeBinariesBox)

    $includeContentBox = New-Object System.Windows.Forms.CheckBox
    $includeContentBox.Text = "Include Content"
    $includeContentBox.Checked = [bool]$Profile.include_content
    $includeContentBox.Location = New-Object System.Drawing.Point(180, $top)
    $includeContentBox.Size = New-Object System.Drawing.Size(150, 24)
    $form.Controls.Add($includeContentBox)

    $p4Box = New-Object System.Windows.Forms.CheckBox
    $p4Box.Text = "p4 reconcile"
    $p4Box.Checked = [bool]$Profile.run_p4_reconcile
    $p4Box.Location = New-Object System.Drawing.Point(340, $top)
    $p4Box.Size = New-Object System.Drawing.Size(130, 24)
    $form.Controls.Add($p4Box)

    $modeBox = New-Object System.Windows.Forms.ComboBox
    $modeBox.DropDownStyle = "DropDownList"
    [void]$modeBox.Items.Add("BackupThenMirror")
    [void]$modeBox.Items.Add("OverwriteOnly")
    $modeBox.SelectedItem = $Profile.import_mode
    $modeBox.Location = New-Object System.Drawing.Point(485, $top)
    $modeBox.Size = New-Object System.Drawing.Size(180, 24)
    $form.Controls.Add($modeBox)
    $top += 35

    $previewButton = New-Object System.Windows.Forms.Button
    $previewButton.Text = "Preview"
    $previewButton.Location = New-Object System.Drawing.Point(10, $top)
    $previewButton.Size = New-Object System.Drawing.Size(100, 28)
    $form.Controls.Add($previewButton)

    $importButton = New-Object System.Windows.Forms.Button
    $importButton.Text = "Import"
    $importButton.Location = New-Object System.Drawing.Point(120, $top)
    $importButton.Size = New-Object System.Drawing.Size(100, 28)
    $form.Controls.Add($importButton)

    $undoButton = New-Object System.Windows.Forms.Button
    $undoButton.Text = "Undo Last"
    $undoButton.Location = New-Object System.Drawing.Point(230, $top)
    $undoButton.Size = New-Object System.Drawing.Size(100, 28)
    $form.Controls.Add($undoButton)

    $saveButton = New-Object System.Windows.Forms.Button
    $saveButton.Text = "Save Profile"
    $saveButton.Location = New-Object System.Drawing.Point(340, $top)
    $saveButton.Size = New-Object System.Drawing.Size(120, 28)
    $form.Controls.Add($saveButton)
    $top += 40

    $listView = New-Object System.Windows.Forms.ListView
    $listView.View = "Details"
    $listView.FullRowSelect = $true
    $listView.GridLines = $true
    $listView.Location = New-Object System.Drawing.Point(10, $top)
    $listView.Size = New-Object System.Drawing.Size(940, 360)
    [void]$listView.Columns.Add("Operation", 100)
    [void]$listView.Columns.Add("Path", 610)
    [void]$listView.Columns.Add("Local", 70)
    [void]$listView.Columns.Add("Size", 100)
    $form.Controls.Add($listView)
    $top += 370

    $logBox = New-Object System.Windows.Forms.TextBox
    $logBox.Multiline = $true
    $logBox.ScrollBars = "Vertical"
    $logBox.ReadOnly = $true
    $logBox.Location = New-Object System.Drawing.Point(10, $top)
    $logBox.Size = New-Object System.Drawing.Size(940, 120)
    $form.Controls.Add($logBox)

    $currentPlan = $null

    function Sync-ProfileFromGui {
        $Profile.profile_name = $profileBox.Text.Trim()
        $Profile.source_repo = $sourceBox.Text.Trim()
        $Profile.target_plugin_dir = $targetBox.Text.Trim()
        $Profile.target_plugins_root = ""
        $Profile.source_ref = $refBox.Text.Trim()
        $Profile.include_binaries = $includeBinariesBox.Checked
        $Profile.include_content = $includeContentBox.Checked
        $Profile.include_config = $true
        $Profile.run_p4_reconcile = $p4Box.Checked
        $Profile.import_mode = [string]$modeBox.SelectedItem
    }

    function Write-GuiLog {
        param([string]$Message)
        $logBox.AppendText(("[{0:HH:mm:ss}] {1}`r`n" -f (Get-Date), $Message))
    }

    function Render-PlanToList {
        param($Plan)
        $listView.Items.Clear()
        foreach ($operation in @($Plan.operations)) {
            $item = New-Object System.Windows.Forms.ListViewItem($operation.operation)
            [void]$item.SubItems.Add($operation.path)
            [void]$item.SubItems.Add($(if ($operation.target_local_changed) { "YES" } else { "" }))
            [void]$item.SubItems.Add([string]$operation.length)
            if ($operation.operation -eq "added") { $item.ForeColor = [System.Drawing.Color]::Green }
            elseif ($operation.operation -eq "modified") { $item.ForeColor = [System.Drawing.Color]::DarkOrange }
            elseif ($operation.operation -eq "deleted") { $item.ForeColor = [System.Drawing.Color]::Red }
            if ($operation.target_local_changed) { $item.BackColor = [System.Drawing.Color]::MistyRose }
            [void]$listView.Items.Add($item)
        }
    }

    $browseSource.Add_Click({
        $selected = Select-VdjmImportFolder "원본 Git/folder를 선택하세요."
        if ($selected) { $sourceBox.Text = $selected }
    })
    $browseTarget.Add_Click({
        $selected = Select-VdjmImportFolder "대상 Plugins\VdjmMobileUi 폴더를 선택하세요."
        if ($selected) { $targetBox.Text = $selected }
    })
    $saveButton.Add_Click({
        Sync-ProfileFromGui
        Save-VdjmImportProfile -Profile $Profile | Out-Null
        Write-GuiLog "Profile saved."
    })
    $previewButton.Add_Click({
        try {
            Sync-ProfileFromGui
            Write-GuiLog "Building preview plan..."
            $script:GuiPlan = New-VdjmImportPreviewPlan -Profile $Profile
            $currentPlan = $script:GuiPlan
            Render-PlanToList -Plan $currentPlan
            Write-GuiLog ("Preview ready. Operations={0}, LocalChanges={1}" -f $currentPlan.operations.Count, $currentPlan.local_change_count)
        }
        catch {
            Write-GuiLog ("ERROR: {0}" -f $_.Exception.Message)
        }
    })
    $importButton.Add_Click({
        try {
            Sync-ProfileFromGui
            $currentPlan = New-VdjmImportPreviewPlan -Profile $Profile
            Render-PlanToList -Plan $currentPlan
            if ($currentPlan.local_change_count -gt 0) {
                $result = [System.Windows.Forms.MessageBox]::Show("Target local changes detected. Import anyway?", "Confirm Import", "YesNo", "Warning")
                if ($result -ne [System.Windows.Forms.DialogResult]::Yes) { return }
            }
            else {
                $result = [System.Windows.Forms.MessageBox]::Show("Import selected plugin copy?", "Confirm Import", "YesNo", "Question")
                if ($result -ne [System.Windows.Forms.DialogResult]::Yes) { return }
            }
            $transaction = Invoke-VdjmPluginImport -Profile $Profile -AssumeYes $true
            Write-GuiLog ("Import complete. Transaction={0}" -f $transaction.transaction_id)
            $currentPlan = $null
        }
        catch {
            Write-GuiLog ("ERROR: {0}" -f $_.Exception.Message)
        }
    })
    $undoButton.Add_Click({
        try {
            Sync-ProfileFromGui
            $result = [System.Windows.Forms.MessageBox]::Show("Undo last transaction $($Profile.last_transaction_id)?", "Confirm Undo", "YesNo", "Warning")
            if ($result -ne [System.Windows.Forms.DialogResult]::Yes) { return }
            $transaction = Undo-VdjmPluginImport -Profile $Profile
            Write-GuiLog ("Undo complete. Transaction={0}" -f $transaction.transaction_id)
        }
        catch {
            Write-GuiLog ("ERROR: {0}" -f $_.Exception.Message)
        }
    })

    [void]$form.ShowDialog()
}

function Show-VdjmImportMenu {
    param([Parameter(Mandatory = $true)]$Profile)

    $originalBackground = $Host.UI.RawUI.BackgroundColor
    $originalForeground = $Host.UI.RawUI.ForegroundColor
    $Host.UI.RawUI.BackgroundColor = "DarkBlue"
    $Host.UI.RawUI.ForegroundColor = "Gray"

    try {
        Write-VdjmImportIntro
        while ($true) {
            Write-VdjmImportHeader
            Show-VdjmImportSettings -Profile $Profile
            $menuSel = Read-Host "▶ 원하시는 메뉴 번호를 입력하십시오 (0-14)"

            switch ($menuSel) {
                "1" {
                    Show-VdjmImportProfilesInteractive
                    $Profile = $script:CurrentProfile
                }
                "2" { Set-VdjmImportSourceRepoInteractive -Profile $Profile }
                "3" { Set-VdjmImportPluginsRootInteractive -Profile $Profile }
                "4" { Set-VdjmImportTargetDirInteractive -Profile $Profile }
                "5" { Set-VdjmImportRefInteractive -Profile $Profile }
                "6" {
                    $Profile.import_mode = if ($Profile.import_mode -eq "BackupThenMirror") { "OverwriteOnly" } else { "BackupThenMirror" }
                    Set-VdjmImportLastMessage ("ImportMode = {0}" -f $Profile.import_mode)
                }
                "7" {
                    $Profile.include_binaries = -not [bool]$Profile.include_binaries
                    Set-VdjmImportLastMessage ("IncludeBinaries = {0}" -f $Profile.include_binaries)
                }
                "8" {
                    $Profile.include_content = -not [bool]$Profile.include_content
                    Set-VdjmImportLastMessage ("IncludeContent = {0}" -f $Profile.include_content)
                }
                "9" {
                    $Profile.run_p4_reconcile = -not [bool]$Profile.run_p4_reconcile
                    Set-VdjmImportLastMessage ("RunP4Reconcile = {0}" -f $Profile.run_p4_reconcile)
                }
                "10" {
                    Save-VdjmImportProfile -Profile $Profile | Out-Null
                    Set-VdjmImportLastMessage "Profile saved"
                }
                "11" { Show-VdjmImportPreviewInteractive -Profile $Profile }
                "12" { Invoke-VdjmImportInteractive -Profile $Profile }
                "13" { Invoke-VdjmUndoInteractive -Profile $Profile }
                "14" { Show-VdjmImportGui -Profile $Profile }
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

            $script:CurrentProfile = $Profile
        }
    }
    finally {
        $Host.UI.RawUI.BackgroundColor = $originalBackground
        $Host.UI.RawUI.ForegroundColor = $originalForeground
        Clear-Host
    }
}

Initialize-VdjmImportState
$script:CurrentProfile = Read-VdjmImportProfile -Name $ProfileName
$script:CurrentProfile = Update-VdjmImportProfileFromParameters -Profile $script:CurrentProfile

switch ($Run) {
    "Gui" {
        Show-VdjmImportGui -Profile $script:CurrentProfile
    }
    "Preview" {
        $plan = New-VdjmImportPreviewPlan -Profile $script:CurrentProfile
        Write-VdjmImportPlanSummary -Plan $plan -MaxLines 120
    }
    "Import" {
        $transaction = Invoke-VdjmPluginImport -Profile $script:CurrentProfile -AssumeYes ([bool]$Force)
        Write-Host ("Imported transaction: {0}" -f $transaction.transaction_id) -ForegroundColor Cyan
    }
    "Undo" {
        $transaction = Undo-VdjmPluginImport -Profile $script:CurrentProfile
        Write-Host ("Undone transaction: {0}" -f $transaction.transaction_id) -ForegroundColor Cyan
    }
    "Profiles" {
        Get-ChildItem -LiteralPath $script:ProfileRoot -Filter "*.json" -ErrorAction SilentlyContinue |
            ForEach-Object { [System.IO.Path]::GetFileNameWithoutExtension($_.Name) }
    }
    "None" {
        Write-Host "[vdjm] Loaded plugin import manager."
        Write-Host "[vdjm] State root: $script:StateRoot"
        Write-Host "[vdjm] Menu      : powershell -ExecutionPolicy Bypass -File .\Tools\Import-VdjmPlugin.ps1"
        Write-Host "[vdjm] GUI       : powershell -ExecutionPolicy Bypass -File .\Tools\Import-VdjmPlugin.ps1 -Run Gui"
    }
    default {
        Show-VdjmImportMenu -Profile $script:CurrentProfile
    }
}
