param(
    [Parameter(Mandatory)] [string] $Name,   # test name: scripts\<Name>.txt -> logs\<Name>.log
    [int] $LateJoin = 0,                     # seconds before "Luc" joins (0 = never)
    [int] $LeaveAfter = 0,                   # seconds before "Luc" leaves (0 = never)
    [switch] $NoServer,                      # main client only, no server, no musicians
    [int] $Timeout = 90,
    [string] $Lang = '',
    [string[]] $ServerArgs = @(),
    [string[]] $Events = @()               # 'seconds|paul or server|method|{params json}'
)
# Off-screen accessibility test of the JamulusAccess client (test build, no audio device).
# Local server on 127.0.0.1:22300 with fake musicians; the GUI client runs the script.
$ErrorActionPreference = 'Stop'
# Build first, next to the repository: ..\build_test, qmake JamulusAccess.pro CONFIG+=release CONFIG+=accesstest
# Scripts: tests\scripts\<Name>.txt (commands: wait, key, type, focus, tab, rows, log, quit; see src\access\accesstest.cpp)
# Results: ..\build_test\testdata\logs\<Name>.log
$scripts   = Join-Path $PSScriptRoot 'scripts'
$buildTest = Join-Path (Split-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) -Parent) 'build_test'
$here      = Join-Path $buildTest 'testdata'
$exe       = Join-Path $buildTest 'release\JamulusAccess.exe'
New-Item -ItemType Directory -Force "$here\logs", "$here\ini" | Out-Null

function Write-Ini($file, $name, $instrument, $city, $skill) {
    $b64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($name))
    $cityB64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($city))
    @"
<client>
 <name_base64>$b64</name_base64>
 <instrument>$instrument</instrument>
 <city_base64>$cityB64</city_base64>
 <skill>$skill</skill>
</client>
"@ | Set-Content -Encoding UTF8 $file
}

function Send-Rpc($target, $method, $params) {
    $port = if ($target -eq 'server') { 22401 } else { 22400 }
    try {
        $c = [Net.Sockets.TcpClient]::new('127.0.0.1', $port); $s = $c.GetStream()
        $w = [IO.StreamWriter]::new($s); $w.AutoFlush = $true; $r = [IO.StreamReader]::new($s)
        $w.WriteLine('{"id":1,"jsonrpc":"2.0","method":"jamulus/apiAuth","params":{"secret":"access-test-secret-0123456789"}}'); $null = $r.ReadLine()
        $w.WriteLine("{`"id`":2,`"jsonrpc`":`"2.0`",`"method`":`"$method`",`"params`":$params}"); "RPC $method -> " + $r.ReadLine()
        $c.Close()
    } catch { "RPC $method failed: $_" }
}

$procs = @()
$env:QT_QPA_PLATFORM = 'offscreen'
Remove-Item Env:JAMULUSACCESS_SCRIPT -ErrorAction SilentlyContinue
try {
    if (-not $NoServer) {
        'access-test-secret-0123456789' | Set-Content -Encoding ascii "$here\ini\secret.txt"
        New-Item -ItemType Directory -Force "$here\rec" | Out-Null
        $procs += Start-Process $exe -ArgumentList (@('-s','-n','-p','22300','--serverbindip','127.0.0.1','-R',"$here\rec",'--norecord','-i',"$here\ini\server.ini",'--jsonrpcport','22401','--jsonrpcsecretfile',"$here\ini\secret.txt") + $ServerArgs) -PassThru -WindowStyle Hidden
        Start-Sleep 2
        Write-Ini "$here\ini\paul.ini"  'Paul'  1 'Montreal' 3
        Write-Ini "$here\ini\marie.ini" 'Marie' 4 'Quebec' 2
        Write-Ini "$here\ini\luc.ini"   'Luc'   2 'Paris' 1
        $port = 22311
        foreach ($m in 'paul','marie') {
            $rpc = if ($m -eq 'paul') { @('--jsonrpcport','22400','--jsonrpcsecretfile',"$here\ini\secret.txt") } else { @() }
            $procs += Start-Process $exe -ArgumentList (@('-n','-p',"$port",'-c','127.0.0.1:22300','-i',"$here\ini\$m.ini") + $rpc) -PassThru -WindowStyle Hidden; $port++
        }
        Start-Sleep 3
    }

    $env:JAMULUSACCESS_SCRIPT = "$scripts\$Name.txt"
    $env:JAMULUSACCESS_LOG    = "$here\logs\$Name.log"
    $args = @('-p', '22314', '-i', "$here\ini\me_$Name.ini")
    if (-not $NoServer) { $args += @('-c', '127.0.0.1:22300') }
    Write-Ini "$here\ini\me_$Name.ini" 'Lee' 5 'Montreal' 3
    if ($Lang) { (Get-Content "$here\ini\me_$Name.ini") -replace '</client>'," <language>$Lang</language>`n</client>" | Set-Content -Encoding UTF8 "$here\ini\me_$Name.ini" }
    $main = Start-Process $exe -ArgumentList $args -PassThru -WindowStyle Hidden -RedirectStandardError "$here\logs\$Name.err"
    Remove-Item Env:JAMULUSACCESS_SCRIPT

    $luc = $null
    $done = [Collections.Generic.HashSet[string]]::new()
    $start = Get-Date
    while (-not $main.HasExited -and ((Get-Date) - $start).TotalSeconds -lt $Timeout) {
        $t = ((Get-Date) - $start).TotalSeconds
        if ($LateJoin -gt 0 -and -not $luc -and $t -ge $LateJoin) {
            $luc = Start-Process $exe -ArgumentList '-n','-p','22313','-c','127.0.0.1:22300','-i',"$here\ini\luc.ini" -PassThru -WindowStyle Hidden -RedirectStandardError "$here\logs\luc.err" -RedirectStandardOutput "$here\logs\luc.out"
            $procs += $luc
        }
        foreach ($e in @($Events)) {
            $parts = $e -split '\|', 4
            if (-not $done.Contains($e) -and $t -ge [double]$parts[0]) { $null = $done.Add($e); Send-Rpc $parts[1] $parts[2] $parts[3] }
        }
        if ($LeaveAfter -gt 0 -and $luc -and -not $luc.HasExited -and $t -ge $LeaveAfter) {
            $luc.Kill()
        }
        Start-Sleep -Milliseconds 250
    }
    if (-not $main.HasExited) { $main.Kill(); "TIMEOUT" }
}
finally {
    foreach ($p in $procs) { if ($p -and -not $p.HasExited) { $p.Kill() } }
}
Get-Content "$here\logs\$Name.log"
