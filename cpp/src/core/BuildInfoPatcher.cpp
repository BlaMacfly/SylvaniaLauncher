#include "BuildInfoPatcher.h"
#include "Constants.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>

namespace {

// Index of the column whose name (the part before '!') equals `name`, or -1.
int columnIndex(const QStringList& header, const QString& name) {
    for (int i = 0; i < header.size(); ++i) {
        const QString& col = header[i];
        const int bang = col.indexOf(QLatin1Char('!'));
        const QString colName = (bang >= 0 ? col.left(bang) : col).trimmed();
        if (colName == name) return i;
    }
    return -1;
}

}  // namespace

bool patchBuildInfoCdn(const QString& gameDir, QString* err) {
    const auto fail = [&](const QString& m) {
        if (err) *err = m;
        return false;
    };

    const QString path = gameDir + QStringLiteral("/.build.info");
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        return fail(QStringLiteral(".build.info introuvable (%1)").arg(path));
    }

    QFile in(path);
    if (!in.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral(".build.info illisible (%1)").arg(in.errorString()));
    }
    const QByteArray raw = in.readAll();
    in.close();

    // Decode UTF-8, remember a trailing newline, split on LF and strip any CR
    // (a file edited on Windows may carry CRLF; we normalise to LF on write).
    const QString text = QString::fromUtf8(raw);
    const bool trailingNewline = text.endsWith(QLatin1Char('\n'));
    QStringList lines = text.split(QLatin1Char('\n'));
    if (trailingNewline && !lines.isEmpty() && lines.last().isEmpty()) {
        lines.removeLast();
    }
    for (QString& l : lines) {
        if (l.endsWith(QLatin1Char('\r'))) l.chop(1);
    }
    if (lines.isEmpty()) {
        return fail(QStringLiteral(".build.info vide"));
    }

    // Locate the two CDN columns by header name — never a fixed index.
    const QStringList header = lines.first().split(QLatin1Char('|'));
    const int hostsIdx = columnIndex(header, QStringLiteral("CDN Hosts"));
    const int serversIdx = columnIndex(header, QStringLiteral("CDN Servers"));
    if (hostsIdx < 0 || serversIdx < 0) {
        return fail(QStringLiteral("colonnes 'CDN Hosts'/'CDN Servers' absentes de l'en-tête"));
    }

    const QString host = QString::fromUtf8(SylvaniaConstants::kCdnMirrorHost);
    const QString servers = QString::fromUtf8(SylvaniaConstants::kCdnMirrorServers);

    // Patch every real data row (>= 9 columns); track whether anything changed.
    bool changed = false;
    for (int i = 1; i < lines.size(); ++i) {
        QStringList cols = lines[i].split(QLatin1Char('|'));
        if (cols.size() < 9 || cols.size() <= hostsIdx || cols.size() <= serversIdx) {
            continue;  // not a complete data row — leave it untouched
        }
        if (cols[hostsIdx] != host || cols[serversIdx] != servers) {
            cols[hostsIdx] = host;
            cols[serversIdx] = servers;
            lines[i] = cols.join(QLatin1Char('|'));
            changed = true;
        }
    }

    // Idempotent: already pointing at the mirror -> no write, no backup.
    if (!changed) return true;

    // Back up the untouched original exactly once.
    const QString bak = path + QStringLiteral(".bak");
    if (!QFileInfo::exists(bak)) {
        QFile::copy(path, bak);  // best-effort; non-fatal if it fails
    }

    // Reassemble: LF endings, UTF-8 without BOM (raw bytes), keep trailing LF.
    QString out = lines.join(QLatin1Char('\n'));
    if (trailingNewline) out += QLatin1Char('\n');

    QSaveFile sf(path);  // atomic: temp file + rename on commit
    if (!sf.open(QIODevice::WriteOnly)) {
        return fail(QStringLiteral("écriture impossible (%1)").arg(sf.errorString()));
    }
    sf.write(out.toUtf8());
    if (!sf.commit()) {
        return fail(QStringLiteral("commit échoué (%1)").arg(sf.errorString()));
    }
    return true;
}
