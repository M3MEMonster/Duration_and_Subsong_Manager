# Duration & Subsong Manager

A [foobar2000](https://www.foobar2000.org/) component that lets you override the length of any track, hide individual subsongs from the Media Library, and read playback time at millisecond precision.

**Language:** English | [中文](#中文)


## Features

### Duration Manager — custom track length
- Set a custom length for any track via the context menu **"Set custom duration"**.
- Manage all overrides in **Preferences → Tools → Duration & Subsong Manager → Duration Manager** (search, edit, delete).
- Overrides are matched by file **content hash + subsong index**, so they survive file moves/renames.
- During playback the track is automatically truncated to the custom length (jumps to the next track when reached).

### Subsong Manager — hide subsongs
- For multi-subsong files, choose exactly which subsongs are visible in the autoplaylists.
- Configure include/exclude ranges (e.g. `1-3,5,7`) or checkbox in **Preferences → Tools → Duration & Subsong Manager → Subsong Manager**.
- Note: this filters the Media Library query at runtime, so changes take effect after a restart.

### High-Precision Timer (UI element)
- An auxiliary UI element for the Duration Manager that shows playback position and track length in millisecond precision: `[HH:]MM:SS.mmm`.
- Add it via the layout editor like any other UI element ("High-Precision Timer").

## Compatibility
- foobar2000 **v2.0 or newer**, Windows, 32-bit and 64-bit.
- Built against the foobar2000 SDK (2025-03-07).

## Installation
1. Download the latest `foo_duration_subsong_manager.fb2k-component` from the [Releases](https://github.com/M3MEMonster/Duration_and_Subsong_Manager/releases) page.
2. In foobar2000: **Preferences → Components → Install...** (or just drag the file onto the window), then **Apply** and restart.

## Building from source

### Prerequisites
- Visual Studio 2022 with the **Desktop development with C++** workload (and a Windows 10/11 SDK).
- The official **foobar2000 SDK (2025-03-07 or compatible)**.
- **WTL 10** (Windows Template Library) headers.

### Directory layout
The project files reference the SDK with relative paths (`..\..\..\Foobar2000SDK-2025\...`). Place the SDK so this repo sits **three directory levels below** the folder that contains `Foobar2000SDK-2025`:

```
<root>\
├─ Foobar2000SDK-2025\
│  ├─ foobar2000\
│  │  ├─ SDK\
│  │  ├─ helpers\
│  │  ├─ shared\
│  │  └─ foobar2000_component_client\
│  ├─ libPPUI\
│  ├─ pfc\
│  └─ WTL10_01_Release\Include\        <- WTL goes here
└─ <dir1>\<dir2>\Duration_and_Subsong_Manager\   <- this repo (3 levels below <root>)
```

If your layout differs, update these entries in `DurationAndSubsongManager.vcxproj`:
- `AdditionalIncludeDirectories` (SDK + WTL include paths)
- `AdditionalDependencies` (`shared-$(Platform).lib`)
- the five `ProjectReference` paths (pfc, libPPUI, foobar2000_SDK, foobar2000_sdk_helpers, foobar2000_component_client)

### Build steps
1. Open `Foo_Duration_Subsong_Manager.sln`.
2. Build **Release | Win32** and **Release | x64** (both are required to package).
3. The Release|x64 post-build step runs `fb2k_component_pack.ps1`, producing `dist\foo_duration_subsong_manager.fb2k-component` (x86 DLL in root, x64 DLL under `x64\`).

> Source files are compiled with `/utf-8`; keep that flag if you add non-ASCII comments.

## How it works (overview)
The custom-duration feature is upheld by three independent mechanisms:
- `duration_database.cpp` `refresh_metadb` — writes the length into the metadb via hints (on load / edit).
- `info_filter.cpp` — re-applies the length whenever foobar2000 re-reads file info.
- `playback_monitor.cpp` — truncates playback at the custom length.

Subsong hiding (`subsong_hider.cpp`) uses MinHook to intercept the Media Library's sqlite query and drop hidden subsong rows.

## Third-party notices
This component statically links **MinHook**, distributed under the BSD-2-Clause license:

> MinHook - The Minimalistic API Hooking Library for x64/x86
> Copyright (C) 2009-2017 Tsuda Kageyu. All rights reserved.
> Portions Copyright (c) 2008-2009 Vyacheslav Patkov (Hacker Disassembler Engine).
>
> Redistribution and use in source and binary forms, with or without modification,
> are permitted provided that the conditions of the BSD-2-Clause license are met.
> THIS SOFTWARE IS PROVIDED "AS IS" ...

Full text and disclaimer: [minhook_lib/LICENSE.txt](minhook_lib/LICENSE.txt) and [minhook_lib/AUTHORS.txt](minhook_lib/AUTHORS.txt).

## License
Released under the [MIT License](LICENSE).

---

## 中文

一个 [foobar2000](https://www.foobar2000.org/) 插件：可以自定义任意音轨的时长、把多子歌曲文件中的某些子歌曲从autoplaylist中隐藏，并以毫秒精度显示播放时间。


**语言：** [English](#duration--subsong-manager) | 中文

### 功能

#### 时长管理器（Duration Manager）—— 自定义音轨时长
- 通过右键菜单 **“Set custom duration”** 为任意音轨设置自定义时长。
- 在 **Preferences → Tools → Duration & Subsong Manager → Duration Manager** 中统一管理（搜索、编辑、删除）。
- 记录以文件**内容哈希 + 子歌曲索引**为键匹配，因此文件移动 / 改名后依然有效。
- 播放时会按自定义时长自动截断。

#### 子歌曲管理器（Subsong Manager）—— 隐藏子歌曲
- 对于含多个子歌曲的文件，可精确选择哪些子歌曲显示在autoplaylist中。
- 在 **Preferences → Tools → Duration & Subsong Manager → Subsong Manager** 中以区间方式（例如 `1-3,5,7`）或勾选框配置。
- 注意：该功能在运行时过滤媒体库查询，改动需要重启后才生效。

#### 高精度计时器（High-Precision Timer，UI 元素）
- 作为时长管理器的辅助 UI 元素，以毫秒精度显示播放位置和音轨长度：`[HH:]MM:SS.mmm`，方便核对精确时长。
- 像其他 UI 元素一样在布局编辑器中添加（名称 “High-Precision Timer”）。

### 兼容性
- foobar2000 **v2.0 或更新版本**，Windows，同时提供 32 位与 64 位。
- 基于 foobar2000 SDK（2025-03-07）构建。

### 安装
1. 从 [Releases](https://github.com/M3MEMonster/Duration_and_Subsong_Manager/releases) 页面下载最新的 `foo_duration_subsong_manager.fb2k-component`。
2. 在 foobar2000 中：**首选项 → Components → Install...**（或直接把文件拖到窗口里），然后 **Apply** 并重启。

### 从源码构建

#### 前置条件
- Visual Studio 2022，安装 **“使用 C++ 的桌面开发”** 工作负载（以及 Windows 10/11 SDK）。
- 官方 **foobar2000 SDK（2025-03-07 或兼容版本）**。
- **WTL 10**（Windows Template Library）头文件。

#### 目录布局
工程文件以相对路径引用 SDK（`..\..\..\Foobar2000SDK-2025\...`）。请把 SDK 放好，使本仓库位于「包含 `Foobar2000SDK-2025` 的那个文件夹」**往下数第三层**：

```
<root>\
├─ Foobar2000SDK-2025\
│  ├─ foobar2000\
│  │  ├─ SDK\
│  │  ├─ helpers\
│  │  ├─ shared\
│  │  └─ foobar2000_component_client\
│  ├─ libPPUI\
│  ├─ pfc\
│  └─ WTL10_01_Release\Include\        <- WTL 放这里
└─ <dir1>\<dir2>\Duration_and_Subsong_Manager\   <- 本仓库（位于 <root> 下第三层）
```

如果你的目录结构不同，请修改 `DurationAndSubsongManager.vcxproj` 中以下条目：
- `AdditionalIncludeDirectories`（SDK 与 WTL 的 include 路径）
- `AdditionalDependencies`（`shared-$(Platform).lib`）
- 五个 `ProjectReference` 路径（pfc、libPPUI、foobar2000_SDK、foobar2000_sdk_helpers、foobar2000_component_client）

#### 构建步骤
1. 打开 `Foo_Duration_Subsong_Manager.sln`。
2. 分别构建 **Release | Win32** 和 **Release | x64**（打包需要两者都构建）。
3. Release|x64 的生成后事件会运行 `fb2k_component_pack.ps1`，输出 `dist\foo_duration_subsong_manager.fb2k-component`（x86 DLL 在根目录，x64 DLL 在 `x64\` 子目录）。

> 源文件以 `/utf-8` 编译；如果你要添加非 ASCII（如中文）注释，请保留该编译选项。

### 实现概览
自定义时长功能由三条相互独立的机制共同保证：
- `duration_database.cpp` 的 `refresh_metadb` —— 在加载 / 编辑时通过 metadb hints 把时长写入元数据库。
- `info_filter.cpp` —— 当 foobar2000 重新读取文件信息时再次覆盖时长。
- `playback_monitor.cpp` —— 播放时监控位置，到达自定义时长就切下一首，实现截断。

子歌曲隐藏（`subsong_hider.cpp`）使用 MinHook 拦截媒体库的 sqlite 查询，丢弃被隐藏的子歌曲行。

### 第三方声明
本组件静态链接了 **MinHook**，其以 BSD-2-Clause 许可证分发：

> MinHook - The Minimalistic API Hooking Library for x64/x86
> Copyright (C) 2009-2017 Tsuda Kageyu. All rights reserved.
> Portions Copyright (c) 2008-2009 Vyacheslav Patkov (Hacker Disassembler Engine).
>
> Redistribution and use in source and binary forms, with or without modification,
> are permitted provided that the conditions of the BSD-2-Clause license are met.
> THIS SOFTWARE IS PROVIDED "AS IS" ...

完整文本与免责条款见：[minhook_lib/LICENSE.txt](minhook_lib/LICENSE.txt) 与 [minhook_lib/AUTHORS.txt](minhook_lib/AUTHORS.txt)。

### 许可证
本项目以 [MIT 许可证](LICENSE) 发布。
