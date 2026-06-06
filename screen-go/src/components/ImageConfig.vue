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

// 当前选中状态（用于高亮）
const curLattice = ref(0)    // 0=阳码 1=阴码
const curMode = ref(0)       // 0=逐行式 1=逐列式 2=列行式 3=行列式
const curDirection = ref(0)  // 0=逆向 1=顺向
const curDecimal = ref(0)    // 0=十六进制 1=十进制

onMounted(() => {
  let tempArray = JSON.parse(getItem('configArray'))
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
  let presetArray = JSON.parse(getItem('presetArray'))
  if (presetArray != null)
    presetArray.forEach((element: any, index: number) => {
      preSize[index] = element
    })
})

// -------------------------------- 图片导入 ---------------------------------
const fileInput = ref<HTMLInputElement | null>(null)

const triggerImport = () => {
  fileInput.value?.click()
}

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
  reader.onerror = () => {
    XBox.popMes('图片读取失败！')
  }
  reader.readAsDataURL(file)
}

const openEditor = () => {
  win.api.createNewWindow({
    route: '/screen',
    windowName: 'screen',
    modal: true,
  })
}

// -------------------------------- 图片设置 ---------------------------------
const resizeText = ref<string>('图片缩放预览')
const picSizeData = reactive({
  width: '128',
  height: '160',
})

watch(
  () => screenStore.configArray[4],
  () => {
    if (screenStore.configArray[4]) {
      curLattice.value = 1
      curMode.value = 2
      curDirection.value = 0
      curDecimal.value = 0
      setLatticeFormat(1)
      setModeMethod(2)
      setModeDirection(0)
      setOutputDecimal(0)
    } else {
      curLattice.value = 1
      curMode.value = 0
      curDirection.value = 0
      curDecimal.value = 0
      setLatticeFormat(1)
      setModeMethod(0)
      setModeDirection(0)
      setOutputDecimal(0)
    }
  }
)

watch(
  () => screenStore.isCroped,
  () => {
    picSizeData.width = screenStore.resizeWidth.toString()
    picSizeData.height = screenStore.resizeHeight.toString()
  },
  { deep: true, immediate: true }
)

watch(
  picSizeData,
  () => {
    screenStore.resizeWidth = parseInt(picSizeData.width) || 0
    screenStore.resizeHeight = parseInt(picSizeData.height) || 0
  },
  { deep: true }
)

const preSize: object[] = reactive([{}, {}, {}])

watch(
  () => screenStore.isCountModify,
  () => {
    if (screenStore.isCountModify == true) {
      screenStore.setCountModify(false)
      preSize[screenStore.preSizeCount] = {
        width: picSizeData.width,
        height: picSizeData.height,
      }
    }
  },
  { immediate: true }
)

const setPreSize = (v: any) => {
  picSizeData.width = v['width']
  picSizeData.height = v['height']
}

// 获取图片数据（兼容多种来源）
const getBaseData = () => {
  return configStore.screenData.baseData
    || getItem('screenDataBase64')
    || screenStore.editorPicData
    || ''
}

const resizePic = async () => {
  const baseData = getBaseData()
  if (!baseData) {
    XBox.popMes('请先导入一张图片！')
    return
  }
  if (!picSizeData.width || !picSizeData.height ||
      picSizeData.width == '0' || picSizeData.height == '0') {
    XBox.popMes('请正确设置图片大小!')
    return
  }

  if (screenStore.isResized) {
    screenStore.setResized(false)
    resizeText.value = '图片缩放预览'
    return
  }

  screenStore.setWaitExecute(true)
  try {
    const data = await win.api.resizeImage(
      parseInt(picSizeData.width),
      parseInt(picSizeData.height),
      baseData,
      screenStore.configArray[4]
    )
    screenStore.setResizePicData(data)
    screenStore.setResized(true)
    resizeText.value = '返回原始图片'
    XBox.popMes('缩放完成')
  } catch (e) {
    XBox.popMes('缩放失败，请重试')
  }
  screenStore.setWaitExecute(false)
}

// -------------------------------- 取模设置 ---------------------------------
const setLatticeFormat = (k: number) => {
  curLattice.value = k
  screenStore.setConfigArray(0, k)
}
const setModeMethod = (k: number) => {
  if (!screenStore.configArray[4] && k != 0)
    XBox.popMes('目前彩色取模只支持逐行哦！')
  curMode.value = k
  screenStore.setConfigArray(1, k)
}
const setModeDirection = (k: number) => {
  curDirection.value = k
  screenStore.setConfigArray(2, k)
}
const setOutputDecimal = (k: number) => {
  curDecimal.value = k
  screenStore.setConfigArray(3, k)
}

watch(
  screenStore.configArray,
  () => {
    setItem('configArray', JSON.stringify(screenStore.configArray))
  },
  { deep: true, immediate: true }
)
</script>

<template>
  <div id="config-content">
    <div id="img-size-config">
      <!-- 隐藏的文件选择器 -->
      <input ref="fileInput" type="file" accept="image/*" style="display:none" @change="onFileSelected" />
      <!-- 导入和选择按钮 -->
      <div id="import-btns">
        <div class="import-btn import-btn-primary" @click="openEditor">选择图片</div>
        <div class="import-btn import-btn-secondary" @click="triggerImport">导入图片</div>
      </div>
      <div id="func-box">
        <FuncBox />
      </div>
      <div id="resize-config-box">
        <div class="input-group">
          <input type="text" class="input" v-model="picSizeData.width" />
          <label class="user-label">宽度:</label>
        </div>
        <div class="input-group">
          <input type="text" class="input" v-model="picSizeData.height" />
          <label class="user-label">高度:</label>
        </div>
      </div>
      <div id="size-choose-box">
        <div v-for="(v, k) in preSize" :key="k" @click="setPreSize(v)">
          {{ v['width'] || '--' }} - {{ v['height'] || '--' }}
        </div>
      </div>
      <div @click="resizePic">{{ resizeText }}</div>
    </div>
    <div id="img-data-config">
      <div class="div1">点阵<br />格式</div>
      <div class="div2">
        <div :class="{ active: curLattice === 0 }" @click="setLatticeFormat(0)">阳码</div>
        <div :class="{ active: curLattice === 1 }" @click="setLatticeFormat(1)">阴码</div>
      </div>
      <div class="div3">取模方式</div>
      <div class="div4">
        <div :class="{ active: curMode === 0 }" @click="setModeMethod(0)">逐行式</div>
        <div :class="{ active: curMode === 1 }" @click="setModeMethod(1)">逐列式</div>
        <div :class="{ active: curMode === 2 }" @click="setModeMethod(2)">列行式</div>
        <div :class="{ active: curMode === 3 }" @click="setModeMethod(3)">行列式</div>
      </div>
      <div class="div5">取模走向</div>
      <div class="div6">
        <div :class="{ active: curDirection === 0 }" @click="setModeDirection(0)">逆向</div>
        <div :class="{ active: curDirection === 1 }" @click="setModeDirection(1)">顺向</div>
      </div>
      <div class="div7">输出进制</div>
      <div class="div8">
        <div :class="{ active: curDecimal === 0 }" @click="setOutputDecimal(0)">十六进制</div>
        <div :class="{ active: curDecimal === 1 }" @click="setOutputDecimal(1)">十进制</div>
      </div>
    </div>
  </div>
</template>

<style scoped lang="scss">
#config-content {
  width: 100%;
  height: 100%;
  display: flex;
  flex-flow: row nowrap;
  justify-content: space-between;
  align-items: center;
  padding: 0.5em;
  border: none;

  > div {
    width: 49%;
    height: 100%;
    border-radius: 10px;
    border: none;
  }

  #img-size-config {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    grid-template-rows: repeat(6, 1fr);
    column-gap: 5px;
    row-gap: 5px;

    > div {
      border: none;
      background: var(--data-config-title-box-color);
    }

    #import-btns {
      grid-area: 1 / 1 / 2 / 4;
      display: flex;
      gap: 6px;
      padding: 4px;
      background: transparent;

      .import-btn {
        flex: 1;
        display: flex;
        justify-content: center;
        align-items: center;
        border-radius: 8px;
        font-size: 14px;
        cursor: pointer;
        color: white;
        font-weight: 600;
        transition: all 0.2s;
      }
      .import-btn-primary {
        background: #4a90d9;
        &:hover { background: #357abd; }
      }
      .import-btn-secondary {
        background: #5c9b5c;
        &:hover { background: #4a8a4a; }
      }
    }

    #func-box {
      grid-area: 2 / 1 / 6 / 2;
      display: flex;
      flex-flow: column wrap;
      justify-content: center;
      align-items: center;
      color: var(--text-color-2);
    }

    #resize-config-box {
      width: 100%;
      height: 100%;
      grid-area: 2 / 2 / 4 / 4;
      display: flex;
      flex-flow: column nowrap;
      justify-content: space-around;
      align-items: center;
      padding: 0.5em;
      background: var(--data-config-input-color);

      > div { border: none; overflow: visible; }

      .input-group {
        width: 100%;
        height: 42%;
        position: relative;
        display: flex;
        justify-content: center;
        align-items: center;
        color: var(--text-color-2);
        font-weight: 900;
        padding: 0.1em;
        box-shadow: 0 0 0 2px rgba(255, 255, 255, 0.8);

        .input {
          width: 100%;
          height: 100%;
          border-radius: 15px;
          display: block;
          border: none;
          font-size: 18px;
          color: var(--text-color-1);
          font-family: 'ceyy';
          text-align: center;
        }
        .user-label {
          width: auto;
          height: auto;
          position: absolute;
          font-size: 12px;
          left: 6%;
          top: 50%;
          transform: translateY(-50%);
          transition: all 0.3s ease-in-out;
        }
        .input:focus + label {
          left: 50%;
          top: 7%;
          font-size: 12px;
          transform: translate(-50%, -50%);
        }
      }
    }

    #size-choose-box {
      grid-area: 4 / 2 / 6 / 4;
      display: flex;
      flex-flow: column nowrap;
      justify-content: space-between;
      align-items: center;
      padding: 0.66em;
      box-shadow: 0 0 0 2px rgba(255, 255, 255, 0.8);

      > div {
        width: 100%;
        height: 27%;
        background: var(--data-size-box-color);
        display: flex;
        justify-content: center;
        align-items: center;
        color: rgba(51, 51, 51, 0.6);
        border: 0.03px solid rgba(51, 51, 51, 0.1);
        border-radius: 7px;
        cursor: pointer;
      }
    }

    :nth-child(6) {
      grid-area: 6 / 1 / 7 / 4;
      display: flex;
      justify-content: center;
      align-items: center;
      font-size: 18px;
      color: var(--text-color-2);
      cursor: pointer;
      background: var(--config-botton-color);
    }
  }

  // ------------------ 取模设置 -------------------
  #img-data-config {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    grid-template-rows: repeat(7, 1fr);
    column-gap: 4px;
    row-gap: 4px;
    color: var(--text-color-1);
    box-shadow: 0 0 0 4px rgba(255, 255, 255, 0.8);

    > div {
      border: none;
      background: var(--data-config-title-box-color);
      border-radius: 10px;
    }
    div {
      border: none;
      display: flex;
      justify-content: center;
      align-items: center;
    }

    .div1 { grid-area: 1 / 1 / 2 / 2; font-size: 12px; }
    .div2 {
      grid-area: 1 / 2 / 2 / 5;
      justify-content: space-around;
      background: var(--data-config-func-box-color);
      font-size: 14.5px;
      gap: 4px;
      padding: 4px;
      > div {
        flex: 1;
        height: 100%;
        cursor: pointer;
        border-radius: 8px;
        transition: all 0.2s;
      }
      > div.active {
        background: #4a90d9 !important;
        color: white;
      }
    }

    .div3 { grid-area: 2 / 1 / 3 / 5; font-size: 13px; }
    .div4 {
      grid-area: 3 / 1 / 5 / 5;
      justify-content: space-between;
      align-items: center;
      padding: 4px;
      background: var(--data-config-func-box-color);
      gap: 4px;
      > div {
        flex: 1;
        height: 100%;
        writing-mode: tb-rl;
        cursor: pointer;
        font-size: 14px;
        border-radius: 8px;
        transition: all 0.2s;
      }
      > div.active {
        background: #4a90d9 !important;
        color: white;
      }
    }

    .div5 { grid-area: 5 / 1 / 6 / 3; font-size: 13px; }
    .div6 {
      grid-area: 6 / 1 / 8 / 3;
      justify-content: space-around;
      align-items: center;
      padding: 4px;
      background: var(--data-config-func-box-color);
      gap: 4px;
      > div {
        flex: 1;
        height: 100%;
        writing-mode: tb-rl;
        cursor: pointer;
        font-size: 13px;
        border-radius: 8px;
        transition: all 0.2s;
      }
      > div.active {
        background: #4a90d9 !important;
        color: white;
      }
    }

    .div7 { grid-area: 5 / 3 / 6 / 5; font-size: 13px; }
    .div8 {
      grid-area: 6 / 3 / 8 / 5;
      justify-content: space-around;
      align-items: center;
      padding: 4px;
      background: var(--data-config-func-box-color);
      gap: 4px;
      > div {
        flex: 1;
        height: 100%;
        writing-mode: tb-rl;
        cursor: pointer;
        font-size: 12px;
        border-radius: 8px;
        transition: all 0.2s;
      }
      > div.active {
        background: #4a90d9 !important;
        color: white;
      }
    }
  }
}
</style>
