/*
 *   Copyright (c) 2021 Project CHIP Authors
 *   All rights reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

#include "DiscoverCommissionablesCommand.h"
#include <lib/support/BytesToHex.h>

using namespace ::chip;

CHIP_ERROR DiscoverCommissionablesCommand::Run()
{
    mCommissioner = GetExecContext()->commissioner;
    Mdns::DiscoveryFilter filter(Mdns::DiscoveryFilterType::kNone, (uint64_t) 0);
    return mCommissioner->DiscoverCommissionableNodes(filter);
}

void DiscoverCommissionablesCommand::Shutdown()
{
    for (int i = 0; i < mCommissioner->GetMaxCommissionableNodesSupported(); ++i)
    {
        const chip::Mdns::DiscoveredNodeData * dnsSdInfo = mCommissioner->GetDiscoveredDevice(i);
        if (dnsSdInfo == nullptr)
        {
            continue;
        }

        char rotatingId[chip::Mdns::kMaxRotatingIdLen * 2 + 1] = "";
        Encoding::BytesToUppercaseHexString(dnsSdInfo->rotatingId, dnsSdInfo->rotatingIdLen, rotatingId, sizeof(rotatingId));

        ChipLogProgress(Discovery, "Commissionable Node %d", i);
        ChipLogProgress(Discovery, "\tHost name:\t\t%s", dnsSdInfo->hostName);
        ChipLogProgress(Discovery, "\tPort:\t\t\t%u", dnsSdInfo->port);
        ChipLogProgress(Discovery, "\tLong discriminator:\t%u", dnsSdInfo->longDiscriminator);
        ChipLogProgress(Discovery, "\tVendor ID:\t\t%u", dnsSdInfo->vendorId);
        ChipLogProgress(Discovery, "\tProduct ID:\t\t%u", dnsSdInfo->productId);
        ChipLogProgress(Discovery, "\tCommissioning Mode\t%u", dnsSdInfo->commissioningMode);
        ChipLogProgress(Discovery, "\tDevice Type\t\t%u", dnsSdInfo->deviceType);
        ChipLogProgress(Discovery, "\tDevice Name\t\t%s", dnsSdInfo->deviceName);
        ChipLogProgress(Discovery, "\tRotating Id\t\t%s", rotatingId);
        ChipLogProgress(Discovery, "\tPairing Instruction\t%s", dnsSdInfo->pairingInstruction);
        ChipLogProgress(Discovery, "\tPairing Hint\t\t0x%x", dnsSdInfo->pairingHint);
        for (int j = 0; j < dnsSdInfo->numIPs; ++j)
        {
            char buf[chip::Inet::kMaxIPAddressStringLength];
            dnsSdInfo->ipAddress[j].ToString(buf);
            ChipLogProgress(Discovery, "\tAddress %d:\t\t%s", j, buf);
        }
    }
}
