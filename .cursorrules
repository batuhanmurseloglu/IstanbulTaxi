# İstanbul Taksi Simülatörü — Permanent Agent & Workspace Rules

## 1. Mandatory Automatic Git Sync After Every Task
Whenever you complete **any** coding task, bugfix, config update, or feature milestone in this workspace, you **MUST AUTOMATICALLY** perform the following Git workflow before ending your turn (without waiting for the user to ask):

1. **Stage Changes**:
   Run `git add -A` in `c:\Users\batuh\Desktop\IstanbulTaxi`.
2. **Verify Clean Index & Security**:
   - Run `git status --short` and `git diff --cached --name-only`.
   - Ensure **no** ignored build/cache folders (`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`, `*.sln`) or sensitive files (`.env*`, `Config/*Crypto*.ini`, API keys, tokens, credentials) are staged.
   - If any unwanted or sensitive file is staged, immediately unstage it (`git reset HEAD <file>`) and update `.gitignore` if needed.
3. **Commit with a Descriptive Message**:
   Create a clean, concise commit message summarizing the completed change (e.g., `git commit -m "feat(weather): implement UWeatherSubsystem and wet grip integration"`).
4. **Push to GitHub**:
   Run `git push origin main` so `https://github.com/batuhanmurseloglu/IstanbulTaxi.git` is always up to date.

---

## 2. Core Engineering Principles (UE 5.4 C++)
- **The city is the main character**: Prioritize Istanbul atmosphere, urban density, and authenticity.
- **Zero-broken-builds**: Compile changes against `IstanbulTaxiEditor Win64 Development` (`D:\UE_5.4\Engine\Build\BatchFiles\Build.bat`) before committing and pushing C++ modifications.
- **Subsystem & Data-Driven Architecture**: Use `UWorldSubsystem` / `UGameInstanceSubsystem`, GameplayTags (`IstanbulTaxiTags`), and DataAssets/DataTables with zero-asset C++ runtime fallbacks so the game always runs out of the box.
- **Financial Precision**: Store all Turkish Lira amounts in integer **kuruş** (`int32`, `1 TL = 100 kuruş`).
