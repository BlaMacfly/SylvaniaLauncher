#pragma once

#include <QString>

/**
 * @brief Repoints a CASC client's CDN to the Sylvania mirror.
 *
 * The WoW 7.3.5 client streams missing files (models, etc.) from the CDN listed
 * in `<gameDir>/.build.info`. Blizzard's 7.3.5 CDN is dead (404), so we rewrite
 * ONLY the "CDN Hosts" and "CDN Servers" columns to point at our mirror
 * (SylvaniaConstants::kCdnMirrorHost / kCdnMirrorServers).
 *
 * Properties:
 *  - Header-driven: columns are located by name (part before '!'), never by a
 *    hardcoded index, so build/CDN keys and every other field stay untouched.
 *  - Idempotent: if the host already points at the mirror, nothing is written
 *    and no backup is taken.
 *  - Backup: the original is copied to `.build.info.bak` once, before the first
 *    rewrite.
 *  - Writes UTF-8 without BOM, LF line endings, preserving a trailing newline.
 *  - Patches every data row with >= 9 columns, not just the first.
 *  - CASC only: a 3.3.5 (MPQ) install has no `.build.info`, so this is a no-op
 *    there — callers should still gate on the Legion edition.
 *
 * Non-blocking by contract: returns false and sets *err on a missing/unreadable
 * file or absent CDN columns; the caller logs the warning but still launches.
 *
 * @return true if the file was patched OR already up to date; false on error.
 */
bool patchBuildInfoCdn(const QString& gameDir, QString* err = nullptr);
