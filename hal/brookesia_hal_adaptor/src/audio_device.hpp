/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file audio_device.hpp
 * @brief Board-backed audio adaptor that exposes playback and recording HAL interfaces.
 */
#pragma once

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interface.hpp"

using namespace esp_brookesia::hal;

/**
 * @brief Singleton audio adaptor backed by board-managed codec devices.
 *
 * The adaptor exposes both `AudioPlayerIface` and `AudioRecorderIface` and
 * bridges them to the board-manager audio ADC and DAC devices.
 */
class AudioDevice : public DeviceImpl<AudioDevice>, public AudioPlayerIface, public AudioRecorderIface {
public:
    /**
     * @brief Returns the singleton audio adaptor instance.
     *
     * @return Reference to the process-wide audio adaptor.
     */
    static AudioDevice &get_instance()
    {
        static AudioDevice inst;
        return inst;
    }

    AudioDevice(const AudioDevice &) = delete;
    AudioDevice &operator=(const AudioDevice &) = delete;
    AudioDevice(AudioDevice &&) = delete;
    AudioDevice &operator=(AudioDevice &&) = delete;

    /**
     * @brief Reports whether the adaptor is supported on the current board.
     *
     * @return Always true for this adaptor implementation.
     */
    bool probe() override
    {
        return true;
    }

    /**
     * @brief Returns whether both player and recorder handles have been initialized.
     *
     * @return true when both underlying handles are available, otherwise false.
     */
    bool check_initialized() const override;

    /**
     * @brief Initializes the board-managed audio devices and configuration.
     *
     * @return true on success, otherwise false.
     */
    bool init() override;

    /**
     * @brief Deinitializes the board-managed audio devices.
     *
     * @return true on success, otherwise false.
     */
    bool deinit() override;

    /**
     * @brief Returns the native playback handle.
     *
     * @return Opaque player handle, or `nullptr` when unavailable.
     */
    const void *get_play_handle() const
    {
        return AudioPlayerIface::get_handle();
    }

    /**
     * @brief Returns the native recording handle.
     *
     * @return Opaque recorder handle, or `nullptr` when unavailable.
     */
    const void *get_rec_handle() const
    {
        return AudioRecorderIface::get_handle();
    }

    /**
     * @brief Sets the output playback volume.
     *
     * @param volume Requested output volume in percent.
     * @return true on success, otherwise false.
     */
    bool set_volume(uint8_t volume) override;

    /**
     * @brief Returns the current output playback volume.
     *
     * @return Current output volume in percent.
     */
    uint8_t get_volume() const override;

    /**
     * @brief Opens the playback path using the configured sample format.
     *
     * @return true on success, otherwise false.
     */
    bool open_player() override;

    /**
     * @brief Closes the playback path.
     *
     * @return true on success, otherwise false.
     */
    bool close_player() override;

    /**
     * @brief Opens the recording path using the configured sample format.
     *
     * @return true on success, otherwise false.
     */
    bool open_recorder() override;

    /**
     * @brief Closes the recording path.
     *
     * @return true on success, otherwise false.
     */
    bool close_recorder() override;

    /**
     * @brief Applies the configured recorder gain values to the hardware.
     *
     * @return true on success, otherwise false.
     */
    bool set_recorder_gain() override;

private:
    /**
     * @brief Constructs the singleton adaptor.
     */
    AudioDevice(): DeviceImpl<AudioDevice>("AudioDevice") {}

    /**
     * @brief Destroys the adaptor.
     */
    ~AudioDevice() = default;

    /**
     * @brief Resolves runtime interface queries supported by this adaptor.
     *
     * @param id Hashed interface identifier.
     * @return Pointer to the matching interface, or `nullptr` when unsupported.
     */
    void *query_interface(uint64_t id) override
    {
        return build_table<AudioPlayerIface, AudioRecorderIface>(id);
    }

    /**
     * @brief Internal unlocked variant of `check_initialized()`.
     *
     * @return true when both playback and recording handles are available.
     */
    bool check_initialized_intern() const;

    mutable boost::mutex mutex_;        ///< Mutex guarding adaptor state.
};
