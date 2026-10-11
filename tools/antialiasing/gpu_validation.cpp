// SPDX-License-Identifier: GPL-2.0-or-later
// GPU regression harness: real Vulkan dispatch and readback of presentation
// shaders.
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "AreaTex.h"
#include "SearchTex.h"
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
constexpr vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1,
                                          0, 1};
struct Image {
  vk::Image handle;
  vk::DeviceMemory memory;
  vk::ImageView view;
  unsigned w, h;
};
struct Buffer {
  vk::Buffer handle;
  vk::DeviceMemory memory;
  size_t size;
};
int main(int argc, char **argv) try {
  if (argc != 2)
    throw std::runtime_error("usage: gpu_validation SHADER_DIRECTORY");
  vk::detail::DynamicLoader loader;
  auto get_proc =
      loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
  VULKAN_HPP_DEFAULT_DISPATCHER.init(get_proc);
  const vk::ApplicationInfo app{.pApplicationName = "Azahar AA GPU test",
                                .apiVersion = VK_API_VERSION_1_1};
  const auto instance = vk::createInstance({.pApplicationInfo = &app});
  VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
  const auto physical = instance.enumeratePhysicalDevices().at(0);
  std::cout << physical.getProperties().deviceName << '\n';
  const auto queues = physical.getQueueFamilyProperties();
  unsigned family = 0;
  while (!(queues.at(family).queueFlags & vk::QueueFlagBits::eCompute))
    ++family;
  const float priority = 1;
  const vk::DeviceQueueCreateInfo qinfo{.queueFamilyIndex = family,
                                        .queueCount = 1,
                                        .pQueuePriorities = &priority};
  const auto device = physical.createDevice(
      {.queueCreateInfoCount = 1, .pQueueCreateInfos = &qinfo});
  VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
  const auto queue = device.getQueue(family, 0);
  auto memory_type = [&](unsigned bits, vk::MemoryPropertyFlags flags) {
    const auto props = physical.getMemoryProperties();
    for (unsigned i = 0; i < props.memoryTypeCount; ++i)
      if ((bits & (1u << i)) &&
          (props.memoryTypes[i].propertyFlags & flags) == flags)
        return i;
    throw std::runtime_error("memory type");
  };
  std::vector<Image> images;
  auto image = [&](unsigned w, unsigned h) {
    Image img{.w = w, .h = h};
    img.handle =
        device.createImage({.imageType = vk::ImageType::e2D,
                            .format = vk::Format::eR8G8B8A8Unorm,
                            .extent = {w, h, 1},
                            .mipLevels = 1,
                            .arrayLayers = 1,
                            .samples = vk::SampleCountFlagBits::e1,
                            .usage = vk::ImageUsageFlagBits::eSampled |
                                     vk::ImageUsageFlagBits::eStorage |
                                     vk::ImageUsageFlagBits::eTransferSrc |
                                     vk::ImageUsageFlagBits::eTransferDst});
    const auto req = device.getImageMemoryRequirements(img.handle);
    img.memory = device.allocateMemory(
        {.allocationSize = req.size,
         .memoryTypeIndex = memory_type(
             req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)});
    device.bindImageMemory(img.handle, img.memory, 0);
    img.view = device.createImageView({.image = img.handle,
                                       .viewType = vk::ImageViewType::e2D,
                                       .format = vk::Format::eR8G8B8A8Unorm,
                                       .subresourceRange = range});
    images.push_back(img);
    return img;
  };
  std::vector<Buffer> buffers;
  auto buffer = [&](size_t size) {
    Buffer b{.size = size};
    b.handle =
        device.createBuffer({.size = size,
                             .usage = vk::BufferUsageFlagBits::eTransferSrc |
                                      vk::BufferUsageFlagBits::eTransferDst});
    const auto req = device.getBufferMemoryRequirements(b.handle);
    b.memory = device.allocateMemory(
        {.allocationSize = req.size,
         .memoryTypeIndex =
             memory_type(req.memoryTypeBits,
                         vk::MemoryPropertyFlagBits::eHostVisible |
                             vk::MemoryPropertyFlagBits::eHostCoherent)});
    device.bindBufferMemory(b.handle, b.memory, 0);
    buffers.push_back(b);
    return b;
  };
  const auto pool = device.createCommandPool(
      {.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
       .queueFamilyIndex = family});
  const auto cmd =
      device.allocateCommandBuffers({.commandPool = pool,
                                     .level = vk::CommandBufferLevel::ePrimary,
                                     .commandBufferCount = 1})[0];
  auto begin = [&]() {
    cmd.reset();
    cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  };
  auto finish = [&]() {
    cmd.end();
    const vk::SubmitInfo info{.commandBufferCount = 1, .pCommandBuffers = &cmd};
    queue.submit(info);
    queue.waitIdle();
  };
  auto barrier = [&]() {
    const vk::MemoryBarrier b{
        .srcAccessMask =
            vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eMemoryRead,
        .dstAccessMask =
            vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eMemoryRead};
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                        vk::PipelineStageFlagBits::eAllCommands, {}, b, {}, {});
  };
  auto upload = [&](Image img, const std::vector<unsigned char> &bytes) {
    auto b = buffer(bytes.size());
    auto data = device.mapMemory(b.memory, 0, b.size);
    std::memcpy(data, bytes.data(), b.size);
    device.unmapMemory(b.memory);
    begin();
    const vk::ImageMemoryBarrier transition{
        .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eGeneral,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = img.handle,
        .subresourceRange = range};
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                        vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
                        transition);
    const vk::BufferImageCopy copy{
        .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .imageExtent = {img.w, img.h, 1}};
    cmd.copyBufferToImage(b.handle, img.handle, vk::ImageLayout::eGeneral,
                          copy);
    barrier();
    finish();
  };
  auto lookup = [&](const unsigned char *bytes, unsigned w, unsigned h,
                    unsigned channels) {
    auto img = image(w, h);
    std::vector<unsigned char> rgba(w * h * 4, 0);
    for (unsigned p = 0; p < w * h; ++p)
      for (unsigned c = 0; c < channels; ++c)
        rgba[p * 4 + c] = bytes[p * channels + c];
    upload(img, rgba);
    return img;
  };
  const auto area = lookup(areaTexBytes, AREATEX_WIDTH, AREATEX_HEIGHT, 2);
  const auto search =
      lookup(searchTexBytes, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1);
  constexpr unsigned w = 127, h = 93;
  auto input = image(w, h);
  std::array<Image, 4> targets;
  for (auto &t : targets) {
    t = image(w, h);
    upload(t, std::vector<unsigned char>(w * h * 4, 0));
  }
  auto upload_input = [&](const std::vector<unsigned char> &bytes) {
    // Subsequent uploads keep GENERAL layout.
    auto b = buffer(bytes.size());
    auto p = device.mapMemory(b.memory, 0, b.size);
    std::memcpy(p, bytes.data(), b.size);
    device.unmapMemory(b.memory);
    begin();
    barrier();
    const vk::BufferImageCopy copy{
        .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .imageExtent = {w, h, 1}};
    cmd.copyBufferToImage(b.handle, input.handle, vk::ImageLayout::eGeneral,
                          copy);
    barrier();
    finish();
  };
  upload(input, std::vector<unsigned char>(w * h * 4, 0));
  const auto sampler = device.createSampler(
      {.magFilter = vk::Filter::eLinear,
       .minFilter = vk::Filter::eLinear,
       .addressModeU = vk::SamplerAddressMode::eClampToEdge,
       .addressModeV = vk::SamplerAddressMode::eClampToEdge,
       .addressModeW = vk::SamplerAddressMode::eClampToEdge});
  std::array<vk::DescriptorSetLayoutBinding, 5> bindings;
  for (unsigned i = 0; i < 5; ++i)
    bindings[i] = {i,
                   i == 1 ? vk::DescriptorType::eStorageImage
                          : vk::DescriptorType::eCombinedImageSampler,
                   1, vk::ShaderStageFlagBits::eCompute};
  const auto set_layout = device.createDescriptorSetLayout(
      {.bindingCount = 5, .pBindings = bindings.data()});
  const std::array sizes{
      vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 16},
      vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 4}};
  const auto descriptor_pool = device.createDescriptorPool(
      {.maxSets = 4, .poolSizeCount = 2, .pPoolSizes = sizes.data()});
  const std::array layouts{set_layout, set_layout, set_layout, set_layout};
  const auto sets =
      device.allocateDescriptorSets({.descriptorPool = descriptor_pool,
                                     .descriptorSetCount = 4,
                                     .pSetLayouts = layouts.data()});
  struct Params {
    std::array<float, 4> crop;
    std::array<float, 2> size;
    float reserved;
  };
  const vk::PushConstantRange push{vk::ShaderStageFlagBits::eCompute, 0,
                                   sizeof(Params)};
  const auto layout =
      device.createPipelineLayout({.setLayoutCount = 1,
                                   .pSetLayouts = &set_layout,
                                   .pushConstantRangeCount = 1,
                                   .pPushConstantRanges = &push});
  std::array<vk::Pipeline, 5> pipelines;
  const std::array names{"aa_copy", "fxaa", "smaa_edges", "smaa_weights",
                         "smaa_blend"};
  for (unsigned i = 0; i < 5; ++i) {
    std::ifstream file(std::string(argv[1]) + "/" + names[i] + ".spv",
                       std::ios::binary | std::ios::ate);
    if (!file)
      throw std::runtime_error("shader file");
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<uint32_t> spv(size / 4);
    file.read(reinterpret_cast<char *>(spv.data()), size);
    const auto module =
        device.createShaderModule({.codeSize = size, .pCode = spv.data()});
    pipelines[i] =
        device
            .createComputePipeline(
                {}, {.stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                               .module = module,
                               .pName = "main"},
                     .layout = layout})
            .value;
    device.destroyShaderModule(module);
  }
  auto run = [&](unsigned mode) {
    const unsigned count = mode == 1 ? 2 : 4;
    const std::array<unsigned, 4> passes{0, mode == 1 ? 1u : 2u, 3, 4};
    begin();
    for (unsigned i = 0; i < count; ++i) {
      unsigned pass = passes[i];
      const auto src = pass == 0 ? input : pass == 3 ? targets[1] : targets[0];
      const std::array views{src.view, targets[i].view,
                             pass == 4 ? targets[2].view : src.view, area.view,
                             search.view};
      std::array<vk::DescriptorImageInfo, 5> infos;
      std::array<vk::WriteDescriptorSet, 5> writes;
      for (unsigned b = 0; b < 5; ++b) {
        infos[b] = {sampler, views[b], vk::ImageLayout::eGeneral};
        writes[b] = {.dstSet = sets[i],
                     .dstBinding = b,
                     .descriptorCount = 1,
                     .descriptorType =
                         b == 1 ? vk::DescriptorType::eStorageImage
                                : vk::DescriptorType::eCombinedImageSampler,
                     .pImageInfo = &infos[b]};
      }
      device.updateDescriptorSets(writes, {});
      barrier();
      cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipelines[pass]);
      cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0,
                             sets[i], {});
      const Params params{{0, 0, 1, 1}, {float(w), float(h)}, 0};
      cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0,
                        sizeof(params), &params);
      cmd.dispatch((w + 7) / 8, (h + 7) / 8, 1);
    }
    barrier();
    auto b = buffer(w * h * 4);
    const vk::BufferImageCopy copy{
        .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .imageExtent = {w, h, 1}};
    cmd.copyImageToBuffer(targets[count - 1].handle, vk::ImageLayout::eGeneral,
                          b.handle, copy);
    barrier();
    finish();
    auto p = device.mapMemory(b.memory, 0, b.size);
    std::vector<unsigned char> result(b.size);
    std::memcpy(result.data(), p, b.size);
    device.unmapMemory(b.memory);
    return result;
  };
  std::vector<unsigned char> pattern(w * h * 4, 255);
  for (unsigned y = 0; y < h; ++y)
    for (unsigned x = 0; x < w; ++x) {
      bool white = int(x) > int(y) * 3 / 2 + 11;
      for (unsigned c = 0; c < 3; ++c)
        pattern[(y * w + x) * 4 + c] = white ? 255 : 0;
    }
  upload_input(pattern);
  for (unsigned mode = 1; mode <= 2; ++mode) {
    const auto result = run(mode);
    unsigned changed = 0, intermediate = 0;
    for (unsigned p = 0; p < w * h; ++p) {
      if (result[p * 4] != pattern[p * 4])
        ++changed;
      if (result[p * 4] > 0 && result[p * 4] < 255)
        ++intermediate;
      if (result[p * 4 + 3] != 255)
        throw std::runtime_error("output alpha");
    }
    std::cout << names[mode == 1 ? 1 : 4] << ": changed=" << changed
              << " intermediate=" << intermediate << '\n';
    if (changed < 20 || intermediate < 20 || changed > w * h / 3)
      throw std::runtime_error("AA edge regression");
  }
  std::vector<unsigned char> flat(w * h * 4, 255);
  for (unsigned p = 0; p < w * h; ++p)
    for (unsigned c = 0; c < 3; ++c)
      flat[p * 4 + c] = 96;
  upload_input(flat);
  for (unsigned mode = 1; mode <= 2; ++mode) {
    auto result = run(mode);
    if (result != flat)
      throw std::runtime_error("flat image changed / stale SMAA edges");
  }
  std::cout << "PASS: FXAA and SMAA smooth diagonal edges; both preserve a "
               "flat image after reuse; odd dimensions covered\n";
  device.waitIdle();
  for (auto p : pipelines)
    device.destroyPipeline(p);
  device.destroyPipelineLayout(layout);
  device.destroyDescriptorPool(descriptor_pool);
  device.destroyDescriptorSetLayout(set_layout);
  device.destroySampler(sampler);
  device.destroyCommandPool(pool);
  for (auto i : images) {
    device.destroyImageView(i.view);
    device.destroyImage(i.handle);
    device.freeMemory(i.memory);
  }
  for (auto b : buffers) {
    device.destroyBuffer(b.handle);
    device.freeMemory(b.memory);
  }
  device.destroy();
  instance.destroy();
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
