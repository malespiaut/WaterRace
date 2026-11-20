# WaterRace → Linux/OpenGL port: plan

Plan for porting WaterRace (French Touch, 2001, Mac OS 9 / PowerPC) to Linux, following the
strategy of Pangea's [CroMagRally port](https://github.com/jorio/CroMagRally): an in-place
source port that uses [Pomme](https://github.com/jorio/Pomme) for the Mac APIs and OpenGL
for rendering. Pomme was checked at commit `8e2a367` (2026-10-27).

---

## 1. Strategy

Edit the original source where it lives. Don't build a parallel layer on top of it.

1. **Compile and link first.** Get the untouched game tree building against Pomme, with
   missing pieces stubbed. CroMagRally's first commits were "Make it compile", "Make it link",
   "Entry point".
2. **Keep the structure.** Same files, same names, same engine and game code. Replace only
   the platform layers: RAVE, QuickDraw, Dialog Manager, InputSprocket, Open Transport and
   the CD/screen code.
3. **Keep the data.** Use the original game data. Fix byte order where a loader reads it,
   the way CroMagRally calls `ByteswapInts` in `File.c`. Don't write new decoders.
4. **Convert only the images.** Like CroMagRally's `rawdata/` scripts, convert the QuickTime
   PICT textures to PNG once, with a prep script. The game then never has to read PICT.
5. **Use Pomme as the Mac layer.** Keep the Mac API names and let Pomme provide them (files,
   resources, memory, Sound Manager). Pomme's graphics, input, QuickDraw 3D and video stay
   off, as in CroMagRally's `CMakeLists.txt`.
6. **Track Pomme.** Keep it as a submodule and sync it regularly. Fix generic Pomme gaps
   upstream or in a fork.

The main differences from CroMagRally are the renderer and the data. CroMagRally used
QuickDraw 3D names with its own OpenGL code. WaterRace uses RAVE, a 3D API with its own
rasterization pipeline, and its MetaFile models are binary structs with pointer fields. Both
need more work than CroMagRally's port did.

---

## 2. Verdict

**Difficulty: moderate. Rendering is the largest single item; the data is manageable.**

| Area | Difficulty | Why |
|---|---|---|
| 3D rendering (RAVE → OpenGL) | **Medium** | The engine transforms, lights, clips and projects everything on the CPU. RAVE only rasterizes screen-space triangles. About 28 `QA*` entry points are used, in 17 files. Replacing them in place, behind the same call sites, keeps the engine code. |
| Asset loading (MetaFile) | **Medium** | Each tag's payload is read straight into a native struct. The loaders must read the on-disk layout and copy fields with byte swaps, in place. |
| Textures (PICT) | **Low with prep** | About 60% of textures are QuickTime-JPEG PICTs (opcode `0x8200`), which Pomme can't decode. Converting them to PNG once avoids a runtime decoder. |
| 2D interface (QuickDraw menus) | **Medium–high** | Menus are built from `CopyBits` ×88, `DrawPicture` ×67 and `SetGWorld` ×126, and some code writes 16-bit pixels directly into memory. |
| Dialogs and screen management | **Medium** | Dialog Manager, Display Manager and Navigation Services have no Pomme equivalent. Replace them with in-game screens, as CroMagRally does in `Source/Screens/`. |
| Input (InputSprocket) | Low | One wrapper file, `ISpRoutines.cp`, over SDL. |
| Audio (Sound Manager + QuickTime music) | Low–medium | Pomme's Sound Manager covers most commands used. The QuickTime music players are replaced by MP3 playback of `Music/*.mp3`. |
| Network (Open Transport TCP) | Medium, **optional** | About 70 OT call sites in 4 files, raw-struct packets. Do it after single-player works. |

Effort is a rough guess for one engineer who knows Pomme: about **8–12 weeks to playable
single-player**, then 2–3 weeks for networking and 1–2 weeks for polish. The biggest
unknown is the number of distinct on-disk structs (§5.2).

---

## 3. What the repo contains

```
Source/                       ~58k lines C++ (CodeWarrior, Mac OS 9, PPC), LF endings, ASCII
  Common code/      7.7k      screen/gamma, dialogs, audio players, Nav Services, CD protection
  Infinity Engine/ 20.7k      French Touch "Infinity" 3D engine (RAVE), MetaFile I/O, post-FX
  WaterRace final/ 29.8k      game: physics, AI, menus, HUD, network, prefs, replays
  *.r                          Rez sources (12×STR#, plus isap/setl/tset)
  *.rsrc_fork                  raw resource forks (build resources; see §5.4)
  *.µ                          CodeWarrior projects (binary, reference only)
WaterRace Data/
  Characters/ Ships/ Locations/ Core/   MetaFile assets (data fork, big-endian)
  Interface.rsrc_fork          92 PICT, Btns, FtDf, Camr, clut … (menu artwork)
  Sounds.rsrc_fork             22 'snd '
Music/*.mp3                    soundtrack (CC BY-NC-SA 4.0, unlike the GPLv2 code)
Builds/*.sit                   original binaries + tools (reference under emulation)
Add-Ons/*.sit                  extra tracks/ships (StuffIt; later goal)
tools/                         data-preparation and checking scripts (see §5.4)
```

Notes:
- Two folder names start with a space (` Infinity Project`, ` Conditionals Headers`). Quote them in CMake or rename them.
- The original CD holds WaterRace **1.0.1** with the same game data. Every file in `WaterRace Data/` on the disc is byte-identical to the repo's copy, including the three resource forks. The repo's `Builds/` has 1.0.2 and 1.0.3. The repo's source matches none of the released builds exactly.
- The CD's data is already in AppleDouble form. `tools/hfs2adf.py IMAGE OUTDIR` extracts it from the `(Track 01).bin` (or a 2048-byte ISO), writes each file's data fork and an AppleDouble `NAME.rsrc`, and writes `_hfs_manifest.tsv` with Finder type, creator and dates. `tools/test_hfs2adf.py [OUTDIR]` checks the output.
- Pomme reads resource forks only from AppleDouble `X.rsrc` files (magic `0x00051607`, see `HostVolume.cpp`). The repo's raw `X.rsrc_fork` files need wrapping before Pomme can use them (see §5.4).
- Build configuration is set in `Source/WaterRace final/ Conditionals Headers/Conditionals*.h` (language, demo, VR, network-client, movie and similar variants). Start with the **retail English** configuration.

---

## 4. Rendering: RAVE on OpenGL 1.4

### 4.1 Target version

**OpenGL 1.4 compatibility, fixed-function, no shaders.** CroMagRally also uses fixed-function
GL (`glBegin`/`glEnd`, `glFogf`, `glAlphaFunc`, `glLightfv`, `glMatrixMode`).

- 1.4 is the lowest core version with both secondary colour (RAVE's `Highlight` texture op,
  used on most lit textured shapes) and a fog coordinate (needed because the vertices arrive
  already projected).
- GL 1.2 with `EXT_secondary_color` and `EXT_fog_coord` works too. Check at startup.
- Shaders would only help if transform and lighting moved to the GPU, which means rewriting
  the engine. Out of scope.

### 4.2 RAVE features used, and their GL equivalents

From `Shaders/*.cp`, `Projection Utils.h`, `Infinity.cp` and `Post FX/*.cp`:

| RAVE feature used | OpenGL equivalent | Min GL |
|---|---|---|
| Screen-space vertices `x,y` (pixels), `z` ∈ [0,1], `invW`, perspective-correct `uOverW/vOverW` | `glVertex4f(x·w, y·w, z·w, w)` with `w = 1/invW`, plus a fixed projection (§4.3). GL interpolates texcoords and colours perspective-correctly. | 1.0 |
| Indexed tri meshes (`QASubmitVertices*` + `QADrawTriMesh*`), `QADrawTriGouraud`, `QADrawLine` | Vertex arrays + `glDrawElements(GL_TRIANGLES / GL_LINES)` | 1.1 |
| Texture formats `RGB16`, `ARGB16` (1-bit alpha), `ARGB32` | Everything as `GL_RGBA8`; `GL_CLAMP_TO_EDGE` | 1.2 |
| `kQATextureOp_None / Modulate` | `GL_TEXTURE_ENV_MODE` = `GL_REPLACE` / `GL_MODULATE` (primary colour = `kd_r,kd_g,kd_b,a`) | 1.0 |
| `kQATextureOp_Highlight` (adds specular `ks_r,ks_g,ks_b` after modulation) | `GL_COLOR_SUM` + `glSecondaryColor3f(ks)` | **1.4** (or `EXT_secondary_color`) |
| Fog linear / exp / exp² over **eye depth** | `GL_FOG` with `GL_FOG_COORD_SRC = GL_FOG_COORD`, `glFogCoordf(w)` | **1.4** (or `EXT_fog_coord`) |
| `kQABlend_Interpolate` / `kQABlend_PreMultiply` | `glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)` / `(ONE, ONE_MINUS_SRC_ALPHA)` | 1.0 |
| `kQAZFunction_LT / None / True`, `kQAContext_NoZBuffer` | `glDepthFunc(GL_LESS)`, depth test off, `GL_ALWAYS` | 1.0 |
| `kQATextureFilter_Fast/Mid/Best`, `kQATexture_Mipmap` | `NEAREST` / `LINEAR` / `LINEAR_MIPMAP_LINEAR`; mips built on the CPU or with `GL_GENERATE_MIPMAP` | 1.0–1.4 |
| `QADrawBitmap` (menu background behind 3D previews) | Textured screen-space quad | 1.1 |
| `kQAContext_Scale`, `BitmapScale` (low-res and 2× AA modes) | **Dropped**. Render at native resolution; add MSAA through SDL if wanted. | – |

Uses, most first: `Infinity rendering.cp` (65 `QA*` calls), `Textures.cp` (42),
`Infinity rendering Texture.cp` (37), `… Gouraud.cp` (28), `WaterRace final/Infinity.cp` (26),
`… Terrain.cp`, `… Sprite.cp`, `… Line.cp`, `Infinity Player.cp`, `… Particle.cp`,
`Post FX LensFlare.cp`, `WaterRace final/Utils.cp`, `Post FX Text.cp`, `… WireFrame.cp`,
`… Light.cp`, `Post FX Moon.cp`, `… Lens.cp`.

Tags used: `ZFunction, TextureOp, Blend, Texture, FogMode/Color/Start/End/Density/MaxDepth,
ColorBG, BitmapScale, TextureFilter, PerspectiveZ, Antialias`.

### 4.3 Where it goes

The RAVE calls are replaced **in place**, with a GL implementation behind the same function
names, the way CroMagRally's `Source/3D/OGL_Support.c` stands in for its QuickDraw 3D code.

- Add one GL implementation file to `Source/Infinity Engine/` (for example `Source/Infinity Engine/Core/OGL_Support.cp`), providing the RAVE types, tags and calls the engine uses. The engine files change only where a RAVE call needs a different argument.
- Apple's `RAVE.h` is not in the repo and must not be copied. Declare only the types used: `TQAVGouraud`, `TQAVTexture`, `TQAIndexedTriangle`, `TQADrawContext`, `TQATexture`, `TQABitmap`, and the tags and enums above.
- `QASubmitVertices*` copies into a client array. `QADrawTriMesh*` turns `vertices[3]` into an index list and calls `glDrawElements`. Ignore `triangleFlags`; the engine culls backfaces itself.
- `QARenderStart` runs the context's *ImageBufferInitialize* notice method: colour clear (`ColorBG`), no clear (`Buffer_InitializeMethod_None`), or copy a background pixmap (`Buffer_InitializeMethod_BufferCopy`, `Utils.cp:185`). The copy case becomes "draw this sub-rect of the 2D interface texture" or "clear depth only".
- `QARenderEnd` and `QAFlush` do nothing per context. Presenting happens once per frame (§6.3).
- Engine enumeration (`QADeviceGetFirstEngine`, `QAEngineGestalt`, `Check_ATIDrivers`, `Check_3DfxDrivers`, `Check_Rave_1_6`, `IsAppleSoftwareEngine`) returns one OpenGL engine that reports every optional feature. The 3Dfx 256² limit and VRAM queries go away.
- `QATextureNew` receives decoded RGBA8 pixels and their mip chain, uploads them and returns a GL name wrapped in `TQATexture`.

### 4.4 Projection for pre-projected vertices

Vertices are submitted as `(X, Y, Z, W) = (x·w, y·w, z·w, w)`, where `x,y` are pixels in the
draw context (y down) and `z` ∈ [0,1]. Load this once as `GL_PROJECTION`, with
`GL_MODELVIEW` at identity:

```
| 2/Wpx   0       0   -1 |
| 0      -2/Hpx   0   +1 |
| 0       0       2   -1 |
| 0       0       0    1 |
```

Each draw context maps to a `glViewport` + `glScissor` rectangle. `w > 0` holds because the
engine clips against the near plane in software (`Clipping Texture.cp`, `Clipping Gouraud.cp`).

---

## 5. Data and platform layers

### 5.1 Game data stays

- The models, terrain, skeletons, sounds and scripts stay in their MetaFile form in `WaterRace Data/`.
- Loaders keep their structure and read the on-disk bytes as they do now. The byte-order and layout fixes go at the read sites (§5.2).
- Resource-fork content for the app (`STR#`, `DLOG`, `TEXT`, `MENU`, and so on) is read through Pomme's resource manager, unchanged.

### 5.2 MetaFile loading (**the main data risk**)

Asset files are MetaFiles: a big-endian header (`version 0x0104`, creator `MFEd`/`TrBd`/`MdBd`),
then a tag table of 24-byte entries `{depth, flags, version, tag, subTag, ID, dataSize,
dataPosition}`, packed to 2 bytes. A throwaway parser confirmed this layout on every file.
Each tag's payload is read with `FSRead` **directly into a native struct**:

```c
*shape = (ShapePtr) NewPtr(sizeof(Shape));
FSRead(fileRefNum, &tagEntry->dataSize, *shape);   // MetaFile Read.cp:471-477
```

That breaks on x86-64 Linux in four ways:

1. **Endianness.** Floats, longs, shorts and OSTypes need byte swapping.
2. **Pointer and `long` sizes.** `Shape` holds `pointList`, `triangleList`, `renderingProc`, `normalList`, `dataPtr`, `texturePtr` and `parent`. On LP64 `long` is 8 bytes, so in-memory layouts no longer match the 32-bit on-disk ones.
3. **Alignment.** Some structs are `mac68k` or `pack(2)`; the padding differs on x86-64.
4. **Bitfields.** `Shape::backfaceCulling : 1` and the `State` flags follow big-endian bitfield order.

**Strategy, in place:** each existing read site reads the payload into a buffer of the on-disk
size, then copies fields into the native struct, swapping bytes as it goes. The copy stays in
`MetaFile Read.cp` beside the `FSRead`, the same pattern as CroMagRally's `ByteswapInts` calls in
`File.c`. Add a size check at each site, so a mismatched struct fails loudly. Don't add a
separate decoder file or layer.

On-disk sizes seen in the data:

| Tag | Struct | On-disk size |
|---|---|---|
| `shap` | `Shape` | 300 |
| `cptx` | `CompressedTexture` header (+ trailing alpha plane) | 28 + w·h |
| `txst` | `TextureSet` | 1680 |
| `scrp` / `anim` | `Script` / `Animation` | 192 / var. |
| `cmra` | `Camera` | 136 |
| `mega` | `MegaObject` | 3004 / 3932 (two versions) |
| `terr` | `Terrain` | 5636 |
| `tmsh` / `smsh` | `TerrainMesh` / `SeaMesh` | 12792 / 12796 |
| `pnts`/`trig`/`nrls`/`ligt` | `Vertex[]` (24 B) / `TriFace[]` (12 B) / `Vector[]` (12 B) / Lightning[] | arrays |
| `shdt` | `ShapeData_{Sprite,Light,Particle,Lens,Audio}`, chosen by `shape->shading` | var. |
| `data` subtags | `shds`, `shpp`, `chds`, `trds`, `lcpy`, `imag`, `snd `, `vers`, read in `Data Files.cp` | var. |

Other notes:
- `MetaFile Write.cp` is for the editor tools. Leave it out of the game build.
- **Preferences, pilot and replay files** (`Preferences.cp:398`, `Pilots.cp:158`, `Replay.cp:144`) write raw structs with `FSWrite`. Give them an explicit field-by-field format in the preferences directory, as CroMagRally did with its generic user-file routines. Old Mac saves can be ignored.
- `(unsigned long)` pointer stores (`Infinity rendering.cp:694,697`) work on LP64 Linux, since `unsigned long` is 64-bit there. Note them and move on.

### 5.3 Textures and images: convert once

Measured across all assets (710 texture images):

| Encoding | Count | Pomme support |
|---|---|---|
| PICT `0x8200` CompressedQuickTime, codec **`jpeg`** | ~418 | ❌ |
| PICT `0x009A` DirectBitsRect (packType 4 / 3) | ~292 | ✅ |
| alphaDepth 0 / 1-bit / 8-bit (separate plane after the header) | 476 / 110 / 124 | n/a |

`Interface.rsrc_fork` PICTs are almost all `0x9A` (81), plus a few others with one JPEG.

**Strategy:** do what CroMagRally did with its textures. A prep script under `tools/`, like
CroMagRally's `rawdata/` scripts, converts every texture PICT to PNG once, merges the alpha plane
into RGBA, and writes the PNGs into `WaterRace Data/`. The game loads them with `stb_image`
(already in CroMagRally's `Source/Headers/`). The runtime never has to decode a QuickTime JPEG.

- Pomme's `DrawPicture` can't scale and only draws pictures it decoded itself. The 2D interface
  artwork (`Interface.rsrc_fork`) is converted the same way, so `GetPicture`/`DrawPicture` calls
  become texture draws.
- `NewGWorld` ignores depth, so every offscreen buffer is ARGB32. Code that writes 16-bit pixels
  moves to 32-bit (§6.5).
- Rewrite `Texture_NewFromCompressedTexture` and `Texture_Decompress` to load the PNG and call
  `QATextureNew`. Drop the 16-bit GWorld path. Keep the "reduce" options only if they stay useful
  as a quality setting.
- If the prep script must read PICT on Linux, the QuickTime JPEG handler can go in Pomme's
  `PICT.cpp` instead. This is simpler, but it puts more code into Pomme.

### 5.4 Resources and build resources

- The repo's `WaterRace Data/` resource forks (`Interface`, `Sounds`, `Core/Logo Intro`) are
  byte-identical to the CD's. The CD copies are AppleDouble, which Pomme reads directly.
- `Source/**/*.rsrc_fork` are developer build resources. They may not match 1.0.1. Keep
  them as they are for the build, and compare them with the CD before replacing any.
- To replace a raw fork with an AppleDouble `X.rsrc`, the data-fork placeholder `X.rsrc` (a
  0-byte file in `Source/`) has to be renamed first. A plain conversion would overwrite it.
  `hfs2adf.py` refuses this case rather than overwrite.
- The English app's compiled `STR#` resources are in its CD fork, so Pomme can read them.
  Reading them through Pomme is expected to work but is untested.
- `rez_strings.py` (planned) is only needed to rebuild resources from the `.r` sources.

---

## 6. Platform layers, in place

### 6.1 Startup

Replace `main.cp`'s Mac boot (`Init_Toolbox`, `Screen_Init`, CD checks, the Apple Event
handler, and so on) with a `Boot.cpp`-style entry point. That is the path CroMagRally took
in "Refresh Boot.cpp". The CD-protection code (`CD Protection.h`) is removed; the game
doesn't need the disc at runtime.

### 6.2 Dialogs and screens

`GetNewDialog`/`ModalDialog`/`SetDialogItem*` in `Preferences.cp` (largest),
`Network menu.cp`, `Dialog Boxes.cp`, `Agreement.cp` (licence), `ScreenLib.cp` (display and
refresh pickers), `Error system.cp` (`StandardAlert`), `Navigation Services.cp`, and the
Apple Event `odoc` handler in `main.cp`.

**Strategy:** draw each screen in-game, in the style of CroMagRally's `Source/Screens/`, over
the same 640×480 interface. Errors go to `SDL_ShowSimpleMessageBox`. The licence is shown once as
text. Drop the display pickers. Replace Nav Services with a fixed replay folder. Add Dear ImGui
only if the preferences screen becomes too awkward to draw by hand.

### 6.3 Frame and presentation

A frame can show both 2D interface output and several RAVE contexts, for example the Browser menu
with a rotating ship preview. Each frame:

1. Upload the dirty region of Pomme's 640×480 ARGB32 window port to a GL texture (`IsPortDamaged` and `GetPortDamageRegion` exist in Pomme for this).
2. Draw it as a letterboxed quad scaled to the window, with the 2D projection.
3. Render every RAVE context drawn this frame into its own viewport and scissor, mapped from interface coordinates to window pixels. During a race the game context fills the window.
4. Draw the gamma-fade overlay, then `SDL_GL_SwapWindow`.

Gamma fades (`Fade1ToBlack`, `Fade1ToColor`) become a full-screen colour quad with animated alpha.
`InfinityRendering_MaxFPS` busy-waits on `Microseconds`; replace it with vsync and `SDL_DelayNS`.

Widescreen: `pixelConversion = (-viewWidth/2)/zoom` fixes the *horizontal* FOV, so a 16:9 window
shows less vertically. For a better default, scale `zoom` by `(4/3)/aspect` in
`InfinityRendering_UpdateStateParams`, which keeps the 4:3 vertical FOV.

### 6.4 Input

`ISpRoutines.cp` (InputSprocket needs, elements and events) plus `GetKeys`/`Button`/`GetMouse`.
Pomme already maps `GetKeys`/`Button`/`GetMouse` onto SDL. Rewrite `ISpRoutines.cp` against SDL3
keyboard and `SDL_Gamepad`, keeping its public functions (`InputSprocket_GetShipInput`, …).

### 6.5 2D code that assumes 16-bit pixels

`Text Engine.cp` (custom text rasterizer, `unsigned short color`), `WaterRace final/Utils.cp`,
`Texture Utils.cp`, `Infinity rendering.cp` (buffer copy), `Infinity Player.cp` and
`Screen Capture.cp` write pixels directly. Move them to 32-bit.

### 6.6 Audio

- Sound Manager commands used: `bufferCmd soundCmd callBackCmd volumeCmd freqCmd quietCmd nullCmd`. Pomme supports all but `callBackCmd`, which only the streaming players use.
- `Infinity Audio Engine.cp` (positional ambient and engine sounds through channel volume and rate) runs on Pomme's mixer with small changes. Sample data in `absd` and `data/'snd '` is big-endian PCM inside `ExtSoundHeader`, and gets swapped at the read site (§5.2).
- Music: `QDesign Player.cp` (the one `Game.cp` and `Champion.cp` use), `MP3 Player.cp` and `Audio CD.cp` are replaced by one player that plays `Music/*.mp3`. Keep the `QDPlayer_*` names as thin wrappers so call sites stay unchanged.
- `Sounds` (22 `snd `) loads through Pomme's resource manager from its AppleDouble file.

### 6.7 Networking (later phase)

Open Transport TCP endpoints with notifiers, in `Network Engine*.cp` and `Network menu.cp`.
Port to non-blocking POSIX sockets polled once per frame. Packets are raw structs, so serialize them
explicitly in network byte order. Compatibility with original Mac clients is not a goal.

### 6.8 Compile issues

- `<fp.h>` (36 files) → `<math.h>`. `<MacHeadersPPC++>`, `Traps.h`, `Devices.h`, `Video.h`, `DriverGestalt.h`, `QD3DAcceleration.h` and `InstrumentationMacros.h` get stub headers or are removed.
- `__frsqrte` (`FastSqrt`) is provided by Pomme.
- CodeWarrior idioms: `void main()`, unnamed `typedef struct X {…};`, multi-character OSType literals (`-Wno-multichar`; the values match when read as big-endian u32), `pascal` callbacks, and `UniversalProcPtr` checks against `kUnresolvedCFragSymbolAddress`.
- Remove: CD protection, the demo, expo, VR and movie build variants, the `Pub*.rsrc` ads, and the spinning cursor. Keep `__PREVENT_FILE_HACKING__` and the pilot checksums, recomputed over the new format.

---

## 7. Phased plan

Each phase ends with something runnable. The order follows CroMagRally's history.

**P0: Compile and link (1 wk).** Pomme submodule, SDL3, CMake. Stub headers until the retail
English build compiles and links, with unported functions calling `TODOFATAL`. Exclude
editor-only files (`MetaFile Write.cp`, the model chooser in `MetaFile Utils.cp`, the new-texture
dialog in `Texture Utils.cp`) after confirming the game doesn't use them.
*Done when:* the binary links and reaches the entry point.

**P1: Entry point and data (0.5 wk).** `Boot.cpp`-style startup (§6.1). Point the FSSpec root at
`WaterRace Data/`, from the CD extraction or the repo. Run `tools/test_hfs2adf.py` on the
extracted tree. *Done when:* `GetIndString` and `Get1Resource` work through Pomme.

*P1 status (branch `p1-entry-data`):* done. `Source/WaterRace final/Boot.cp` starts Pomme, opens
`WaterRace 1.0.1 [English]` and checks `STR# 129` (`WaterRace Data`) and `'STR#' 200` through
`Get1Resource`. `main()` returns after that check; the Mac boot below it is unreachable until P3.
`port/stage-data.py OUTDIR --app-rsrc CD_COPY` builds the game root: the repo's `*.rsrc_fork`
files wrapped as AppleDouble, plus the English app fork from the 1.0.1 CD. Run the binary from
that root. The root is the folder that holds `WaterRace Data/`, not the Data folder itself,
because the game finds `WaterRace Data` relative to the app's folder. The `tools/` scripts named
above are not in the repo; `port/stage-data.py` replaces the extraction step for the repo copy.
Also in P1: all 405 Pascal literals (`"\pName"`) now carry an explicit length byte (octal
escape), and Pascal names copied into `FSSpec.cName` go through `CopyPascalToCName`. The
`link-stubs` generator no longer stubs libc and libm symbols (`memcpy`, `sin`, ...), which P0
had wrongly stubbed. Left for later: `FSSpec.name` in `#if`-disabled music code (P6),
`ioNamePtr` in Pilots and Data Files (P4, with the PB calls), and `ParamText` with `cName` (P3).

**P2: Textures (1 wk).** The prep script converts PICTs to PNG (§5.3). Load them with `stb_image`.
*Done when:* every converted texture is checked by eye against the original.

**P3: Logos and menus (2 wks).** First pixels. The GL implementation (§4.3) comes up, the 2D
compositor (§6.3) draws the interface, and the title, logo and character-select screens run.
Bring-up order for RAVE: Gouraud meshes → textured with modulate → highlight → fog → sprites,
particles and lens flare → lines and wireframe → sky. Reference scenes: `Core/Logo Intro`, then a
`Core/Practice Zone` fly-through. *Done when:* the logo intro and a terrain render match reference
screenshots from the original game.

**P4: MetaFile loading (2–3 wks).** Fix the read sites type by type (§5.2): tag table, then
Shape, Vertex, TriFace and normals; TextureSet and CompressedTexture; MegaObject, Script,
Animation, Skeleton and Camera; Terrain, TerrainMesh, SeaMesh, Track, Enclosure, Items and
AmbientSound; then the `data` subtags. *Done when:* every file in `WaterRace Data/` loads with
size checks passing, and its counts, bounding boxes and texture sizes match a dump from `metadump.py` (planned, not yet written).

**P5: Race (2 wks).** Terrain and sprites on the GL layer, sound on Pomme's mixer (§6.6), the HUD,
32-bit fixes (§6.5), input (§6.4). Remove the screen-switching code. *Done when:* a Practice race
starts, renders, plays sound and takes keyboard and gamepad input.

**P6: Menus and saves (1.5 wks).** In-game screens replace the dialogs (§6.2). Preferences,
pilot and replay files get explicit formats (§5.2). Music plays from `Music/*.mp3`.
*Done when:* a tournament can be played, saved, quit and resumed, and replays record and play back.

**P7: Networking (2–3 wks, optional).** Sockets backend, explicit packet serialization, LAN
host and join.

**P8: Polish and release (1–2 wks).** Widescreen FOV, vsync and frame pacing, optional MSAA,
`.desktop` file and AppImage or Flatpak, licence notices (GPLv2 code, CC BY-NC-SA music, Pomme's
licence), README. Add-Ons (`.sit`) can come later, if they use the same MetaFile format.

---

## 8. Verification

- **Reference:** run the original `Builds/WaterRace_1.0.3.sit` under SheepShaver or QEMU-PPC with Mac OS 9 (it has a RAVE software renderer) to capture reference screenshots of each track and menu.
- **Data check:** a headless mode that loads every file in `WaterRace Data/` and reports the counts, bounding boxes and texture sizes, compared with a Python dump of the same files (the `metadump.py` idea, in `tools/`).
- **Render checks:** fixed-camera screenshots of each `Locations/*` file through `glReadPixels`, compared by eye with the reference. Exact pixel matches aren't expected, since RAVE's software rasterizer differs from GL.
- **Determinism:** physics and AI use `RClock` time steps. A recorded replay should finish with the same ranking every run; use this as a regression test once replays work (P6).
- Build with `-fsanitize=address,undefined` during P3–P5. Mis-sized reads show up as out-of-bounds accesses.
- `tools/test_hfs2adf.py` checks the disc extraction.

---

## 9. Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| On-disk struct layouts misread (alignment, bitfields, `mega` 3004 vs 3932 versions) | Corrupt geometry or crashes | Size check at each read site; sanitizer builds; handle both `kVersion_MegaObject` layouts explicitly |
| QuickTime JPEG variants in PICT (unusual headers, `qdgx` seen once) | Missing textures | Log them in the prep script and fix the outliers by hand |
| RAVE fog units vs. GL fog coordinate | Wrong fog density | Calibrate against reference screenshots; fog start and end come from `Terrain` |
| Mixed 2D and 3D screens need drawing order beyond immediate GL | Previews flicker or get overdrawn | Queue the contexts and replay them in the compositor (§6.3) |
| Pomme QuickDraw gaps (CopyBits modes, scaled DrawPicture, text) | Menu glitches | Fix them in a Pomme fork, and upstream the generic parts |
| 64-bit `long` assumptions outside I/O (`sizeof` arithmetic, such as `5 * sizeof(CameraState)` in `MetaFile_Read_CameraLayout`) | Subtle bugs | Grep for `sizeof(` in I/O and network code; use fixed-width types at boundaries. Don't build as 32-bit; that hides the problem. |
| Prep-script alpha and colour differ from the PICT original | Visible texture changes | Compare each converted texture with the original in the reference build |
| Music licence (NC) differs from code (GPL) | Distribution constraints | Ship the music as a separate data package with its own licence notice |
