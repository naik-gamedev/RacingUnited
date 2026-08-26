Heritage KTX2 Workbench v2.6

16K NASA BC6H path:
  EXR -> CompressonatorCLI -> BC6H KTX2

For HDR BC6H at scale 1.0 you only need CompressonatorCLI.exe.
KTX is optional for validation. BasisU and ImageMagick are retained only for other presets or scaling.

Recommended NASA 16K settings:
- HDR - BC6H unsigned
- Linear / data / HDR
- Scale 1.000
- Mipmaps OFF
- Quality 3
- Validate ON if ktx.exe is available

v2.6 fixes command-line argument passing, resolves the actual CompressonatorCLI.exe file, and removes unnecessary BC6H HDR switches.


v2.6 FIXES:
- Accepts either CompressonatorCLI.exe or its containing folder.
- Recursively resolves the actual EXE.
- Removes a damaged legacy fallback line.
- Guards all empty path checks.
- Logs exact PowerShell script line and stack on errors.


v2.6 fix:
- Removed use of PowerShell automatic variable $input throughout the Workbench.
- Internal paths now use $sourcePath and $destinationPath.
- Fixes BC6H direct conversion falsely reporting that an existing source EXR does not exist.
