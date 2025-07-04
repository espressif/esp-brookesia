# GMF AI Audio

- [中文版](./README_CN.md)

`GMF AI Audio` is an artificial intelligence audio processing module that provides users with convenient and easy-to-use intelligent audio processing algorithms at the [GMF](https://github.com/espressif/esp-gmf) framework, such as voice wake-up, command word recognition, and echo cancellation. Currently, it offers the following modules based on `esp-sr`:

- [esp_gmf_afe_manager](./src/esp_gmf_afe_manager.c): `audio front end(afe)` manager
- [esp_gmf_aec](./src/esp_gmf_aec.c): Echo Cancellation
- [esp_gmf_wn](./src/esp_gmf_wn.c): A standalone wake word detection module that can be used independently
- [esp_gmf_afe](./src/esp_gmf_afe.c): An easy-to-use interface based on the `audio front end (afe)` from `esp-sr`, providing functionalities such as voice wake-up, command word recognition, and speech detection

| Name | Tag | Function | Method | Input Channel Number | Output Channel Number | Model Partition Dependency | Input Frame Length | Notes |
|:----:|:----:|:----:|:----:|:----:|:----:|:----:|:----:|:----|
| esp_gmf_afe | ai_afe | Audio front-end processing: Wake word detection, command word recognition, voice enhancement, echo cancellation, noise suppression, automatic gain control | `start_vcmd_det` | 1-4 | 1 | Yes | 256(sample) | Currently supports up to 2 microphone channels + 1 speaker reference signal, remaining channel selection marked as N, requires following voice command detection procedure |
| esp_gmf_aec | ai_aec | Echo cancellation: Eliminates echo interference in audio, improves voice quality | None | 1-4 | 1 | No | 256(sample) | Input channels can be set to multiple microphones, uses first microphone channel and reference channel for calculation, must include reference signal |
| esp_gmf_wn | ai_wn | Independent wake word detection: Lightweight wake word detection, independent of AFE, low resource consumption | None | 1-4 | 1 | Yes | 256(sample) | Supports up to 3 microphone channels, microphone channel count in input format must match working mode |

## AFE Manager `esp_gmf_afe_manager`

### Features

- Manages the data path and task scheduling of the Audio Front-End (AFE)
- Supports dynamic enabling/disabling of features (e.g., wake-up, AEC, VAD)
- Provides multi-task coordination to ensure real-time audio stream processing

### Key Characteristics

- Task Management: Creates independent `feed_task` and `fetch_task` for audio data input and result processing
- Feature Control: Dynamically toggle algorithm modules via `esp_afe_manager_enable_features`
- Event-Driven: Supports suspend/resume operations (`esp_afe_manager_suspend`) for low-power scenarios

### System Diagram

  ```mermaid
  graph LR
    subgraph AFE Manager
        DATA_CB[Raw Data]
        FEED_TASK[feed_task<br>Audio Input Task]
        FETCH_TASK[fetch_task<br>Result Processing Task]
        AFE_CORE[AFE Core<br>esp_afe_sr_iface]
        EVENT_CB[Result Callback]
        CTRL_IF[Control Interface]
    end

    FEED_TASK -->|Data Callback| DATA_CB
    FEED_TASK -->|Raw Audio Data| AFE_CORE
    AFE_CORE -->|Processing Results| FETCH_TASK
    FETCH_TASK -->|Result Callback<br>Data/Wake-Up/VAD/Command Words| EVENT_CB
    CTRL_IF -->|Suspend/Resume Interface| FEED_TASK
    CTRL_IF -->|Suspend/Resume Interface| FETCH_TASK
    CTRL_IF -->|Feature Toggle Interface<br>AEC/VAD/SE| AFE_CORE
  ```

## Echo Cancellation `esp_gmf_aec`

### Features

- Eliminates echo interference in audio to improve voice quality
- Supports multi-channel input and single-channel output

### Technical Specifications

- Supported Hardware: `ESP32`, `ESP32-S3`, and `ESP32-P4`
- Input Format
  - Sampling Rate: 8 kHz and 16 kHz
  - Bit Width: 16-bit PCM
- Channel Configuration: String identifiers (e.g., `MMNR`)
  - `M`: Microphone Channel
  - `R`: Reference Signal Channel
  - `N`: Invalid Signal
- Output Format: 16-bit single-channel PCM

## Wake Word Detection `esp_gmf_wn`

### Features

- Runs independently without AFE dependency, low resource usage
- Supports multi-channel input and single-channel output
- Notifies wake word detection results via callback function

### Technical Specifications

- Supported Hardware: `ESP32`, `ESP32-S3`, `ESP32-C3`, `ESP32-C5` and `ESP32-P4`
- Input Format
  - Sampling Rate: 16 kHz
  - Bit Width: 16-bit PCM
- Channel Configuration: String identifiers (e.g., `MMNR`)
  - `M`: Microphone Channel
  - `R`: Reference Signal Channel
  - `N`: Invalid Signal
- Output Format: 16-bit single-channel PCM

## Speech Recognition `esp_gmf_afe`

### Features

- Voice Wake-Up (Wake Word Detection)
- Command Word Recognition
- Voice Activity Detection (VAD) and State Management

### Key Characteristics

- Supported Hardware: `ESP32`, `ESP32-S3`, and `ESP32-P4`
- Input and Output: Same as described in `esp_gmf_aec`
- Unified Event Interface: Notifies events (wake-up start/end, VAD state changes, command word triggers) via callback functions
- State Machine Management: Automatically maintains transitions between wake-up, speech, and silence states

### System Diagram

  ```mermaid
  graph TD
    subgraph Audio Preprocessing
      RAW[Audio Data]
    end

    subgraph GMF_AFE
        AFE_MGR[AFE Manager<br>esp_gmf_afe_manager]
        INPUT_RB[Input Buffer]
        OUTPUT_RB[Output Buffer]
        STATE_MACHINE[Wake-Up/VAD State Machine]
        MN_DET[Command Word Detection]
        DELAY[Delay Mechanism]
    end

    subgraph Application Layer
      EVENT_CB[Event Callback]
      OUT_PORT[Data Output]
    end

    RAW -->|Processed Audio Data| INPUT_RB
    INPUT_RB -->|Data Callback| AFE_MGR
    AFE_MGR -->|Result Callback<br>Data| OUTPUT_RB
    AFE_MGR -->|Result Callback<br>Wake-Up/VAD Events| STATE_MACHINE
    AFE_MGR -->|Result Callback<br>Data| MN_DET
    OUTPUT_RB --> DELAY --> OUT_PORT
    STATE_MACHINE -->|Event Notification| EVENT_CB
    MN_DET -->|Event Notification| EVENT_CB
  ```

### `esp_gmf_afe_cfg_t` Advanced Configuration Parameters

  | Parameter | Description | Default Value |
  | --- | --- | --- |
  | **wakeup_time**   | Time in milliseconds to trigger wake-up end event if no voice activity is detected after enabling wake-up | 10000 (ms) |
  | **wakeup_end**    | Time in milliseconds to trigger wake-up end event after detecting silence following voice activity | 2000 (ms) |
  | **vcmd_timeout**  | Timeout for command word detection. After timeout, the interface must be called again to start a new detection round | 5760 (ms) |
  | **delay_samples** | Output data delay to compensate for VAD detection lag. The specified data length (in samples) should be greater than or equal to `afe_config_t.vad_min_speech_ms` configured during AFE manager initialization | 2048 (samples) |

### Wake Word and VAD State Machine

The following illustrates state transitions when features are enabled. The `/` character indicates the triggered user event

- **Enable Wake Word Detection, Disable VAD**

  This scenario focuses on detecting wake words and can be configured (`wakeup_time`) to trigger an end event after a certain period

  Modify the configuration in the example [wwe](./examples/wwe/main/main.c) to use this scenario

  ```c
  #define WAKENET_ENABLE (true)
  #define VAD_ENABLE     (false)
  ```

  ```mermaid
  stateDiagram-v2
      [*] --> IDLE

      IDLE --> WAKEUP: Wake Word / (WAKEUP_START)

      WAKEUP --> IDLE: Timer (wakeup_time) Timeout / (WAKEUP_END)
  ```

- **Enable VAD, Disable Wake Word Detection**

  This scenario focuses on detecting voice activity and triggers corresponding events at the start and end of speech

  Modify the configuration in the example [wwe](./examples/wwe/main/main.c) to use this scenario

  ```c
  #define WAKENET_ENABLE (false)
  #define VAD_ENABLE     (true)
  ```

  ```mermaid
  stateDiagram-v2
      [*] --> IDLE

      IDLE --> SPEECHING: Voice Detected / (VAD_START)

      SPEECHING --> IDLE: Noise or Silence Detected / (VAD_END)
  ```

- **Enable Both Wake Word Detection and VAD**

  This scenario combines wake word detection with voice activity detection to avoid frequent voice activity events outside the wake word interval

  Modify the configuration in the example [wwe](./examples/wwe/main/main.c) to use this scenario

  ```c
  #define WAKENET_ENABLE (true)
  #define VAD_ENABLE     (true)
  ```

  ```mermaid
  stateDiagram-v2
      [*] --> IDLE

      IDLE --> WAKEUP: Wake Word / (WAKEUP_START)

      WAKEUP --> SPEECHING: Voice Detected / (VAD_START)
      WAKEUP --> IDLE: Timer (wakeup_time) Timeout / (WAKEUP_END)

      SPEECHING --> WAIT_FOR_SLEEP: Noise or Silence Detected / (VAD_END)

      WAIT_FOR_SLEEP --> SPEECHING: Voice Detected / (VAD_START)
      WAIT_FOR_SLEEP --> IDLE: Timer (wakeup_end) Timeout / (WAKEUP_END)
  ```

### Command Word Detection

Users need to decide when to start command word detection. A typical use case is to enable detection after the state machine pushes a (WAKEUP_START) event and determine the next operation based on the detected command word index in the callback function

- Command word detection is independent of the wake word state machine
- Command word detection supports continuous detection until timeout

## Usage

For example code, refer to the [examples](./examples) folder for the Wake Word Detection and Audio Echo Cancellation demos.
