/*
 * Copyright 2016 The Cartographer Authors
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

#include "cartographer/sensor/internal/collator.h"

namespace cartographer {
namespace sensor {

void Collator::AddTrajectory(
    const int trajectory_id,
    const absl::flat_hash_set<std::string>& expected_sensor_ids,
    const Callback& callback) {
  for (const auto& sensor_id : expected_sensor_ids) {
    const auto queue_key = QueueKey{trajectory_id, sensor_id};
    queue_.AddQueue(queue_key,
                    [callback, sensor_id](std::unique_ptr<Data> data) {
                      callback(sensor_id, std::move(data));
                    });
    queue_keys_[trajectory_id].push_back(queue_key);
  }
}

void Collator::FinishTrajectory(const int trajectory_id) {
  for (const auto& queue_key : queue_keys_[trajectory_id]) {
    queue_.MarkQueueAsFinished(queue_key);
  }
}

void Collator::AddSensorData(const int trajectory_id,
                             std::unique_ptr<Data> data) {
  QueueKey queue_key{trajectory_id, data->GetSensorId()};
  //常左值引用绑定到右值
  queue_.Add(std::move(queue_key), std::move(data)); 
}

void Collator::Flush() { queue_.Flush(); }

absl::optional<int> Collator::GetBlockingTrajectoryId() const {
  return absl::optional<int>(queue_.GetBlocker().trajectory_id);
}

void Collator::PauseCollating(const int trajectory_id,
                              const std::string& sensor_id) {
  LOG(INFO) << "Pausing to collate sensor: " << trajectory_id << ", " << sensor_id;
  QueueKey queue_key{trajectory_id, sensor_id};
  queue_.MarkQueueAsFinished(queue_key);
  const auto it =
      std::find(queue_keys_[trajectory_id].begin(),
                queue_keys_[trajectory_id].end(), queue_key);
  CHECK(it != queue_keys_[trajectory_id].end());
  queue_keys_[trajectory_id].erase(it);
}

void Collator::ResumeCollating(const int trajectory_id,
                               const std::string& sensor_id,
                               const Callback& callback) {
  LOG(INFO) << "Resuming to collate sensor: " << trajectory_id << ", " << sensor_id;
  QueueKey queue_key{trajectory_id, sensor_id};
  queue_.AddQueue(queue_key,
                  [callback, sensor_id](std::unique_ptr<Data> data) {
                    callback(sensor_id, std::move(data));
                  });
  const auto it =
      std::find(queue_keys_[trajectory_id].begin(),
                queue_keys_[trajectory_id].end(), queue_key);
  CHECK(it == queue_keys_[trajectory_id].end());
  queue_keys_[trajectory_id].push_back(queue_key);
}

}  // namespace sensor
}  // namespace cartographer
