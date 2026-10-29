# Move the work to a new Windows PC

This document says what to install, clone and copy to continue the work of
hexdom on a second Windows PC. On the new PC, the repositories are in
`D:\src`, not in `C:\src`. The wall hook in .claude/settings.json finds
wall.js through CLAUDE_PROJECT_DIR, so it works on each drive.

## 1. What git carries
All the commits of all the branches are on GitHub. Clone these two
repositories into `D:\src`:

| Folder | Remote | Use |
|---|---|---|
| `D:\src\hexdom` | `git@github.com:yksi7417/sanguodream.git` | the engine and the games |
| `D:\src\san11-analysis` | `git@github.com:yksi7417/sango-scratch-pad.git` | the analysis. Clone it only for the work on the original game |

The submodules of hexdom (spec-src, imgui, imgui_test_engine) come with
`--recurse-submodules`. The worktrees `C:\src\hexdom-*` of the old PC are
not necessary. Their branches are on GitHub. Make a worktree again only for
a stage that is not complete.

The project settings of Claude are in git: `.claude/settings.json`, the
wall hook and the rules.

## 2. What you must copy by hand
| From the old PC | To the new PC | Size | Note |
|---|---|---|---|
| `C:\src\hexdom\art-src\` | `D:\src\hexdom\art-src\` | 63 MB | The source art of the bases, the models and the splash. You can also download it again from the shared Drive folder (docs/ASSETS.md) |
| `C:\Users\<you>\.claude\projects\C--src\memory\` | `C:\Users\<you>\.claude\projects\D--src\memory\` | 80 KB | The Claude memory of this work: 15 files. The name of the folder comes from the folder of the session. `D:\src` gives `D--src` |
| `C:\Users\<you>\.claude\scheduled-tasks\hexdom-drive-models\SKILL.md` | the same path | 4 KB | The prompt of the task that imports the 3D models from Drive. It names `C:\src\hexdom-models`. Change it to `D:\src\hexdom-models` |
| `C:\src\download\` | optional | 61 MB | The Meshy GLBs of the old downloads |

The memory files and the task prompt name `C:\src` in their text. After
the copy, change `C:\src` to `D:\src` in these files.

Copy these only for the work on the original game, in `D:\src\san11-analysis`:

| From the old PC | Size | Note |
|---|---|---|
| `raw\` | 5.8 GB | The Ghidra project, the dumps, the captures of the play sessions and the briefs |
| `fixtures-bin\` | 45 MB | The frozen copy of the game files |
| `captures\` | 947 MB | The video captures |
| the Steam game San11WPK and `Documents\KOEI\35th\San11WPK_TC\SaveData` | | The analysis drives the real game |

The file `.mcp.json` of the analysis names `C:/src/san11-analysis/raw`.
Change it to `D:/src/san11-analysis/raw`.

Do not copy `build\`. build.bat and build_web.bat make it again.

## 3. What you must install
Install these tools in this order:
1. Git, the GitHub CLI and Node.js 22: `winget install Git.Git GitHub.cli OpenJS.NodeJS.LTS`.
2. Visual Studio 2022 Build Tools with the C++ workload and the Windows SDK
   10.0.26100. build.bat finds vcvars64.bat at the default path on C:.
3. Python 3.13, and numpy for misc/make_world_map.py: `pip install numpy`.
4. The venv of the site, in `C:\tools\hexdom-venv` or in `D:\tools\hexdom-venv`:
   `python -m venv D:\tools\hexdom-venv`, then
   `D:\tools\hexdom-venv\Scripts\pip install aiohttp boto3 PyJWT cryptography fonttools`.
5. The Emscripten SDK 6.0.13, in `C:\tools\emsdk` or in `D:\tools\emsdk`.
   It needs approximately 2 GB.
   `git clone https://github.com/emscripten-core/emsdk D:\tools\emsdk`, then
   `emsdk install 6.0.13` and `emsdk activate 6.0.13`.
6. ffmpeg and flyctl: `winget install Gyan.FFmpeg Fly-io.flyctl`.
7. The Claude desktop app. Then install the ECC plugin from the marketplace
   `https://github.com/affaan-m/ECC.git`. The GateGuard hook and the ECC
   agents come from that plugin.
8. For the analysis only: Ghidra 12.1.4 and the venv `ghidra-mcp-venv`.
   See the memory file san11-rebuild-project.md.

If you put the tools in `D:\tools`, set two user environment variables.
The scripts then find the tools there:

| Variable | Value | Used by |
|---|---|---|
| EMSDK | `D:\tools\emsdk` | build_web.bat, run_tests.bat |
| HEXDOM_PYTHON | `D:\tools\hexdom-venv\Scripts\python.exe` | run_tests.bat, tests/web/playwright.config.js |

## 4. The accounts and the keys
Do not copy the private keys of the old PC. Make new ones:
- Make an SSH key with `ssh-keygen -t ed25519`. Add the public key to
  GitHub with `gh ssh-key add`. Before that, sign in with `gh auth login`.
- Sign in to fly.io with `flyctl auth login`. The secrets of the site stay
  on fly.io (SESSION_SECRET, HOUSE_PASSWORD, the sign-in providers, the
  bucket and the two Discord values). You do not move them.
- Sign in to Google Drive in the Claude app, for the import of the models.

## 5. The first build
Do these steps in `D:\src\hexdom`:
1. `git clone --recurse-submodules git@github.com:yksi7417/sanguodream.git D:\src\hexdom`
2. `build.bat`, then `run_tests.bat`, then `run_tests.bat --demo-fast`.
3. `D:\tools\hexdom-venv\Scripts\python -I misc\subset_font.py`. It
   writes the font of the web build. Use the path of your venv.
4. `build_web.bat tests`, `run_tests.bat --web`, `build_web.bat referee`,
   then `run_tests.bat --server`.
5. In tests\web: `npm install`, then
   `node node_modules/@playwright/test/cli.js install chromium`.
   Then `build_web.bat test-game` and `npm test`. The suite takes
   approximately 25 minutes.

The result: each run prints "RESULT PASS" or "ALL PASS", and
`deploy_web.bat` can deploy.

To check the wall hook, start a Claude session in `D:\src\hexdom`. Ask it
to read `spec-src/raw/README.md`. The hook must stop the read with the
message "Clean-room wall".

## 6. One PC at a time
The two PCs push to the same master. Before you start on one PC, run
`git pull` there. Do not enable the Drive task on the two PCs at the same
time. The task is off on the old PC since 2026-10-29.
