<script setup lang="ts">
import { reactive, ref, watch, onMounted } from 'vue'
import { useScreenStore } from '../stores/store'
import { useConfigStore } from '../stores/configStore'
import { getItem, setItem } from '../utils/storage'
import FuncBox from '../components/FuncBox.vue'
import { XBox } from 'ilx1-x-box'

const win = window as any
const screenStore = useScreenStore()
const configStore = useConfigStore()

const curLattice = ref(0)
const curMode = ref(0)
const curDirection = ref(0)
const curDecimal = ref(0)

onMounted(() => {
  const tempArray = JSON.parse(getItem('configArray'))
  if (tempArray != null) {
    curLattice.value = tempArray[0]
    curMode.value = tempArray[1]
    curDirection.value = tempArray[2]
    curDecimal.value = tempArray[3]
    setLatticeFormat(tempArray[0])
    setModeMethod(tempArray[1])
    setModeDirection(tempArray[2])
    setOutputDecimal(tempArray[3])
  }
})

// --- 图片导入 ---
const fileInput = ref<HTMLInputElement | null>(null)
const triggerImport = () => fileInput.value?.click()

const onFileSelected = (e: Event) => {
  const target = e.target as HTMLInputElement
  const file = target.files?.[0]
  if (!file) return
  const reader = new FileReader()
  reader.onload = () => {
    const base64 = reader.result as string
    configStore.screenData.baseData = base64
    screenStore.setEiditorPicData(base64)
    setItem('screenDataBase64', base64)
    XBox.popMes('图片导入成功！')
  }
  reader.onerror = () => XBox.popMes('图片读取失败！')
  reader.readAsDataURL(file)
}

const openEditor = () => {
  win.api.createNewWindow({ route: '/screen', windowName: 'screen', modal: true })
}

// --- 尺寸设置 ---
const picSizeData = reactive({ width: '128', height: '160' })

const commonSizes = [
  { label: '0.96″', w: 128, h: 64 },
  { label: '1.3″', w: 240, h: 240 },
  { label: '1.44″', w: 128, h: 128 },
  { label: '1.8″', w: 128, h: 160 },
  { label: '2.0″', w: 240, h: 320 },
  { label: '2.4″', w: 240, h: 320 },
  { label: '2.8″', w: 240, h: 320 },
]

const setCommonSize = (w: number, h: number) => {
  picSizeData.width = String(w)
  picSizeData.height = String(h)
}

watch(() => screenStore.configArray[4], () => {
  if (screenStore.configArray[4]) {
    curLattice.value = 1; curMode.value = 2; curDirection.value = 0; curDecimal.value = 0
    setLatticeFormat(1); setModeMethod(2); setModeDirection(0); setOutputDecimal(0)
  } else {
    curLattice.value = 1; curMode.value = 0; curDirection.value = 0; curDecimal.value = 0
    setLatticeFormat(1); setModeMethod(0); setModeDirection(0); setOutputDecimal(0)
  }
})

watch(picSizeData, () => {
  screenStore.resizeWidth = parseInt(picSizeData.width) || 0
  screenStore.resizeHeight = parseInt(picSizeData.height) || 0
}, { deep: true, immediate: true })

// --- 取模设置 ---
const setLatticeFormat = (k: number) => { curLattice.value = k; screenStore.setConfigArray(0, k) }
const setModeMethod = (k: number) => {
  if (!screenStore.configArray[4] && k !== 0) XBox.popMes('彩色取模仅支持逐行式')
  curMode.value = k; screenStore.setConfigArray(1, k)
}
const setModeDirection = (k: number) => { curDirection.value = k; screenStore.setConfigArray(2, k) }
const setOutputDecimal = (k: number) => { curDecimal.value = k; screenStore.setConfigArray(3, k) }

watch(screenStore.configArray, () => {
  setItem('configArray', JSON.stringify(screenStore.configArray))
}, { deep: true, immediate: true })
</script>

<template>
  <div class="config-panel">
    <input ref="fileInput" type="file" accept="image/*" hidden @change="onFileSelected" />

    <!-- 操作按钮 -->
    <div class="action-row">
      <button class="btn btn-import" @click="triggerImport">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
        导入图片
      </button>
      <button class="btn btn-edit" @click="openEditor">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/><path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/></svg>
        编辑文字
      </button>
    </div>

    <!-- 功能切换 -->
    <FuncBox />

    <!-- 尺寸设置 -->
    <div class="section">
      <div class="section-label">屏幕尺寸</div>
      <div class="size-row">
        <div class="input-wrap">
          <input type="text" class="size-input" v-model="picSizeData.width" />
          <span class="input-hint">W</span>
        </div>
        <span class="multiply">×</span>
        <div class="input-wrap">
          <input type="text" class="size-input" v-model="picSizeData.height" />
          <span class="input-hint">H</span>
        </div>
      </div>
      <div class="preset-chips">
        <button
          v-for="s in commonSizes" :key="s.label"
          :class="['chip', { active: +picSizeData.width === s.w && +picSizeData.height === s.h }]"
          @click="setCommonSize(s.w, s.h)"
        >{{ s.label }} <span class="chip-res">{{ s.w }}×{{ s.h }}</span></button>
      </div>
    </div>

    <!-- 取模设置 -->
    <div class="section">
      <div class="section-label">取模参数</div>

      <div class="config-row">
        <span class="config-key">点阵格式</span>
        <div class="toggle-group">
          <button :class="['toggle', { active: curLattice === 0 }]" @click="setLatticeFormat(0)">阳码</button>
          <button :class="['toggle', { active: curLattice === 1 }]" @click="setLatticeFormat(1)">阴码</button>
        </div>
      </div>

      <div class="config-row">
        <span class="config-key">取模方式</span>
        <div class="toggle-group">
          <button :class="['toggle', { active: curMode === 0 }]" @click="setModeMethod(0)">逐行式</button>
          <button :class="['toggle', { active: curMode === 1 }]" @click="setModeMethod(1)">逐列式</button>
          <button :class="['toggle', { active: curMode === 2 }]" @click="setModeMethod(2)">列行式</button>
          <button :class="['toggle', { active: curMode === 3 }]" @click="setModeMethod(3)">行列式</button>
        </div>
      </div>

      <div class="config-row">
        <span class="config-key">取模走向</span>
        <div class="toggle-group">
          <button :class="['toggle', { active: curDirection === 0 }]" @click="setModeDirection(0)">逆向</button>
          <button :class="['toggle', { active: curDirection === 1 }]" @click="setModeDirection(1)">顺向</button>
        </div>
      </div>

      <div class="config-row">
        <span class="config-key">输出进制</span>
        <div class="toggle-group">
          <button :class="['toggle', { active: curDecimal === 0 }]" @click="setOutputDecimal(0)">HEX</button>
          <button :class="['toggle', { active: curDecimal === 1 }]" @click="setOutputDecimal(1)">DEC</button>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped lang="scss">
.config-panel {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  gap: 14px;
  padding: 12px;
  box-sizing: border-box;
  font-family: 'ceyy', -apple-system, BlinkMacSystemFont, sans-serif;
  overflow-y: auto;
}

// --- Buttons ---
.action-row {
  display: flex;
  gap: 10px;

  .btn {
    flex: 1;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
    padding: 10px 0;
    border: none;
    border-radius: 8px;
    font-size: 13px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.15s;
    font-family: inherit;
    letter-spacing: 0.3px;

    svg { flex-shrink: 0; }
    &:active { transform: scale(0.97); }
  }
  .btn-import {
    background: #3b82f6;
    color: #fff;
    &:hover { background: #2563eb; }
  }
  .btn-edit {
    background: #f1f5f9;
    color: #475569;
    border: 1px solid #e2e8f0;
    &:hover { background: #e2e8f0; }
  }
}

// --- Section ---
.section {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.section-label {
  font-size: 11px;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.8px;
  color: #94a3b8;
  padding-left: 2px;
}

// --- Size ---
.size-row {
  display: flex;
  align-items: center;
  gap: 8px;

  .input-wrap {
    flex: 1;
    position: relative;
  }
  .size-input {
    width: 100%;
    padding: 8px 24px 8px 8px;
    border: 1px solid #e2e8f0;
    border-radius: 8px;
    font-size: 16px;
    font-family: inherit;
    text-align: center;
    background: #f8fafc;
    color: #1e293b;
    outline: none;
    transition: all 0.15s;
    box-sizing: border-box;
    &:focus {
      border-color: #3b82f6;
      box-shadow: 0 0 0 3px rgba(59,130,246,0.1);
    }
  }
  .input-hint {
    position: absolute;
    right: 8px;
    top: 50%;
    transform: translateY(-50%);
    font-size: 10px;
    font-weight: 600;
    color: #94a3b8;
    pointer-events: none;
  }
  .multiply {
    font-size: 18px;
    color: #94a3b8;
    font-weight: 300;
  }
}

.preset-chips {
  display: flex;
  flex-wrap: wrap;
  gap: 5px;

  .chip {
    padding: 4px 8px;
    border: 1px solid #e2e8f0;
    border-radius: 6px;
    background: #f8fafc;
    color: #64748b;
    font-size: 11px;
    cursor: pointer;
    transition: all 0.15s;
    font-family: inherit;
    white-space: nowrap;

    .chip-res { color: #94a3b8; margin-left: 2px; font-size: 10px; }
    &:hover { border-color: #3b82f6; }
    &.active {
      background: #3b82f6;
      color: #fff;
      border-color: #3b82f6;
      .chip-res { color: rgba(255,255,255,0.7); }
    }
  }
}

// --- Config Rows ---
.config-row {
  display: flex;
  align-items: center;
  gap: 10px;
}

.config-key {
  font-size: 12px;
  font-weight: 600;
  color: #475569;
  flex-shrink: 0;
  width: 56px;
}

.toggle-group {
  display: flex;
  flex: 1;
  gap: 4px;

  .toggle {
    flex: 1;
    padding: 6px 4px;
    border: 1px solid #e2e8f0;
    border-radius: 6px;
    background: #f8fafc;
    color: #64748b;
    font-size: 12px;
    cursor: pointer;
    transition: all 0.15s;
    font-family: inherit;
    white-space: nowrap;

    &:hover { border-color: #93c5fd; }
    &.active {
      background: #3b82f6;
      color: #fff;
      border-color: #3b82f6;
      font-weight: 600;
    }
  }
}
</style>
