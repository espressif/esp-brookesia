# ESP RainMaker App — User Guide (English)

> 简体中文：[USER_GUIDE_CN.md](USER_GUIDE_CN.md)

This guide describes the **ESP RainMaker**-style smart home app running on the **ESP-Brookesia** phone framework (component `brookesia_app_rainmaker`). The UI is built with LVGL and works with your **ESP RainMaker** account and home data (devices, schedules, scenes, etc.), subject to firmware and backend configuration.

## Switching pages (horizontal swipe)

After you sign in, you move between the four main tabs—**Home**, **Schedules**, **Scenes**, and **My Profile**—mainly by **swiping left or right** on the screen. You can also use the **bottom navigation bar** (revealed from the bottom edge of the screen, where supported).

The main screens are arranged **left to right** in this fixed order:

1. **Home** — device grid and home overview
2. **Schedules** — timers / schedules
3. **Scenes**
4. **My Profile**

- **Swipe left**: go to the **next** screen on the right (e.g. Home → Schedules → Scenes → My Profile). From **My Profile**, another swipe left wraps back to **Home**.
- **Swipe right**: go to the **previous** screen on the left (e.g. from **Home**, swipe right to **My Profile**; from Schedules, swipe right back to Home).

**Sub-screens** (device details, create/edit schedule or scene, etc.) are **not** part of this four-page loop; use **Back** (or similar) to return to the previous level.

---

### 1. Sign-in

- On first launch you see the **Sign-in** screen.
- Enter your RainMaker **email** and **password**; tapping a field opens the **on-screen keyboard**.
- Tap **Sign in** (or the equivalent login button). A busy indicator may appear while signing in; on success you land on **Home** and device data is loaded.
- On failure, an error dialog is shown—check network, credentials, or backend status.
- A **language** shortcut may be available on the login screen (if enabled in your build).

### 2. Home

- **Header**: Shows the current home or group name; a **“Home >”**-style control may let you pick another home (if supported by your product).
- **Welcome & common actions**: Welcome text; **Refresh** pulls the latest home and device list from the cloud (a loading state may appear).
- **Device grid**: Cards for devices in the current home. Each card typically shows an icon, name, and sometimes an **inline power / toggle** control (depends on device type and capabilities).
- **Open device details**: Tap the card (outside the switch, if any) to open **light / switch / fan** detail screens for brightness, color temperature, speed, etc. (depends on the device model).

### 3. Schedules (Timers)

- **Schedules** lists all **schedules** (timed automations) for the current home.
- **Enable / disable**: Use the **switch** on each row to enable or disable that schedule in the cloud; the list and home view update after the backend refresh.
- **Create**: Tap **Add** (“+”), then follow the flow: name, time, repeat days, **actions** (e.g. control a device), then **Save**.
- **Edit**: **Long-press** a schedule **card** to open the **edit schedule** screen; change fields and save.
- **Delete**: Enter **Edit** mode on the screen, then use the **delete** control on each card (icons/layout as shown in UI).

### 4. Scenes

- **Scenes** lists **scenes** (preset groups of device actions).
- **Run a scene**: Tap the **activate / run** control on the card (wording/icons as in UI) to execute that scene in the cloud.
- **Create**: Tap **Add**, configure the scene name and actions, then **Save**.
- **Edit**: **Long-press** a scene **card** to open **edit scene**.
- **Delete**: In **Edit** mode, use the **delete** control on the scene card.

### 5. My Profile

- **My Profile** shows an **avatar placeholder** (often the first letter of your email) and your **email** address.
- **Important — interaction model**: **Menu rows**, the **top-right settings** icon, and **Log out** respond to **long-press only**; a **short tap does nothing**. This reduces accidental taps when **swiping horizontally** to change pages.
  - **Language**: Long-press the **Language** row to open the language picker.
  - **Notification center / Privacy policy / Terms of use** (or similar rows): Long-press may show an **informational dialog** (placeholder or extended in your firmware).
  - **Top-right settings**: Long-press opens **Settings** messaging (placeholder where applicable).
  - **Log out**: Long-press the **Log out** area to sign out; on success you return to the **Sign-in** screen.

---

*Tab order matches the source: `UI_NAV_TAB_HOME` → `SCHEDULES` → `SCENES` → `USER` in `ui_nav.h`; swipe behavior in `ui_gesture_nav.c`.*

*For developer build/run instructions, see [README.md](README.md), [README_CN.md](README_CN.md), [pc/README.md](pc/README.md), and [pc/README_CN.md](pc/README_CN.md).*
