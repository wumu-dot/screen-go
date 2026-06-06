# Screen-GO 修复记录

原项目：[iLx11/screen-go](https://github.com/iLx11/screen-go)（MIT 协议）

## 修复的 Bug

### 1. 提交生成卡死

**原因**：CommitBox.vue 调用 `ilx1-x-tool` 库中的 `resizeImageWithKonva()` 和 `generate()`。该库在 Electron 环境下存在 Image 加载时序问题——`img.src` 在 `img.onload` 之前被赋值，数据 URL 的图片在 Chromium 中同步加载完毕时 `onload` 尚未绑定，导致 Canvas 的 `drawImage()` 收到无效对象而报错卡死。

**修复**：移除 `ilx1-x-tool` 依赖，直接用浏览器原生 Canvas API 实现图片缩放和取模（单色/彩色 RGB565）。函数在 `CommitBox.vue` 中内联实现。

### 2. 导入图片数据为文件路径而非图片数据

**原因**：ImageConfig.vue 的导入按钮调用 `win.api.getImgPath()`，该方法返回的是文件系统路径（如 `C:\Users\...\image.png`），而非 base64 图片数据。后续 Canvas API 无法从路径加载图片。

**修复**：改用隐藏的 `<input type="file">` + `FileReader.readAsDataURL()` 在渲染进程直接读取图片为 base64 数据。

### 3. 编辑器确认后图片数据未能传回主窗口

**原因**：ScreenEditor 窗口通过 Electron IPC（`store-set`）将图片数据发往主窗口，主窗口的 `storeSetter` 通过 `setMixedPathValue` 逐层穿透 reactive 对象时，未能正确触发 Pinia 的响应式更新。

**修复**：编辑器确认时同时将图片写入 `localStorage`。主窗口通过 `setInterval` 轮询 `localStorage` 作为兜底，确保数据不丢失。

### 4. CommitBox 取模参数读取错误的 Store

**原因**：CommitBox.vue 从 `configStore.screenConfig` 读取取模参数（尺寸、配置数组、阈值），但界面 ImageConfig.vue 实际写入的是 `screenStore`（不同的 Pinia Store）。`configStore.screenConfig` 永远是初始值 0。

**修复**：CommitBox.vue 改为从 `screenStore` 读取所有取模参数，与 ImageConfig.vue 保持一致。

### 5. 选项点击无视觉反馈

**原因**：原版通过 `<div>` 的绝对定位和 `left` 属性实现选中滑块的动画效果，依赖 z-index 和 CSS 过渡。在实际运行时滑块的 z-index 被其他元素遮挡，用户看不到选中状态变化。

**修复**：移除滑块动画方案，改为直接给选中项添加 `.active` 类（蓝色背景 + 白色文字），简单可靠。

### 6. 提交生成无错误处理

**原因**：`imgHandle()` 函数在 `setWaitExecute(true)` 后直接 `await` 异步操作，如果中间抛出异常，`setWaitExecute(false)` 永远不会执行，导致全屏遮罩层不消失，应用假死。

**修复**：添加 `try-catch` 包裹异步操作，`finally` 中恢复状态，失败时弹窗显示具体错误信息。
