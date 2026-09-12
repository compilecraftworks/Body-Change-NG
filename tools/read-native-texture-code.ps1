# Read-only diagnostics for the explicitly scoped TuLED SE 1.5.97 process.
# Reads five texture virtual implementations, never actor/save/private data.
# Does not suspend, inject, call engine functions, or write process memory.
$ErrorActionPreference = 'Stop'
$bcngProcess = Get-Process SkyrimSE -ErrorAction Stop | Select-Object -First 1
$bcngModule = $bcngProcess.MainModule
$bcngExpectedGame = 'D:\TuLED13E\STOCK GAME\Skyrim Special Edition\SkyrimSE.exe'
if ($bcngModule.FileName -ine $bcngExpectedGame) { throw 'Not the scoped TuLED executable' }
$bcngVersion = $bcngModule.FileVersionInfo
if ($bcngVersion.FileVersion.Trim() -ne '1.5.97.0') {
    throw 'This diagnostic is pinned to SE 1.5.97'
}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class BCNGTextureCodeRead {
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool ReadProcessMemory(IntPtr handle, IntPtr address, byte[] data, UIntPtr size, out UIntPtr read);
    [DllImport("kernel32.dll")]
    public static extern bool CloseHandle(IntPtr handle);
}
'@
$bcngHandle = [BCNGTextureCodeRead]::OpenProcess(0x410, $false, $bcngProcess.Id)
if ($bcngHandle -eq [IntPtr]::Zero) { throw 'Read-only process access denied' }
try {
    $bcngBase = $bcngModule.BaseAddress.ToInt64()
    $bcngEnd = $bcngBase + $bcngModule.ModuleMemorySize
    function Read-TextureCodeBytes([long]$address, [int]$count) {
        if ($address -lt $bcngBase -or $address + $count -gt $bcngEnd) { throw 'Address outside Skyrim module' }
        $bytes = New-Object byte[] $count
        $read = [UIntPtr]::Zero
        if (-not [BCNGTextureCodeRead]::ReadProcessMemory($bcngHandle, [IntPtr]$address, $bytes,
                [UIntPtr]$count, [ref]$read) -or $read.ToUInt64() -ne $count) { throw 'Incomplete code read' }
        return ,$bytes
    }
    # Vtable RVAs verified using the installed Address Library and on-disk PE.
    $bcngTargets = @(
        @{name='TXST_GetPath'; table=0x1578790; slot=0x25},
        @{name='TXST_LoadTexture'; table=0x1578790; slot=0x26},
        @{name='TXST_SetPath'; table=0x1578790; slot=0x27},
        @{name='TESTexture_NormalPath'; table=0x153E688; slot=5},
        @{name='TESTexture_DefaultPath'; table=0x153E688; slot=6}
    )
    $bcngResults = foreach ($bcngTarget in $bcngTargets) {
        $bcngPointer = Read-TextureCodeBytes ($bcngBase + $bcngTarget.table + 8 * $bcngTarget.slot) 8
        $bcngAddress = [BitConverter]::ToInt64($bcngPointer, 0)
        $bcngBytes = Read-TextureCodeBytes $bcngAddress 384
        @{ name=$bcngTarget.name; rva=($bcngAddress-$bcngBase); hex=[Convert]::ToHexString($bcngBytes) }
    }
    $bcngPrefix = [Text.Encoding]::ASCII.GetString((Read-TextureCodeBytes ($bcngBase + 0x1578960) 32)).Split([char]0)[0]
    @{imageBase=$bcngBase; runtime='1.5.97'; textureLoaderPrefix=$bcngPrefix;
        functions=@($bcngResults)} | ConvertTo-Json -Depth 4 -Compress
} finally {
    [void][BCNGTextureCodeRead]::CloseHandle($bcngHandle)
}
