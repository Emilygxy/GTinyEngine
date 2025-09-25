# VideoPlayer FFmpeg 集成说明

## 概述

VideoPlayer现在集成了FFmpeg库，能够真正解析和解码各种视频格式。这个实现提供了完整的视频播放功能，包括真实的视频解码、格式转换和帧缓存。

## 主要改进

### 1. 真实的视频解码
- 使用FFmpeg的`avformat`和`avcodec`库进行视频文件解析
- 支持多种视频格式（MP4、AVI、MOV、MKV等）
- 自动检测视频编解码器并选择合适的解码器

### 2. 完整的视频信息获取
- **宽度和高度**: 从视频流中获取真实的视频尺寸
- **帧率**: 从视频流的时间基准计算真实帧率
- **时长**: 从容器格式中获取视频总时长
- **编解码器信息**: 显示使用的视频编解码器

### 3. 像素格式转换
- 使用`libswscale`进行像素格式转换
- 将各种输入格式转换为RGB24格式
- 支持不同的颜色空间和像素排列

## 技术实现

### FFmpeg组件使用

```cpp
// 主要FFmpeg组件
AVFormatContext* formatContext_;  // 格式上下文，处理容器格式
AVCodecContext* codecContext_;    // 编解码器上下文
AVCodec* codec_;                 // 编解码器
AVFrame* frame_;                  // 原始解码帧
AVFrame* frameRGB_;              // RGB转换后的帧
AVPacket* packet_;               // 数据包
SwsContext* swsContext_;         // 图像缩放/转换上下文
```

### 视频加载流程

1. **文件验证**: 检查文件是否存在
2. **格式解析**: 使用`avformat_open_input`打开视频文件
3. **流信息获取**: 使用`avformat_find_stream_info`获取流信息
4. **视频流定位**: 查找视频流索引
5. **解码器设置**: 创建并配置解码器上下文
6. **内存分配**: 分配帧和包的内存
7. **格式转换**: 设置RGB转换上下文

### 解码流程

1. **包读取**: 使用`av_read_frame`读取数据包
2. **包发送**: 使用`avcodec_send_packet`发送到解码器
3. **帧接收**: 使用`avcodec_receive_frame`接收解码帧
4. **格式转换**: 使用`sws_scale`转换为RGB格式
5. **数据复制**: 将RGB数据复制到VideoFrame结构

## 使用方法

### 基本使用

```cpp
// 创建视频播放器
auto videoPlayer = std::make_shared<te::VideoPlayer>();

// 加载视频文件
if (videoPlayer->LoadVideo("path/to/video.mp4")) {
    // 获取视频信息
    int width = videoPlayer->GetWidth();
    int height = videoPlayer->GetHeight();
    double frameRate = videoPlayer->GetFrameRate();
    double duration = videoPlayer->GetDuration();
    
    // 开始播放
    videoPlayer->Play();
    
    // 在渲染循环中更新
    while (playing) {
        videoPlayer->Update();
        // 渲染...
    }
}
```

### 支持的视频格式

- **容器格式**: MP4, AVI, MOV, MKV, FLV, WebM
- **视频编解码器**: H.264, H.265, VP8, VP9, AV1
- **像素格式**: 自动转换为RGB24

### 错误处理

```cpp
if (!videoPlayer->LoadVideo("video.mp4")) {
    std::cout << "Failed to load video" << std::endl;
    // 处理错误...
}
```

## 性能优化

### 1. 多线程解码
- 解码在独立线程中进行
- 使用RingBuffer缓存解码后的帧
- 避免阻塞主渲染线程

### 2. 内存管理
- 使用FFmpeg的内存管理函数
- 自动清理资源
- 避免内存泄漏

### 3. 格式转换优化
- 使用高效的SWS算法
- 缓存转换上下文
- 批量处理像素数据

## 调试信息

VideoPlayer会输出详细的调试信息：

```
VideoPlayer::LoadVideo - Loading: video.mp4
VideoPlayer::LoadVideo - Video loaded successfully
  Width: 1920, Height: 1080
  Frame Rate: 30 fps
  Duration: 120.5 seconds
  Codec: h264
```

## 故障排除

### 常见问题

1. **文件无法打开**
   - 检查文件路径是否正确
   - 确认文件格式是否支持
   - 检查文件是否损坏

2. **解码器不支持**
   - 确认FFmpeg编译时包含了相应的解码器
   - 检查视频编解码器是否常见

3. **内存不足**
   - 检查视频分辨率是否过大
   - 调整RingBuffer大小
   - 监控内存使用情况

### 调试技巧

1. **启用详细日志**
   ```cpp
   // 在FFmpeg初始化前设置日志级别
   av_log_set_level(AV_LOG_DEBUG);
   ```

2. **检查FFmpeg版本**
   ```cpp
   std::cout << "FFmpeg version: " << av_version_info() << std::endl;
   ```

3. **验证编解码器支持**
   ```cpp
   const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
   if (!codec) {
       std::cout << "H.264 decoder not available" << std::endl;
   }
   ```

## 未来增强

1. **硬件加速**: 集成GPU解码支持
2. **音频支持**: 添加音频流处理
3. **流媒体**: 支持网络视频流
4. **更多格式**: 支持更多视频格式
5. **实时处理**: 支持实时视频处理

## 依赖项

- FFmpeg 4.0+
- libavformat
- libavcodec
- libavutil
- libswscale

确保在CMakeLists.txt中正确链接这些库：

```cmake
target_link_libraries(GTinyEngine
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/lib/avcodec.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/lib/avformat.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/lib/avutil.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ffmpeg/lib/swscale.lib
)
```

这个FFmpeg集成为GTinyEngine提供了强大的视频播放能力，支持真实的视频文件解码和渲染。




