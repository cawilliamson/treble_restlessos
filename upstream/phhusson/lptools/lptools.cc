/*
 * Copyright (C) 2020 Pierre-Hugues Husson <phh@phh.me>
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

#include <getopt.h>
#include <inttypes.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <vector>
#include <chrono>

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <cutils/android_get_control_file.h>
#include <fs_mgr.h>
#include <liblp/builder.h>
#include <liblp/liblp.h>
#include <fs_mgr_dm_linear.h>
#include <libdm/dm.h>

#ifndef LPTOOLS_STATIC
#include <android/hardware/boot/1.1/IBootControl.h>
#include <android/hardware/boot/1.1/types.h>
#endif

using namespace android;
using namespace android::fs_mgr;

using namespace std::literals;

// https://cs.android.com/android/platform/superproject/+/android-13.0.0_r3:system/core/fastboot/device/utility.cpp;l=202
bool UpdateAllPartitionMetadata(const std::string& super_name,
                                const android::fs_mgr::LpMetadata& metadata) {
    // https://github.com/phhusson/vendor_lptools/blob/ff09b3f150cb0bceaf7c3bd428bfec812d7440bc/lptools.cc#L82
    size_t num_slots = 2;//pt->geometry.metadata_slot_count;

    bool ok = true;
    for (size_t i = 0; i < num_slots; i++) {
        ok &= UpdatePartitionTable(super_name, metadata, i);
    }
    return ok;
}

// https://cs.android.com/android/platform/superproject/+/android-13.0.0_r3:system/core/fastboot/device/utility.cpp;l=96
std::optional<std::string> FindPhysicalPartition(const std::string& name) {
    if (android::base::StartsWith(name, "../") || name.find("/../") != std::string::npos) {
        return {};
    }
    std::string path = "/dev/block/by-name/" + name;
    if (access(path.c_str(), W_OK) < 0) {
        return {};
    }
    return path;
}

// https://cs.android.com/android/platform/superproject/+/android-13.0.0_r3:system/core/fastboot/device/utility.cpp;l=217
std::string GetSuperSlotSuffix(const std::string& partition_name) {
    std::string current_slot_suffix = ::android::base::GetProperty("ro.boot.slot_suffix", "");
    uint32_t current_slot_number = SlotNumberForSlotSuffix(current_slot_suffix);
    std::string super_partition = fs_mgr_get_super_partition_name(current_slot_number);
    if (GetPartitionSlotSuffix(super_partition).empty()) {
        return current_slot_suffix;
    }

    std::string slot_suffix = GetPartitionSlotSuffix(partition_name);
    if (!slot_suffix.empty()) {
        return slot_suffix;
    }
    return current_slot_suffix;
}

// https://cs.android.com/android/platform/superproject/+/android-13.0.0_r3:system/core/fastboot/device/commands.cpp;l=440
class PartitionBuilder {
public:
    explicit PartitionBuilder();

    bool Write();
    bool Valid() const { return !!builder_; }
    MetadataBuilder* operator->() const { return builder_.get(); }

private:
    std::string super_device_;
    uint32_t slot_number_;
    std::unique_ptr<MetadataBuilder> builder_;
};

PartitionBuilder::PartitionBuilder() {
    auto partition_name = "system" + ::android::base::GetProperty("ro.boot.slot_suffix", "");
    std::string slot_suffix = GetSuperSlotSuffix(partition_name);
    slot_number_ = android::fs_mgr::SlotNumberForSlotSuffix(slot_suffix);
    auto super_device = FindPhysicalPartition(fs_mgr_get_super_partition_name(slot_number_));
    if (!super_device) {
        return;
    }
    super_device_ = *super_device;
    builder_ = MetadataBuilder::New(super_device_, slot_number_);
}

bool PartitionBuilder::Write() {
    auto metadata = builder_->Export();
    if (!metadata) {
        return false;
    }
    return UpdateAllPartitionMetadata(super_device_, *metadata.get());
}

bool saveToDisk(PartitionBuilder builder) {
    return builder.Write();
}

inline bool ends_with(std::string const & value, std::string const & ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

std::string findGroup(PartitionBuilder& builder) {
    auto groups = builder->ListGroups();

    auto partitionName = "system" + ::android::base::GetProperty("ro.boot.slot_suffix", "");
    for(auto groupName: groups) {
        auto partitions = builder->ListPartitionsInGroup(groupName);
        for (const auto& partition : partitions) {
            if(partition->name() == partitionName) {
                return groupName;
            }
        }
    }

    std::string maxGroup = "";
    uint64_t maxGroupSize = 0;
    for(auto groupName: groups) {
        auto group = builder->FindGroup(groupName);
        if(group->maximum_size() > maxGroupSize) {
            maxGroup = groupName;
            maxGroupSize = group->maximum_size();
        }
    }

    return maxGroup;
}

int main(int argc, char **argv) {
    if(argc<=1) {
        std::cerr << "Usage: " << argv[0] << " <create|remove|resize|replace|map|unmap|free|unlimited-group|clear-cow>" << std::endl;
        exit(1);
    }
    PartitionBuilder builder;
    auto group = findGroup(builder);
    std::cout << "Best group seems to be " << group << std::endl;

    if(strcmp(argv[1], "create") == 0) {
        if(argc != 4) {
            std::cerr << "Usage: " << argv[0] << " create <partition name> <partition size>" << std::endl;
            exit(1);
        }
        auto partName = argv[2];
        auto size = strtoll(argv[3], NULL, 0);
        auto partition = builder->FindPartition(partName);
        if(partition != nullptr) {
            std::cerr << "Partition " << partName << " already exists." << std::endl;
            exit(1);
        }
        partition = builder->AddPartition(partName, group, 0);
        if(partition == nullptr) {
            std::cerr << "Failed to add partition" << std::endl;
            exit(1);
        }
        auto result = builder->ResizePartition(partition, size);
        std::cout << "Growing partition " << result << std::endl;
        if(!result) {
            std::cerr << "Not enough space to resize partition" << std::endl;
            exit(1);
        }
        if(!saveToDisk(std::move(builder))) {
            std::cerr << "Failed to write partition table" << std::endl;
            exit(1);
        }

        std::string dmPath;
        CreateLogicalPartitionParams params {
                .block_device = "/dev/block/by-name/super",
                .metadata_slot = 0,
                .partition_name = partName,
                .timeout_ms = std::chrono::milliseconds(10000),
                .force_writable = true,
        };
        auto dmCreateRes = android::fs_mgr::CreateLogicalPartition(params, &dmPath);
        if(!dmCreateRes) {
            std::cerr << "Could not map partition: " << partName << std::endl;
            exit(1);
        }
        std::cout << "Creating dm partition for " << partName << " answered " << dmCreateRes << " at " << dmPath << std::endl;
        exit(0);
    } else if(strcmp(argv[1], "remove") == 0) {
        if(argc != 3) {
            std::cerr << "Usage: " << argv[0] << " remove <partition name>" << std::endl;
            exit(1);
        }
        auto partName = argv[2];
        auto dmState = android::dm::DeviceMapper::Instance().GetState(partName);
        if(dmState == android::dm::DmDeviceState::ACTIVE) {
            android::fs_mgr::DestroyLogicalPartition(partName);
        }
        builder->RemovePartition(partName);
        if(!saveToDisk(std::move(builder))) {
            std::cerr << "Failed to write partition table" << std::endl;
            exit(1);
        }
        exit(0);
    } else if(strcmp(argv[1], "resize") == 0) {
        if(argc != 4) {
            std::cerr << "Usage: " << argv[0] << " resize <partition name> <newsize>" << std::endl;
            exit(1);
        }
        auto partName = argv[2];
        auto size = strtoll(argv[3], NULL, 0);
        auto partition = builder->FindPartition(partName);
        if(partition == nullptr) {
            std::cerr << "Partition does not exist" << std::endl;
            exit(1);
        }
        auto result = builder->ResizePartition(partition, size);
        if(!result) {
            std::cerr << "Not enough space to resize partition" << std::endl;
            exit(1);
        }
        if(!saveToDisk(std::move(builder))) {
            std::cerr << "Failed to write partition table" << std::endl;
            exit(1);
        }
        std::cout << "Resizing partition " << result << std::endl;
        exit(0);
    } else if(strcmp(argv[1], "replace") == 0) {
        if(argc != 4) {
            std::cerr << "Usage: " << argv[0] << " replace <original partition name> <new partition name>" << std::endl;
            std::cerr << "This will delete <new partition name> and rename <original partition name> to <new partition name>" << std::endl;
            exit(1);
        }
        auto src = argv[2];
        auto dst = argv[3];
        auto srcPartition = builder->FindPartition(src);
        if(srcPartition == nullptr) {
            srcPartition = builder->FindPartition(src + ::android::base::GetProperty("ro.boot.slot_suffix", ""));
        }
        if(srcPartition == nullptr) {
            std::cerr << "Original partition does not exist" << std::endl;
            exit(1);
        }
        auto dstPartition = builder->FindPartition(dst);
        if(dstPartition == nullptr) {
            dstPartition = builder->FindPartition(dst + ::android::base::GetProperty("ro.boot.slot_suffix", ""));
        }
        std::string dstPartitionName = dst;
        if(dstPartition != nullptr) {
            dstPartitionName = dstPartition->name();
        }
        std::vector<std::unique_ptr<Extent>> originalExtents;

        const auto& extents = srcPartition->extents();
        for(unsigned i=0; i<extents.size(); i++) {
            const auto& extend = extents[i];
            auto linear = extend->AsLinearExtent();
            if(linear != nullptr) {
                auto copyLinear = std::make_unique<LinearExtent>(linear->num_sectors(), linear->device_index(), linear->physical_sector());
                originalExtents.push_back(std::move(copyLinear));
            } else {
                auto copyZero = std::make_unique<ZeroExtent>(extend->num_sectors());
                originalExtents.push_back(std::move(copyZero));
            }
        }
        builder->RemovePartition(srcPartition->name());
        builder->RemovePartition(dstPartitionName);
        auto newDstPartition = builder->AddPartition(dstPartitionName, group, 0);
        if(newDstPartition == nullptr) {
            std::cerr << "Failed to add partition" << std::endl;
            exit(1);
        }
        for(auto&& extent: originalExtents) {
            newDstPartition->AddExtent(std::move(extent));
        }
        if(!saveToDisk(std::move(builder))) {
            std::cerr << "Failed to write partition table" << std::endl;
            exit(1);
        }
        exit(0);
    } else if(strcmp(argv[1], "map") == 0) {
        if(argc != 3) {
            std::cerr << "Usage: " << argv[0] << " map <partition name>" << std::endl;
            exit(1);
        }
        auto partName = argv[2];
        std::string dmPath;
        CreateLogicalPartitionParams params {
                .block_device = "/dev/block/by-name/super",
                .metadata_slot = 0,
                .partition_name = partName,
                .timeout_ms = std::chrono::milliseconds(10000),
                .force_writable = true,
        };
        auto dmCreateRes = android::fs_mgr::CreateLogicalPartition(params, &dmPath);
        if(!dmCreateRes) {
            std::cerr << "Could not map partition: " << partName << std::endl;
            exit(1);
        }
        std::cout << "Creating dm partition for " << partName << " answered " << dmCreateRes << " at " << dmPath << std::endl;
        exit(0);
    } else if(strcmp(argv[1], "unmap") == 0) {
        if(argc != 3) {
            std::cerr << "Usage: " << argv[0] << " unmap <partition name>" << std::endl;
            exit(1);
        }
        auto partName = argv[2];
        auto dmState = android::dm::DeviceMapper::Instance().GetState(partName);
        if(dmState == android::dm::DmDeviceState::ACTIVE) {
            if (!android::fs_mgr::DestroyLogicalPartition(partName)) {
                std::cerr << "Unable to unmap " << partName << std::endl;
                exit(1);
            }
        }
        exit(0);
    } else if(strcmp(argv[1], "free") == 0) {
        if(argc != 2) {
            std::cerr << "Usage: " << argv[0] << " free" << std::endl;
            exit(1);
        }
        auto groupO = builder->FindGroup(group);
        uint64_t maxSize = groupO->maximum_size();

        uint64_t total = 0;
        auto partitions = builder->ListPartitionsInGroup(group);
        for (const auto& partition : partitions) {
            total += partition->BytesOnDisk();
        }

        uint64_t groupAllocatable = maxSize - total;
        uint64_t superFreeSpace = builder->AllocatableSpace() - builder->UsedSpace();
        if(groupAllocatable > superFreeSpace || maxSize == 0)
            groupAllocatable = superFreeSpace;

        printf("Free space: %" PRIu64 "\n", groupAllocatable);

        exit(0);
    } else if(strcmp(argv[1], "unlimited-group") == 0) {
        builder->ChangeGroupSize(group, 0);
        saveToDisk(std::move(builder));
        return 0;
    } else if(strcmp(argv[1], "clear-cow") == 0) {
#ifndef LPTOOLS_STATIC
        // Ensure this is a V AB device, and that no merging is taking place (merging? in gsi? uh)
        auto svc1_1 = ::android::hardware::boot::V1_1::IBootControl::tryGetService();
        if(svc1_1 == nullptr) {
            std::cerr << "Couldn't get a bootcontrol HAL. You can clear cow only on V AB devices" << std::endl;
            return 1;
        }
        auto mergeStatus = svc1_1->getSnapshotMergeStatus();
        if(mergeStatus != ::android::hardware::boot::V1_1::MergeStatus::NONE) {
            std::cerr << "Merge status is NOT none, meaning a merge is pending. Clearing COW isn't safe" << std::endl;
            return 1;
        }
#endif

        uint64_t superFreeSpace = builder->AllocatableSpace() - builder->UsedSpace();
        std::cerr << "Super allocatable " << superFreeSpace << std::endl;

        uint64_t total = 0;
        auto partitions = builder->ListPartitionsInGroup("cow");
        for (const auto& partition : partitions) {
            std::cout << "Deleting partition? " << partition->name() << std::endl;
            if(ends_with(partition->name(), "-cow")) {
                std::cout << "Deleting partition " << partition->name() << std::endl;
                builder->RemovePartition(partition->name());
            }
        }
        if(!saveToDisk(std::move(builder))) {
            std::cerr << "Failed to write partition table" << std::endl;
            exit(1);
        }
        return 0;
    }

    return 0;
}
