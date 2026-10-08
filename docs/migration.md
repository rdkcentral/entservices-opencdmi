# OCDM Client Library Migration (RDKEMW-19729)

## Overview

This document describes the migration of all downstream patches from two Yocto
recipes into individual git commits on this repository, on the branch
`feature/RDKEMW-19729-ocdm-client-migration`.

**Goal:** Build both the Thunder OCDM plugin (`plugin/`) and the OCDM client
library (`libocdm.so`, under `client/`) in the same repository, eliminating the
need for out-of-tree patches in Yocto.

### Source Recipes (patch origins)

| Recipe | Yocto path |
|---|---|
| `lib32-entservices-opencdmi` | `meta-rdk-video/recipes-extended/entservices/` |
| `lib32-wpeframework-clientlibraries` | `meta-rdk-video/recipes-extended/wpeframework/` |

---

## Migration Steps

### 1. Establish the branch base

Branch `feature/RDKEMW-19729-ocdm-client-migration` was created from the
`develop` branch at tag `1.0.6`.

The `client/` subdirectory (previously maintained in the separate
`ThunderClientLibraries` repository at tag `R4.4.2`) was imported via two merge
commits that preserve its history:

- `b3c61d4` — Import OCDM client source history from ThunderClientLibraries R4.4.2
- `61c9ca9` — Import Thunder CMake module history for OCDM client
- `d91a955` — Add `client/` subdirectory to build under `PLUGIN_OPENCDMI`
- `06a21c2` — Fix cmake: add `ocdm::ocdm` alias and guard `find_package` in plugin

### 2. Phase 1 — `entservices-opencdmi` plugin patches

These patches originated from
`meta-rdk-video/recipes-extended/entservices/files/` and modify files under
`plugin/` and `helpers/`.

| Commit | Summary | Source patch |
|---|---|---|
| `a1f367e` | `ocdm: manage OCDM buffers under a dedicated directory in tmpfs` | `0003-set-OCDM-sharepath-to-tmp-OCDM.patch` |
| `545d611` | `RDK-31882: add GstCaps parsing in OCDM for media stream type detection` | `0001-RDK-31882-Add-GstCaps-parsing-in-OCDM-to-rdkservices.patch` |
| `a9add09` | `RDK-32598: add GstCaps parsing support to determine media type for Cobalt` | `0001-add_gstcaps_forcobalt_mediatype.patch` |
| `1574974` | `RDKTV-20749: revert returnResponse macro change` | `0001-RDKTV-20749-Revert-Merge-pull-request-3336-from-npol.patch` |
| `5d46787` | `ocdm: refactor CBCS decryption with protobuf-based PSSH parsing` | `0001-rdkservices_cbcs_changes.patch` |
| `799e910` | `ocdm: add conditional Thunder R4 compilation support` | `0002-Adding-Support-For-R4.patch` |
| `5f74438` | `RDKTV-25123: add metrics pass-through on the OCDM framework` | `0001-Add-a-new-metrics-punch-through-on-the-OCDM-framework-rdkservice.patch` |
| `551592b` | `RDKEVD-5457: set OCDM decrypt worker thread name for correct scheduling` | `0001-set-OCDM-process-thread-name.patch` |

### 3. Phase 2 — `wpeframework-clientlibraries` client patches

These patches originated from
`meta-rdk-video/recipes-extended/wpeframework/files/` and modify files under
`client/`. All patches use the path prefix `Source/ocdm/` which must be
translated to `client/` before applying.

#### Path translation

```bash
sed 's|Source/ocdm/|client/|g' <patch> | patch -p1 --fuzz=3
```

| Commit | Summary | Source patch(es) |
|---|---|---|
| `a583a9e` | `RDK-41887: align OCDM client library version with R4.4 release` | `r4.4/Library-version-Matched-With-Release-ClientLibs.patch` (single-line `sed -i` — multi-component patch, only `CMakeLists.txt` hunk relevant) |
| `9b94288` | `RDK-47917,RDK-48326: add functionality to construct session private data` | `r4.4/0001-Add-functionality-to-construct-Session-private-data.patch`, `r4.4/0001-RDK-OCDM-adapter.patch`, `r4.4/0003_MediaType_name_changed.patch`, `r4.4/0001-OCDM-enhancement-for-ocdm-adapter.patch` |
| `12dc06a` | `ocdm: reconnect to plugin and return error during session creation if connection establishment fails` | `r4.4/0001-reconnect-if-connection-establishment-is-failed.patch` |
| `3a63970` | `ocdm: check _session has a valid pointer before accessing in MediaKeySession` | `r4.4/0001-check-_session-has-a-valid-pointer.patch` |
| `b183a8d` | `RDK-40666,RDK-37617,RDK-41828: extend SVP to support secAPI2 and secAPI3` | `r4.4/0001-add-svp-header-to-data-before-decryption.patch` |
| `7d0dc25` | `ocdm: add error handling for invalid external input in client API` | `0001-error-handling-if-invalid-external-input.patch` |
| `17f06b1` | `ocdm: preallocate secure memory before decrypt to reduce frame drops` | `r4.4/0001-DELIA-64727-Prealloc-secure-memory-before-decrypt.patch` |
| `82a9d46` | `RDKEMW-7064: don't call decrypt for fake allocations after Essos RM revoke` | `r4.4/0001-RDKEMW-7064-Dont-decrypt-fake-buffer-is-revoke-has-b.patch` |
| `e615708` | `RDKEMW-13372: support for additional clear data after subsample map` | `r4.4/0001-RDKEMW-13372-Support-for-additional-clear-data-after.patch` |
| `5b1a2a7` | `RDKEMW-15917: add decrypt-to-host support for concurrent dual-pipeline SVP` | `wpeframework-clientlibraries_dth.patch`, `wpeframework-clientlibraries_dual_pipeline.patch` |
| `57fc339` | `RDKTV-1645: add keysystem parser to facilitate overriding keysystems for certain domains` | `0007-OCDM-added-keysystem-parser-to-facilitate-overriding.patch` |

---

## Skipped Patches

The following patches from the `wpeframework-clientlibraries` recipe were
determined to be **not relevant** to OCDM and were not migrated (they target
`SecurityAgent`, `Cryptography`, `Vault`, or `PowerController` components):

- `R4.2_compilation_error_assert.patch`
- `0003-trace_l2-BuildError-fix.patch`
- `RDK-55149.patch`
- `0001-Implement-IPersistent-interface-for-RPC-Vault.patch`
- `0001-SecAPI-Re-acquire-sec-handle-after-flush.patch`
- `0004-R4-Security-Agent-Library-NameChange.patch`
- `0001-Cryptography-CipherNetflix.r4.4.patch`
- `0001-Add-vault-platform-case.patch`
- `0001-PowerManagerClient-library-implementation.patch`

---

## Commit Message Format

All migration commits follow the project gitmessage template:

```
JIRA-ID: Summary of change

Reason for change: <description>
Test Procedure: <description>
Risks: <level>
```

Jira prefixes `DELIA-`, `XIONE-`, `LLAMA-`, and `XRE-` were scrubbed from
commit messages (internal Sky/Comcast trackers not visible externally).
Prefixes `RDK-`, `RDKTV-`, `RDKEMW-`, and `RDKEVD-` were preserved.

---

## Applying Patches (reference)

For any future patches from `wpeframework-clientlibraries` that target
`Source/ocdm/`:

```bash
# Single patch
sed 's|Source/ocdm/|client/|g' <patch-file> | patch -p1 --fuzz=3

# Multiple patches in a loop
for p in patch1.patch patch2.patch; do
  sed 's|Source/ocdm/|client/|g' "$p" | patch -p1 --fuzz=3
done
```

For patches from `entservices-opencdmi` that already target `plugin/` or
`helpers/`:

```bash
patch -p1 --fuzz=3 < <patch-file>
```

> **Note:** Use `patch -p1 --fuzz=3` rather than `git apply` — context line
> offsets between the upstream source and this repo are common and `git apply`
> is too strict to handle them gracefully.
