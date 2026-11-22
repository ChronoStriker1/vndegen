# SystemAoi runtime: where the knowledge came from

The SystemAoi runtime (`Packages/VNDegenKit/Sources/CSystemAoi`, hosts in `Engines/SystemAoi/host`) is being written
for VNDegen. It derives from no GPL or LGPL code. The game's exe and DLLs are run under Wine as a black box and not
disassembled; their strings were read for file names only.

| Part | Sources |
|---|---|
| Archives (`VF`, `VL`) | GARbro `ArcFormats/Aoi/ArcVFS.cs` (MIT): the header and the version 2 index. The `VL` part letter from the game's files |
| Script archives (`AOIMY01`) | GARbro `ArcFormats/Aoi/ArcBOX.cs` (MIT): the Unicode index and the offset-keyed XOR |
| Pictures (AGF) | GARbro `ArcFormats/Aoi/ImageAGF.cs` (MIT): the header and the five pixel operations |
| Voices (`AoiOgg`) | GARbro `ArcFormats/Aoi/AudioAOG.cs` (MIT): the wrapper's header |
| Bytecode | The game's 378 scripts: operand tags and their sizes worked out by tokenizing every script and checking that the jump targets fall on instruction starts (3,109 of 3,111 do). Opcode meanings from how the scripts use them |
| Tables (`obj.vfs`) | The game's files |
| 3D models and maps | The game's files: Pegasus3D models are text. The maps' open-side mask from the symmetry between neighbouring cells |
| The game's rules | The game's HTML manual (`html/page/*.htm`) and printed manual, then play of the original under Wine |
| Saves | Diffs of the files the original writes under Wine |
