/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Mdns.h>
#include <app/server/Server.h>
#include <lib/mdns/Advertiser.h>
#include <lib/support/CodeUtils.h>
#include <platform/CHIPDeviceLayer.h>

namespace {

// As per specifications (Section 13.3), Nodes SHALL exit commissioning mode after 20 failed commission attempts.
constexpr uint8_t kMaxFailedCommissioningAttempts = 20;

void HandleCommissioningWindowTimeout(chip::System::Layer * aSystemLayer, void * aAppState)
{
    chip::CommissioningWindowManager * commissionMgr = static_cast<chip::CommissioningWindowManager *>(aAppState);
    commissionMgr->CloseCommissioningWindow();
}

void OnPlatformEventWrapper(const chip::DeviceLayer::ChipDeviceEvent * event, intptr_t arg)
{
    chip::CommissioningWindowManager * commissionMgr = reinterpret_cast<chip::CommissioningWindowManager *>(arg);
    commissionMgr->OnPlatformEvent(event);
}
} // namespace

namespace chip {

void CommissioningWindowManager::OnPlatformEvent(const DeviceLayer::ChipDeviceEvent * event)
{
    if (event->Type == DeviceLayer::DeviceEventType::kCommissioningComplete)
    {
        if (event->CommissioningComplete.status == CHIP_NO_ERROR)
        {
            ChipLogProgress(AppServer, "Commissioning completed successfully");
            Cleanup();
        }
        else
        {
            ChipLogError(AppServer, "Commissioning failed with error %" CHIP_ERROR_FORMAT,
                         event->CommissioningComplete.status.Format());
            OnSessionEstablishmentError(event->CommissioningComplete.status);
        }
    }
    else if (event->Type == DeviceLayer::DeviceEventType::kOperationalNetworkEnabled)
    {
        app::MdnsServer::Instance().AdvertiseOperational();
        ChipLogError(AppServer, "Operational advertising enabled");
    }
}

void CommissioningWindowManager::Cleanup()
{
    StopAdvertisement();

    mUseECM = false;

    mECMDiscriminator = 0;
    mECMPasscodeID    = 0;
    mECMIterations    = 0;
    mECMSaltLength    = 0;

    memset(&mECMPASEVerifier, 0, sizeof(mECMPASEVerifier));
    memset(mECMSalt, 0, sizeof(mECMSalt));

    // reset all advertising
    app::MdnsServer::Instance().StartServer(Mdns::CommissioningMode::kDisabled);
}

void CommissioningWindowManager::OnSessionEstablishmentError(CHIP_ERROR err)
{
    mFailedCommissioningAttempts++;
    ChipLogError(AppServer, "Commissioning failed (attempt %d): %s", mFailedCommissioningAttempts, ErrorStr(err));

    if (mFailedCommissioningAttempts < kMaxFailedCommissioningAttempts)
    {
        // If the number of commissioning attempts have not exceeded maximum retries, let's reopen
        // the pairing window.
        err = OpenCommissioningWindow();
    }

    // If the commissioning attempts limit exceeded, or reopening the commissioning window failed.
    if (err != CHIP_NO_ERROR)
    {
        Cleanup();

        if (mAppDelegate != nullptr)
        {
            mAppDelegate->OnRendezvousStopped();
        }
    }
}

void CommissioningWindowManager::OnSessionEstablished()
{
    CHIP_ERROR err = mServer->GetSecureSessionManager().NewPairing(
        Optional<Transport::PeerAddress>::Value(mPairingSession.GetPeerAddress()), mPairingSession.GetPeerNodeId(),
        &mPairingSession, CryptoContext::SessionRole::kResponder, 0);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Commissioning failed while setting up secure channel: err %s", ErrorStr(err));
        OnSessionEstablishmentError(err);
        return;
    }

    ChipLogProgress(AppServer, "Commissioning completed session establishment step");
    if (mAppDelegate != nullptr)
    {
        mAppDelegate->OnRendezvousStarted();
    }

    DeviceLayer::PlatformMgr().AddEventHandler(OnPlatformEventWrapper, reinterpret_cast<intptr_t>(this));

    StopAdvertisement();
    ChipLogProgress(AppServer, "Device completed Rendezvous process");
}

CHIP_ERROR CommissioningWindowManager::OpenCommissioningWindow()
{
    uint16_t keyID = 0;
    ReturnErrorOnFailure(mIDAllocator->Allocate(keyID));

    mPairingSession.Clear();
    ReturnErrorOnFailure(mPairingSession.MessageDispatch().Init(&mServer->GetSecureSessionManager()));

    if (mCommissioningTimeoutSeconds != kNoCommissioningTimeout)
    {
        ReturnErrorOnFailure(
            DeviceLayer::SystemLayer().StartTimer(mCommissioningTimeoutSeconds * 1000, HandleCommissioningWindowTimeout, this));
    }

    ReturnErrorOnFailure(mServer->GetExchangManager().RegisterUnsolicitedMessageHandlerForType(
        Protocols::SecureChannel::MsgType::PBKDFParamRequest, &mPairingSession));

    ReturnErrorOnFailure(StartAdvertisement());

    if (mUseECM)
    {
        ReturnErrorOnFailure(SetTemporaryDiscriminator(mECMDiscriminator));
        ReturnErrorOnFailure(mPairingSession.WaitForPairing(mECMPASEVerifier, mECMIterations, ByteSpan(mECMSalt, mECMSaltLength),
                                                            mECMPasscodeID, keyID, this));

        // reset all advertising, indicating we are in commissioningMode
        app::MdnsServer::Instance().StartServer(Mdns::CommissioningMode::kEnabledEnhanced);
    }
    else
    {
        uint32_t pinCode;
        ReturnErrorOnFailure(DeviceLayer::ConfigurationMgr().GetSetupPinCode(pinCode));

        ReturnErrorOnFailure(mPairingSession.WaitForPairing(
            pinCode, kSpake2p_Iteration_Count,
            ByteSpan(reinterpret_cast<const uint8_t *>(kSpake2pKeyExchangeSalt), strlen(kSpake2pKeyExchangeSalt)), keyID, this));

        // reset all advertising, indicating we are in commissioningMode
        app::MdnsServer::Instance().StartServer(Mdns::CommissioningMode::kEnabledBasic);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR CommissioningWindowManager::OpenBasicCommissioningWindow(uint16_t commissioningTimeoutSeconds,
                                                                    CommissioningWindowAdvertisement advertisementMode)
{
    RestoreDiscriminator();

#if CONFIG_NETWORK_LAYER_BLE
    // Enable BLE advertisements if commissioning window is to be opened on all supported
    // transports, and BLE is supported on the current device.
    SetBLE(advertisementMode == chip::CommissioningWindowAdvertisement::kAllSupported);
#else
    SetBLE(false);
#endif // CONFIG_NETWORK_LAYER_BLE

    mFailedCommissioningAttempts = 0;
    mCommissioningTimeoutSeconds = commissioningTimeoutSeconds;

    mUseECM = false;

    CHIP_ERROR err = OpenCommissioningWindow();
    if (err != CHIP_NO_ERROR)
    {
        Cleanup();
    }

    return err;
}

CHIP_ERROR CommissioningWindowManager::OpenEnhancedCommissioningWindow(uint16_t commissioningTimeoutSeconds, uint16_t discriminator,
                                                                       PASEVerifier & verifier, uint32_t iterations, ByteSpan salt,
                                                                       uint16_t passcodeID)
{
#if CONFIG_NETWORK_LAYER_BLE
    // TODO: Don't use BLE for commissioning additional fabrics on a device
    SetBLE(true);
#else
    SetBLE(false);
#endif

    VerifyOrReturnError(salt.size() <= sizeof(mECMSalt), CHIP_ERROR_INVALID_ARGUMENT);

    memcpy(mECMSalt, salt.data(), salt.size());
    mECMSaltLength = static_cast<uint32_t>(salt.size());

    mFailedCommissioningAttempts = 0;
    mCommissioningTimeoutSeconds = commissioningTimeoutSeconds;

    mECMDiscriminator = discriminator;
    mECMPasscodeID    = passcodeID;
    mECMIterations    = iterations;

    memcpy(&mECMPASEVerifier, &verifier, sizeof(PASEVerifier));

    mUseECM = true;

    CHIP_ERROR err = OpenCommissioningWindow();
    if (err != CHIP_NO_ERROR)
    {
        Cleanup();
    }
    return err;
}

void CommissioningWindowManager::CloseCommissioningWindow()
{
    if (mCommissioningWindowOpen)
    {
        ChipLogProgress(AppServer, "Closing pairing window");
        Cleanup();
    }
}

CHIP_ERROR CommissioningWindowManager::StartAdvertisement()
{
    if (mIsBLE)
    {
        ReturnErrorOnFailure(chip::DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(true));
    }
    if (mAppDelegate != nullptr)
    {
        mAppDelegate->OnPairingWindowOpened();
    }
    mCommissioningWindowOpen = true;
    return CHIP_NO_ERROR;
}

CHIP_ERROR CommissioningWindowManager::StopAdvertisement()
{
    RestoreDiscriminator();

    mServer->GetExchangManager().UnregisterUnsolicitedMessageHandlerForType(Protocols::SecureChannel::MsgType::PBKDFParamRequest);
    mPairingSession.Clear();

    mCommissioningWindowOpen = false;

    if (mIsBLE)
    {
        ReturnErrorOnFailure(chip::DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(false));
    }

    if (mAppDelegate != nullptr)
    {
        mAppDelegate->OnPairingWindowClosed();
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR CommissioningWindowManager::SetTemporaryDiscriminator(uint16_t discriminator)
{
    if (!mOriginalDiscriminatorCached)
    {
        // Cache the original discriminator
        ReturnErrorOnFailure(DeviceLayer::ConfigurationMgr().GetSetupDiscriminator(mOriginalDiscriminator));
        mOriginalDiscriminatorCached = true;
    }

    return DeviceLayer::ConfigurationMgr().StoreSetupDiscriminator(discriminator);
}

CHIP_ERROR CommissioningWindowManager::RestoreDiscriminator()
{
    if (mOriginalDiscriminatorCached)
    {
        // Restore the original discriminator
        ReturnErrorOnFailure(DeviceLayer::ConfigurationMgr().StoreSetupDiscriminator(mOriginalDiscriminator));
        mOriginalDiscriminatorCached = false;
    }

    return CHIP_NO_ERROR;
}

} // namespace chip
