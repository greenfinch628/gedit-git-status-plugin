# gedit-git-status-plugin

Source repositories: [GitHub](https://github.com/greenfinch628/-gedit-git-status-plugin)
and [Gitea](ssh://git@scopuli:2224/scopuli/gedit-git-status-plugin.git).
The GitHub repository name includes a leading hyphen. The plugin's module and
installed filenames remain `git-status` on both hosts.

A standalone, read-only plugin for gedit 50 and gedit+ 50 on Fedora 44.
Adds Git badges beside names in the existing File Browser, including the
gedit+ Workspace view. It does not change gedit core or filenames.

Enable **File Browser** and **Git Status** in Preferences → Plugins. Disable
Git Status to immediately remove the badges. No separate panel is created.
Hover over the badge column to see the current branch.

## Badges

Git's two-character status preserves index and working-tree information:
` M` means modified on disk, `M ` staged modification, `A ` staged addition,
`??` untracked, `R ` renamed, and `UU` an unresolved conflict. `MM` means both
staged and unstaged modifications. Clean and ignored files have no badge.
A folder's `●` means changes somewhere below it, including collapsed folders.
Deleted files cannot appear if the File Browser no longer lists them, but
their parent folders are marked. Unsaved editor changes are not Git changes;
save the file first. Other side-panel plugins and tabs are not decorated.

## Build and install

Install the Fedora development packages: `gcc`, `pkgconf-pkg-config`,
`gedit-devel`, `gtk3-devel`, `libpeas-devel`, `libgedit-tepl-devel`, and `git`.

```sh
./build.sh
./install.sh
```

If using headers from a local gedit+ installation instead of gedit-devel:

```sh
GEDIT_INCLUDE_DIR=/path/to/install/include/gedit-50 ./build.sh
```

The plugin resolves gedit symbols from the running application, avoiding a
hard-coded stock or fork library path. The default per-user plugin directory
is `${XDG_DATA_HOME:-$HOME/.local/share}/gedit/plugins`; both installations
normally share it. Restart gedit after installing or replacing the binary.
An optional first argument to install/uninstall selects another plugin directory.

```sh
./uninstall.sh
```

## Implementation and scope

The File Browser's message bus provides the tree view and current folder.
The plugin adds its own column and validates the file model before reading
locations. It does not alter the browser's name/markup or rename renderer.
Disabling cancels pending queries, removes the column, and disconnects handlers.

Git runs in a background worker approximately every two seconds, with
`--no-optional-locks`, porcelain output and NUL-delimited filenames. No staging,
commits, checkouts or repository writes are performed. Missing Git, remote
locations and folders outside a repository show no badges. The current browser
folder must be inside a repository; browsing a parent containing separate repos
does not discover those repos. Nested repositories are not queried separately.
Large repositories may take longer; only one query runs per window at a time.
Detached HEAD is identified explicitly. This initial version targets the gedit
50 File Browser model; future incompatible models are skipped.

## License

GPL-2.0-or-later. See LICENSE.

## Validation

The initial build passed isolated virtual-display checks against the installed
Fedora gedit 50.0 and gedit+ 50.0: module loading, actual modified-file badge
rendering, removal on disable, re-enable, and cancellation during disable.
Git query tests cover staged additions, modifications, newline-containing
filenames, ancestor folder markers, non-repositories and cancellation.
`test-query.c` and `test-integration.c` contain the test harnesses; integration
tests must use `GSETTINGS_BACKEND=memory` to keep editor preferences isolated.
