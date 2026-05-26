// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
constexpr const char* kCompilerRepo = "openmultiplayer/compiler";
constexpr const char* kQawnoRepo    = "Mac-Andreas/open.mp-Qawno-Silicon";
constexpr const char* kWineRepo     = "Gcenx/macOS_Wine_builds";

int idx(UpdateChecker::Component c) {
  switch (c) {
    case UpdateChecker::Component::Compiler: return 0;
    case UpdateChecker::Component::Qawno:    return 1;
    case UpdateChecker::Component::Wine:     return 2;
  }
  return 0;
}

QString normaliseTag(const QString& tag) {
  // Tags vary: "v3.10.10", "3.10.10", "1.2.0.2670". Pull the leading
  // dotted-numeric run so equality checks ignore the "v" prefix.
  QRegularExpression re(R"((\d+(?:\.\d+)+))");
  auto m = re.match(tag);
  return m.hasMatch() ? m.captured(1) : tag;
}
}  // namespace

UpdateChecker* UpdateChecker::instance() {
  // Heap-leaked — see LocaleManager for rationale (avoids double-free at exit).
  static UpdateChecker* s_instance = new UpdateChecker(nullptr);
  return s_instance;
}

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {
  nam_ = new QNetworkAccessManager(this);
  connect(nam_, &QNetworkAccessManager::finished, this, &UpdateChecker::onReply);
}

QString UpdateChecker::repoFor(Component which) const {
  switch (which) {
    case Component::Compiler: return kCompilerRepo;
    case Component::Qawno:    return kQawnoRepo;
    case Component::Wine:     return kWineRepo;
  }
  return kQawnoRepo;
}

void UpdateChecker::checkAll() {
  check(Component::Compiler);
  check(Component::Qawno);
  check(Component::Wine);
}

void UpdateChecker::check(Component which) {
  if (checking_[idx(which)]) return;
  checking_[idx(which)] = true;
  emit checkStarted(which);

  QUrl url(QString("https://api.github.com/repos/%1/releases/latest").arg(repoFor(which)));
  QNetworkRequest req(url);
  req.setRawHeader("Accept", "application/vnd.github+json");
  req.setRawHeader("User-Agent", "Qawno-Updater");
  // Attribute the reply to the right component on completion.
  req.setAttribute(QNetworkRequest::User, int(which));
  nam_->get(req);
}

void UpdateChecker::onReply(QNetworkReply* reply) {
  // Asset downloads use a per-reply finished() lambda; only release-metadata
  // requests are handled here (identified by the github api host + path).
  if (!reply->request().url().path().endsWith("/releases/latest")) {
    return;
  }
  reply->deleteLater();
  Component which = Component(reply->request().attribute(QNetworkRequest::User).toInt());
  checking_[idx(which)] = false;

  if (reply->error() != QNetworkReply::NoError) {
    emit fetchFailed(which, reply->errorString());
    return;
  }

  auto doc = QJsonDocument::fromJson(reply->readAll());
  if (!doc.isObject()) {
    emit fetchFailed(which, tr("Malformed response from GitHub"));
    return;
  }
  auto obj = doc.object();
  Release rel;
  rel.tag = obj.value("tag_name").toString();
  rel.version = normaliseTag(rel.tag);

  // Pick first asset whose name plausibly matches the current platform. On
  // macOS prefer "*mac*" / "*.dmg", on Windows "*win*" / "*.exe", on Linux
  // "*linux*" / "*.tar.*". Falls back to the first asset.
  auto assets = obj.value("assets").toArray();
  QStringList prefer;
  if (which == Component::Wine) {
    // Gcenx ships both staging and devel; prefer staging (better game compat)
    // and require osx64 to skip Linux-flavoured assets the same repo hosts.
    prefer << "wine-staging" << "osx64";
  } else {
#if defined(Q_OS_MAC)
    prefer << "mac" << "osx" << "darwin";
#elif defined(Q_OS_WIN)
    prefer << "win";
#else
    prefer << "linux";
#endif
  }
  QJsonObject picked;
  int bestScore = -1;
  for (const auto& v : assets) {
    auto a = v.toObject();
    const QString name = a.value("name").toString().toLower();
    int score = 0;
    for (const QString& tok : prefer) if (name.contains(tok)) ++score;
    if (score > bestScore) { bestScore = score; picked = a; }
  }
  if (picked.isEmpty() && !assets.isEmpty()) picked = assets.first().toObject();
  rel.assetName = picked.value("name").toString();
  rel.assetUrl  = picked.value("browser_download_url").toString();

  cached_[idx(which)] = rel;
  emit releaseFetched(which, rel);
}

UpdateChecker::Release UpdateChecker::latest(Component which) const {
  return cached_[idx(which)];
}

bool UpdateChecker::isChecking(Component which) const {
  return checking_[idx(which)];
}

QString UpdateChecker::cachePathFor(Component which, const QString& assetName) const {
  const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  QString sub;
  switch (which) {
    case Component::Compiler: sub = "compiler"; break;
    case Component::Qawno:    sub = "qawno";    break;
    case Component::Wine:     sub = "wine";     break;
  }
  QDir().mkpath(base + "/updates/" + sub);
  return base + "/updates/" + sub + "/" + assetName;
}

QString UpdateChecker::downloadedPath(Component which) const {
  return downloaded_[idx(which)];
}

void UpdateChecker::downloadAsset(Component which) {
  const auto rel = cached_[idx(which)];
  if (rel.assetUrl.isEmpty() || rel.assetName.isEmpty()) {
    emit downloadFailed(which, tr("No asset to download"));
    return;
  }
  if (dlReply_[idx(which)]) return;  // already in-flight

  const QString dest = cachePathFor(which, rel.assetName);
  // Trust an existing file of the right name as a previously completed
  // download — versioned tag is in the name, so collisions are rare.
  if (QFileInfo::exists(dest)) {
    downloaded_[idx(which)] = dest;
    emit downloadFinished(which, dest);
    return;
  }

  QNetworkRequest req((QUrl(rel.assetUrl)));
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setRawHeader("User-Agent", "Qawno-Updater");
  req.setAttribute(QNetworkRequest::User, int(which));

  auto* reply = nam_->get(req);
  dlReply_[idx(which)] = reply;

  connect(reply, &QNetworkReply::downloadProgress, this,
          [this, which](qint64 b, qint64 t) { emit downloadProgress(which, b, t); });

  connect(reply, &QNetworkReply::finished, this, [this, which, dest, reply] {
    dlReply_[idx(which)] = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit downloadFailed(which, reply->errorString());
      return;
    }
    QFile f(dest);
    if (!f.open(QIODevice::WriteOnly)) {
      emit downloadFailed(which, tr("Cannot write %1").arg(dest));
      return;
    }
    f.write(reply->readAll());
    f.close();
    downloaded_[idx(which)] = dest;
    emit downloadFinished(which, dest);
  });
}
