/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <unordered_map>

#include "utils.h"

namespace {

using ::android::bluetooth::audio::utils::FrameCount;
using ::android::bluetooth::audio::utils::ParseAudioParams;

class UtilsTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override { map_.clear(); }

  std::unordered_map<std::string, std::string> map_;
};

TEST_F(UtilsTest, HashMapEmptyParams) {
  std::string params = "";
  map_ = ParseAudioParams(params);
  // map = {}
  EXPECT_TRUE(map_.empty());
}

TEST_F(UtilsTest, HashMapDelimitOnly) {
  std::string params = ";";
  map_ = ParseAudioParams(params);
  // map = {}
  EXPECT_TRUE(map_.empty());
}

TEST_F(UtilsTest, HashMapNotKeyValuePair) {
  std::string params = "key0";
  map_ = ParseAudioParams(params);
  // map = {[key0]=""}
  EXPECT_EQ(map_.size(), 1);
  EXPECT_NE(map_.find("key0"), map_.end());
  EXPECT_EQ(map_["key0"].length(), 0);
}

TEST_F(UtilsTest, HashMapEmptyValue) {
  std::string params = "key0=";
  map_ = ParseAudioParams(params);
  // map = {[key0]=""}
  EXPECT_EQ(map_.size(), 1);
  EXPECT_NE(map_.find("key0"), map_.end());
  EXPECT_EQ(map_["key0"].length(), 0);
}

TEST_F(UtilsTest, HashMapEmptyValueWithDelimit) {
  std::string params = "key0=;";
  map_ = ParseAudioParams(params);
  // map = {[key0]=""}
  EXPECT_EQ(map_.size(), 1);
  EXPECT_NE(map_.find("key0"), map_.end());
  EXPECT_EQ(map_["key0"].length(), 0);
}

TEST_F(UtilsTest, HashMapOneKeyValuePair) {
  std::string params = "key0=value0";
  map_ = ParseAudioParams(params);
  // map = {[key0]="value0"}
  EXPECT_EQ(map_.size(), 1);
  EXPECT_EQ(map_["key0"], "value0");
}

TEST_F(UtilsTest, HashMapOneKeyValuePairWithDelimit) {
  std::string params = "key0=value0;";
  map_ = ParseAudioParams(params);
  // map = {[key0]="value0"}
  EXPECT_EQ(map_.size(), 1);
  EXPECT_EQ(map_["key0"], "value0");
}

TEST_F(UtilsTest, HashMapTwoKeyValuePairs) {
  std::string params = "key0=value0;key1=value1";
  map_ = ParseAudioParams(params);
  // map = {[key0]="value0", [key1]="value1"}
  EXPECT_EQ(map_.size(), 2);
  EXPECT_EQ(map_["key0"], "value0");
  EXPECT_EQ(map_["key1"], "value1");
}

TEST_F(UtilsTest, HashMapEmptyKey) {
  std::string params = "=value";
  map_ = ParseAudioParams(params);
  // map = {}
  EXPECT_TRUE(map_.empty());
}

TEST_F(UtilsTest, HashMapEmptyKeyWithDelimit) {
  std::string params = "=value;";
  map_ = ParseAudioParams(params);
  // map = {}
  EXPECT_TRUE(map_.empty());
}

TEST_F(UtilsTest, HashMapEquivalentOnly) {
  std::string params = "=";
  map_ = ParseAudioParams(params);
  // map = {}
  EXPECT_TRUE(map_.empty());
}

TEST_F(UtilsTest, HashMapNoKeyValuePair) {
  std::string params = "=;";
  map_ = ParseAudioParams(params);
  // map = {}
  EXPECT_TRUE(map_.empty());
}

TEST_F(UtilsTest, HashMapTwoPairsWithFirstKeyEmpty) {
  std::string params = "=value0;key1=value1";
  map_ = ParseAudioParams(params);
  // map = {[key1]="value1"}
  EXPECT_EQ(map_.size(), 1);
  EXPECT_EQ(map_["key1"], "value1");
}

TEST_F(UtilsTest, FrameCountTest) {
  EXPECT_EQ(FrameCount(120000, 44100), 5292);
  EXPECT_EQ(FrameCount(7500, 32000), 240);
}

}  // namespace
