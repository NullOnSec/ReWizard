# Build a minimal bootable disk image for Bochs.
# Creates a 1.44MB floppy image with an MBR bootloader that:
#   1. Switches from 16-bit real mode to 64-bit long mode
#   2. Sets up identity-mapped page tables (PML4 -> PDPT -> PD with 2MB pages)
#   3. Jumps to a 64-bit kernel stub that writes "ReWizard" to the VGA text buffer
#   4. Halts the CPU
#
# The output is written to tests/fixtures/boot_disk.img (flat binary).
# This image can be booted by Bochs with:
#   BochsExecutor.SetDiskImage("tests/fixtures/boot_disk.img");
#   BochsExecutor.Initialize();
#   BochsExecutor.BootFromDisk(100000);

param(
    [string]$OutputPath = "$PSScriptRoot\..\tests\fixtures\boot_disk.img"
)

$ErrorActionPreference = "Stop"

# --- NASM assembly for the bootloader + kernel ---
# Sector 0: MBR bootloader (512 bytes, loaded at 0x7C00 by BIOS)
# Sectors 1+: 64-bit kernel stub
$asmCode = @'
; ReWizard minimal bootable disk image
; Boot sector: real mode -> protected mode -> long mode -> jump to kernel
; Kernel stub: write "ReWizard" to VGA text buffer at 0xB8000, then halt

[BITS 16]
[ORG 0x7C00]

start:
    ; Disable interrupts
    cli

    ; Load kernel from disk (sectors 2-18, LBA 1-17) to 0x10000
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Load kernel sectors using INT 13h
    ; We read from CHS=0/0/2 (sector 2) onwards, 17 sectors to 0x10000
    mov ah, 0x02        ; BIOS read sectors
    mov al, 17           ; number of sectors to read
    mov ch, 0            ; cylinder 0
    mov cl, 2            ; start from sector 2 (1-indexed, sector 1 is MBR)
    mov dh, 0            ; head 0
    mov dl, 0x80         ; first hard drive
    mov bx, 0x1000      ; ES:BX = 0x1000:0000 = physical 0x10000
    mov es, bx
    mov bx, 0x0000
    int 0x13
    jc disk_error       ; CF set on error

    ; Enable A20 line via fast A20 gate (port 0x92)
    in al, 0x92
    or al, 2
    and al, 0xFE        ; Reset bit 0 to avoid fast reset
    out 0x92, al

    ; Disable paging and set up identity mapping before switching modes
    ; Point CR3 at PML4 at 0x70000
    mov eax, 0x70000
    mov cr3, eax

    ; Set up PML4[0] -> PDPT at 0x71000
    mov dword [0x70000], 0x71003   ; present + writable
    mov dword [0x70004], 0

    ; Set up PDPT[0] -> PD at 0x72000
    mov dword [0x71000], 0x72003   ; present + writable
    mov dword [0x71004], 0

    ; Set up PD: 256 entries mapping first 512MB with 2MB pages
    mov ecx, 0           ; counter
    mov edx, 0x000083     ; present + writable + page size (2MB)
    mov edi, 0x72000      ; PD base
.fill_pd:
    mov dword [edi], edx
    mov dword [edi+4], 0
    add edx, 0x200000    ; next 2MB
    add edi, 8
    inc ecx
    cmp ecx, 256
    jne .fill_pd

    ; Load GDT
    lgdt [gdt_ptr]

    ; Enter 32-bit protected mode
    mov eax, cr0
    or eax, 1            ; PE bit
    mov cr0, eax

    ; Far jump to 32-bit code
    jmp 0x08:pm_entry

disk_error:
    ; Print 'E' to screen on disk error
    mov al, 'E'
    mov ah, 0x0E
    int 0x10
    hlt
    jmp $

; --- 32-bit protected mode code ---
[BITS 32]
pm_entry:
    ; Set up segment registers for 32-bit protected mode
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Enable PAE (CR4.PAE = bit 5)
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Load CR3 with PML4 (already set above)
    mov eax, 0x70000
    mov cr3, eax

    ; Enable long mode (IA32_EFER.LME = bit 8)
    mov ecx, 0xC0000080   ; IA32_EFER MSR
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging (CR0.PG = bit 31) - enters compatibility mode
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Far jump to 64-bit code at 0x10000
    jmp 0x08:0x10000

; --- GDT ---
align 16
gdt_start:
    dq 0                  ; null descriptor
code64_desc:
    dw 0                  ; limit low (ignored in 64-bit mode)
    dw 0                  ; base low
    db 0                  ; base mid
    db 10011010b          ; access: present, ring 0, code, read
    db 00100000b          ; flags: long mode (L=1), limit high
    db 0                  ; base high
data64_desc:
    dw 0xFFFF             ; limit low
    dw 0                  ; base low
    db 0                  ; base mid
    db 10010010b          ; access: present, ring 0, data, writable
    db 00000000b          ; flags: 32-bit, limit high
    db 0                  ; base high
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1   ; GDT limit
    dd gdt_start                    ; GDT base (32-bit)

; Pad to 510 bytes, add boot signature
times 510 - ($ - $$) db 0
dw 0xAA55

; --- Sector 1: not used (padding) ---
times 512 db 0
'@

# --- 64-bit kernel stub (loaded at 0x10000 by bootloader) ---
$kernelCode = @'
; ReWizard 64-bit kernel stub
; Writes "ReWizard" to VGA text buffer, writes magic to port 0x501 (Bochs debug exit), halts
[BITS 64]

kernel_entry:
    ; Set up segment registers for long mode
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x90000

    ; Write "ReWizard" to VGA text buffer at 0xB8000
    ; Each character is 2 bytes: ASCII + attribute (0x1F = white on blue)
    mov rdi, 0xB8000
    mov rax, 0x1F521F52    ; 'Re' in little-endian
    mov [rdi], rax
    mov rax, 0x1F691F57    ; 'Wi'
    mov [rdi+4], rax
    mov rax, 0x1F611F72    ; 'ar' -- actually 'za'
    mov [rdi+8], rax
    mov rax, 0x1F641F72    ; 'rd'
    mov [rdi+12], rax

    ; Write magic value to Bochs debug exit port (0x501)
    ; This lets tests detect that the kernel actually ran.
    mov dx, 0x501
    mov al, 0x31            ; exit code 0x31 (arbitrary signature)
    out dx, al

    ; Halt
    cli
.halt:
    hlt
    jmp .halt
'@

# Create output directory if needed
$outputDir = Split-Path -Parent $OutputPath
if ($outputDir -and !(Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

# Check if NASM is available
$nasmPath = Get-Command nasm -ErrorAction SilentlyContinue
if (-not $nasmPath) {
    # Try common NASM locations
    $nasmCandidates = @(
        "Z:\nasm\nasm.exe",
        "C:\nasm\nasm.exe",
        "${env:ProgramFiles}\nasm\nasm.exe"
    )
    foreach ($cand in $nasmCandidates) {
        if (Test-Path $cand) {
            $nasmPath = $cand
            break
        }
    }
}

if (-not $nasmPath) {
    Write-Host "NASM not found. Installing NASM via winget or downloading..."
    # Try winget
    $wingetResult = winget install --id NASM.NASM --accept-source-agreements --accept-package-agreements 2>&1
    if ($LASTEXITCODE -eq 0) {
        $nasmPath = Get-Command nasm -ErrorAction SilentlyContinue
    }
}

if (-not $nasmPath) {
    Write-Error "NASM is required to build the boot disk image. Install from https://www.nasm.us/ or via: winget install NASM.NASM"
    exit 1
}

Write-Host "Using NASM: $nasmPath"

# Create temp directory for assembly files
$tempDir = Join-Path $env:TEMP "rewizard_boot_disk_$(Get-Random)"
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

try {
    # Write bootloader assembly
    $bootAsm = Join-Path $tempDir "boot.asm"
    $bootBin = Join-Path $tempDir "boot.bin"
    $asmCode | Set-Content -Path $bootAsm -Encoding ASCII

    # Write kernel assembly
    $kernelAsm = Join-Path $tempDir "kernel.asm"
    $kernelBin = Join-Path $tempDir "kernel.bin"
    $kernelCode | Set-Content -Path $kernelAsm -Encoding ASCII

    # Assemble bootloader
    & $nasmPath "-f" "bin" $bootAsm "-o" $bootBin
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to assemble bootloader"
        exit 1
    }

    # Assemble kernel
    & $nasmPath "-f" "bin" $kernelAsm "-o" $kernelBin
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to assemble kernel"
        exit 1
    }

    # Build disk image: 1.44MB floppy (2880 sectors of 512 bytes)
    # Layout: sector 0 = MBR (boot.bin), sectors 1+ = kernel
    $diskSize = 2880 * 512  # 1.44MB floppy
    $diskImage = New-Object byte[] $diskSize

    # Read bootloader
    $bootData = [System.IO.File]::ReadAllBytes($bootBin)
    [Array]::Copy($bootData, 0, $diskImage, 0, $bootData.Length)

    # Read kernel and place at offset 512 (sector 1, loaded at 0x10000)
    $kernelData = [System.IO.File]::ReadAllBytes($kernelBin)
    [Array]::Copy($kernelData, 0, $diskImage, 512, $kernelData.Length)

    # Write output
    [System.IO.File]::WriteAllBytes($OutputPath, $diskImage)
    Write-Host "Boot disk image written to: $OutputPath"
    Write-Host "  Boot sector: $($bootData.Length) bytes"
    Write-Host "  Kernel: $($kernelData.Length) bytes"
    Write-Host "  Total image: $($diskImage.Length) bytes ($($diskImage.Length / 1024) KB)"
}
finally {
    Remove-Item -Recurse -Force $tempDir -ErrorAction SilentlyContinue
}