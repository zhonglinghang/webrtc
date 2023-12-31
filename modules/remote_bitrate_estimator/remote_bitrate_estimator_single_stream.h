/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "api/transport/field_trial_based_config.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/inter_arrival.h"
#include "modules/remote_bitrate_estimator/overuse_detector.h"
#include "modules/remote_bitrate_estimator/overuse_estimator.h"
#include "rtc_base/rate_statistics.h"

namespace webrtc {

class Clock;
struct RTPHeader;

class RemoteBitrateEstimatorSingleStream : public RemoteBitrateEstimator {
 public:
  RemoteBitrateEstimatorSingleStream(RemoteBitrateObserver* observer,
                                     Clock* clock);

  RemoteBitrateEstimatorSingleStream() = delete;
  RemoteBitrateEstimatorSingleStream(
      const RemoteBitrateEstimatorSingleStream&) = delete;
  RemoteBitrateEstimatorSingleStream& operator=(
      const RemoteBitrateEstimatorSingleStream&) = delete;

  ~RemoteBitrateEstimatorSingleStream() override;

  void IncomingPacket(const RtpPacketReceived& rtp_packet) override;
  TimeDelta Process() override;
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;
  void RemoveStream(uint32_t ssrc) override;
  DataRate LatestEstimate() const override;

 private:
  struct Detector {
    Detector();

    int64_t last_packet_time_ms;
    InterArrival inter_arrival;
    OveruseEstimator estimator;
    OveruseDetector detector;
  };

  // Triggers a new estimate calculation.
  void UpdateEstimate(int64_t time_now);

  std::vector<uint32_t> GetSsrcs() const;

  Clock* const clock_;
  const FieldTrialBasedConfig field_trials_;
  std::map<uint32_t, Detector> overuse_detectors_;
  RateStatistics incoming_bitrate_;
  uint32_t last_valid_incoming_bitrate_;
  AimdRateControl remote_rate_;
  RemoteBitrateObserver* const observer_;
  int64_t last_process_time_;
  int64_t process_interval_ms_;
  bool uma_recorded_;
};

}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_SINGLE_STREAM_H_
