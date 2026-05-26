// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "TelemetryClient.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSysInfo>
#include <QUuid>

// Telemetry posts to a Supabase Edge Function that holds the DB key
// server-side, so this app ships NO Supabase credential. Only the public
// function URL is embedded. Override at build time with QAWNO_TELEMETRY_URL
// (cmake/Telemetry.env); otherwise the deployed default is used.
#ifdef QAWNO_TELEMETRY_URL
#define TELEMETRY_URL_STR QAWNO_TELEMETRY_URL
#else
#define TELEMETRY_URL_STR "https://tmenljjfshefocoqnwgi.supabase.co/functions/v1/telemetry"
#endif

TelemetryClient* TelemetryClient::instance() {
  // Heap-leaked — see LocaleManager for rationale.
  static TelemetryClient* s_instance = new TelemetryClient(nullptr);
  return s_instance;
}

TelemetryClient::TelemetryClient(QObject* parent) : QObject(parent) {
  nam_ = new QNetworkAccessManager(this);
  endpointUrl_ = QString::fromUtf8(TELEMETRY_URL_STR);
}

TelemetryClient::Consent TelemetryClient::consent() const {
  const QString v = QSettings().value("TelemetryConsent", "pending").toString();
  if (v == "granted") return Consent::Granted;
  if (v == "denied")  return Consent::Denied;
  return Consent::Pending;
}

void TelemetryClient::setConsent(Consent c) {
  const char* v = c == Consent::Granted ? "granted"
                : c == Consent::Denied  ? "denied"
                                         : "pending";
  QSettings().setValue("TelemetryConsent", QString::fromLatin1(v));
}

QString TelemetryClient::deviceId() {
  QSettings s;
  QString id = s.value("TelemetryDeviceId").toString();
  if (id.isEmpty()) {
    id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.setValue("TelemetryDeviceId", id);
  }
  return id;
}

void TelemetryClient::resetDeviceId() {
  QSettings().remove("TelemetryDeviceId");
}

void TelemetryClient::track(const QString& eventName, const QVariantMap& props) {
  if (consent() != Consent::Granted) return;
  if (endpointUrl_.isEmpty()) return;  // dev build without an endpoint

  // Shape matches the Supabase Edge Function's whitelist (telemetry_events).
  // Any extra event-specific fields ride along in event_properties.
  QVariantMap payload;
  payload["anonymous_id"]  = deviceId();
  payload["event_name"]    = eventName;
  payload["app_version"]   = QCoreApplication::applicationVersion();
  payload["os_name"]       = QStringLiteral("macOS");
  payload["os_version"]    = QSysInfo::productVersion();
  payload["architecture"]  = QSysInfo::currentCpuArchitecture();
  payload["locale"]        = QLocale::system().name();
  payload["platform"]      = QStringLiteral("macos");
  payload["timestamp_utc"] =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  payload["source"]        = QStringLiteral("qawno");
  if (!props.isEmpty()) payload["event_properties"] = props;
  send(payload);
}

void TelemetryClient::trackSessionStart() {
  track("session_start");
}

void TelemetryClient::checkBackend() {
  if (endpointUrl_.isEmpty()) { emit backendStatus(false); return; }
  QNetworkRequest req((QUrl(endpointUrl_)));
  auto* reply = nam_->sendCustomRequest(req, "OPTIONS");
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const bool ok = reply->error() == QNetworkReply::NoError;
    emit backendStatus(ok);
    reply->deleteLater();
  });
}

void TelemetryClient::send(const QVariantMap& payload) {
  // POSTs to the Edge Function (no auth header — the function is public and
  // inserts with a server-side key). No Supabase credential in this binary.
  QNetworkRequest req((QUrl(endpointUrl_)));
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  const QJsonDocument doc(QJsonObject::fromVariantMap(payload));
  auto* reply = nam_->post(req, doc.toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}
