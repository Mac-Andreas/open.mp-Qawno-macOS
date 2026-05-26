// This file is part of qawno.
//
// qawno is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef TELEMETRYCLIENT_H
#define TELEMETRYCLIENT_H

#include <QObject>
#include <QString>
#include <QVariantMap>

class QNetworkAccessManager;

// Anonymous telemetry sender. POSTs JSON events to a Supabase Edge Function
// that holds the database key server-side — this app embeds NO Supabase
// credential, only the public function URL. The default URL is compiled in;
// override it with QAWNO_TELEMETRY_URL via cmake/Telemetry.env (gitignored).
// If no endpoint is set, every call is a no-op so dev builds don't ping prod.
//
// Privacy contract:
//  - No user-identifying data ever leaves the device.
//  - A persistent anonymous device_id (random UUID v4) is generated once and
//    stored in QSettings("TelemetryDeviceId"). Reset via the Privacy tab.
//  - Consent must be granted via ConsentDialog before any event is sent.
class TelemetryClient : public QObject {
  Q_OBJECT
 public:
  enum class Consent { Pending, Granted, Denied };

  static TelemetryClient* instance();

  Consent consent() const;
  void setConsent(Consent c);

  // Fire-and-forget event. Common fields (app, version, os, device_id, ts)
  // are added automatically; props is the event-specific payload.
  void track(const QString& eventName, const QVariantMap& props = {});

  // Convenience for the "session starts" event sent on app launch.
  void trackSessionStart();

  // Wipes the local device_id so the next track() generates a new one.
  void resetDeviceId();

  // Pings the telemetry Edge Function (CORS preflight) to learn whether the
  // backend is reachable; emits backendStatus(bool) with the result.
  void checkBackend();

 signals:
  // True = telemetry backend reachable, false = not. Drives the status dot.
  void backendStatus(bool reachable);

 private:
  explicit TelemetryClient(QObject* parent = nullptr);
  void send(const QVariantMap& payload);
  QString deviceId();

  QNetworkAccessManager* nam_ = nullptr;
  QString endpointUrl_;
};

#endif // TELEMETRYCLIENT_H
