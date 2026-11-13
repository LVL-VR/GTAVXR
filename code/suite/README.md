# GTAVXR Suite development sources

These sources include the Suite UI and the synthetic installer tests.
Build into a new directory on Windows with the .NET Framework compiler:

```powershell
powershell -NoProfile -File tools/build.ps1 -Output out
```

The fixture build runs `--bundle-fixtures`, `--render-all` and `--version`.
The fixture generator is `tools/bundle_fixtures.py` and uses Python 3.14
with a compatible x64 Zstandard library. The library's licence is included.
The fixture provider rejects retail archives and the build rejects embedded
release bundles. A release build requires a compatible content provider.
