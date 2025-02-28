/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <inet/InetInterface.h>
#include <lib/core/CHIPConfig.h>
#include <lib/core/CHIPError.h>
#include <lib/mdns/Advertiser.h>
#include <lib/mdns/MdnsCache.h>
#include <lib/mdns/Resolver.h>
#include <lib/mdns/platform/Mdns.h>
#include <platform/CHIPDeviceConfig.h>

// Enable detailed mDNS logging for publish
#undef DETAIL_LOGGING
// #define DETAIL_LOGGING

namespace chip {
namespace Mdns {

class DiscoveryImplPlatform : public ServiceAdvertiser, public Resolver
{
public:
    CHIP_ERROR Init();

    /// Starts the service advertiser if not yet started. Otherwise, removes all existing services.
    CHIP_ERROR Start(Inet::InetLayer * inetLayer, uint16_t port) override;

    /// Starts the service resolver if not yet started
    CHIP_ERROR StartResolver(Inet::InetLayer * inetLayer, uint16_t port) override { return Init(); }
    void ShutdownResolver() override { ChipMdnsShutdown(); }

    /// Advertises the CHIP node as an operational node
    CHIP_ERROR Advertise(const OperationalAdvertisingParameters & params) override;

    /// Advertises the CHIP node as a commissioner/commissionable node
    CHIP_ERROR Advertise(const CommissionAdvertisingParameters & params) override;

    /// This function stops publishing the device on mDNS.
    CHIP_ERROR StopPublishDevice() override;

    /// Returns DNS-SD instance name formatted as hex string
    CHIP_ERROR GetCommissionableInstanceName(char * instanceName, size_t maxLength) override;

    /// Registers a resolver delegate if none has been registered before
    CHIP_ERROR SetResolverDelegate(ResolverDelegate * delegate) override;

    /// Requests resolution of a node ID to its address
    CHIP_ERROR ResolveNodeId(const PeerId & peerId, Inet::IPAddressType type) override;

    CHIP_ERROR FindCommissionableNodes(DiscoveryFilter filter = DiscoveryFilter()) override;

    CHIP_ERROR FindCommissioners(DiscoveryFilter filter = DiscoveryFilter()) override;

    static DiscoveryImplPlatform & GetInstance();

private:
    DiscoveryImplPlatform();

    DiscoveryImplPlatform(const DiscoveryImplPlatform &) = delete;
    DiscoveryImplPlatform & operator=(const DiscoveryImplPlatform &) = delete;

    CHIP_ERROR PublishUnprovisionedDevice(chip::Inet::IPAddressType addressType, chip::Inet::InterfaceId interface);
    CHIP_ERROR PublishProvisionedDevice(chip::Inet::IPAddressType addressType, chip::Inet::InterfaceId interface);

    static void HandleNodeIdResolve(void * context, MdnsService * result, CHIP_ERROR error);
    static void HandleMdnsInit(void * context, CHIP_ERROR initError);
    static void HandleMdnsError(void * context, CHIP_ERROR initError);
    static void HandleNodeBrowse(void * context, MdnsService * services, size_t servicesSize, CHIP_ERROR error);
    static void HandleNodeResolve(void * context, MdnsService * result, CHIP_ERROR error);
    static CHIP_ERROR GenerateRotatingDeviceId(char rotatingDeviceIdHexBuffer[], size_t & rotatingDeviceIdHexBufferSize);
#ifdef DETAIL_LOGGING
    static void PrintEntries(const MdnsService * service);
#endif

    OperationalAdvertisingParameters mOperationalAdvertisingParams;
    CommissionAdvertisingParameters mCommissionableNodeAdvertisingParams;
    CommissionAdvertisingParameters mCommissionerAdvertisingParams;
    bool mIsOperationalPublishing        = false;
    bool mIsCommissionableNodePublishing = false;
    bool mIsCommissionerPublishing       = false;
    uint64_t mCommissionInstanceName;

    bool mMdnsInitialized                = false;
    ResolverDelegate * mResolverDelegate = nullptr;

    static DiscoveryImplPlatform sManager;
#if CHIP_CONFIG_MDNS_CACHE_SIZE > 0
    static MdnsCache<CHIP_CONFIG_MDNS_CACHE_SIZE> sMdnsCache;
#endif
};

} // namespace Mdns
} // namespace chip
