/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/video_dumping.h"

#include <stdio.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "rtc_base/random.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/video_frame_writer.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Test;

using VideoResolution = ::webrtc::webrtc_pc_e2e::
    PeerConnectionE2EQualityTestFixture::VideoResolution;

uint8_t RandByte(Random& random) {
  return random.Rand(255);
}

VideoFrame CreateRandom2x2VideoFrame(uint16_t id, Random& random) {
  rtc::scoped_refptr<I420Buffer> buffer = I420Buffer::Create(2, 2);

  uint8_t data[6] = {RandByte(random), RandByte(random), RandByte(random),
                     RandByte(random), RandByte(random), RandByte(random)};

  memcpy(buffer->MutableDataY(), data, 2);
  memcpy(buffer->MutableDataY() + buffer->StrideY(), data + 2, 2);
  memcpy(buffer->MutableDataU(), data + 4, 1);
  memcpy(buffer->MutableDataV(), data + 5, 1);

  return VideoFrame::Builder()
      .set_id(id)
      .set_video_frame_buffer(buffer)
      .set_timestamp_us(1)
      .build();
}

std::vector<uint8_t> AsVector(const uint8_t* data, size_t size) {
  std::vector<uint8_t> out;
  out.assign(data, data + size);
  return out;
}

void AssertFramesEqual(rtc::scoped_refptr<webrtc::I420BufferInterface> actual,
                       rtc::scoped_refptr<VideoFrameBuffer> expected) {
  ASSERT_THAT(actual->width(), Eq(expected->width()));
  ASSERT_THAT(actual->height(), Eq(expected->height()));
  rtc::scoped_refptr<webrtc::I420BufferInterface> expected_i420 =
      expected->ToI420();

  int height = actual->height();

  EXPECT_THAT(AsVector(actual->DataY(), actual->StrideY() * height),
              ElementsAreArray(expected_i420->DataY(),
                               expected_i420->StrideY() * height));
  EXPECT_THAT(AsVector(actual->DataU(), actual->StrideU() * (height + 1) / 2),
              ElementsAreArray(expected_i420->DataU(),
                               expected_i420->StrideU() * (height + 1) / 2));
  EXPECT_THAT(AsVector(actual->DataV(), actual->StrideV() * (height + 1) / 2),
              ElementsAreArray(expected_i420->DataV(),
                               expected_i420->StrideV() * (height + 1) / 2));
}

void AssertFrameIdsAre(const std::string& filename,
                       std::vector<std::string> expected_ids) {
  FILE* file = fopen(filename.c_str(), "r");
  ASSERT_TRUE(file != nullptr);
  std::vector<std::string> actual_ids;
  char buffer[8];
  while (fgets(buffer, sizeof buffer, file) != nullptr) {
    std::string current_id(buffer);
    ASSERT_GE(current_id.size(), 2lu);
    // Trim "\n" at the end.
    actual_ids.push_back(current_id.substr(0, current_id.size() - 1));
  }
  EXPECT_THAT(actual_ids, ElementsAreArray(expected_ids));
}

class VideoDumpingTest : public Test {
 protected:
  ~VideoDumpingTest() override = default;

  void SetUp() override {
    video_filename_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                                 "video_dumping_test");
    ids_filename_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                               "video_dumping_test");
  }

  void TearDown() override {
    remove(video_filename_.c_str());
    remove(ids_filename_.c_str());
  }

  std::string video_filename_;
  std::string ids_filename_;
};

using CreateVideoFrameWriterTest = VideoDumpingTest;

TEST_F(CreateVideoFrameWriterTest, VideoIsWritenCorrectly) {
  Random random(/*seed=*/100);
  VideoFrame frame1 = CreateRandom2x2VideoFrame(1, random);
  VideoFrame frame2 = CreateRandom2x2VideoFrame(2, random);

  std::unique_ptr<test::VideoFrameWriter> writer = CreateVideoFrameWriter(
      video_filename_,
      /*frame_ids_dump_file_name=*/absl::nullopt,
      VideoResolution(/*width=*/2, /*height=*/2, /*fps=*/2));

  ASSERT_TRUE(writer->WriteFrame(frame1));
  ASSERT_TRUE(writer->WriteFrame(frame2));
  writer->Close();

  test::Y4mFrameReaderImpl frame_reader(video_filename_, /*width=*/2,
                                        /*height=*/2);
  ASSERT_TRUE(frame_reader.Init());
  EXPECT_THAT(frame_reader.NumberOfFrames(), Eq(2));
  AssertFramesEqual(frame_reader.ReadFrame(), frame1.video_frame_buffer());
  AssertFramesEqual(frame_reader.ReadFrame(), frame2.video_frame_buffer());
}

TEST_F(CreateVideoFrameWriterTest, VideoIsWritenWithFrameIds) {
  Random random(/*seed=*/100);
  VideoFrame frame1 = CreateRandom2x2VideoFrame(1, random);
  VideoFrame frame2 = CreateRandom2x2VideoFrame(2, random);

  std::unique_ptr<test::VideoFrameWriter> writer = CreateVideoFrameWriter(
      video_filename_, ids_filename_,
      VideoResolution(/*width=*/2, /*height=*/2, /*fps=*/2));

  ASSERT_TRUE(writer->WriteFrame(frame1));
  ASSERT_TRUE(writer->WriteFrame(frame2));
  writer->Close();

  test::Y4mFrameReaderImpl frame_reader(video_filename_, /*width=*/2,
                                        /*height=*/2);
  ASSERT_TRUE(frame_reader.Init());
  EXPECT_THAT(frame_reader.NumberOfFrames(), Eq(2));
  AssertFramesEqual(frame_reader.ReadFrame(), frame1.video_frame_buffer());
  AssertFramesEqual(frame_reader.ReadFrame(), frame2.video_frame_buffer());
  AssertFrameIdsAre(ids_filename_, {"1", "2"});
}

using VideoWriterTest = VideoDumpingTest;

TEST_F(VideoWriterTest, AllFramesAreWrittenWithSamplingModulo1) {
  Random random(/*seed=*/100);
  VideoFrame frame1 = CreateRandom2x2VideoFrame(1, random);
  VideoFrame frame2 = CreateRandom2x2VideoFrame(2, random);

  {
    std::unique_ptr<test::VideoFrameWriter> frame_writer =
        CreateVideoFrameWriter(
            video_filename_, /*frame_ids_dump_file_name=*/absl::nullopt,
            VideoResolution(/*width=*/2, /*height=*/2, /*fps=*/2));
    VideoWriter writer(frame_writer.get(), /*sampling_modulo=*/1);

    writer.OnFrame(frame1);
    writer.OnFrame(frame2);
    frame_writer->Close();
  }

  test::Y4mFrameReaderImpl frame_reader(video_filename_, /*width=*/2,
                                        /*height=*/2);
  ASSERT_TRUE(frame_reader.Init());
  EXPECT_THAT(frame_reader.NumberOfFrames(), Eq(2));
  AssertFramesEqual(frame_reader.ReadFrame(), frame1.video_frame_buffer());
  AssertFramesEqual(frame_reader.ReadFrame(), frame2.video_frame_buffer());
}

TEST_F(VideoWriterTest, OnlyEvery2ndFramesIsWrittenWithSamplingModulo2) {
  Random random(/*seed=*/100);
  VideoFrame frame1 = CreateRandom2x2VideoFrame(1, random);
  VideoFrame frame2 = CreateRandom2x2VideoFrame(2, random);
  VideoFrame frame3 = CreateRandom2x2VideoFrame(3, random);

  {
    std::unique_ptr<test::VideoFrameWriter> frame_writer =
        CreateVideoFrameWriter(
            video_filename_, /*frame_ids_dump_file_name=*/absl::nullopt,
            VideoResolution(/*width=*/2, /*height=*/2, /*fps=*/2));
    VideoWriter writer(frame_writer.get(), /*sampling_modulo=*/2);

    writer.OnFrame(frame1);
    writer.OnFrame(frame2);
    writer.OnFrame(frame3);
    frame_writer->Close();
  }

  test::Y4mFrameReaderImpl frame_reader(video_filename_, /*width=*/2,
                                        /*height=*/2);
  ASSERT_TRUE(frame_reader.Init());
  EXPECT_THAT(frame_reader.NumberOfFrames(), Eq(2));
  AssertFramesEqual(frame_reader.ReadFrame(), frame1.video_frame_buffer());
  AssertFramesEqual(frame_reader.ReadFrame(), frame3.video_frame_buffer());
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
