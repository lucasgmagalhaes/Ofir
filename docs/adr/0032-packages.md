# 0032. Packages: dependencies in `ofir.json`, `ofir.lock`, one version each

- Status: Accepted
- Date: 2026-11-07

## Context

A project can only use its own modules and std. Roadmap 6.5 adds
dependencies on other projects. Several things need deciding: where they
are declared, how a module names a dependency's modules, how versions are
chosen and pinned, and where the code comes from (a directory, a git
repository, a registry). Other ecosystems decided these differently:

- **Cargo**: a manifest table, semver requirements, a lock file, several
  major versions of one crate side by side, a central registry.
- **Go modules**: import paths that are URLs, minimal version selection,
  `go.sum` checksums, a proxy instead of a registry.
- **npm**: nested copies of different versions, a lock file.

Ofir builds a whole program from source and monomorphizes it (ADR 0007),
so two copies of a package mean two copies of its types, which do not mix.

## Decision

**Declaring.** The `dependencies` object of `ofir.json` (ADR 0026) maps a
package name to where it comes from:

```jsonc
"dependencies": {
  "json": "^1.2",                                       // the registry
  "http": {"git": "https://example.com/http.git", "version": "^0.3"},
  "util": {"path": "../util"},                          // a directory
},
```

A package is a project: a directory with an `ofir.json` whose `build.kind`
is `"lib"`. Its name is the key, which must be a valid module name.

**Importing.** `import json.parser` is the module `parser` of the package
`json`: a dependency's name is the first segment of an import, as `std` is.
Its modules are found from the directory of its `build.root` file
(`src/index.of` makes `import json` that file and `import json.parser`
`src/parser.of`). A module can import the packages its own project
declares, not those of its dependencies. A dependency named like a
top-level module of the project, or `std`, is an error (E0915).

**Versions.** Versions are semver (`MAJOR.MINOR.PATCH`). A requirement is
`^1.2.3` (the default for a bare `1.2.3`: compatible, the same leftmost
non-zero part), `~1.2.3` (the same minor), `=1.2.3`, `>=1.2.3` or `*`. A
git dependency's versions are its tags `vX.Y.Z`; `rev` (a commit) or
`branch` pin one instead of `version`.

A program has **one version of each package**: the highest that every
requirement on it accepts, across the whole graph, recomputed until the
requirements of the chosen versions agree. If none fits, resolution fails
and names the requirements (E0912). There is no backtracking: a conflict
is resolved by the user, not by an older version picked silently.

**Locking.** `ofir fetch` resolves the graph and writes **`ofir.lock`** next
to `ofir.json`: generated JSON, sorted by name, with each package's
version, source (path, git URL and commit, or registry) and a checksum
(SHA-256 of its files, in a fixed order). A build uses the lock as it is
when it matches the manifest, and resolves again for only the entries
that changed. `ofir update [name]` resolves again on purpose. `--locked`
fails instead of changing the lock (E0913); `--offline` uses only the
cache. A fetched package whose checksum differs from the lock is an error
(E0914). Path dependencies are not checksummed: they are being edited.

**Fetching.** Git repositories and their checkouts are cached under
`$OFIR_HOME` (default `~/.ofir`): `cache/git/<hash of URL>` and
`cache/src/<name>-<version>`. Fetching runs `git`, which must be
installed.

**The registry** (implemented in roadmap 6.5e). An index is a git
repository with a file per package (`js/on/json`, by its first letters):
a JSON line per published version with its version, dependencies,
checksum and whether it was yanked. An archive of each version is
downloaded from a URL the index's `config.json` gives. A bare version
string means the default registry. Publishing, accounts and the hosted
service are out of scope.

## Consequences

- Imports stay short and read like std's; renaming a dependency is the
  key of its entry (`"json2": {"git": ..., }`) and changes no code in it.
- One version per package keeps types single and builds small, at the
  cost of conflicts the user must settle, which Go and Cargo avoid
  (by MVS, or by duplicate majors). A later ADR can allow two majors
  side by side under different keys, which already works by renaming.
- Builds are reproducible from `ofir.lock`, offline after one fetch.
- `std` is not a package: it comes with the compiler.
- The package manager is stage 1 only; stage 0 builds the compiler and
  its tests, which have no dependencies. The steps are roadmap 6.5a-6.5e;
  the codes E0910-E0915 are in docs/errors.
