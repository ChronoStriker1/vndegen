# Third-party code in RuntimePackages

- `CLzmaDec/LzmaDec.c`, `LzmaDec.h`, `7zTypes.h`, `Precomp.h` and `Compiler.h` are from the LZMA SDK 25.01 by Igor Pavlov (https://www.7-zip.org/sdk.html), unmodified. The LZMA SDK is in the public domain.
- `InnoSetup/InnoHeaders.swift`, `InnoSetup/InnoData.swift` and `InnoSetup/InnoSetupInstaller.swift` port the Inno Setup format handling of innoextract (https://github.com/dscharrer/innoextract) to Swift. innoextract is under the zlib license, reproduced below; each of those files keeps the notice too.
- The system's zlib, bzip2 and libarchive are linked, not vendored.

## innoextract

```
Copyright (C) 2011-2020 Daniel Scharrer <daniel@constexpr.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the author(s) be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```
