<script setup lang="ts">
import { useScreenStore } from '../stores/store'
import { getItem, setItem } from '../utils/storage'
import { onMounted, reactive } from 'vue'
import { XBox } from 'ilx1-x-box'
import { useConfigStore } from '@/stores/configStore'

const configStore = useConfigStore()
const win = window as any
const screenStore = useScreenStore()
const presetArray: object[] = reactive([{}, {}, {}])
let presetCount: number = 0

// 直接用 Canvas API 缩放图片，返回 base64
const resizeImage = (base64: string, w: number, h: number, gray: number): Promise<string> => {
  return new Promise((resolve, reject) => {
    const img = new Image()
    img.onload = () => {
      const canvas = document.createElement('canvas')
      canvas.width = w
      canvas.height = h
      const ctx = canvas.getContext('2d')!
      ctx.drawImage(img, 0, 0, w, h)
      if (gray === 1) {
        const imageData = ctx.getImageData(0, 0, w, h)
        const d = imageData.data
        for (let i = 0; i < d.length; i += 4) {
          const grayVal = d[i] * 0.299 + d[i + 1] * 0.587 + d[i + 2] * 0.114
          d[i] = d[i + 1] = d[i + 2] = grayVal
        }
        ctx.putImageData(imageData, 0, 0)
      }
      resolve(canvas.toDataURL('image/png'))
    }
    img.onerror = () => reject(new Error('图片加载失败'))
    img.src = base64
  })
}

// 单色取模：逐行式，从左到右，从上到下
const monoSamplingRow = (pixels: number[], imgW: number, imgH: number, config: number[]): number[] => {
  const byteW = Math.ceil(imgW / 8)
  const result = new Uint8Array(byteW * imgH)
  const latticeInvert = config[0] === 1  // 阴码
  const dirReverse = config[2] === 0     // 逆向

  for (let i = 0; i < pixels.length; i++) {
    const x = i % imgW
    const y = Math.floor(i / imgW)
    const byteIdx = y * byteW + Math.floor(x / 8)
    let bitIdx = x % 8
    if (dirReverse) bitIdx = 7 - bitIdx

    let val = pixels[i]
    if (latticeInvert) val = val === 0 ? 1 : 0
    if (val === 0) {
      result[byteIdx] |= (1 << bitIdx)
    } else {
      result[byteIdx] &= ~(1 << bitIdx)
    }
  }
  return Array.from(result)
}

// 彩色取模：RGB565
const colorSampling = (imageData: ImageData, config: number[]): number[] => {
  const w = imageData.width, h = imageData.height
  const d = imageData.data
  const result: number[] = []
  const latticeInvert = config[0] === 1
  const dirReverse = config[2] === 0

  // 逐行式扫描
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      let px = dirReverse ? (w - 1 - x) : x
      const idx = (y * w + px) * 4
      const r = d[idx] >> 3
      const g = d[idx + 1] >> 2
      const b = d[idx + 2] >> 3
      let hi = (r << 3) | (g >> 3)
      let lo = ((g & 0x07) << 5) | b
      if (latticeInvert) { hi ^= 0xFF; lo ^= 0xFF }
      result.push(hi, lo)
    }
  }
  return result
}

const imgHandle = async () => {
  const baseData = configStore.screenData.baseData || getItem('screenDataBase64')
  if (!baseData || baseData == '') {
    XBox.popMes('请先导入一张图片！')
    return
  }
  const w = screenStore.resizeWidth
  const h = screenStore.resizeHeight
  if (w == 0 || h == 0) {
    XBox.popMes('请设置图片的大小！')
    return
  }

  screenStore.setWaitExecute(true)
  try {
    const isMono = screenStore.configArray[4] === 1
    const threshold = screenStore.thresholdData

    // 缩放图片
    const resized = await resizeImage(baseData, w, h, isMono ? 1 : 0)

    // 加载缩放后的图片提取像素
    const img = await new Promise<HTMLImageElement>((resolve, reject) => {
      const i = new Image()
      i.onload = () => resolve(i)
      i.onerror = () => reject(new Error('缩放后图片加载失败'))
      i.src = resized
    })

    const canvas = document.createElement('canvas')
    canvas.width = w
    canvas.height = h
    const ctx = canvas.getContext('2d')!
    ctx.drawImage(img, 0, 0)
    const imageData = ctx.getImageData(0, 0, w, h)

    let arrData: number[]
    if (isMono) {
      // 单色取模
      const pixels: number[] = []
      for (let i = 0; i < imageData.data.length; i += 4) {
        const gray = imageData.data[i] * 0.299 + imageData.data[i + 1] * 0.587 + imageData.data[i + 2] * 0.114
        pixels.push(gray > threshold ? 1 : 0)
      }
      arrData = monoSamplingRow(pixels, w, h, screenStore.configArray)
    } else {
      arrData = colorSampling(imageData, screenStore.configArray)
    }

    // 格式化输出
    const useHex = screenStore.configArray[3] === 0
    let resultStr: string
    if (useHex) {
      resultStr = arrData.map(v => '0x' + (v & 0xFF).toString(16).padStart(2, '0')).join(', ')
    } else {
      resultStr = arrData.join(', ')
    }

    screenStore.setDataLength(arrData.length)
    screenStore.setResultString(resultStr)
    configStore.showPop('生成成功！')
    screenStore.setCountModify(true)
    screenStore.setPreCount(presetCount)
  } catch (err: any) {
    console.error('取模失败:', err)
    XBox.popMes('生成失败：' + (err.message || '未知错误'))
  }
  screenStore.setWaitExecute(false)
}

// 视频取模处理
const videoHandle = async () => {
  if (screenStore.resizeWidth == 0 || screenStore.resizeHeight == 0) {
    XBox.popMes('请设置要取模的大小！')
    return
  }
  // console.info(screenStore.videoPath)
  if (screenStore.videoPath == '') {
    XBox.popMes('请选择一个视频文件！')
    return
  }
  if (screenStore.videoDur == 0 || screenStore.videoFrame == 0) {
    XBox.popMes('请配置时长与帧数！')
    return
  }
  screenStore.setWaitExecute(true)
  const data = await win.api.getVideoFrameData(
    screenStore.videoPath,
    screenStore.resizeWidth,
    screenStore.resizeHeight,
    screenStore.videoDur,
    screenStore.videoFrame,
    screenStore.thresholdData,
    ...screenStore.configArray
  )
  // console.info(data)
  let result = ''
  data.forEach(o => {
    result += `{${o.join(',')}},`
  })
  // console.info(data[0].length)
  screenStore.setDataLength(data[0].length)
  screenStore.setResultString(result)
  XBox.popMes('生成成功！')
  screenStore.setCountModify(true)
  screenStore.setPreCount(presetCount)
  screenStore.setWaitExecute(false)
}

// 图片大小配置预设
const imgPreSet = () => {
  let preA = JSON.parse(getItem('presetArray'))
  if (preA != null) {
    // while (Object.keys(preA[presetCount]).length != 0 && presetCount <= 2) presetCount++
    // if(presetCount == 2) presetCount = 0
    preA[presetCount] = {
      width: screenStore.resizeWidth,
      height: screenStore.resizeHeight,
    }
    setItem('presetArray', JSON.stringify(preA))
  } else {
    presetArray[presetCount] = {
      width: screenStore.resizeWidth,
      height: screenStore.resizeHeight,
    }
    setItem('presetArray', JSON.stringify(presetArray))
  }
  presetCount >= 2 ? (presetCount = 0) : presetCount++
}

// 提交处理
const commit = async () => {
  // 图片取模
  if (!screenStore.curMode) {
    // 处理图片
    await imgHandle()
    // 记录处理的大小预设
    imgPreSet()
    // 视频取模
  } else {
    await videoHandle()
    // 记录处理的大小预设
    imgPreSet()
  }
}
</script>

<template>
  <div
    id="commit-box-content"
    @click="commit"
  >
    提交生成
  </div>
</template>

<style lang="scss">
#commit-box-content {
  width: 100%;
  height: 100%;
  display: flex;
  justify-content: center;
  align-items: center;
  border: none;
  font-size: 20px;
  color: rgba(51, 51, 51, 0.7);
  cursor: pointer;
  background: var(--commit-box-color);
}
</style>
