# AGENTS.md

Working notes for agents editing this repository. Keep this file short and true; if a rule below stops matching
the code, correct it in the same commit.

## What this repository is
- A "Progressive Vanilla" continuation of MaNGOS One / Elysium (vMaNGOS) for World of Warcraft clients 1.5.1
  through 1.12.1. `README.md` carries the project guidelines and they are binding: accuracy first (any custom
  behaviour goes behind a config option, off by default), content in the database rather than in C++ (DB
  scripting, and expand the scripting when it is not enough), and full patch progression (data marked with the
  patch that added or changed it).
- On top of the upstream core there is local work on player bots. Read `src/game/PlayerBots/AGENTS.md` before
  touching that subsystem, and `src/game/PlayerBots/BotGearScoring.md` before touching how bots are equipped.

## Language and toolchain
- C++14 only: `CMakeLists.txt:23` sets `CMAKE_CXX_STANDARD 14`. No structured bindings, no `if` with an
  initializer, no `std::optional`, no designated initializers.
- `.clang-format` at the root is the style (4 space indent, braces of functions, classes and control statements
  on their own line). Format the lines you write; do not reformat a whole file that does not already match.
- Several tracked files are stored with CRLF line endings (`src/game/Chat/Chat.cpp`, `src/game/Chat/Channel.h`,
  `src/game/Objects/Unit.cpp`, others). Keep the line endings a file already has: adding an LF line to a CRLF
  file makes `git diff --check` report every added line as trailing whitespace.
- Naming follows the surrounding MaNGOS code: `UpperCamelCase` for types and functions, `m_` for members,
  `sPlayer`-style names for the static singletons. Comments explain why, in full English sentences.

## SQL
- Every sql change of this fork - schema or data - is a file in `sql/custom/repack/`, applied by hand. Nothing is
  added to `sql/migrations/`, which is upstream's: `migrations_list.h` is generated from that folder when CMake
  configures, and the world server refuses to start when a listed migration is missing. A file of the fork has no
  `add_migration` wrapper and does not touch the `migrations` table.
- Do not edit an upstream migration that has been pushed; the change is another file of `sql/custom/repack/`.
- A file of `sql/custom/repack/` must be re-runnable without damaging the database: `CREATE TABLE IF NOT EXISTS`
  for a table, the rows of one object deleted and written back as a whole, a flag set with `x = x | flag`. The
  order the files are applied in, and what each of them targets, is in `sql/custom/repack/README.md` - keep that
  file true when a new file depends on another.
- The content that this fork was started with stays where it is in `sql/custom/` (`sql/custom/README.md`); new
  content goes to the `repack/` directory under it.
- Prefer the existing patch aware table conventions (`*_add` / `*_remove` / patch columns) over encoding patch
  behaviour in C++.

## Git
- The only branch here is `development`, and it is also the default of the remote. Push there.
- Subject line: a short imperative sentence with a trailing period, e.g. "Dress the bots with gear rated for the
  talent that they have." The body then says what changed and why, in the same voice; ending the body with a
  short 中文 paragraph is normal in this repository.
- Do not commit build directories, database dumps, scratch files or agent notes that are not meant to be shared.

## What can be verified in this checkout
- This sandbox has no compiler, no MySQL server and no DBC data. The core cannot be built, the database cannot be
  queried and nothing can be run. Static checks only: `git diff --check`, reading the headers and the DBC
  definitions, grepping that an API exists and that its callers are updated.
- Say that plainly in the final message and leave the build and the in-game test to the maintainer. Do not
  describe a change as working because it looks correct; describe what was and was not verified.
