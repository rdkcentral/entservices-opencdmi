# OCDM Client Migration from ThunderClientLibraries

## Summary

The OCDM client library (`libocdm.so`) and its public headers (`opencdm/open_cdm.h`, etc.)
have been moved from `ThunderClientLibraries` into this repository (`entservices-opencdmi`).

The source now lives under the `client/` subdirectory and is built as part of the
`PLUGIN_OPENCDMI` CMake target alongside the server-side plugin.

---

## Why

`ThunderClientLibraries` is pinned to the WPEFramework R4.4 release cadence and cannot
version independently. The OCDM client code accumulated a growing set of platform patches
that needed to be carried as `.patch` files in the Yocto recipe. Moving the client into
`entservices-opencdmi` allows:

- Independent versioning of the OCDM client on its own release cadence
- Patches applied directly to the source tree with full git history, rather than carried
  as external patch files
- A single source of truth for `libocdm.so` — server plugin and client library are versioned
  together

---

## What moved

| Artifact | Previously from | Now from |
|----------|----------------|----------|
| `libocdm.so` | `ThunderClientLibraries` (built via `PACKAGECONFIG[opencdm]`) | `entservices-opencdmi` (`client/`) |
| `opencdm/open_cdm.h` | `ThunderClientLibraries` sysroot | `entservices-opencdmi` sysroot |
| `WPEFramework/ocdm/open_cdm.h` | `ThunderClientLibraries` sysroot | `entservices-opencdmi` sysroot |
| `ocdm.pc` | `ThunderClientLibraries` | `entservices-opencdmi` |
| `ocdmConfig.cmake` | `ThunderClientLibraries` | `entservices-opencdmi` |

The `client/` directory was imported from `ThunderClientLibraries` at tag `4.4.2` using
`git filter-repo`, preserving the full git history of every file. Use
`git log --follow client/<filename>` to see the complete history of any client file
back through its Thunder origins.

---

## Patches that were migrated

The following patches were previously carried in `meta-rdk-video` and platform layers
as `file://` entries in the `wpeframework-clientlibraries` Yocto recipe. They are now
committed directly into this repository's git history on the `client/` path:

| Original patch file | Applied to |
|--------------------|-----------|
| `r4.4/0001-RDK-OCDM-adapter.patch` | `client/` |
| `r4.4/0003_MediaType_name_changed.patch` | `client/` |
| `r4.4/0001-OCDM-enhancement-for-ocdm-adapter.patch` | `client/` |
| `r4.4/0001-add-svp-header-to-data-before-decryption.patch` | `client/` |
| `r4.4/0001-Add-functionality-to-construct-Session-private-data.patch` | `client/` |
| `r4.4/0001-DELIA-64727-Prealloc-secure-memory-before-decrypt.patch` | `client/` |
| `r4.4/0001-RDKEMW-7064-Dont-decrypt-fake-buffer-is-revoke-has-b.patch` | `client/` |
| `r4.4/0001-RDKEMW-13372-Support-for-additional-clear-data-after.patch` | `client/` |
| `r4.4/0001-check-_session-has-a-valid-pointer.patch` | `client/` |
| `r4.4/0001-reconnect-if-connection-establishment-is-failed.patch` | `client/` |
| `0001-error-handling-if-invalid-external-input.patch` | `client/` |
| `0007-OCDM-added-keysystem-parser-to-facilitate-overriding.patch` | `client/` |

---

## Yocto recipe changes required

### For platforms adopting this change

**`entservices-opencdmi.bb`** — update `SRCREV` to `>= 1.0.7` and `PV = "1.0.7"`.
No patch `file://` entries needed for the OCDM client.

**`ocdm.inc`** — `wpeframework-clientlibraries` removed from `PACKAGECONFIG[opencdmi]`.

**Any recipe that previously got `libocdm.so` or `open_cdm.h` transitively through
`wpeframework-clientlibraries`** must now add an explicit `DEPENDS` entry:

```bitbake
DEPENDS += "entservices-opencdmi"
```

Known affected recipes:
- `aamp-middleware.inc`
- `libloader-app_24.lts.stable.bb`
- `libloader-app_25.lts.stable.bb`
- `netflix_7.0.0.bb`
- `rialto-ocdm_git.bb`
- `wpe-webkit.inc` (via `PACKAGECONFIG[wpeframework_opencdm]`)

### For platforms NOT yet adopting this change (opt-out)

Platforms that still want `ThunderClientLibraries` to provide `libocdm.so` should add
a bbappend that keeps the OCDM packageconfigs enabled and does **not** add
`entservices-opencdmi` to `DEPENDS`. No action is needed — the default
`wpeframework-clientlibraries` recipe behaviour is unchanged.

### For platforms that have made the switch

Add a platform bbappend to remove the now-redundant OCDM build from
`ThunderClientLibraries` and the stale patch entries:

```bitbake
# wpeframework-clientlibraries_4.4.bbappend
# This platform builds libocdm.so from entservices-opencdmi instead.
PACKAGECONFIG:remove = "opencdm opencdm_gst opencdmi_rdk_svp"
SRC_URI:remove = " \
    file://r4.4/0001-RDK-OCDM-adapter.patch \
    file://r4.4/0003_MediaType_name_changed.patch \
    file://r4.4/0001-OCDM-enhancement-for-ocdm-adapter.patch \
    file://r4.4/0001-add-svp-header-to-data-before-decryption.patch \
    file://r4.4/0001-Add-functionality-to-construct-Session-private-data.patch \
    file://r4.4/0001-DELIA-64727-Prealloc-secure-memory-before-decrypt.patch \
    file://r4.4/0001-RDKEMW-7064-Dont-decrypt-fake-buffer-is-revoke-has-b.patch \
    file://r4.4/0001-RDKEMW-13372-Support-for-additional-clear-data-after.patch \
    file://r4.4/0001-check-_session-has-a-valid-pointer.patch \
    file://r4.4/0001-reconnect-if-connection-establishment-is-failed.patch \
    file://0001-error-handling-if-invalid-external-input.patch \
    file://0007-OCDM-added-keysystem-parser-to-facilitate-overriding.patch \
"
# Backwards-compatible runtime dependency
RDEPENDS:${PN} += "entservices-opencdmi"
```

---

## CMake notes for downstream consumers

If your project uses `find_package(ocdm)`, the `ocdmConfig.cmake` and `ocdm.pc` files
are installed by `entservices-opencdmi` at the same sysroot paths as before. No changes
to downstream CMake or pkg-config usage are required.

If your project is built **in-tree** alongside `entservices-opencdmi` (i.e., as a
CMake subdirectory), use the alias target `ocdm::ocdm` directly and guard your
`find_package` call:

```cmake
if(NOT TARGET ocdm::ocdm)
    find_package(ocdm REQUIRED)
endif()
```

---

## DRM runtime behaviour — unchanged

The DRM backend plugins (`WideVine.drm`, `PlayReady4.drm`) are runtime-loaded and
are **not** compiled against `libocdm.so`. They remain in their own recipes and are
unaffected by this migration.
