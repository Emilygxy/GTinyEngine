以下是TensorRT与3DGS集成的核心代码片段，涵盖模型加载、推理执行、与Vulkan渲染管线交互等关键环节，适配自研引擎的工程化需求：

## 1. 核心数据结构定义（高斯点参数与TensorRT输入输出）
```c++
// 3DGS高斯点参数（与训练时保持一致）
struct GaussParams {
    glm::vec3 position;    // 世界空间位置
    glm::vec3 scale;       // 缩放因子
    glm::vec4 rotation;    // 四元数旋转（w,x,y,z）
    float opacity;         // 不透明度
    std::array<float, 16> sh_coeffs; // 16个SH颜色系数（4阶）
};

// TensorRT推理输入（每个高斯点的特征向量）
struct TensorRTInput {
    std::vector<float> data; // 按position(3)+scale(3)+rotation(4)+opacity(1)+sh_coeffs(16)拼接
};

// TensorRT推理输出（每个高斯点的最终颜色，RGB）
struct TensorRTOutput {
    std::vector<glm::vec3> colors;
};
```
## 2. TensorRT引擎初始化（加载3DGS的ONNX模型）
```c++
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime.h>
#include <glm/glm.hpp>

// TensorRT日志工具
class TRTLogger : public nvinfer1::ILogger {
    void log(Severity severity, const char* msg) override {
        if (severity != Severity::kINFO) { // 过滤INFO日志
            printf("[TRT] %s\n", msg);
        }
    }
} g_trt_logger;

// TensorRT核心组件
struct TRTEngine {
    nvinfer1::ICudaEngine* engine = nullptr;
    nvinfer1::IExecutionContext* context = nullptr;
    cudaStream_t stream; // 异步流，用于并行推理与渲染

    ~TRTEngine() {
        if (context) context->destroy();
        if (engine) engine->destroy();
        if (stream) cudaStreamDestroy(stream);
    }
};

// 从ONNX模型创建TensorRT引擎
std::unique_ptr<TRTEngine> createTRTEngine(const std::string& onnx_path) {
    auto engine = std::make_unique<TRTEngine>();
    cudaStreamCreate(&engine->stream);

    // 1. 创建Builder与Network
    nvinfer1::IBuilder* builder = nvinfer1::createInferBuilder(g_trt_logger);
    const uint32_t explicit_batch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
    nvinfer1::INetworkDefinition* network = builder->createNetworkV2(explicit_batch);

    // 2. 解析ONNX模型
    auto parser = nvonnxparser::createParser(*network, g_trt_logger);
    if (!parser->parseFromFile(onnx_path.c_str(), static_cast<int>(nvinfer1::ILogger::Severity::kERROR))) {
        throw std::runtime_error("ONNX parse failed");
    }

    // 3. 配置推理参数（FP16优化+工作空间）
    nvinfer1::IBuilderConfig* config = builder->createBuilderConfig();
    config->setFlag(nvinfer1::BuilderFlag::kFP16); // 启用FP16量化
    config->setMaxWorkspaceSize(1ULL << 30); // 1GB工作空间

    // 4. 构建引擎
    engine->engine = builder->buildEngineWithConfig(*network, *config);
    engine->context = engine->engine->createExecutionContext();

    // 释放临时资源
    parser->destroy();
    network->destroy();
    config->destroy();
    builder->destroy();

    return engine;
}
```
## 3. 3. 高斯点数据预处理与GPU内存管理
```c++
// 将CPU端高斯点参数转换为TensorRT输入格式
TensorRTInput prepareTRTInput(const std::vector<GaussParams>& gaussians) {
    TensorRTInput input;
    input.data.reserve(gaussians.size() * (3 + 3 + 4 + 1 + 16)); // 预分配内存
    for (const auto& g : gaussians) {
        // 按顺序拼接特征：position -> scale -> rotation -> opacity -> sh_coeffs
        input.data.insert(input.data.end(), glm::value_ptr(g.position), glm::value_ptr(g.position) + 3);
        input.data.insert(input.data.end(), glm::value_ptr(g.scale), glm::value_ptr(g.scale) + 3);
        input.data.insert(input.data.end(), glm::value_ptr(g.rotation), glm::value_ptr(g.rotation) + 4);
        input.data.push_back(g.opacity);
        input.data.insert(input.data.end(), g.sh_coeffs.begin(), g.sh_coeffs.end());
    }
    return input;
}

// 分配GPU内存（输入/输出缓冲）
struct CudaBuffers {
    float* d_input = nullptr;  // TensorRT输入（GPU端）
    float* d_output = nullptr; // TensorRT输出（GPU端）
    size_t input_size = 0;
    size_t output_size = 0;

    ~CudaBuffers() {
        if (d_input) cudaFree(d_input);
        if (d_output) cudaFree(d_output);
    }

    void allocate(size_t input_bytes, size_t output_bytes) {
        input_size = input_bytes;
        output_size = output_bytes;
        cudaMalloc(&d_input, input_size);
        cudaMalloc(&d_output, output_size);
    }
};
```
## 4. 执行TensorRT推理（获取高斯点颜色）
```c++
// 执行推理并返回结果
TensorRTOutput runInference(TRTEngine& trt_engine, CudaBuffers& buffers, const TensorRTInput& input) {
    // 1. 拷贝输入数据到GPU（异步）
    cudaMemcpyAsync(buffers.d_input, input.data.data(), buffers.input_size, 
                   cudaMemcpyHostToDevice, trt_engine.stream);

    // 2. 设置TensorRT绑定（输入输出地址）
    void* bindings[] = {buffers.d_input, buffers.d_output};
    trt_engine.context->enqueueV2(bindings, trt_engine.stream, nullptr);

    // 3. 拷贝输出结果到CPU（异步等待）
    TensorRTOutput output;
    output.colors.resize(input.data.size() / (3+3+4+1+16)); // 每个高斯点对应一个颜色
    cudaMemcpyAsync(output.colors.data(), buffers.d_output, buffers.output_size,
                   cudaMemcpyDeviceToHost, trt_engine.stream);
    cudaStreamSynchronize(trt_engine.stream); // 等待推理完成

    return output;
}
```
## 5. 与Vulkan渲染管线集成（更新GS点云颜色）
```c++
// Vulkan缓冲更新：将TensorRT输出的颜色绑定到GS渲染的SSBO
void updateGaussColorsVulkan(VkDevice device, VkBuffer gauss_color_ssbo, const TensorRTOutput& output) {
    // 映射SSBO到CPU内存
    void* mapped_memory;
    vkMapMemory(device, gauss_color_memory, 0, output.colors.size() * sizeof(glm::vec3), 0, &mapped_memory);
    
    // 拷贝颜色数据
    memcpy(mapped_memory, output.colors.data(), output.colors.size() * sizeof(glm::vec3));
    
    // 解绑内存并触发缓冲可见性屏障
    vkUnmapMemory(device, gauss_color_memory);
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(
        vulkan_command_buffer,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr
    );
}
```
## 6. 完整流程示例
```c++
int main() {
    // 1. 加载3DGS模型（PLY文件）
    std::vector<GaussParams> gaussians = load3DGSPly("model.ply");

    // 2. 初始化TensorRT引擎
    auto trt_engine = createTRTEngine("3dgs_color_pred.onnx");
    if (!trt_engine->engine) {
        return -1;
    }

    // 3. 准备输入数据与GPU缓冲
    auto trt_input = prepareTRTInput(gaussians);
    size_t input_bytes = trt_input.data.size() * sizeof(float);
    size_t output_bytes = gaussians.size() * sizeof(glm::vec3);
    CudaBuffers buffers;
    buffers.allocate(input_bytes, output_bytes);

    // 4. 执行推理
    auto trt_output = runInference(*trt_engine, buffers, trt_input);

    // 5. 更新Vulkan渲染缓冲，执行3DGS渲染
    updateGaussColorsVulkan(vk_device, gauss_color_ssbo, trt_output);
    render3DGS(vk_command_buffer); // 调用Vulkan Compute Shader渲染

    return 0;
}
```
## 关键说明
1. 模型适配：示例中的ONNX模型需提前训练，输入为高斯点的原始参数，输出为经光照/材质处理的最终颜色（替代传统SH解码逻辑）。

2. 性能优化：通过cudaStream实现推理与渲染并行，FP16量化可降低显存占用并提升速度（需确保颜色精度损失在可接受范围）。

3. Vulkan交互：重点通过内存屏障确保CPU写入的颜色数据能被GPU Compute Shader正确读取，避免数据同步问题。

可根据自研引擎的内存管理、渲染管线设计调整细节，核心是保证TensorRT推理结果与3DGS渲染流程的高效衔接。