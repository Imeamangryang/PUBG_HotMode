param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $TestFilter
)

$ErrorActionPreference = 'Stop'

function Write-Info {
    param([Parameter(Mandatory = $true)][string] $Message)
    Write-Host $Message -ForegroundColor Cyan
}

function Write-Dim {
    param([Parameter(Mandatory = $true)][string] $Message)
    Write-Host $Message -ForegroundColor DarkGray
}

function Write-Success {
    param([Parameter(Mandatory = $true)][string] $Message)
    Write-Host $Message -ForegroundColor Green
}

function Write-Failure {
    param([Parameter(Mandatory = $true)][string] $Message)
    Write-Host $Message -ForegroundColor Red
}

function Write-Warn {
    param([Parameter(Mandatory = $true)][string] $Message)
    Write-Host $Message -ForegroundColor Yellow
}

function Write-ResultLine {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Label,

        [Parameter(Mandatory = $true)]
        [string] $Value,

        [Parameter(Mandatory = $true)]
        [ConsoleColor] $ValueColor
    )

    Write-Host "${Label}: " -NoNewline
    Write-Host $Value -ForegroundColor $ValueColor
}

function Write-TestStatusLine {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Status,

        [Parameter(Mandatory = $true)]
        [string] $TestPath
    )

    Write-Host '  ' -NoNewline
    if ($Status -eq 'Success') {
        Write-Host "[$Status]" -ForegroundColor Green -NoNewline
    }
    else {
        Write-Host "[$Status]" -ForegroundColor Red -NoNewline
    }
    Write-Host " $TestPath"
}

function Write-TestCompleteLine {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Line,

        [Parameter(Mandatory = $true)]
        [bool] $bSucceeded
    )

    if ($Line -match '^(.*EXIT CODE:\s*)(\d+)(.*)$') {
        Write-Host $Matches[1] -NoNewline
        if ($bSucceeded) {
            Write-Host $Matches[2] -ForegroundColor Green -NoNewline
        }
        else {
            Write-Host $Matches[2] -ForegroundColor Red -NoNewline
        }
        Write-Host $Matches[3]
        return
    }

    Write-Host $Line
}

function Resolve-UnrealEngineRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string] $EngineAssociation
    )

    if ([string]::IsNullOrWhiteSpace($EngineAssociation)) {
        throw 'EngineAssociation is empty.'
    }

    if (Test-Path -LiteralPath $EngineAssociation -PathType Container) {
        return (Resolve-Path -LiteralPath $EngineAssociation).Path
    }

    $buildsKey = 'HKCU:\Software\Epic Games\Unreal Engine\Builds'
    if (Test-Path -LiteralPath $buildsKey) {
        $match = (Get-ItemProperty -LiteralPath $buildsKey).PSObject.Properties |
            Where-Object { $_.Name -eq $EngineAssociation } |
            Select-Object -First 1

        if ($match -and (Test-Path -LiteralPath $match.Value -PathType Container)) {
            return (Resolve-Path -LiteralPath $match.Value).Path
        }
    }

    $launcherRoots = @(
        'HKLM:\SOFTWARE\EpicGames\Unreal Engine',
        'HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine',
        'HKCU:\SOFTWARE\EpicGames\Unreal Engine',
        'HKCU:\SOFTWARE\Epic Games\Unreal Engine'
    )

    foreach ($launcherRoot in $launcherRoots) {
        $versionKey = Join-Path $launcherRoot $EngineAssociation
        if (-not (Test-Path -LiteralPath $versionKey)) {
            continue
        }

        $installedDirectory = (Get-ItemProperty -LiteralPath $versionKey).InstalledDirectory
        if ($installedDirectory -and (Test-Path -LiteralPath $installedDirectory -PathType Container)) {
            return (Resolve-Path -LiteralPath $installedDirectory).Path
        }
    }

    $programFiles = ${env:ProgramFiles}
    $candidateRoots = @(
        (Join-Path $programFiles "Epic Games\UE_$EngineAssociation"),
        (Join-Path $programFiles "Epic Games\UE_${EngineAssociation}_Source")
    )

    foreach ($candidateRoot in $candidateRoots) {
        if (Test-Path -LiteralPath $candidateRoot -PathType Container) {
            return (Resolve-Path -LiteralPath $candidateRoot).Path
        }
    }

    throw "Could not resolve Unreal Engine root for EngineAssociation '$EngineAssociation'."
}

function Write-AutomationSummary {
    param(
        [Parameter(Mandatory = $true)]
        [string] $ProjectFile,

        [Parameter(Mandatory = $true)]
        [int] $ExitCode
    )

    $projectDir = Split-Path -Parent $ProjectFile
    $projectName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectFile)
    $logFile = Join-Path $projectDir "Saved\Logs\$projectName.log"

    Write-Host ''
    Write-Info 'Automation result summary'
    if ($ExitCode -eq 0) {
        Write-ResultLine -Label 'ExitCode' -Value ([string] $ExitCode) -ValueColor Green
    }
    else {
        Write-ResultLine -Label 'ExitCode' -Value ([string] $ExitCode) -ValueColor Red
    }
    Write-Dim "LogFile: $logFile"

    if (-not (Test-Path -LiteralPath $logFile -PathType Leaf)) {
        Write-Warn 'Automation log file was not found.'
        return
    }

    $logLines = Get-Content -LiteralPath $logFile
    $foundLine = @($logLines | Select-String -Pattern 'Found \d+ automation tests' | Select-Object -Last 1)
    $completedTests = @($logLines | Select-String -Pattern 'Test Completed\. Result=\{[^}]+\} Name=\{[^}]+\} Path=\{[^}]+\}')
    $passedTests = @($completedTests | Where-Object { $_.Line -match 'Result=\{Success\}' })
    $failedTests = @($completedTests | Where-Object { $_.Line -notmatch 'Result=\{Success\}' })
    $completeLine = @($logLines | Select-String -Pattern '\*\*\*\* TEST COMPLETE\. EXIT CODE: \d+ \*\*\*\*' | Select-Object -Last 1)

    if ($foundLine.Count -gt 0) {
        Write-Dim $foundLine[0].Line
    }

    if ($passedTests.Count -gt 0) {
        Write-ResultLine -Label 'Passed' -Value ([string] $passedTests.Count) -ValueColor Green
    }
    else {
        Write-ResultLine -Label 'Passed' -Value '0' -ValueColor DarkGray
    }

    if ($failedTests.Count -gt 0) {
        Write-ResultLine -Label 'Failed' -Value ([string] $failedTests.Count) -ValueColor Red
    }
    else {
        Write-ResultLine -Label 'Failed' -Value '0' -ValueColor DarkGray
    }

    if ($completedTests.Count -gt 0 -and $completedTests.Count -le 20) {
        Write-Host ''
        Write-Info 'Completed tests:'
        foreach ($test in $completedTests) {
            if ($test.Line -match 'Result=\{([^}]+)\} Name=\{([^}]+)\} Path=\{([^}]+)\}') {
                Write-TestStatusLine -Status $Matches[1] -TestPath $Matches[3]
            }
        }
    }

    if ($failedTests.Count -gt 0) {
        Write-Host ''
        Write-Failure 'Failed test lines:'
        foreach ($test in $failedTests) {
            Write-Failure "  $($test.Line)"
        }
    }

    if ($completeLine.Count -gt 0) {
        Write-Host ''
        Write-TestCompleteLine -Line $completeLine[0].Line -bSucceeded ($ExitCode -eq 0 -and $failedTests.Count -eq 0)
    }

    if ($ExitCode -ne 0 -or $failedTests.Count -gt 0) {
        $errorLines = @($logLines | Select-String -Pattern 'Fatal|Error:' | Select-Object -First 20)
        if ($errorLines.Count -gt 0) {
            Write-Host ''
            Write-Failure 'First error lines:'
            foreach ($errorLine in $errorLines) {
                Write-Failure "  $($errorLine.Line)"
            }
        }
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectFile = Join-Path $scriptDir 'PUBG_HotMode.uproject'

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    Write-Error "Project file not found: $projectFile"
    exit 1
}

$project = Get-Content -Raw -LiteralPath $projectFile | ConvertFrom-Json
$engineAssociation = [string] $project.EngineAssociation
$engineRoot = Resolve-UnrealEngineRoot -EngineAssociation $engineAssociation
$editorCmd = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'

if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
    Write-Error "UnrealEditor-Cmd.exe not found: $editorCmd"
    exit 1
}

if ($TestFilter.Count -gt 0) {
    $testFilterText = $TestFilter -join ' '
}
else {
    $testFilterText = 'PUBG_HotMode'
}

Write-Dim "EngineAssociation: $engineAssociation"
Write-Dim "Unreal Engine Root: $engineRoot"
Write-Info "Running Unreal automation tests: $testFilterText"

& $editorCmd $projectFile -unattended -nop4 -NullRHI "-ExecCmds=Automation RunTests $testFilterText; Quit" "-TestExit=Automation Test Queue Empty"
$exitCode = $LASTEXITCODE
Write-AutomationSummary -ProjectFile $projectFile -ExitCode $exitCode
exit $exitCode
