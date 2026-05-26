// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// Polls GitHub's "latest release" endpoint for the Pawn compiler and Qawno.
// Singleton — every consumer (Settings dialog, status bar) listens to the same
// instance so a single network round-trip serves the whole app.
class UpdateChecker : public QObject {
  Q_OBJECT
 public:
  enum class Component { Compiler, Qawno, Wine };
  static constexpr int kComponentCount = 3;

  struct Release {
    QString tag;          // Raw tag, e.g. "v3.10.10" or "1.2.0.2670"
    QString version;      // Normalised "x.y.z" extracted from tag
    QString assetUrl;     // Download URL of the platform-appropriate asset
    QString assetName;    // File name of the asset
  };

  static UpdateChecker* instance();

  // Fire async fetches. Emits releaseFetched per component on success or
  // fetchFailed on error. Safe to call repeatedly; an in-flight request for
  // the same component is left alone.
  void checkAll();
  void check(Component which);

  // Cached results from the most recent successful check (empty until then).
  Release latest(Component which) const;
  bool isChecking(Component which) const;

  // Fire & forget asset download. Writes into the user's cache dir under
  // "<AppCache>/updates/<component>/<assetName>". Emits downloadFinished
  // with the on-disk path on success.
  void downloadAsset(Component which);
  // Path of an already-downloaded asset for this release, empty if none.
  QString downloadedPath(Component which) const;

 signals:
  void checkStarted(UpdateChecker::Component which);
  void releaseFetched(UpdateChecker::Component which, UpdateChecker::Release rel);
  void fetchFailed(UpdateChecker::Component which, QString error);
  void downloadProgress(UpdateChecker::Component which, qint64 bytes, qint64 total);
  void downloadFinished(UpdateChecker::Component which, QString path);
  void downloadFailed(UpdateChecker::Component which, QString error);

 private slots:
  void onReply(QNetworkReply* reply);

 private:
  explicit UpdateChecker(QObject* parent = nullptr);
  QString repoFor(Component which) const;
  QString cachePathFor(Component which, const QString& assetName) const;

  QNetworkAccessManager* nam_ = nullptr;
  bool checking_[kComponentCount] = {false, false, false};
  Release cached_[kComponentCount];
  QString downloaded_[kComponentCount];
  QNetworkReply* dlReply_[kComponentCount] = {nullptr, nullptr, nullptr};
};

#endif // UPDATECHECKER_H
