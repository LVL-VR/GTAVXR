# GTAVXR Trainer

See `module/NOTICE.md`, `module/COPYING.txt`, `module/LICENSE.txt` and
`pugixml/LICENSE.md` for provenance and licence texts.

The identifiers used by the Trainer are listed in the [public CitizenFX table](https://github.com/citizenfx/fivem/blob/ebbf1487d9b4f8437dbff20677d4f2c907f7c999/code/components/rage-scripting-five/include/CrossMapping_Universal.h).

Build on Windows x64 using Python and Microsoft C++ tools 14.51.36248:

```powershell
python reproduce_trainer.py --output out --vcvars 'C:/path/to/vcvars64.bat'
```

The script verifies `out/gtavxr_trainer.dll` against the release SHA-256.
To build a local revision, call `build_trainer.cmd` with the module directory,
the pugixml directory, a new output directory and `vcvars64.bat`.
